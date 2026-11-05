// Package mock provides a deterministic in-process Agent Adapter used by the
// Node standalone server, simulator, tests and Mobile/Terminal development.
package mock

import (
	"context"
	"errors"
	"fmt"
	"strings"
	"sync"
	"time"

	"github.com/FlyingBassHot/meetagent/internal/node/adapter"
	"github.com/FlyingBassHot/meetagent/pkg/protocol"
)

const defaultReplyDelay = 250 * time.Millisecond

// Options configures one mock Agent instance.
type Options struct {
	Sessions    []adapter.Session
	ReplyDelay  time.Duration
	Permissions bool
}

type activePrompt struct {
	cancel context.CancelFunc
}

// Adapter is a deterministic AgentAdapter for development and integration
// tests. It never touches the network.
type Adapter struct {
	options Options

	mu        sync.RWMutex
	connected bool
	sessions  []adapter.Session
	active    map[string]*activePrompt
	pending   map[string]chan string
	events    chan adapter.Event
}

// New creates a mock adapter. When no sessions are configured, a default
// "mock-session" is exposed.
func New(options Options) *Adapter {
	if options.ReplyDelay <= 0 {
		options.ReplyDelay = defaultReplyDelay
	}
	sessions := append([]adapter.Session(nil), options.Sessions...)
	if len(sessions) == 0 {
		now := time.Now().UTC()
		sessions = []adapter.Session{{
			ID:        "mock-session",
			Title:     "Mock Session",
			Status:    "idle",
			UpdatedAt: now,
		}}
	}
	for index := range sessions {
		if sessions[index].Status == "" {
			sessions[index].Status = "idle"
		}
		if sessions[index].UpdatedAt.IsZero() {
			sessions[index].UpdatedAt = time.Now().UTC()
		}
	}
	return &Adapter{
		options:  options,
		sessions: sessions,
		active:   make(map[string]*activePrompt),
		pending:  make(map[string]chan string),
		events:   make(chan adapter.Event, 256),
	}
}

// Type returns the adapter type.
func (a *Adapter) Type() string { return "mock" }

// Capabilities reports every normalized operation.
func (a *Adapter) Capabilities() adapter.Capabilities {
	return adapter.Capabilities{
		adapter.CapabilityListSessions:       true,
		adapter.CapabilitySendPrompt:         true,
		adapter.CapabilityInterrupt:          true,
		adapter.CapabilitySubscribeEvents:    true,
		adapter.CapabilityPermissionResponse: a.options.Permissions,
	}
}

// Manifest describes the mock adapter.
func (a *Adapter) Manifest() adapter.Manifest {
	return adapter.Manifest{
		ID:         "mock",
		Name:       "Mock Agent",
		Version:    "2.1.0",
		APIVersion: 1,
		Runtime:    "in-process",
		Transport:  []string{"memory"},
		Capabilities: []string{
			adapter.CapabilityListSessions,
			adapter.CapabilitySendPrompt,
			adapter.CapabilityInterrupt,
			adapter.CapabilitySubscribeEvents,
		},
		Permissions: []string{"session.read", "session.control"},
	}
}

// Connect marks the adapter online.
func (a *Adapter) Connect(context.Context) error {
	a.mu.Lock()
	a.connected = true
	a.mu.Unlock()
	return nil
}

// Disconnect cancels active prompts and marks the adapter offline.
func (a *Adapter) Disconnect(context.Context) error {
	a.mu.Lock()
	a.connected = false
	for id, active := range a.active {
		active.cancel()
		delete(a.active, id)
	}
	for id, decision := range a.pending {
		select {
		case decision <- "reject":
		default:
		}
		delete(a.pending, id)
	}
	a.mu.Unlock()
	return nil
}

// ListSessions returns a copy of the configured sessions.
func (a *Adapter) ListSessions(context.Context) ([]adapter.Session, error) {
	a.mu.RLock()
	defer a.mu.RUnlock()
	if !a.connected {
		return nil, adapter.ErrUnavailable
	}
	return append([]adapter.Session(nil), a.sessions...), nil
}

// SubscribeEvents returns the adapter event channel.
func (a *Adapter) SubscribeEvents(context.Context) (<-chan adapter.Event, error) {
	a.mu.RLock()
	connected := a.connected
	a.mu.RUnlock()
	if !connected {
		return nil, adapter.ErrUnavailable
	}
	return a.events, nil
}

// SendPrompt admits a prompt and produces a short streamed reply in the
// background. A prompt containing "permission" emits a permission request
// first when permission support is enabled.
func (a *Adapter) SendPrompt(ctx context.Context, sessionID, prompt string) (adapter.PromptResult, error) {
	a.mu.Lock()
	if !a.connected {
		a.mu.Unlock()
		return adapter.PromptResult{}, adapter.ErrUnavailable
	}
	if !a.hasSessionLocked(sessionID) {
		a.mu.Unlock()
		return adapter.PromptResult{}, adapter.ErrSessionNotFound
	}
	if _, busy := a.active[sessionID]; busy {
		a.mu.Unlock()
		return adapter.PromptResult{}, fmt.Errorf("mock: session %s is busy", sessionID)
	}
	// A prompt outlives the command request context: the Agent executes it
	// asynchronously, just like a real adapter. The context is only used to
	// cancel via Interrupt/Disconnect.
	promptCtx, cancel := context.WithTimeout(context.Background(), 10*time.Minute)
	a.active[sessionID] = &activePrompt{cancel: cancel}
	a.mu.Unlock()

	messageID := protocol.NewID("msg")
	go a.runPrompt(promptCtx, sessionID, messageID, prompt)
	return adapter.PromptResult{MessageID: messageID}, nil
}

// Interrupt cancels the active prompt of a session.
func (a *Adapter) Interrupt(_ context.Context, sessionID string) (adapter.InterruptResult, error) {
	a.mu.Lock()
	active, ok := a.active[sessionID]
	if ok {
		active.cancel()
		delete(a.active, sessionID)
	}
	a.mu.Unlock()
	if !ok {
		return adapter.InterruptResult{Interrupted: false}, nil
	}
	a.emit(adapter.Event{
		Type:      protocol.EventSessionStatusChange,
		SessionID: sessionID,
		Status:    "idle",
		Payload: adapter.SessionEventPayload{
			SessionID: sessionID,
			Status:    "idle",
			UpdatedAt: time.Now().UTC(),
		},
	})
	return adapter.InterruptResult{Interrupted: true}, nil
}

// RespondPermission resolves a pending mock permission request.
func (a *Adapter) RespondPermission(_ context.Context, sessionID, permissionID, decision, message string) error {
	a.mu.Lock()
	decisionCh, ok := a.pending[permissionID]
	if ok {
		delete(a.pending, permissionID)
	}
	a.mu.Unlock()
	if !ok {
		return adapter.ErrSessionNotFound
	}
	select {
	case decisionCh <- decision:
		return nil
	default:
		return errors.New("mock: permission decision was not accepted")
	}
}

func (a *Adapter) runPrompt(ctx context.Context, sessionID, messageID, prompt string) {
	defer func() {
		a.mu.Lock()
		delete(a.active, sessionID)
		a.mu.Unlock()
	}()

	a.emit(adapter.Event{
		Type:      protocol.EventSessionStatusChange,
		SessionID: sessionID,
		Status:    "running",
		Payload: adapter.SessionEventPayload{
			SessionID: sessionID,
			Status:    "running",
			UpdatedAt: time.Now().UTC(),
		},
	})

	if a.options.Permissions && strings.Contains(strings.ToLower(prompt), "permission") {
		if !a.requestPermission(ctx, sessionID) {
			a.emit(adapter.Event{
				Type:      protocol.EventSessionStatusChange,
				SessionID: sessionID,
				Status:    "idle",
				Payload: adapter.SessionEventPayload{
					SessionID: sessionID,
					Status:    "idle",
					UpdatedAt: time.Now().UTC(),
				},
			})
			return
		}
	}

	text := fmt.Sprintf("mock reply: %s", strings.TrimSpace(prompt))
	a.emit(adapter.Event{
		Type:      protocol.EventMessageStarted,
		SessionID: sessionID,
		Payload:   adapter.MessageEventPayload{MessageID: messageID, Kind: "assistant"},
	})

	delay := a.options.ReplyDelay
	if delay <= 0 {
		delay = defaultReplyDelay
	}
	if !wait(ctx, delay) {
		return
	}

	a.emit(adapter.Event{
		Type:      protocol.EventMessageDelta,
		SessionID: sessionID,
		Payload:   adapter.MessageEventPayload{MessageID: messageID, Delta: text},
	})
	a.emit(adapter.Event{
		Type:      protocol.EventMessageCompleted,
		SessionID: sessionID,
		Payload:   adapter.MessageEventPayload{MessageID: messageID, Text: text, Finish: "stop"},
	})
	a.emit(adapter.Event{
		Type:      protocol.EventSessionStatusChange,
		SessionID: sessionID,
		Status:    "idle",
		Payload: adapter.SessionEventPayload{
			SessionID: sessionID,
			Status:    "idle",
			UpdatedAt: time.Now().UTC(),
		},
	})
}

func (a *Adapter) requestPermission(ctx context.Context, sessionID string) bool {
	permissionID := protocol.NewID("per")
	decisionCh := make(chan string, 1)
	a.mu.Lock()
	a.pending[permissionID] = decisionCh
	a.mu.Unlock()

	a.emit(adapter.Event{
		Type:      protocol.EventToolStarted,
		SessionID: sessionID,
		Payload:   adapter.ToolEventPayload{CallID: permissionID, Name: "mock_tool", Status: "running"},
	})
	a.emit(adapter.Event{
		Type:      protocol.EventPermissionRequested,
		SessionID: sessionID,
		Payload: protocol.PermissionEventPayload{
			PermissionID: permissionID,
			Description:  "mock permission request",
			SessionID:    sessionID,
			Status:       protocol.PermissionPending,
			Action:       "mock",
			Resources:    []string{"mock"},
		},
	})

	select {
	case <-ctx.Done():
		a.mu.Lock()
		delete(a.pending, permissionID)
		a.mu.Unlock()
		return false
	case decision := <-decisionCh:
		approved := decision == "once" || decision == "always" || decision == "approve"
		status := protocol.PermissionRejected
		if approved {
			status = protocol.PermissionApproved
		}
		a.emit(adapter.Event{
			Type:      protocol.EventPermissionResolved,
			SessionID: sessionID,
			Payload: protocol.PermissionEventPayload{
				PermissionID: permissionID,
				SessionID:    sessionID,
				Status:       status,
				Decision:     decision,
			},
		})
		if approved {
			a.emit(adapter.Event{
				Type:      protocol.EventToolCompleted,
				SessionID: sessionID,
				Payload:   adapter.ToolEventPayload{CallID: permissionID, Name: "mock_tool", Status: "completed"},
			})
			return true
		}
		a.emit(adapter.Event{
			Type:      protocol.EventToolFailed,
			SessionID: sessionID,
			Payload:   adapter.ToolEventPayload{CallID: permissionID, Name: "mock_tool", Status: "failed"},
		})
		return false
	}
}

func (a *Adapter) hasSessionLocked(sessionID string) bool {
	for _, session := range a.sessions {
		if session.ID == sessionID {
			return true
		}
	}
	return false
}

func (a *Adapter) emit(event adapter.Event) {
	if event.Timestamp.IsZero() {
		event.Timestamp = time.Now().UTC()
	}
	select {
	case a.events <- event:
	default:
		// The manager's buffer is intentionally bounded; drop rather than block
		// a mock prompt forever.
	}
}

func wait(ctx context.Context, delay time.Duration) bool {
	timer := time.NewTimer(delay)
	defer timer.Stop()
	select {
	case <-ctx.Done():
		return false
	case <-timer.C:
		return true
	}
}
