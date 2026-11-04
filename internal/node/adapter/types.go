// Package adapter defines the Node-side Agent Adapter contract and lifecycle
// manager. Adapters translate one native Agent protocol into the platform's
// normalized concepts; Cloud never sees native payloads.
package adapter

import (
	"context"
	"encoding/json"
	"errors"
	"time"

	"github.com/FlyingBassHot/meetagent/pkg/protocol"
)

// Capability names shared by adapters, the Node command executor and the
// public AgentView.
const (
	CapabilityListSessions       = "list_sessions"
	CapabilitySendPrompt         = "send_prompt"
	CapabilityInterrupt          = "interrupt"
	CapabilitySubscribeEvents    = "subscribe_events"
	CapabilityPermissionResponse = "permission_response"
	CapabilitySwitchModel        = "switch_model"
)

// Capabilities advertises what an Adapter can actually do. Missing keys are
// false; adapters must not claim a capability they cannot perform.
type Capabilities map[string]bool

// Has reports whether a capability is enabled.
func (c Capabilities) Has(name string) bool { return c[name] }

// Clone returns a defensive copy.
func (c Capabilities) Clone() Capabilities {
	if c == nil {
		return Capabilities{}
	}
	out := make(Capabilities, len(c))
	for key, value := range c {
		out[key] = value
	}
	return out
}

// Status is the normalized lifecycle/availability state of one Agent instance.
type Status string

const (
	StatusOffline    Status = "offline"
	StatusConnecting Status = "connecting"
	StatusOnline     Status = "online"
	StatusBusy       Status = "busy"
	StatusError      Status = "error"
)

// Session is the adapter-neutral view of one Agent session.
type Session struct {
	ID               string
	Title            string
	WorkingDirectory string
	Status           string
	UpdatedAt        time.Time
}

// SessionEventPayload is used by adapters for session lifecycle events.
type SessionEventPayload struct {
	SessionID        string    `json:"session_id"`
	Title            string    `json:"title,omitempty"`
	WorkingDirectory string    `json:"working_directory,omitempty"`
	Status           string    `json:"status,omitempty"`
	UpdatedAt        time.Time `json:"updated_at,omitempty"`
}

// MessageEventPayload describes streaming assistant output.
type MessageEventPayload struct {
	MessageID string `json:"message_id,omitempty"`
	Ordinal   int    `json:"ordinal,omitempty"`
	Kind      string `json:"kind,omitempty"`
	Delta     string `json:"delta,omitempty"`
	Text      string `json:"text,omitempty"`
	Finish    string `json:"finish,omitempty"`
}

// ToolEventPayload describes a tool invocation without exposing full tool
// output to Cloud.
type ToolEventPayload struct {
	CallID   string `json:"call_id,omitempty"`
	Name     string `json:"name,omitempty"`
	Input    string `json:"input,omitempty"`
	Status   string `json:"status,omitempty"`
	Executed *bool  `json:"executed,omitempty"`
}

// PromptResult is returned after a prompt was durably admitted.
type PromptResult struct {
	MessageID string
}

// InterruptResult reports whether an active execution was interrupted.
type InterruptResult struct {
	Interrupted bool
}

// Event is the normalized adapter event before the Manager adds identity and
// protocol sequencing metadata. Status, when set, updates the session state.
type Event struct {
	Type      string
	SessionID string
	Timestamp time.Time
	Status    string
	Payload   any
}

var (
	// ErrNotSupported means the Adapter does not implement the capability.
	ErrNotSupported = errors.New("adapter does not support this capability")
	// ErrSessionNotFound means the target session is not known to the Agent.
	ErrSessionNotFound = errors.New("session not found")
	// ErrUnauthorized means the local Agent rejected the configured credential.
	ErrUnauthorized = errors.New("agent authentication failed")
	// ErrUnavailable means the Adapter is not connected.
	ErrUnavailable = errors.New("agent is not available")
)

// AgentAdapter is the single integration point for a specific Agent product.
// Implementations must be safe for concurrent calls after Connect returns.
type AgentAdapter interface {
	Type() string

	Connect(ctx context.Context) error

	Disconnect(ctx context.Context) error

	Capabilities() Capabilities

	ListSessions(ctx context.Context) ([]Session, error)

	SendPrompt(ctx context.Context, sessionID, prompt string) (PromptResult, error)

	Interrupt(ctx context.Context, sessionID string) (InterruptResult, error)

	// SubscribeEvents returns a channel of normalized events. The channel is
	// closed when ctx is canceled; adapters are responsible for reconnecting
	// their native stream and refreshing state after a gap.
	SubscribeEvents(ctx context.Context) (<-chan Event, error)
}

// PermissionResponder is an optional capability. Agents without a permission
// model simply do not implement it.
type PermissionResponder interface {
	RespondPermission(ctx context.Context, sessionID, permissionID, decision, message string) error
}

// MarshalPayload is a helper for adapters constructing event payloads.
func MarshalPayload(payload any) json.RawMessage {
	if payload == nil {
		return nil
	}
	raw, err := json.Marshal(payload)
	if err != nil {
		return nil
	}
	return raw
}

// ProtocolSession converts an adapter Session to the wire model.
func ProtocolSession(nodeID, agentID string, session Session) protocol.SessionView {
	return protocol.SessionView{
		SessionID:        session.ID,
		AgentID:          agentID,
		NodeID:           nodeID,
		Title:            session.Title,
		WorkingDirectory: session.WorkingDirectory,
		Status:           session.Status,
		UpdatedAt:        session.UpdatedAt,
	}
}
