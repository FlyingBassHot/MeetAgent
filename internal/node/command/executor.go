// Package command implements the Node-side whitelist executor. There is no
// generic shell passthrough: every action is implemented explicitly, checked
// against the local security policy and dispatched to an Agent Adapter.
package command

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"runtime"
	"strings"
	"time"

	"github.com/FlyingBassHot/meetagent/internal/node/adapter"
	"github.com/FlyingBassHot/meetagent/internal/node/config"
	"github.com/FlyingBassHot/meetagent/pkg/protocol"
)

// Version is this Node build's protocol-compatible version.
const Version = "0.4.0-phase3"

// Executor runs remote actions against local Node state and Agent adapters.
type Executor struct {
	nodeID    string
	nodeName  string
	startedAt time.Time
	cfg       config.Config
	manager   *adapter.Manager
}

// New creates an Executor. manager may be nil in tests; Agent actions then
// fail cleanly instead of panicking.
func New(nodeID, nodeName string, cfg config.Config, manager *adapter.Manager) *Executor {
	return &Executor{
		nodeID:    nodeID,
		nodeName:  nodeName,
		startedAt: time.Now(),
		cfg:       cfg,
		manager:   manager,
	}
}

// Execute validates and runs one command. The returned status is always final;
// the connection layer emits an "accepted" update before calling Execute.
func (e *Executor) Execute(ctx context.Context, env *protocol.Envelope) protocol.CommandResult {
	started := time.Now()
	result := e.execute(ctx, env)
	result.DurationMS = time.Since(started).Milliseconds()
	return result
}

func (e *Executor) execute(ctx context.Context, env *protocol.Envelope) protocol.CommandResult {
	if err := ctx.Err(); err != nil {
		return protocol.CommandFailure(protocol.StatusFailed, "node is shutting down")
	}
	if env.Target == nil || env.Target.NodeID != e.nodeID {
		return protocol.CommandFailure(protocol.StatusRejected, "command target does not match this node")
	}
	if !protocol.IsAllowedAction(env.Action) {
		return protocol.CommandFailure(protocol.StatusRejected, "action is not on the remote command whitelist")
	}
	if !e.cfg.PolicyFor(env.Target.AgentID).Allows(env.Action) {
		return protocol.CommandFailure(protocol.StatusRejected, "action is disabled by the local node policy")
	}

	switch env.Action {
	case protocol.ActionPing:
		var payload struct {
			Nonce string `json:"nonce"`
		}
		_ = env.DecodePayload(&payload)
		return completed(map[string]any{
			"pong":        true,
			"nonce":       payload.Nonce,
			"node_id":     e.nodeID,
			"server_time": time.Now().UTC(),
		})
	case protocol.ActionEcho:
		var payload struct {
			Text string `json:"text"`
		}
		if err := env.DecodePayload(&payload); err != nil {
			return protocol.CommandFailure(protocol.StatusRejected, "echo requires a text payload")
		}
		return completed(map[string]any{
			"text":    payload.Text,
			"node_id": e.nodeID,
		})
	case protocol.ActionGetStatus:
		hostname, _ := os.Hostname()
		return completed(map[string]any{
			"node_id":        e.nodeID,
			"name":           e.nodeName,
			"hostname":       hostname,
			"platform":       runtime.GOOS + "/" + runtime.GOARCH,
			"go_version":     runtime.Version(),
			"node_version":   Version,
			"uptime_seconds": int64(time.Since(e.startedAt).Seconds()),
			"server_time":    time.Now().UTC(),
			"agents":         e.managerViews(),
		})
	case protocol.ActionSendPrompt:
		return e.sendPrompt(ctx, env)
	case protocol.ActionInterrupt:
		return e.interrupt(ctx, env)
	case protocol.ActionListSessions:
		return e.listSessions(ctx, env)
	case protocol.ActionPermission:
		return e.respondPermission(ctx, env)
	default:
		return protocol.CommandFailure(protocol.StatusRejected, "action is not implemented by this node")
	}
}

type promptPayload struct {
	Text   string `json:"text"`
	Prompt string `json:"prompt"`
}

func (p promptPayload) value() string {
	if p.Text != "" {
		return p.Text
	}
	return p.Prompt
}

func (e *Executor) sendPrompt(ctx context.Context, env *protocol.Envelope) protocol.CommandResult {
	if e.manager == nil {
		return protocol.CommandFailure(protocol.StatusFailed, "agent adapters are not running")
	}
	if env.Target.AgentID == "" {
		return protocol.CommandFailure(protocol.StatusRejected, "send_prompt requires a target agent")
	}
	if env.Target.SessionID == "" {
		return protocol.CommandFailure(protocol.StatusRejected, "send_prompt requires a target session")
	}

	var payload promptPayload
	if err := env.DecodePayload(&payload); err != nil {
		return protocol.CommandFailure(protocol.StatusRejected, "send_prompt payload is invalid")
	}
	text := payload.value()
	if strings.TrimSpace(text) == "" {
		return protocol.CommandFailure(protocol.StatusRejected, "prompt must not be empty")
	}
	policy := e.cfg.PolicyFor(env.Target.AgentID)
	if policy.MaxPromptBytes > 0 && len(text) > policy.MaxPromptBytes {
		return protocol.CommandFailure(protocol.StatusRejected, fmt.Sprintf("prompt exceeds the local limit of %d bytes", policy.MaxPromptBytes))
	}

	result, err := e.manager.SendPrompt(ctx, env.Target.AgentID, env.Target.SessionID, text)
	if err != nil {
		return adapterFailure("send_prompt", err)
	}
	return completed(map[string]any{
		"agent_id":   env.Target.AgentID,
		"session_id": env.Target.SessionID,
		"message_id": result.MessageID,
	})
}

func (e *Executor) interrupt(ctx context.Context, env *protocol.Envelope) protocol.CommandResult {
	if e.manager == nil {
		return protocol.CommandFailure(protocol.StatusFailed, "agent adapters are not running")
	}
	if env.Target.AgentID == "" || env.Target.SessionID == "" {
		return protocol.CommandFailure(protocol.StatusRejected, "interrupt requires a target agent and session")
	}
	result, err := e.manager.Interrupt(ctx, env.Target.AgentID, env.Target.SessionID)
	if err != nil {
		return adapterFailure("interrupt", err)
	}
	return completed(map[string]any{
		"agent_id":    env.Target.AgentID,
		"session_id":  env.Target.SessionID,
		"interrupted": result.Interrupted,
	})
}

func (e *Executor) listSessions(ctx context.Context, env *protocol.Envelope) protocol.CommandResult {
	if e.manager == nil {
		return protocol.CommandFailure(protocol.StatusFailed, "agent adapters are not running")
	}

	if agentID := env.Target.AgentID; agentID != "" {
		sessions, err := e.manager.ListSessions(ctx, agentID)
		if err != nil {
			return adapterFailure("list_sessions", err)
		}
		return completed(map[string]any{
			"agent_id": agentID,
			"sessions": sessions,
		})
	}

	agents := e.manager.Agents()
	sessions := make([]protocol.SessionView, 0)
	for _, agent := range agents {
		sessions = append(sessions, agent.Sessions...)
	}
	return completed(map[string]any{
		"agents":   agents,
		"sessions": sessions,
	})
}

func (e *Executor) respondPermission(ctx context.Context, env *protocol.Envelope) protocol.CommandResult {
	if e.manager == nil {
		return protocol.CommandFailure(protocol.StatusFailed, "agent adapters are not running")
	}
	var payload struct {
		PermissionID string `json:"permission_id"`
		ID           string `json:"id"`
		Decision     string `json:"decision"`
		Message      string `json:"message"`
	}
	if err := env.DecodePayload(&payload); err != nil {
		return protocol.CommandFailure(protocol.StatusRejected, "permission_response payload is invalid")
	}
	permissionID := payload.PermissionID
	if permissionID == "" {
		permissionID = payload.ID
	}
	if permissionID == "" {
		return protocol.CommandFailure(protocol.StatusRejected, "permission_response requires permission_id")
	}
	if env.Target.AgentID == "" || env.Target.SessionID == "" {
		return protocol.CommandFailure(protocol.StatusRejected, "permission_response requires a target agent and session")
	}
	if err := e.manager.RespondPermission(ctx, env.Target.AgentID, env.Target.SessionID, permissionID, payload.Decision, payload.Message); err != nil {
		return adapterFailure("permission_response", err)
	}
	return completed(map[string]any{
		"permission_id": permissionID,
		"session_id":    env.Target.SessionID,
		"decision":      payload.Decision,
	})
}

func (e *Executor) managerViews() []protocol.AgentView {
	if e.manager == nil {
		return nil
	}
	return e.manager.Agents()
}

func adapterFailure(action string, err error) protocol.CommandResult {
	switch {
	case errors.Is(err, adapter.ErrNotSupported):
		return protocol.CommandFailure(protocol.StatusRejected, action+" is not supported by this agent")
	case errors.Is(err, adapter.ErrSessionNotFound):
		return protocol.CommandFailure(protocol.StatusRejected, action+" target session was not found")
	case errors.Is(err, adapter.ErrUnauthorized):
		return protocol.CommandFailure(protocol.StatusFailed, action+" failed: agent authentication rejected")
	case errors.Is(err, adapter.ErrUnavailable):
		return protocol.CommandFailure(protocol.StatusFailed, action+" failed: agent is offline")
	default:
		return protocol.CommandFailure(protocol.StatusFailed, action+" failed: "+err.Error())
	}
}

func completed(data any) protocol.CommandResult {
	raw, err := json.Marshal(data)
	if err != nil {
		return protocol.CommandFailure(protocol.StatusFailed, "encode command result")
	}
	return protocol.CommandResult{Status: protocol.StatusCompleted, Data: raw}
}
