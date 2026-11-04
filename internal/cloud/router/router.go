// Package router implements Cloud message handling and routing. It is the only
// Cloud package that understands protocol actions and Device -> Node policy.
package router

import (
	"encoding/json"
	"errors"
	"log/slog"
	"strings"
	"time"

	"github.com/FlyingBassHot/meetagent/internal/cloud/auth"
	"github.com/FlyingBassHot/meetagent/internal/cloud/connection"
	"github.com/FlyingBassHot/meetagent/internal/cloud/eventcache"
	"github.com/FlyingBassHot/meetagent/pkg/protocol"
)

const (
	identityRejectionDelay = 300 * time.Millisecond
	maxReplayNodeEntries   = 1000
)

var cloudSource = protocol.Source{Type: protocol.PeerCloud, ID: "cloud"}

// Router dispatches validated envelopes from Nodes and Devices.
type Router struct {
	manager        *connection.Manager
	auth           *auth.Authenticator
	cache          *eventcache.Cache
	snapshotEvents int
	replayLimit    int
	logger         *slog.Logger
}

// New creates a Router.
func New(manager *connection.Manager, authenticator *auth.Authenticator, cache *eventcache.Cache, snapshotEvents, replayLimit int, logger *slog.Logger) *Router {
	if snapshotEvents < 0 {
		snapshotEvents = 0
	}
	if replayLimit <= 0 {
		replayLimit = 1000
	}
	return &Router{
		manager:        manager,
		auth:           authenticator,
		cache:          cache,
		snapshotEvents: snapshotEvents,
		replayLimit:    replayLimit,
		logger:         logger,
	}
}

// Handle processes one validated envelope from a connection.
func (r *Router) Handle(client *connection.Client, env *protocol.Envelope) {
	identity, registered := client.Registered()
	if !registered {
		if env.Type != protocol.TypeRegister {
			r.sendProtocolError(client, env.RequestID, "not_registered", "register before sending other messages", "")
			return
		}
		r.handleRegister(client, env)
		return
	}

	if env.Source != identity {
		r.sendProtocolError(client, env.RequestID, "source_mismatch", "envelope source does not match the authenticated identity", "")
		r.logger.Warn("rejected source mismatch", "connection_id", client.ID, "identity", identity.ID, "claimed_source", env.Source.ID)
		return
	}

	switch env.Type {
	case protocol.TypeRegister:
		r.sendProtocolError(client, env.RequestID, "already_registered", "connection is already registered", "")
	case protocol.TypeHeartbeat:
		r.handleHeartbeat(client, identity, env)
	case protocol.TypeCommand:
		r.handleCommand(client, identity, env)
	case protocol.TypeResponse:
		r.handleResponse(client, identity, env)
	case protocol.TypeSync:
		r.handleSync(client, identity, env)
	case protocol.TypeEvent:
		r.handleEvent(client, identity, env)
	case protocol.TypeError:
		r.logPeerError(client, identity, env)
	default:
		r.sendProtocolError(client, env.RequestID, "unsupported_type", "unsupported message type", string(env.Type))
	}
}

// Disconnect removes a closed connection, publishes a Node offline event and
// pushes fresh authorized snapshots when a Node actually went offline.
func (r *Router) Disconnect(client *connection.Client) {
	info := r.manager.Unregister(client)
	if info.Node == nil {
		if info.Identity.Type == protocol.PeerDevice && info.Identity.ID != "" {
			r.logger.Debug("device disconnected", "device_id", info.Identity.ID)
		}
		return
	}

	r.publishNodeEvent(info.Node.NodeID, protocol.EventNodeOffline, map[string]any{
		"node_id": info.Node.NodeID,
		"name":    info.Node.Name,
		"status":  "offline",
	})
	r.failPendingForNode(info.Node.NodeID, "node disconnected before the response completed")
	r.BroadcastSnapshots()
}

// failPendingForNode resolves every in-flight request targeting a Node that
// went away, so Devices do not wait for their own timeout.
func (r *Router) failPendingForNode(nodeID, message string) {
	for _, pending := range r.manager.TakePendingForNode(nodeID) {
		reply, err := protocol.NewWithPayload(protocol.TypeResponse, pending.Action, cloudSource, protocol.CommandResult{
			Status: protocol.StatusFailed,
			Error:  message,
		})
		if err != nil {
			r.logger.Error("encode pending failure", "error", err)
			continue
		}
		reply.WithRequestID(pending.RequestID)
		reply.WithTarget(protocol.Target{NodeID: nodeID})
		if err := r.manager.SendToDevice(pending.DeviceID, reply); err != nil {
			r.logger.Debug("send pending failure", "device_id", pending.DeviceID, "request_id", pending.RequestID, "error", err)
			continue
		}
		r.logger.Info("pending request failed after node disconnect", "device_id", pending.DeviceID, "node_id", nodeID, "request_id", pending.RequestID)
	}
}

func (r *Router) handleRegister(client *connection.Client, env *protocol.Envelope) {
	if env.Source.Type == protocol.PeerCloud {
		r.sendProtocolError(client, env.RequestID, "invalid_identity", "cloud is not a registrable identity", "")
		return
	}
	if env.Source.Type != protocol.PeerNode && env.Source.Type != protocol.PeerDevice {
		r.sendProtocolError(client, env.RequestID, "invalid_identity", "source.type must be node or device", "")
		return
	}

	var payload protocol.RegisterPayload
	if err := env.DecodePayload(&payload); err != nil {
		r.rejectRegistration(client, env, "invalid_payload", "register payload is invalid")
		return
	}
	if err := r.auth.Authenticate(env.Source.Type, env.Source.ID, payload.Token); err != nil {
		code := "invalid_credentials"
		message := "identity or token is invalid"
		if errors.Is(err, auth.ErrUnknownIdentity) {
			code = "unknown_identity"
			message = "identity is not configured on this cloud"
		}
		r.logger.Warn("registration rejected", "connection_id", client.ID, "identity", env.Source.ID, "reason", err)
		r.rejectRegistration(client, env, code, message)
		return
	}

	// Validate the Node inventory before marking the connection as registered.
	// Otherwise a Node could briefly hold an authenticated identity while its
	// registration is being rejected.
	var agents []protocol.AgentView
	if env.Source.Type == protocol.PeerNode {
		normalized, err := normalizeAgents(env.Source.ID, payload.Agents)
		if err != nil {
			r.rejectRegistration(client, env, "invalid_agents", err.Error())
			return
		}
		agents = normalized
	}

	client.MarkRegistered(env.Source)

	requestID := env.RequestID
	if requestID == "" {
		requestID = env.MessageID
	}
	result := protocol.RegisterResult{
		OK:           true,
		ConnectionID: client.ID,
		Identity:     env.Source,
		ServerTime:   time.Now().UTC(),
	}

	switch env.Source.Type {
	case protocol.PeerNode:
		r.manager.RegisterNode(client, protocol.NodeView{
			NodeID:   env.Source.ID,
			Name:     firstNonEmpty(payload.Name, payload.Hostname, env.Source.ID),
			Hostname: payload.Hostname,
			Platform: payload.Platform,
			Version:  payload.Version,
			Agents:   agents,
		})
	case protocol.PeerDevice:
		r.manager.RegisterDeviceIdentity(client, env.Source.ID)
		snapshot := r.snapshotFor(env.Source.ID)
		result.Snapshot = &snapshot
	}

	ack, err := protocol.NewWithPayload(protocol.TypeResponse, protocol.ActionRegister, cloudSource, result)
	if err != nil {
		r.logger.Error("encode register ack", "error", err)
		return
	}
	ack.WithRequestID(requestID)
	if err := client.Send(ack); err != nil {
		r.logger.Warn("send register ack", "connection_id", client.ID, "error", err)
		return
	}

	r.logger.Info("peer registered",
		"peer_type", env.Source.Type,
		"peer_id", env.Source.ID,
		"connection_id", client.ID,
		"remote_addr", client.RemoteAddr(),
	)

	if env.Source.Type == protocol.PeerNode {
		r.publishNodeEvent(env.Source.ID, protocol.EventNodeOnline, map[string]any{
			"node_id": env.Source.ID,
			"name":    firstNonEmpty(payload.Name, payload.Hostname, env.Source.ID),
			"status":  "online",
			"agents":  len(payload.Agents),
		})
		r.BroadcastSnapshots()
	}
}

func (r *Router) handleHeartbeat(client *connection.Client, identity protocol.Source, env *protocol.Envelope) {
	r.manager.Touch(client)

	var payload protocol.HeartbeatPayload
	_ = env.DecodePayload(&payload)

	reply, err := protocol.NewWithPayload(protocol.TypeHeartbeat, "", cloudSource, protocol.HeartbeatPayload{
		Nonce:      payload.Nonce,
		ServerTime: time.Now().UTC(),
	})
	if err != nil {
		return
	}
	if env.RequestID != "" {
		reply.WithRequestID(env.RequestID)
	}
	if err := client.Send(reply); err != nil {
		r.logger.Debug("send heartbeat reply", "peer_id", identity.ID, "error", err)
	}
}

func (r *Router) handleCommand(client *connection.Client, identity protocol.Source, env *protocol.Envelope) {
	if identity.Type != protocol.PeerDevice {
		r.sendProtocolError(client, env.RequestID, "forbidden", "only devices may send commands", "")
		return
	}
	if !protocol.IsAllowedAction(env.Action) {
		r.sendCommandFailure(client, env, protocol.StatusRejected, "action is not on the remote command whitelist")
		return
	}
	if env.Target == nil || env.Target.NodeID == "" {
		r.sendCommandFailure(client, env, protocol.StatusRejected, "target node is required")
		return
	}
	if !r.auth.CanAccessNode(identity.ID, env.Target.NodeID) {
		r.sendCommandFailure(client, env, protocol.StatusRejected, "device is not authorized for the target node")
		r.logger.Warn("command rejected by node ACL", "device_id", identity.ID, "node_id", env.Target.NodeID)
		return
	}
	if !r.manager.NodeOnline(env.Target.NodeID) {
		r.sendCommandFailure(client, env, protocol.StatusRejected, "target node is offline")
		return
	}
	if err := r.validateTargetAgent(env.Target); err != nil {
		r.sendCommandFailure(client, env, protocol.StatusRejected, err.Error())
		return
	}

	pending := connection.PendingRequest{
		RequestID: env.RequestID,
		DeviceID:  identity.ID,
		NodeID:    env.Target.NodeID,
		Action:    env.Action,
	}
	if err := r.manager.AddPending(pending); err != nil {
		message := "request cannot be accepted"
		switch {
		case errors.Is(err, connection.ErrDuplicateReq):
			message = "request_id was already used by another in-flight request"
		case errors.Is(err, connection.ErrTooManyPending):
			message = "cloud pending request limit reached"
		}
		r.sendCommandFailure(client, env, protocol.StatusRejected, message)
		return
	}

	if err := r.manager.SendToNode(env.Target.NodeID, env); err != nil {
		r.manager.DropPending(env.RequestID)
		r.sendCommandFailure(client, env, protocol.StatusFailed, "failed to forward command to node")
		r.logger.Warn("forward command failed", "node_id", env.Target.NodeID, "request_id", env.RequestID, "error", err)
		return
	}

	r.logger.Info("command forwarded",
		"device_id", identity.ID,
		"node_id", env.Target.NodeID,
		"agent_id", env.Target.AgentID,
		"action", env.Action,
		"request_id", env.RequestID,
	)
}

func (r *Router) handleResponse(client *connection.Client, identity protocol.Source, env *protocol.Envelope) {
	if identity.Type != protocol.PeerNode {
		r.sendProtocolError(client, env.RequestID, "forbidden", "only nodes may send command responses", "")
		return
	}

	var result protocol.CommandResult
	if err := env.DecodePayload(&result); err != nil {
		r.sendProtocolError(client, env.RequestID, "invalid_payload", "response payload is invalid", err.Error())
		return
	}
	if !validStatus(result.Status) {
		r.sendProtocolError(client, env.RequestID, "invalid_status", "unknown command status", string(result.Status))
		return
	}

	var (
		pending connection.PendingRequest
		err     error
	)
	if result.Status.IsTerminal() {
		pending, err = r.manager.ConsumePending(env.RequestID, identity.ID)
	} else {
		pending, err = r.manager.GetPending(env.RequestID)
		if err == nil && pending.NodeID != identity.ID {
			err = connection.ErrRequestNodeMiss
		}
	}
	if err != nil {
		r.sendProtocolError(client, env.RequestID, "unknown_request", "response does not match an in-flight request", err.Error())
		r.logger.Warn("unmatched command response", "node_id", identity.ID, "request_id", env.RequestID, "error", err)
		return
	}

	if err := r.manager.SendToDevice(pending.DeviceID, env); err != nil {
		r.logger.Warn("forward response failed", "device_id", pending.DeviceID, "request_id", env.RequestID, "error", err)
	}

	r.logger.Info("command response forwarded",
		"device_id", pending.DeviceID,
		"node_id", identity.ID,
		"action", pending.Action,
		"request_id", env.RequestID,
		"status", result.Status,
	)
}

func (r *Router) handleSync(client *connection.Client, identity protocol.Source, env *protocol.Envelope) {
	if identity.Type == protocol.PeerNode {
		if env.Action == protocol.ActionAgentState {
			r.handleAgentState(client, identity, env)
			return
		}
		r.sendProtocolError(client, env.RequestID, "forbidden", "nodes may only send agent_state sync", "")
		return
	}
	if identity.Type != protocol.PeerDevice {
		r.sendProtocolError(client, env.RequestID, "forbidden", "only devices may request sync", "")
		return
	}
	switch env.Action {
	case protocol.ActionSnapshot:
		r.sendSnapshot(client, identity.ID, env.RequestID)
	case protocol.ActionEvents:
		r.handleEventReplay(client, identity.ID, env)
	case protocol.ActionAgentState:
		r.sendProtocolError(client, env.RequestID, "forbidden", "only nodes may send agent_state sync", "")
	default:
		r.sendProtocolError(client, env.RequestID, "unsupported_action", "unsupported sync action", env.Action)
	}
}

// handleAgentState replaces a Node's Agent/Session inventory and broadcasts a
// fresh snapshot to every authorized Device.
func (r *Router) handleAgentState(client *connection.Client, identity protocol.Source, env *protocol.Envelope) {
	var payload protocol.AgentStatePayload
	if err := env.DecodePayload(&payload); err != nil {
		r.sendProtocolError(client, env.RequestID, "invalid_payload", "agent_state payload is invalid", err.Error())
		return
	}
	agents, err := normalizeAgents(identity.ID, payload.Agents)
	if err != nil {
		r.sendProtocolError(client, env.RequestID, "invalid_agents", err.Error(), "")
		return
	}
	view, ok := r.manager.UpdateNodeAgents(identity.ID, agents)
	if !ok {
		r.sendProtocolError(client, env.RequestID, "node_offline", "node is not registered", "")
		return
	}
	r.logger.Debug("node agent state updated", "node_id", identity.ID, "agents", len(view.Agents))
	r.BroadcastSnapshots()
}

func (r *Router) handleEventReplay(client *connection.Client, deviceID string, env *protocol.Envelope) {
	var request protocol.EventReplayRequest
	if len(env.Payload) > 0 {
		if err := env.DecodePayload(&request); err != nil {
			r.sendProtocolError(client, env.RequestID, "invalid_payload", "event replay payload is invalid", err.Error())
			return
		}
	}
	if len(request.LastSequences) > maxReplayNodeEntries {
		r.sendProtocolError(client, env.RequestID, "invalid_payload", "too many last_sequences entries", "")
		return
	}
	if request.LastSequences == nil {
		request.LastSequences = map[string]uint64{}
	}
	limit := request.Limit
	if limit <= 0 || limit > r.replayLimit {
		limit = r.replayLimit
	}

	visible := r.nodeVisibility(deviceID)
	events, latest, truncated := r.cache.Since(request.LastSequences, visible, limit)
	result := protocol.EventReplayResult{
		Events:        nonNilEvents(events),
		LastSequences: nonNilSequences(latest),
		Truncated:     nonNilStrings(truncated),
		ServerTime:    time.Now().UTC(),
	}
	reply, err := protocol.NewWithPayload(protocol.TypeSync, protocol.ActionEvents, cloudSource, result)
	if err != nil {
		r.logger.Error("encode event replay", "error", err)
		return
	}
	if env.RequestID != "" {
		reply.WithRequestID(env.RequestID)
	}
	_ = client.Send(reply)

	if len(truncated) > 0 {
		r.logger.Debug("event replay truncated", "device_id", deviceID, "nodes", strings.Join(truncated, ","))
	}
}

func (r *Router) handleEvent(client *connection.Client, identity protocol.Source, env *protocol.Envelope) {
	if identity.Type != protocol.PeerNode {
		r.sendProtocolError(client, env.RequestID, "forbidden", "only nodes may publish events", "")
		return
	}

	var event protocol.AgentEvent
	if err := env.DecodePayload(&event); err != nil {
		r.sendProtocolError(client, env.RequestID, "invalid_payload", "event payload is invalid", err.Error())
		return
	}
	event.EventType = strings.TrimSpace(firstNonEmpty(event.EventType, env.Action))
	if event.EventType == "" {
		r.sendProtocolError(client, env.RequestID, "invalid_event", "event_type is required", "")
		return
	}
	if env.Action != "" && event.EventType != env.Action {
		r.sendProtocolError(client, env.RequestID, "invalid_event", "event.action and payload.event_type disagree", "")
		return
	}
	if event.NodeID != "" && event.NodeID != identity.ID {
		r.sendProtocolError(client, env.RequestID, "forbidden", "event node_id does not match the authenticated node", "")
		return
	}
	if env.Target != nil && env.Target.NodeID != "" && env.Target.NodeID != identity.ID {
		r.sendProtocolError(client, env.RequestID, "forbidden", "event target does not match the authenticated node", "")
		return
	}
	event.NodeID = identity.ID
	event.Sequence = 0 // Cloud assigns the authoritative sequence.

	stored := r.cache.Append(identity.ID, event)
	r.broadcastEvent(identity, stored)

	if protocol.IsPermissionEvent(stored.EventType) {
		r.BroadcastSnapshots()
	}
}

func (r *Router) publishNodeEvent(nodeID, eventType string, payload any) {
	raw, err := json.Marshal(payload)
	if err != nil {
		r.logger.Error("encode node event payload", "error", err)
		return
	}
	stored := r.cache.Append(nodeID, protocol.AgentEvent{
		NodeID:    nodeID,
		EventType: eventType,
		Timestamp: time.Now().UTC(),
		Payload:   raw,
	})
	r.broadcastEvent(protocol.Source{Type: protocol.PeerNode, ID: nodeID}, stored)
}

func (r *Router) broadcastEvent(source protocol.Source, event protocol.AgentEvent) {
	env, err := protocol.NewWithPayload(protocol.TypeEvent, event.EventType, source, event)
	if err != nil {
		r.logger.Error("encode event", "error", err)
		return
	}
	env.WithTarget(protocol.Target{NodeID: event.NodeID, AgentID: event.AgentID, SessionID: event.SessionID})
	nodeID := event.NodeID
	sent := r.manager.BroadcastToDevicesFiltered(env, func(deviceID string) bool {
		return r.auth.CanAccessNode(deviceID, nodeID)
	})
	if sent > 0 {
		r.logger.Debug("event broadcast", "node_id", nodeID, "event_type", event.EventType, "sequence", event.Sequence, "devices", sent)
	}
}

// BroadcastSnapshots pushes a Device-specific, ACL-filtered snapshot to every
// online Device.
func (r *Router) BroadcastSnapshots() {
	for _, deviceID := range r.manager.DeviceIDs() {
		r.sendSnapshotToDevice(deviceID, "")
	}
}

func (r *Router) sendSnapshot(client *connection.Client, deviceID, requestID string) {
	snapshot := r.snapshotFor(deviceID)
	reply, err := protocol.NewWithPayload(protocol.TypeSync, protocol.ActionSnapshot, cloudSource, snapshot)
	if err != nil {
		r.logger.Error("encode snapshot", "error", err)
		return
	}
	if requestID != "" {
		reply.WithRequestID(requestID)
	}
	_ = client.Send(reply)
}

func (r *Router) sendSnapshotToDevice(deviceID, requestID string) {
	snapshot := r.snapshotFor(deviceID)
	reply, err := protocol.NewWithPayload(protocol.TypeSync, protocol.ActionSnapshot, cloudSource, snapshot)
	if err != nil {
		r.logger.Error("encode snapshot", "error", err)
		return
	}
	if requestID != "" {
		reply.WithRequestID(requestID)
	}
	if err := r.manager.SendToDevice(deviceID, reply); err != nil {
		r.logger.Debug("send snapshot failed", "device_id", deviceID, "error", err)
	}
}

func (r *Router) snapshotFor(deviceID string) protocol.Snapshot {
	visible := r.nodeVisibility(deviceID)
	snapshot := r.manager.SnapshotFor(visible)
	snapshot.Events = nonNilEvents(r.cache.Recent(visible, r.snapshotEvents))
	snapshot.LastSequences = nonNilSequences(r.cache.LatestSequences(visible))
	snapshot.PendingPermissions = r.cache.PendingPermissions(visible)
	if snapshot.PendingPermissions == nil {
		snapshot.PendingPermissions = []protocol.PermissionView{}
	}
	snapshot.ServerTime = time.Now().UTC()
	return snapshot
}

func (r *Router) nodeVisibility(deviceID string) func(nodeID string) bool {
	return func(nodeID string) bool {
		return r.auth.CanAccessNode(deviceID, nodeID)
	}
}

func (r *Router) validateTargetAgent(target *protocol.Target) error {
	if target == nil {
		return nil
	}
	if target.AgentID == "" && target.SessionID == "" {
		return nil
	}
	view, ok := r.manager.NodeView(target.NodeID)
	if !ok {
		return errors.New("target node is unknown")
	}
	if target.AgentID != "" {
		for _, agent := range view.Agents {
			if agent.AgentID != target.AgentID {
				continue
			}
			if target.SessionID == "" {
				return nil
			}
			for _, session := range agent.Sessions {
				if session.SessionID == target.SessionID {
					return nil
				}
			}
			return errors.New("target session is unknown on this agent")
		}
		return errors.New("target agent is unknown on this node")
	}
	for _, agent := range view.Agents {
		for _, session := range agent.Sessions {
			if session.SessionID == target.SessionID {
				return nil
			}
		}
	}
	return errors.New("target session is unknown on this node")
}

func (r *Router) logPeerError(client *connection.Client, identity protocol.Source, env *protocol.Envelope) {
	var payload protocol.ErrorPayload
	_ = env.DecodePayload(&payload)
	r.logger.Warn("peer reported error",
		"peer_type", identity.Type,
		"peer_id", identity.ID,
		"code", payload.Code,
		"message", payload.Message,
	)
}

func (r *Router) rejectRegistration(client *connection.Client, env *protocol.Envelope, code, message string) {
	requestID := env.RequestID
	if requestID == "" {
		requestID = env.MessageID
	}
	result := protocol.RegisterResult{
		OK:         false,
		ServerTime: time.Now().UTC(),
		Error:      message,
		ErrorCode:  code,
	}
	reply, err := protocol.NewWithPayload(protocol.TypeResponse, protocol.ActionRegister, cloudSource, result)
	if err == nil {
		reply.WithRequestID(requestID)
		_ = client.Send(reply)
	}
	// Give the writer a moment to flush the rejection before disconnecting.
	time.AfterFunc(identityRejectionDelay, func() {
		client.Close(4003, "registration rejected")
	})
}

func (r *Router) sendCommandFailure(client *connection.Client, env *protocol.Envelope, status protocol.CommandStatus, message string) {
	reply, err := protocol.NewWithPayload(protocol.TypeResponse, env.Action, cloudSource, protocol.CommandFailure(status, message))
	if err != nil {
		return
	}
	reply.WithRequestID(env.RequestID)
	if env.Target != nil {
		reply.WithTarget(*env.Target)
	}
	_ = client.Send(reply)
}

func (r *Router) sendProtocolError(client *connection.Client, requestID, code, message, detail string) {
	env, err := protocol.NewWithPayload(protocol.TypeError, "", cloudSource, protocol.ErrorPayload{
		Code:    code,
		Message: message,
		Detail:  detail,
	})
	if err != nil {
		return
	}
	if requestID != "" {
		env.WithRequestID(requestID)
	}
	_ = client.Send(env)
}

func validStatus(status protocol.CommandStatus) bool {
	switch status {
	case protocol.StatusAccepted, protocol.StatusExecuting, protocol.StatusCompleted,
		protocol.StatusFailed, protocol.StatusRejected, protocol.StatusTimeout:
		return true
	default:
		return false
	}
}

func normalizeAgents(nodeID string, agents []protocol.AgentView) ([]protocol.AgentView, error) {
	result := make([]protocol.AgentView, 0, len(agents))
	seen := make(map[string]bool, len(agents))
	now := time.Now().UTC()
	for _, agent := range agents {
		agent.AgentID = strings.TrimSpace(agent.AgentID)
		if agent.AgentID == "" {
			return nil, errors.New("agent_id is required for every declared agent")
		}
		if seen[agent.AgentID] {
			return nil, errors.New("duplicate agent_id in registration")
		}
		seen[agent.AgentID] = true
		agent.NodeID = nodeID
		agent.Name = firstNonEmpty(agent.Name, agent.AgentID)
		agent.Type = firstNonEmpty(agent.Type, "unknown")
		agent.Status = firstNonEmpty(agent.Status, "online")
		sessions := make([]protocol.SessionView, len(agent.Sessions))
		for i, session := range agent.Sessions {
			session.NodeID = nodeID
			session.AgentID = agent.AgentID
			if session.SessionID == "" {
				return nil, errors.New("session_id is required for every declared session")
			}
			session.Status = firstNonEmpty(session.Status, "unknown")
			if session.UpdatedAt.IsZero() {
				session.UpdatedAt = now
			}
			sessions[i] = session
		}
		agent.Sessions = sessions
		result = append(result, agent)
	}
	return result, nil
}

func nonNilEvents(events []protocol.AgentEvent) []protocol.AgentEvent {
	if events == nil {
		return []protocol.AgentEvent{}
	}
	return events
}

func nonNilSequences(sequences map[string]uint64) map[string]uint64 {
	if sequences == nil {
		return map[string]uint64{}
	}
	return sequences
}

func nonNilStrings(values []string) []string {
	if values == nil {
		return []string{}
	}
	return values
}

func firstNonEmpty(values ...string) string {
	for _, value := range values {
		if value != "" {
			return value
		}
	}
	return ""
}
