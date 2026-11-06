package server

import (
	"context"
	"errors"
	"strings"
	"sync"
	"time"

	"github.com/FlyingBassHot/meetagent/internal/node/adapter"
	"github.com/FlyingBassHot/meetagent/pkg/clientproto"
	"github.com/FlyingBassHot/meetagent/pkg/protocol"
)

const maxPromptBytes = 64 * 1024

func (s *Server) handleCommand(c *clientConn, cmd clientproto.Command) clientproto.CommandResult {
	started := time.Now()
	key := c.id + "\x00" + cmd.CommandID
	if cached, ok := s.commands.get(key); ok {
		return cached
	}

	finish := func(result clientproto.CommandResult) clientproto.CommandResult {
		result.DurationMS = time.Since(started).Milliseconds()
		result.CompletedAt = time.Now().UTC()
		s.commands.put(key, result)
		s.audit("command.finished",
			"client_id", c.id,
			"command_id", cmd.CommandID,
			"type", cmd.Type,
			"agent_id", cmd.AgentID,
			"session_id", cmd.SessionID,
			"status", string(result.Status),
			"duration_ms", result.DurationMS,
			"error", result.Error,
		)
		return result
	}

	if err := cmd.Validate(); err != nil {
		return finish(commandFailure(cmd, protocol.StatusRejected, err.Error()))
	}
	if cmd.NodeID != "" && cmd.NodeID != s.node.NodeID {
		return finish(commandFailure(cmd, protocol.StatusRejected, "command targets a different node"))
	}

	switch cmd.Type {
	case clientproto.CommandPing:
		return finish(commandSuccess(cmd, map[string]any{"pong": true, "server_time": time.Now().UTC()}))

	case clientproto.CommandNodeStatus:
		if !has(c.perms, clientproto.PermissionSessionRead) {
			return finish(commandFailure(cmd, protocol.StatusRejected, "missing permission: session.read"))
		}
		return finish(commandSuccess(cmd, map[string]any{
			"node":   s.node,
			"agents": s.manager.Agents(),
		}))

	case clientproto.CommandSessionList:
		if !has(c.perms, clientproto.PermissionSessionRead) {
			return finish(commandFailure(cmd, protocol.StatusRejected, "missing permission: session.read"))
		}
		return finish(s.listSessions(cmd))

	case clientproto.CommandSessionPrompt:
		if !has(c.perms, clientproto.PermissionSessionControl) {
			return finish(commandFailure(cmd, protocol.StatusRejected, "missing permission: session.control"))
		}
		if !hasAny(c.caps, clientproto.CapabilityInputText, clientproto.CapabilityInputVoice) {
			return finish(commandFailure(cmd, protocol.StatusRejected, "client does not declare input.text or input.voice"))
		}
		var payload clientproto.PromptPayload
		if err := cmd.DecodePayload(&payload); err != nil {
			return finish(commandFailure(cmd, protocol.StatusRejected, err.Error()))
		}
		if strings.TrimSpace(payload.Text) == "" {
			return finish(commandFailure(cmd, protocol.StatusRejected, "prompt text is empty"))
		}
		if len(payload.Text) > maxPromptBytes {
			return finish(commandFailure(cmd, protocol.StatusRejected, "prompt text is too large"))
		}
		if cmd.AgentID == "" || cmd.SessionID == "" {
			return finish(commandFailure(cmd, protocol.StatusRejected, "agent_id and session_id are required"))
		}
		ctx, cancel := context.WithTimeout(context.Background(), s.opts.CommandTimeout)
		defer cancel()
		result, err := s.manager.SendPrompt(ctx, cmd.AgentID, cmd.SessionID, payload.Text)
		if err != nil {
			return finish(commandError(cmd, err))
		}
		return finish(commandSuccess(cmd, map[string]any{"message_id": result.MessageID}))

	case clientproto.CommandSessionInterrupt:
		if !has(c.perms, clientproto.PermissionSessionControl) {
			return finish(commandFailure(cmd, protocol.StatusRejected, "missing permission: session.control"))
		}
		if cmd.AgentID == "" || cmd.SessionID == "" {
			return finish(commandFailure(cmd, protocol.StatusRejected, "agent_id and session_id are required"))
		}
		ctx, cancel := context.WithTimeout(context.Background(), s.opts.CommandTimeout)
		defer cancel()
		result, err := s.manager.Interrupt(ctx, cmd.AgentID, cmd.SessionID)
		if err != nil {
			return finish(commandError(cmd, err))
		}
		return finish(commandSuccess(cmd, map[string]any{"interrupted": result.Interrupted}))

	case clientproto.CommandPermissionRespond:
		if !has(c.perms, clientproto.PermissionPermissionRespond) {
			return finish(commandFailure(cmd, protocol.StatusRejected, "missing permission: permission.respond"))
		}
		var payload clientproto.PermissionResponsePayload
		if err := cmd.DecodePayload(&payload); err != nil {
			return finish(commandFailure(cmd, protocol.StatusRejected, err.Error()))
		}
		if payload.PermissionID == "" {
			return finish(commandFailure(cmd, protocol.StatusRejected, "permission_id is required"))
		}
		if cmd.AgentID == "" || cmd.SessionID == "" {
			return finish(commandFailure(cmd, protocol.StatusRejected, "agent_id and session_id are required"))
		}
		decision := normalizeDecision(payload.Decision)
		if decision == "" {
			return finish(commandFailure(cmd, protocol.StatusRejected, "decision must be once, always or reject"))
		}
		if !s.beginPermission(payload.PermissionID) {
			return finish(commandFailure(cmd, protocol.StatusRejected, "permission has already been handled"))
		}
		if !s.hub.HasPendingPermission(payload.PermissionID) {
			s.finishPermission(payload.PermissionID, false)
			return finish(commandFailure(cmd, protocol.StatusRejected, "permission is not pending or has expired"))
		}
		ctx, cancel := context.WithTimeout(context.Background(), s.opts.CommandTimeout)
		err := s.manager.RespondPermission(ctx, cmd.AgentID, cmd.SessionID, payload.PermissionID, decision, payload.Message)
		cancel()
		s.finishPermission(payload.PermissionID, err == nil)
		if err != nil {
			return finish(commandError(cmd, err))
		}
		return finish(commandSuccess(cmd, map[string]any{"permission_id": payload.PermissionID, "decision": decision}))

	default:
		return finish(commandFailure(cmd, protocol.StatusRejected, "unsupported command type: "+cmd.Type))
	}
}

func (s *Server) listSessions(cmd clientproto.Command) clientproto.CommandResult {
	var payload clientproto.ListSessionsPayload
	if len(cmd.Payload) > 0 {
		_ = cmd.DecodePayload(&payload)
	}
	agentID := strings.TrimSpace(cmd.AgentID)
	if agentID == "" {
		agentID = strings.TrimSpace(payload.AgentID)
	}
	if agentID == "" {
		sessions := make([]protocol.SessionView, 0)
		for _, agent := range s.manager.Agents() {
			sessions = append(sessions, agent.Sessions...)
		}
		return commandSuccess(cmd, map[string]any{"sessions": sessions})
	}
	ctx, cancel := context.WithTimeout(context.Background(), s.opts.CommandTimeout)
	defer cancel()
	sessions, err := s.manager.ListSessions(ctx, agentID)
	if err != nil {
		return commandError(cmd, err)
	}
	return commandSuccess(cmd, map[string]any{"sessions": sessions})
}

func commandSuccess(cmd clientproto.Command, data any) clientproto.CommandResult {
	return clientproto.CommandResult{
		SchemaVersion: clientproto.Version,
		CommandID:     cmd.CommandID,
		Status:        protocol.StatusCompleted,
		Data:          jsonRaw(data),
	}
}

func commandFailure(cmd clientproto.Command, status protocol.CommandStatus, message string) clientproto.CommandResult {
	return clientproto.CommandResult{
		SchemaVersion: clientproto.Version,
		CommandID:     cmd.CommandID,
		Status:        status,
		Error:         message,
	}
}

func commandError(cmd clientproto.Command, err error) clientproto.CommandResult {
	status := protocol.StatusFailed
	switch {
	case errors.Is(err, context.DeadlineExceeded):
		status = protocol.StatusTimeout
	case errors.Is(err, adapter.ErrNotSupported), errors.Is(err, adapter.ErrUnavailable):
		status = protocol.StatusRejected
	}
	return commandFailure(cmd, status, err.Error())
}

func normalizeDecision(value string) string {
	switch strings.ToLower(strings.TrimSpace(value)) {
	case "once", "approve", "approved", "allow":
		return "once"
	case "always":
		return "always"
	case "reject", "rejected", "deny":
		return "reject"
	default:
		return ""
	}
}

func has(values []string, target string) bool {
	for _, value := range values {
		if value == target {
			return true
		}
	}
	return false
}

func hasAny(values []string, targets ...string) bool {
	for _, target := range targets {
		if has(values, target) {
			return true
		}
	}
	return false
}

type cachedCommand struct {
	result  clientproto.CommandResult
	expires time.Time
}

type commandCache struct {
	mu      sync.Mutex
	entries map[string]cachedCommand
	ttl     time.Duration
	max     int
}

func newCommandCache(ttl time.Duration, max int) *commandCache {
	if ttl <= 0 {
		ttl = 5 * time.Minute
	}
	if max <= 0 {
		max = 1000
	}
	return &commandCache{entries: make(map[string]cachedCommand), ttl: ttl, max: max}
}

func (c *commandCache) get(key string) (clientproto.CommandResult, bool) {
	c.mu.Lock()
	defer c.mu.Unlock()
	entry, ok := c.entries[key]
	if !ok {
		return clientproto.CommandResult{}, false
	}
	if time.Now().After(entry.expires) {
		delete(c.entries, key)
		return clientproto.CommandResult{}, false
	}
	return entry.result, true
}

func (c *commandCache) put(key string, result clientproto.CommandResult) {
	c.mu.Lock()
	defer c.mu.Unlock()
	now := time.Now()
	for existing, entry := range c.entries {
		if now.After(entry.expires) {
			delete(c.entries, existing)
		}
	}
	if len(c.entries) >= c.max {
		for existing := range c.entries {
			delete(c.entries, existing)
			if len(c.entries) < c.max {
				break
			}
		}
	}
	c.entries[key] = cachedCommand{result: result, expires: now.Add(c.ttl)}
}
