package protocol

import (
	"encoding/json"
	"time"
)

// Actions understood by Cloud and Node. Agent related actions are reserved
// for Phase 2; the Node only executes the actions present in AllowedActions.
const (
	ActionPing         = "ping"
	ActionEcho         = "echo"
	ActionGetStatus    = "get_status"
	ActionSendPrompt   = "send_prompt"
	ActionInterrupt    = "interrupt"
	ActionListSessions = "list_sessions"
	ActionPermission   = "permission_response"
	ActionRegister     = "register"
	ActionSnapshot     = "snapshot"
	ActionAgentState   = "agent_state"
	ActionEvents       = "events"
)

// AllowedActions is the complete whitelist accepted by Cloud and Node. It is
// deliberately small: there is no arbitrary shell or passthrough action.
var AllowedActions = map[string]bool{
	ActionPing:         true,
	ActionEcho:         true,
	ActionGetStatus:    true,
	ActionSendPrompt:   true,
	ActionInterrupt:    true,
	ActionListSessions: true,
	ActionPermission:   true,
}

// IsAllowedAction reports whether action is on the remote command whitelist.
func IsAllowedAction(action string) bool {
	return AllowedActions[action]
}

// Event types normalized by the Node adapter layer. Cloud accepts any
// non-empty event_type so future adapters can be added without a wire change,
// but these are the names the MVP UI understands.
const (
	EventNodeOnline          = "node.online"
	EventNodeOffline         = "node.offline"
	EventAgentConnected      = "agent.connected"
	EventAgentDisconnected   = "agent.disconnected"
	EventAgentStatusChanged  = "agent.status_changed"
	EventSessionCreated      = "session.created"
	EventSessionUpdated      = "session.updated"
	EventSessionDeleted      = "session.deleted"
	EventSessionStatusChange = "session.status_changed"
	EventMessageStarted      = "message.started"
	EventMessageDelta        = "message.delta"
	EventMessageCompleted    = "message.completed"
	EventToolStarted         = "tool.started"
	EventToolCompleted       = "tool.completed"
	EventToolFailed          = "tool.failed"
	EventPermissionRequested = "permission.requested"
	EventPermissionResolved  = "permission.resolved"
	EventAgentError          = "agent.error"
)

// Event types that change the pending permission snapshot.
func IsPermissionEvent(eventType string) bool {
	return eventType == EventPermissionRequested || eventType == EventPermissionResolved
}

// CommandStatus is the lifecycle state returned by a Node for a command.
type CommandStatus string

const (
	StatusAccepted  CommandStatus = "accepted"
	StatusExecuting CommandStatus = "executing"
	StatusCompleted CommandStatus = "completed"
	StatusFailed    CommandStatus = "failed"
	StatusRejected  CommandStatus = "rejected"
	StatusTimeout   CommandStatus = "timeout"
)

// IsTerminal reports whether no further status updates are expected.
func (s CommandStatus) IsTerminal() bool {
	switch s {
	case StatusCompleted, StatusFailed, StatusRejected, StatusTimeout:
		return true
	default:
		return false
	}
}

// PermissionStatus is the lifecycle state of an Agent permission request.
type PermissionStatus string

const (
	PermissionPending  PermissionStatus = "pending"
	PermissionApproved PermissionStatus = "approved"
	PermissionRejected PermissionStatus = "rejected"
	PermissionExpired  PermissionStatus = "expired"
)

// RegisterPayload is sent by a Node or Device to authenticate. Nodes may
// declare the Agent instances they host so Devices can discover them.
type RegisterPayload struct {
	Token         string            `json:"token"`
	Name          string            `json:"name,omitempty"`
	Hostname      string            `json:"hostname,omitempty"`
	Platform      string            `json:"platform,omitempty"`
	Version       string            `json:"version,omitempty"`
	Agents        []AgentView       `json:"agents,omitempty"`
	LastSequences map[string]uint64 `json:"last_sequences,omitempty"`
}

// RegisterResult acknowledges a registration attempt. Snapshots are filtered
// by the Device's Node authorization; Nodes receive no snapshot.
type RegisterResult struct {
	OK           bool      `json:"ok"`
	ConnectionID string    `json:"connection_id,omitempty"`
	Identity     Source    `json:"identity,omitempty"`
	ServerTime   time.Time `json:"server_time"`
	Snapshot     *Snapshot `json:"snapshot,omitempty"`
	Error        string    `json:"error,omitempty"`
	ErrorCode    string    `json:"error_code,omitempty"`
}

// HeartbeatPayload is used in both directions. Nonce is echoed by Cloud so the
// peer can measure round-trip time without extra state on the server.
type HeartbeatPayload struct {
	Nonce      string    `json:"nonce,omitempty"`
	ServerTime time.Time `json:"server_time,omitempty"`
}

// CommandResult is returned by a Node for a command. Data is opaque to Cloud.
type CommandResult struct {
	Status     CommandStatus   `json:"status"`
	Data       json.RawMessage `json:"data,omitempty"`
	Error      string          `json:"error,omitempty"`
	DurationMS int64           `json:"duration_ms,omitempty"`
}

// CommandFailure builds a terminal failure result.
func CommandFailure(status CommandStatus, message string) CommandResult {
	return CommandResult{Status: status, Error: message}
}

// ErrorPayload describes a protocol or routing error.
type ErrorPayload struct {
	Code    string `json:"code"`
	Message string `json:"message"`
	Detail  string `json:"detail,omitempty"`
}

// SessionView is the public representation of one Agent session.
type SessionView struct {
	SessionID        string    `json:"session_id"`
	AgentID          string    `json:"agent_id"`
	NodeID           string    `json:"node_id"`
	Title            string    `json:"title,omitempty"`
	WorkingDirectory string    `json:"working_directory,omitempty"`
	Status           string    `json:"status"`
	UpdatedAt        time.Time `json:"updated_at"`
}

// AgentView is the public representation of one Agent instance.
type AgentView struct {
	AgentID      string          `json:"agent_id"`
	NodeID       string          `json:"node_id"`
	Type         string          `json:"type"`
	Name         string          `json:"name"`
	Status       string          `json:"status"`
	Capabilities map[string]bool `json:"capabilities,omitempty"`
	Sessions     []SessionView   `json:"sessions"`
}

// NodeView is the public snapshot representation of a Node.
type NodeView struct {
	NodeID      string      `json:"node_id"`
	Name        string      `json:"name"`
	Hostname    string      `json:"hostname,omitempty"`
	Platform    string      `json:"platform,omitempty"`
	Version     string      `json:"version,omitempty"`
	Status      string      `json:"status"`
	ConnectedAt *time.Time  `json:"connected_at,omitempty"`
	LastSeen    *time.Time  `json:"last_seen,omitempty"`
	RemoteAddr  string      `json:"remote_addr,omitempty"`
	Agents      []AgentView `json:"agents"`
}

// PermissionView is one pending or resolved Agent permission request.
type PermissionView struct {
	PermissionID string           `json:"permission_id"`
	NodeID       string           `json:"node_id"`
	AgentID      string           `json:"agent_id,omitempty"`
	SessionID    string           `json:"session_id,omitempty"`
	Description  string           `json:"description,omitempty"`
	Status       PermissionStatus `json:"status"`
	ExpiresAt    *time.Time       `json:"expires_at,omitempty"`
	CreatedAt    time.Time        `json:"created_at"`
}

// AgentEvent is the normalized event format shared by Node, Cloud and Device.
// Cloud assigns the authoritative Sequence per Node before caching and fanout.
type AgentEvent struct {
	EventID     string          `json:"event_id"`
	Sequence    uint64          `json:"sequence"`
	NodeID      string          `json:"node_id"`
	AgentID     string          `json:"agent_id,omitempty"`
	AdapterType string          `json:"adapter_type,omitempty"`
	SessionID   string          `json:"session_id,omitempty"`
	EventType   string          `json:"event_type"`
	Timestamp   time.Time       `json:"timestamp"`
	Payload     json.RawMessage `json:"payload,omitempty"`
}

// PermissionEventPayload is the conventional payload of permission events.
type PermissionEventPayload struct {
	PermissionID string           `json:"permission_id"`
	Description  string           `json:"description,omitempty"`
	AgentID      string           `json:"agent_id,omitempty"`
	SessionID    string           `json:"session_id,omitempty"`
	Status       PermissionStatus `json:"status,omitempty"`
	ExpiresAt    *time.Time       `json:"expires_at,omitempty"`
	Action       string           `json:"action,omitempty"`
	Resources    []string         `json:"resources,omitempty"`
	Decision     string           `json:"decision,omitempty"`
}

// AgentStatePayload is a Node -> Cloud inventory update. It carries the live
// Agent and Session state produced by the Node's adapters.
type AgentStatePayload struct {
	Agents []AgentView `json:"agents"`
}

// EventReplayRequest asks Cloud to replay cached events after the given
// per-Node sequences. A missing Node entry means "from the start of the cache".
type EventReplayRequest struct {
	LastSequences map[string]uint64 `json:"last_sequences"`
	Limit         int               `json:"limit,omitempty"`
}

// EventReplayResult is the response to EventReplayRequest.
type EventReplayResult struct {
	Events        []AgentEvent      `json:"events"`
	LastSequences map[string]uint64 `json:"last_sequences"`
	Truncated     []string          `json:"truncated"`
	ServerTime    time.Time         `json:"server_time"`
}

// Snapshot is the state a Device needs after (re)connecting. It contains a
// bounded recent-event window; older gaps are recovered with EventReplay.
type Snapshot struct {
	Nodes              []NodeView        `json:"nodes"`
	Events             []AgentEvent      `json:"events"`
	LastSequences      map[string]uint64 `json:"last_sequences"`
	PendingPermissions []PermissionView  `json:"pending_permissions"`
	ServerTime         time.Time         `json:"server_time"`
}

// DeviceView is the public representation of an online Device connection.
type DeviceView struct {
	DeviceID     string     `json:"device_id"`
	Status       string     `json:"status"`
	ConnectedAt  *time.Time `json:"connected_at,omitempty"`
	LastSeen     *time.Time `json:"last_seen,omitempty"`
	RemoteAddr   string     `json:"remote_addr,omitempty"`
	AllowedNodes []string   `json:"allowed_nodes"`
}

// DecodeData decodes a CommandResult.Data value into dst.
func (r CommandResult) DecodeData(dst any) error {
	if len(r.Data) == 0 {
		return nil
	}
	return json.Unmarshal(r.Data, dst)
}
