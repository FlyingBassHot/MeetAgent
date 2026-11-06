// Package clientproto defines the V2.1 client-facing protocol used between
// MeetAgent Node and its Mobile / Terminal clients.
//
// The protocol has three parts:
//   - handshake: hello / welcome
//   - state: snapshot + a sequenced event stream
//   - control: command / command.result
//
// It is intentionally independent from the legacy Cloud envelope in
// pkg/protocol. Cloud relay support can translate between the two while the
// Node remains the authority for local clients.
package clientproto

import (
	"encoding/json"
	"errors"
	"fmt"
	"strings"
	"time"

	"github.com/FlyingBassHot/meetagent/pkg/protocol"
)

// Version is the client protocol schema version.
const Version = 1

// Envelope types.
const (
	TypeHello         = "hello"
	TypeWelcome       = "welcome"
	TypeSnapshot      = "snapshot"
	TypeEvent         = "event"
	TypeCommand       = "command"
	TypeCommandResult = "command.result"
	TypeHeartbeat     = "heartbeat"
	TypeError         = "error"
)

// Command types accepted by the Node command gateway.
const (
	CommandPing              = "ping"
	CommandNodeStatus        = "node.status"
	CommandSessionList       = "session.list"
	CommandSessionPrompt     = "session.prompt"
	CommandSessionInterrupt  = "session.interrupt"
	CommandPermissionRespond = "permission.respond"
)

// Client types.
const (
	ClientMobile   = "mobile"
	ClientTerminal = "terminal"
	ClientWeb      = "web"
	ClientDesktop  = "desktop"
)

// Client capabilities. They describe what the client can do, not what the
// Agent can do.
const (
	CapabilityUIFull           = "ui.full"
	CapabilityUICompact        = "ui.compact"
	CapabilityDisplayColor     = "display.color"
	CapabilityDisplayTouch     = "display.touch"
	CapabilityInputText        = "input.text"
	CapabilityInputButton      = "input.button"
	CapabilityInputEncoder     = "input.encoder"
	CapabilityInputVoice       = "input.voice"
	CapabilityNotificationApp  = "notification.local"
	CapabilityNotificationPush = "notification.system"
)

// User permissions granted to a paired client.
const (
	PermissionSessionRead       = "session.read"
	PermissionSessionControl    = "session.control"
	PermissionPermissionRespond = "permission.respond"
	PermissionNodeManage        = "node.manage"
)

// Envelope is the single WebSocket message wrapper for the client protocol.
type Envelope struct {
	SchemaVersion int             `json:"schema_version"`
	Type          string          `json:"type"`
	MessageID     string          `json:"message_id,omitempty"`
	Payload       json.RawMessage `json:"payload,omitempty"`
}

// NewEnvelope builds an envelope with a generated message ID.
func NewEnvelope(messageType string, payload any) (*Envelope, error) {
	env := &Envelope{
		SchemaVersion: Version,
		Type:          messageType,
		MessageID:     protocol.NewID("cmsg"),
	}
	if payload != nil {
		raw, err := json.Marshal(payload)
		if err != nil {
			return nil, fmt.Errorf("encode %s payload: %w", messageType, err)
		}
		env.Payload = raw
	}
	return env, nil
}

// DecodePayload unmarshals the envelope payload into dst.
func (e *Envelope) DecodePayload(dst any) error {
	if len(e.Payload) == 0 {
		return errors.New("payload is empty")
	}
	if err := json.Unmarshal(e.Payload, dst); err != nil {
		return fmt.Errorf("decode %s payload: %w", e.Type, err)
	}
	return nil
}

// Validate checks the envelope shape.
func (e *Envelope) Validate() error {
	if e == nil {
		return errors.New("envelope is nil")
	}
	if e.SchemaVersion != Version {
		return fmt.Errorf("unsupported schema_version %d", e.SchemaVersion)
	}
	if strings.TrimSpace(e.Type) == "" {
		return errors.New("type is required")
	}
	return nil
}

// Hello is the first message from a client. A client either presents a paired
// token or a pairing code from the Node.
type Hello struct {
	ClientID      string   `json:"client_id,omitempty"`
	ClientType    string   `json:"client_type"`
	ClientVersion string   `json:"client_version,omitempty"`
	Name          string   `json:"name,omitempty"`
	Token         string   `json:"token,omitempty"`
	PairingCode   string   `json:"pairing_code,omitempty"`
	Capabilities  []string `json:"capabilities,omitempty"`
	LastSequence  uint64   `json:"last_sequence,omitempty"`
}

// Validate checks the handshake shape before authentication.
func (h Hello) Validate() error {
	if !ValidClientType(h.ClientType) {
		return fmt.Errorf("unsupported client_type %q", h.ClientType)
	}
	if strings.TrimSpace(h.Token) == "" && strings.TrimSpace(h.PairingCode) == "" {
		return errors.New("token or pairing_code is required")
	}
	return nil
}

// ValidClientType reports whether the client type is supported.
func ValidClientType(clientType string) bool {
	switch clientType {
	case ClientMobile, ClientTerminal, ClientWeb, ClientDesktop:
		return true
	default:
		return false
	}
}

// Welcome is returned after a successful handshake. Token is only present when
// the client paired during this connection.
type Welcome struct {
	SchemaVersion int       `json:"schema_version"`
	ClientID      string    `json:"client_id"`
	ClientType    string    `json:"client_type"`
	Token         string    `json:"token,omitempty"`
	Permissions   []string  `json:"permissions,omitempty"`
	Node          NodeInfo  `json:"node"`
	Snapshot      Snapshot  `json:"snapshot"`
	ServerTime    time.Time `json:"server_time"`
}

// NodeInfo describes the Node a client is attached to.
type NodeInfo struct {
	NodeID          string `json:"node_id"`
	Name            string `json:"name"`
	Hostname        string `json:"hostname,omitempty"`
	Platform        string `json:"platform,omitempty"`
	Version         string `json:"version,omitempty"`
	ProtocolVersion int    `json:"protocol_version"`
}

// AdapterInfo is the public part of an Adapter Manifest.
type AdapterInfo struct {
	ID           string   `json:"id"`
	Name         string   `json:"name"`
	Version      string   `json:"version,omitempty"`
	APIVersion   int      `json:"api_version,omitempty"`
	Runtime      string   `json:"runtime,omitempty"`
	Capabilities []string `json:"capabilities,omitempty"`
	Permissions  []string `json:"permissions,omitempty"`
}

// Snapshot is the full state a client needs after connecting.
type Snapshot struct {
	SchemaVersion      int                       `json:"schema_version"`
	Node               NodeInfo                  `json:"node"`
	Adapters           []AdapterInfo             `json:"adapters,omitempty"`
	Agents             []protocol.AgentView      `json:"agents"`
	Events             []Event                   `json:"events,omitempty"`
	LastSequence       uint64                    `json:"last_sequence"`
	PendingPermissions []protocol.PermissionView `json:"pending_permissions,omitempty"`
	ServerTime         time.Time                 `json:"server_time"`
}

// EventSource identifies where a normalized event came from.
type EventSource struct {
	AdapterID       string `json:"adapter_id,omitempty"`
	NativeEventType string `json:"native_event_type,omitempty"`
}

// Event is the V2.1 event envelope. The Node assigns Sequence.
type Event struct {
	SchemaVersion int                        `json:"schema_version"`
	EventID       string                     `json:"event_id"`
	NodeID        string                     `json:"node_id"`
	Sequence      uint64                     `json:"sequence"`
	Type          string                     `json:"type"`
	OccurredAt    time.Time                  `json:"occurred_at"`
	AgentID       string                     `json:"agent_id,omitempty"`
	SessionID     string                     `json:"session_id,omitempty"`
	Payload       json.RawMessage            `json:"payload,omitempty"`
	Source        *EventSource               `json:"source,omitempty"`
	Extensions    map[string]json.RawMessage `json:"extensions,omitempty"`
}

// EventFromAgent converts an internal normalized event to the client protocol.
func EventFromAgent(nodeID string, event protocol.AgentEvent) Event {
	source := &EventSource{NativeEventType: event.EventType}
	if event.AdapterType != "" {
		source.AdapterID = event.AdapterType
	}
	return Event{
		SchemaVersion: Version,
		EventID:       event.EventID,
		NodeID:        nodeID,
		Sequence:      event.Sequence,
		Type:          event.EventType,
		OccurredAt:    event.Timestamp,
		AgentID:       event.AgentID,
		SessionID:     event.SessionID,
		Payload:       event.Payload,
		Source:        source,
	}
}

// Command is a client control request. NodeID is optional for a single-node
// deployment but retained for future multi-node clients.
type Command struct {
	SchemaVersion int             `json:"schema_version"`
	CommandID     string          `json:"command_id"`
	NodeID        string          `json:"node_id,omitempty"`
	AgentID       string          `json:"agent_id,omitempty"`
	SessionID     string          `json:"session_id,omitempty"`
	Type          string          `json:"type"`
	Payload       json.RawMessage `json:"payload,omitempty"`
	CreatedAt     time.Time       `json:"created_at"`
}

// Validate checks the command shape.
func (c Command) Validate() error {
	if c.SchemaVersion != Version {
		return fmt.Errorf("unsupported schema_version %d", c.SchemaVersion)
	}
	if strings.TrimSpace(c.CommandID) == "" {
		return errors.New("command_id is required")
	}
	if strings.TrimSpace(c.Type) == "" {
		return errors.New("command type is required")
	}
	return nil
}

// DecodePayload unmarshals the command payload.
func (c Command) DecodePayload(dst any) error {
	if len(c.Payload) == 0 {
		return errors.New("command payload is empty")
	}
	if err := json.Unmarshal(c.Payload, dst); err != nil {
		return fmt.Errorf("decode %s payload: %w", c.Type, err)
	}
	return nil
}

// CommandResult is the terminal result of one command.
type CommandResult struct {
	SchemaVersion int                    `json:"schema_version"`
	CommandID     string                 `json:"command_id"`
	Status        protocol.CommandStatus `json:"status"`
	Data          json.RawMessage        `json:"data,omitempty"`
	Error         string                 `json:"error,omitempty"`
	DurationMS    int64                  `json:"duration_ms,omitempty"`
	CompletedAt   time.Time              `json:"completed_at"`
}

// PromptPayload is the payload of session.prompt.
type PromptPayload struct {
	Text string `json:"text"`
}

// InterruptPayload is the payload of session.interrupt (empty).
type InterruptPayload struct{}

// ListSessionsPayload is the payload of session.list.
type ListSessionsPayload struct {
	AgentID string `json:"agent_id,omitempty"`
}

// PermissionResponsePayload is the payload of permission.respond.
type PermissionResponsePayload struct {
	PermissionID string `json:"permission_id"`
	Decision     string `json:"decision"`
	Message      string `json:"message,omitempty"`
}

// ErrorPayload is returned for protocol/authentication errors.
type ErrorPayload struct {
	Code    string `json:"code"`
	Message string `json:"message"`
}

// PairRequest is the HTTP pairing request.
type PairRequest struct {
	ClientType    string   `json:"client_type"`
	ClientVersion string   `json:"client_version,omitempty"`
	Name          string   `json:"name,omitempty"`
	Capabilities  []string `json:"capabilities,omitempty"`
	PairingCode   string   `json:"pairing_code"`
}

// ClientInfo is the admin-facing view of a paired client (no token hash).
type ClientInfo struct {
	ClientID     string     `json:"client_id"`
	Name         string     `json:"name,omitempty"`
	ClientType   string     `json:"client_type"`
	Version      string     `json:"client_version,omitempty"`
	Capabilities []string   `json:"capabilities,omitempty"`
	Permissions  []string   `json:"permissions,omitempty"`
	Online       bool       `json:"online"`
	CreatedAt    time.Time  `json:"created_at"`
	LastSeen     *time.Time `json:"last_seen,omitempty"`
}

// PairResult is returned by the HTTP pairing endpoint.
type PairResult struct {
	SchemaVersion int       `json:"schema_version"`
	ClientID      string    `json:"client_id"`
	Token         string    `json:"token"`
	Permissions   []string  `json:"permissions"`
	Node          NodeInfo  `json:"node"`
	ServerTime    time.Time `json:"server_time"`
}

// Info is returned by GET /api/v2/info for discovery and diagnostics.
type Info struct {
	SchemaVersion    int           `json:"schema_version"`
	ProtocolVersion  int           `json:"protocol_version"`
	Node             NodeInfo      `json:"node"`
	Adapters         []AdapterInfo `json:"adapters,omitempty"`
	PairingRequired  bool          `json:"pairing_required"`
	PairingCodeTTLMS int64         `json:"pairing_code_ttl_ms"`
	WSPath           string        `json:"ws_path"`
}
