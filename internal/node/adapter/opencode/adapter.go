package opencode

import (
	"context"
	"errors"
	"fmt"
	"net/http"
	"strings"
	"sync"
	"time"

	"github.com/FlyingBassHot/meetagent/internal/node/adapter"
	"github.com/FlyingBassHot/meetagent/pkg/protocol"
)

var (
	errUnauthorized = errors.New("opencode authentication failed")
	errClosed       = errors.New("opencode adapter is not connected")
)

// Adapter implements adapter.AgentAdapter for one OpenCode server.
type Adapter struct {
	options Options
	client  *client
	caps    adapter.Capabilities

	mu        sync.RWMutex
	connected bool
	cancel    context.CancelFunc
}

// New creates an OpenCode adapter. Connect performs the first health check.
func New(options Options) *Adapter {
	return &Adapter{
		options: options.withDefaults(),
		client:  newClient(options),
		caps: adapter.Capabilities{
			adapter.CapabilityListSessions:       true,
			adapter.CapabilitySendPrompt:         true,
			adapter.CapabilityInterrupt:          true,
			adapter.CapabilitySubscribeEvents:    true,
			adapter.CapabilityPermissionResponse: true,
			adapter.CapabilitySwitchModel:        false,
		},
	}
}

// Type returns the adapter type used on the wire.
func (a *Adapter) Type() string { return "opencode" }

// Capabilities reports the supported normalized operations.
func (a *Adapter) Capabilities() adapter.Capabilities { return a.caps.Clone() }

// Manifest describes this adapter for discovery and capability negotiation.
func (a *Adapter) Manifest() adapter.Manifest {
	return adapter.Manifest{
		ID:         "opencode",
		Name:       "OpenCode Adapter",
		Version:    "2.1.0",
		APIVersion: 1,
		Runtime:    "local",
		Transport:  []string{"http", "sse"},
		Capabilities: []string{
			adapter.CapabilityListSessions,
			adapter.CapabilitySendPrompt,
			adapter.CapabilityInterrupt,
			adapter.CapabilitySubscribeEvents,
			adapter.CapabilityPermissionResponse,
		},
		Permissions: []string{"session.read", "session.control", "permission.respond"},
	}
}

// Connect verifies the OpenCode server and records the connection.
func (a *Adapter) Connect(ctx context.Context) error {
	var info struct {
		Version string `json:"version"`
	}
	if err := a.client.do(ctx, http.MethodGet, "/api/info", nil, &info); err != nil {
		if errors.Is(err, errUnauthorized) {
			return fmt.Errorf("%w: %v", adapter.ErrUnauthorized, err)
		}
		return fmt.Errorf("connect opencode %s: %w", a.options.BaseURL, err)
	}
	a.mu.Lock()
	a.connected = true
	a.mu.Unlock()
	return nil
}

// Disconnect stops any event stream and marks the adapter unavailable.
func (a *Adapter) Disconnect(ctx context.Context) error {
	a.mu.Lock()
	a.connected = false
	cancel := a.cancel
	a.cancel = nil
	a.mu.Unlock()
	if cancel != nil {
		cancel()
	}
	return nil
}

func (a *Adapter) isConnected() bool {
	a.mu.RLock()
	defer a.mu.RUnlock()
	return a.connected
}

// ListSessions merges the session list with the active-execution map so each
// session carries a normalized running/idle status.
func (a *Adapter) ListSessions(ctx context.Context) ([]adapter.Session, error) {
	if !a.isConnected() {
		return nil, errClosed
	}

	var listed struct {
		Data []sessionInfo `json:"data"`
	}
	path := fmt.Sprintf("/api/session?limit=%d&order=desc", sessionLimit)
	if err := a.client.do(ctx, http.MethodGet, path, nil, &listed); err != nil {
		return nil, err
	}

	active, err := a.activeSessions(ctx)
	if err != nil {
		// The active map is an optimization; listing alone is still useful.
		active = nil
	}

	sessions := make([]adapter.Session, 0, len(listed.Data))
	for _, info := range listed.Data {
		status := "idle"
		if state, ok := active[info.ID]; ok {
			status = normalizeActiveState(state)
		}
		sessions = append(sessions, info.session(status))
	}
	return sessions, nil
}

type activeState struct {
	Type string `json:"type"`
}

func (a *Adapter) activeSessions(ctx context.Context) (map[string]activeState, error) {
	var active struct {
		Data map[string]activeState `json:"data"`
	}
	if err := a.client.do(ctx, http.MethodGet, "/api/session/active", nil, &active); err != nil {
		return nil, err
	}
	if active.Data == nil {
		active.Data = map[string]activeState{}
	}
	return active.Data, nil
}

func normalizeActiveState(state activeState) string {
	switch strings.ToLower(state.Type) {
	case "running", "busy", "working":
		return "running"
	case "retry", "error":
		return "error"
	default:
		return "idle"
	}
}

// SendPrompt submits a prompt to an existing session. OpenCode acknowledges
// admission only; progress arrives through the event stream.
func (a *Adapter) SendPrompt(ctx context.Context, sessionID, prompt string) (adapter.PromptResult, error) {
	if !a.isConnected() {
		return adapter.PromptResult{}, errClosed
	}
	if strings.TrimSpace(sessionID) == "" {
		return adapter.PromptResult{}, adapter.ErrSessionNotFound
	}

	var reply struct {
		Data struct {
			ID string `json:"id"`
		} `json:"data"`
	}
	path := fmt.Sprintf("/api/session/%s/prompt", escapeSession(sessionID))
	err := a.client.do(ctx, http.MethodPost, path, map[string]string{"text": prompt}, &reply)
	if err != nil {
		return adapter.PromptResult{}, mapError(err)
	}
	return adapter.PromptResult{MessageID: reply.Data.ID}, nil
}

// Interrupt asks OpenCode to stop the current execution of a session.
func (a *Adapter) Interrupt(ctx context.Context, sessionID string) (adapter.InterruptResult, error) {
	if !a.isConnected() {
		return adapter.InterruptResult{}, errClosed
	}
	var reply struct {
		Interrupted bool `json:"interrupted"`
	}
	path := fmt.Sprintf("/api/session/%s/interrupt", escapeSession(sessionID))
	if err := a.client.do(ctx, http.MethodPost, path, nil, &reply); err != nil {
		return adapter.InterruptResult{}, mapError(err)
	}
	return adapter.InterruptResult{Interrupted: reply.Interrupted}, nil
}

// RespondPermission replies once|always|reject to a pending permission.
func (a *Adapter) RespondPermission(ctx context.Context, sessionID, permissionID, decision, message string) error {
	if !a.isConnected() {
		return errClosed
	}
	if sessionID == "" || permissionID == "" {
		return adapter.ErrSessionNotFound
	}
	reply := map[string]string{"decision": normalizeDecision(decision)}
	if message != "" {
		reply["message"] = message
	}
	path := fmt.Sprintf("/api/session/%s/permission/%s/reply", escapeSession(sessionID), escapePermission(permissionID))
	if err := a.client.do(ctx, http.MethodPost, path, reply, nil); err != nil {
		return mapError(err)
	}
	return nil
}

func normalizeDecision(decision string) string {
	switch strings.ToLower(strings.TrimSpace(decision)) {
	case "allow", "once", "approved", "approve", "yes":
		return "once"
	case "always", "persist":
		return "always"
	default:
		return "reject"
	}
}

func mapError(err error) error {
	var apiErr *apiError
	if errors.As(err, &apiErr) {
		if apiErr.NotFound() {
			return fmt.Errorf("%w: %v", adapter.ErrSessionNotFound, err)
		}
	}
	if errors.Is(err, errUnauthorized) {
		return fmt.Errorf("%w: %v", adapter.ErrUnauthorized, err)
	}
	return err
}

// SubscribeEvents starts the SSE reader and returns the normalized channel.
func (a *Adapter) SubscribeEvents(ctx context.Context) (<-chan adapter.Event, error) {
	if !a.isConnected() {
		return nil, errClosed
	}
	streamCtx, cancel := context.WithCancel(ctx)
	events := make(chan adapter.Event, 256)

	a.mu.Lock()
	if a.cancel != nil {
		a.cancel()
	}
	a.cancel = cancel
	a.mu.Unlock()

	go a.runStream(streamCtx, events)
	return events, nil
}

func (a *Adapter) runStream(ctx context.Context, events chan<- adapter.Event) {
	defer close(events)
	for {
		err := a.streamOnce(ctx, events)
		if ctx.Err() != nil {
			return
		}
		// OpenCode SSE is volatile: reconnect with a short, bounded backoff and
		// let the Manager's periodic refresh repair any missed state.
		select {
		case <-ctx.Done():
			return
		case <-time.After(reconnectDelay(err)):
		}
	}
}

func reconnectDelay(err error) time.Duration {
	if err == nil {
		return 500 * time.Millisecond
	}
	var apiErr *apiError
	if errors.As(err, &apiErr) && apiErr.Status >= 500 {
		return 2 * time.Second
	}
	return time.Second
}

func protocolStatus(sessionID, status string, at time.Time) adapter.Event {
	return adapter.Event{
		Type:      protocol.EventSessionStatusChange,
		SessionID: sessionID,
		Timestamp: at,
		Status:    status,
		Payload: adapter.SessionEventPayload{
			SessionID: sessionID,
			Status:    status,
			UpdatedAt: at,
		},
	}
}
