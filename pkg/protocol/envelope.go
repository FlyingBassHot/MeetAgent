// Package protocol defines the shared wire format used by MeetAgent Cloud,
// Nodes and Devices. It intentionally contains no Agent specific concepts
// beyond the generic target identifiers, so new adapters do not change it.
package protocol

import (
	"crypto/rand"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"strings"
	"time"
)

const (
	// Version is the wire protocol version implemented by this build.
	Version = "1"

	// MaxMessageSize is the maximum accepted WebSocket message size (1 MiB).
	MaxMessageSize = 1 << 20
)

// MessageType enumerates the envelope types defined by the MVP protocol.
type MessageType string

const (
	TypeRegister  MessageType = "register"
	TypeHeartbeat MessageType = "heartbeat"
	TypeCommand   MessageType = "command"
	TypeResponse  MessageType = "response"
	TypeEvent     MessageType = "event"
	TypeSync      MessageType = "sync"
	TypeError     MessageType = "error"
)

// PeerType identifies one side of a connection.
type PeerType string

const (
	PeerNode   PeerType = "node"
	PeerDevice PeerType = "device"
	PeerCloud  PeerType = "cloud"
)

// Source describes who produced a message.
type Source struct {
	Type PeerType `json:"type"`
	ID   string   `json:"id"`
}

// Target describes where a message should be delivered. Fields that are not
// relevant to a given message are empty.
type Target struct {
	NodeID    string `json:"node_id,omitempty"`
	AgentID   string `json:"agent_id,omitempty"`
	SessionID string `json:"session_id,omitempty"`
}

// Envelope is the single message wrapper used by every connection.
type Envelope struct {
	Version   string          `json:"version"`
	MessageID string          `json:"message_id"`
	Type      MessageType     `json:"type"`
	Action    string          `json:"action,omitempty"`
	RequestID string          `json:"request_id,omitempty"`
	Source    Source          `json:"source"`
	Target    *Target         `json:"target,omitempty"`
	Timestamp time.Time       `json:"timestamp"`
	Payload   json.RawMessage `json:"payload,omitempty"`
}

// New creates an envelope with generated ID and UTC timestamp.
func New(messageType MessageType, action string, source Source) *Envelope {
	return &Envelope{
		Version:   Version,
		MessageID: NewID("msg"),
		Type:      messageType,
		Action:    action,
		Source:    source,
		Timestamp: time.Now().UTC(),
	}
}

// NewWithPayload creates an envelope and encodes payload into it.
func NewWithPayload(messageType MessageType, action string, source Source, payload any) (*Envelope, error) {
	env := New(messageType, action, source)
	if err := env.SetPayload(payload); err != nil {
		return nil, err
	}
	return env, nil
}

// SetPayload marshals value into the envelope payload.
func (e *Envelope) SetPayload(value any) error {
	raw, err := json.Marshal(value)
	if err != nil {
		return fmt.Errorf("encode payload: %w", err)
	}
	e.Payload = raw
	return nil
}

// DecodePayload unmarshals the envelope payload into dst.
func (e *Envelope) DecodePayload(dst any) error {
	if len(e.Payload) == 0 {
		return errors.New("payload is empty")
	}
	if err := json.Unmarshal(e.Payload, dst); err != nil {
		return fmt.Errorf("decode payload: %w", err)
	}
	return nil
}

// WithRequestID sets the request correlation identifier.
func (e *Envelope) WithRequestID(id string) *Envelope {
	e.RequestID = id
	return e
}

// WithTarget sets the routing target.
func (e *Envelope) WithTarget(target Target) *Envelope {
	e.Target = &target
	return e
}

// Validate checks the envelope independent of any business rules.
func (e *Envelope) Validate() error {
	if e == nil {
		return errors.New("envelope is nil")
	}
	if e.Version != Version {
		return fmt.Errorf("unsupported protocol version %q", e.Version)
	}
	if strings.TrimSpace(e.MessageID) == "" {
		return errors.New("message_id is required")
	}
	if !e.Type.Valid() {
		return fmt.Errorf("unknown message type %q", e.Type)
	}
	if !e.Source.Type.ValidPeer() {
		return fmt.Errorf("invalid source type %q", e.Source.Type)
	}
	if strings.TrimSpace(e.Source.ID) == "" {
		return errors.New("source.id is required")
	}
	switch e.Type {
	case TypeCommand:
		if e.Action == "" {
			return errors.New("command action is required")
		}
		if e.RequestID == "" {
			return errors.New("command request_id is required")
		}
		if e.Target == nil || e.Target.NodeID == "" {
			return errors.New("command target.node_id is required")
		}
	case TypeResponse:
		if e.RequestID == "" {
			return errors.New("response request_id is required")
		}
	case TypeSync:
		if e.Action == "" {
			return errors.New("sync action is required")
		}
	}
	return nil
}

// Valid reports whether the message type is known.
func (t MessageType) Valid() bool {
	switch t {
	case TypeRegister, TypeHeartbeat, TypeCommand, TypeResponse, TypeEvent, TypeSync, TypeError:
		return true
	default:
		return false
	}
}

// ValidPeer reports whether the peer type can appear on the wire.
func (t PeerType) ValidPeer() bool {
	switch t {
	case PeerNode, PeerDevice, PeerCloud:
		return true
	default:
		return false
	}
}

// NewID returns a prefixed, random, URL-safe identifier.
func NewID(prefix string) string {
	var buf [8]byte
	if _, err := rand.Read(buf[:]); err != nil {
		// crypto/rand failing is unrecoverable for our purposes. Fall back to
		// a timestamp so callers still receive a unique-enough identifier.
		return fmt.Sprintf("%s_%d", prefix, time.Now().UnixNano())
	}
	return prefix + "_" + hex.EncodeToString(buf[:])
}
