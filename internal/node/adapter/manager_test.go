package adapter

import (
	"context"
	"errors"
	"log/slog"
	"sync"
	"testing"
	"time"

	"github.com/FlyingBassHot/meetagent/pkg/protocol"
)

// fakeAdapter is a controllable in-memory AgentAdapter for Manager tests.
type fakeAdapter struct {
	typeName string
	caps     Capabilities

	connectErr error
	listErr    error
	promptErr  error
	interErr   error
	permErr    error

	mu            sync.Mutex
	connected     bool
	connects      int
	disconnects   int
	sessions      []Session
	events        chan Event
	prompts       []promptCall
	interrupts    []string
	permissions   []permissionCall
	subscribed    int
	subscribeFail error
}

type promptCall struct {
	sessionID string
	text      string
}

type permissionCall struct {
	sessionID    string
	permissionID string
	decision     string
}

func newFakeAdapter(typeName string, sessions ...Session) *fakeAdapter {
	return &fakeAdapter{
		typeName: typeName,
		caps: Capabilities{
			CapabilityListSessions:       true,
			CapabilitySendPrompt:         true,
			CapabilityInterrupt:          true,
			CapabilitySubscribeEvents:    true,
			CapabilityPermissionResponse: true,
		},
		sessions: sessions,
	}
}

func (f *fakeAdapter) Type() string { return f.typeName }

func (f *fakeAdapter) Connect(ctx context.Context) error {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.connects++
	if f.connectErr != nil {
		return f.connectErr
	}
	f.connected = true
	return nil
}

func (f *fakeAdapter) Disconnect(ctx context.Context) error {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.disconnects++
	f.connected = false
	return nil
}

func (f *fakeAdapter) Capabilities() Capabilities { return f.caps.Clone() }

func (f *fakeAdapter) ListSessions(ctx context.Context) ([]Session, error) {
	f.mu.Lock()
	defer f.mu.Unlock()
	if f.listErr != nil {
		return nil, f.listErr
	}
	return append([]Session(nil), f.sessions...), nil
}

func (f *fakeAdapter) SendPrompt(ctx context.Context, sessionID, prompt string) (PromptResult, error) {
	f.mu.Lock()
	defer f.mu.Unlock()
	if f.promptErr != nil {
		return PromptResult{}, f.promptErr
	}
	f.prompts = append(f.prompts, promptCall{sessionID: sessionID, text: prompt})
	return PromptResult{MessageID: "msg_test"}, nil
}

func (f *fakeAdapter) Interrupt(ctx context.Context, sessionID string) (InterruptResult, error) {
	f.mu.Lock()
	defer f.mu.Unlock()
	if f.interErr != nil {
		return InterruptResult{}, f.interErr
	}
	f.interrupts = append(f.interrupts, sessionID)
	return InterruptResult{Interrupted: true}, nil
}

func (f *fakeAdapter) RespondPermission(ctx context.Context, sessionID, permissionID, decision, message string) error {
	f.mu.Lock()
	defer f.mu.Unlock()
	if f.permErr != nil {
		return f.permErr
	}
	f.permissions = append(f.permissions, permissionCall{sessionID: sessionID, permissionID: permissionID, decision: decision})
	return nil
}

func (f *fakeAdapter) SubscribeEvents(ctx context.Context) (<-chan Event, error) {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.subscribed++
	if f.subscribeFail != nil {
		return nil, f.subscribeFail
	}
	if f.events == nil {
		f.events = make(chan Event, 16)
	}
	return f.events, nil
}

func (f *fakeAdapter) push(event Event) {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.events <- event
}

func (f *fakeAdapter) setSessions(sessions []Session) {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.sessions = sessions
}

func testLogger() *slog.Logger {
	return slog.New(slog.NewTextHandler(discardWriter{}, &slog.HandlerOptions{Level: slog.LevelError}))
}

type discardWriter struct{}

func (discardWriter) Write(p []byte) (int, error) { return len(p), nil }

func waitFor(t *testing.T, timeout time.Duration, what string, cond func() bool) {
	t.Helper()
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		if cond() {
			return
		}
		time.Sleep(5 * time.Millisecond)
	}
	t.Fatalf("timed out waiting for %s", what)
}

func startManager(t *testing.T, manager *Manager) context.CancelFunc {
	t.Helper()
	ctx, cancel := context.WithCancel(context.Background())
	manager.Start(ctx)
	t.Cleanup(func() {
		cancel()
		stopCtx, stopCancel := context.WithTimeout(context.Background(), time.Second)
		defer stopCancel()
		manager.Stop(stopCtx)
	})
	return cancel
}

func agentStatus(m *Manager, agentID string) string {
	for _, agent := range m.Agents() {
		if agent.AgentID == agentID {
			return agent.Status
		}
	}
	return ""
}

func sessionStatus(m *Manager, agentID, sessionID string) string {
	for _, agent := range m.Agents() {
		if agent.AgentID != agentID {
			continue
		}
		for _, session := range agent.Sessions {
			if session.SessionID == sessionID {
				return session.Status
			}
		}
	}
	return ""
}

func TestManagerAddValidation(t *testing.T) {
	manager := NewManager("node-1", testLogger())

	if err := manager.Add(Instance{}); err == nil {
		t.Fatal("expected error for missing agent id")
	}
	if err := manager.Add(Instance{ID: "a1"}); err == nil {
		t.Fatal("expected error for missing adapter")
	}
	if err := manager.Add(Instance{ID: "a1", Adapter: newFakeAdapter("fake")}); err != nil {
		t.Fatalf("Add: %v", err)
	}
	if err := manager.Add(Instance{ID: "a1", Adapter: newFakeAdapter("fake")}); err == nil {
		t.Fatal("expected duplicate agent id error")
	}
}

func TestManagerStartsAdapterAndPublishesState(t *testing.T) {
	manager := NewManager("node-1", testLogger())
	fake := newFakeAdapter("fake", Session{ID: "s1", Title: "First"})
	if err := manager.Add(Instance{ID: "a1", Name: "Agent One", Adapter: fake}); err != nil {
		t.Fatal(err)
	}
	startManager(t, manager)

	waitFor(t, 2*time.Second, "agent online", func() bool {
		return agentStatus(manager, "a1") == string(StatusOnline)
	})

	agents := manager.Agents()
	if len(agents) != 1 {
		t.Fatalf("agents = %d, want 1", len(agents))
	}
	if agents[0].Name != "Agent One" || agents[0].Type != "fake" || agents[0].NodeID != "node-1" {
		t.Fatalf("unexpected agent view: %+v", agents[0])
	}
	waitFor(t, 2*time.Second, "session inventory", func() bool {
		return sessionStatus(manager, "a1", "s1") != ""
	})

	// The connected event must be forwarded to the Node connection.
	waitFor(t, 2*time.Second, "agent.connected event", func() bool {
		select {
		case event := <-manager.Events():
			return event.EventType == protocol.EventAgentConnected && event.AgentID == "a1"
		default:
			return false
		}
	})
}

func TestManagerSendPromptAndInterrupt(t *testing.T) {
	manager := NewManager("node-1", testLogger())
	fake := newFakeAdapter("fake", Session{ID: "s1", Status: "idle", UpdatedAt: time.Now()})
	if err := manager.Add(Instance{ID: "a1", Adapter: fake}); err != nil {
		t.Fatal(err)
	}
	startManager(t, manager)
	waitFor(t, 2*time.Second, "agent online", func() bool {
		return agentStatus(manager, "a1") == string(StatusOnline)
	})

	result, err := manager.SendPrompt(context.Background(), "a1", "s1", "hello")
	if err != nil {
		t.Fatalf("SendPrompt: %v", err)
	}
	if result.MessageID != "msg_test" {
		t.Fatalf("MessageID = %q, want msg_test", result.MessageID)
	}
	if got := sessionStatus(manager, "a1", "s1"); got != "running" {
		t.Fatalf("session status = %q, want running", got)
	}
	waitFor(t, time.Second, "agent busy", func() bool {
		return agentStatus(manager, "a1") == string(StatusBusy)
	})

	fake.mu.Lock()
	prompts := append([]promptCall(nil), fake.prompts...)
	fake.mu.Unlock()
	if len(prompts) != 1 || prompts[0].text != "hello" || prompts[0].sessionID != "s1" {
		t.Fatalf("unexpected prompt calls: %+v", prompts)
	}

	if _, err := manager.Interrupt(context.Background(), "a1", "s1"); err != nil {
		t.Fatalf("Interrupt: %v", err)
	}
	fake.mu.Lock()
	interrupts := append([]string(nil), fake.interrupts...)
	fake.mu.Unlock()
	if len(interrupts) != 1 || interrupts[0] != "s1" {
		t.Fatalf("unexpected interrupts: %+v", interrupts)
	}
}

func TestManagerRejectsUnknownTargetsAndCapabilities(t *testing.T) {
	manager := NewManager("node-1", testLogger())
	fake := newFakeAdapter("fake", Session{ID: "s1"})
	fake.caps[CapabilityInterrupt] = false
	if err := manager.Add(Instance{ID: "a1", Adapter: fake, CapabilityOverrides: map[string]bool{CapabilitySendPrompt: false}}); err != nil {
		t.Fatal(err)
	}
	startManager(t, manager)
	waitFor(t, 2*time.Second, "agent online", func() bool {
		return agentStatus(manager, "a1") == string(StatusOnline)
	})

	if _, err := manager.SendPrompt(context.Background(), "missing", "s1", "hi"); err == nil {
		t.Fatal("expected unknown agent error")
	}
	if _, err := manager.SendPrompt(context.Background(), "a1", "s1", "hi"); !errors.Is(err, ErrNotSupported) {
		t.Fatalf("expected ErrNotSupported for overridden capability, got %v", err)
	}
	if _, err := manager.Interrupt(context.Background(), "a1", "s1"); !errors.Is(err, ErrNotSupported) {
		t.Fatalf("expected ErrNotSupported for interrupt, got %v", err)
	}
	if _, err := manager.Interrupt(context.Background(), "a1", "s2"); !errors.Is(err, ErrNotSupported) {
		t.Fatalf("capability check should run before session check, got %v", err)
	}
	if err := manager.RespondPermission(context.Background(), "a1", "s1", "p1", "once", ""); err != nil {
		t.Fatalf("RespondPermission: %v", err)
	}
	fake.mu.Lock()
	permissions := append([]permissionCall(nil), fake.permissions...)
	fake.mu.Unlock()
	if len(permissions) != 1 || permissions[0].permissionID != "p1" {
		t.Fatalf("unexpected permissions: %+v", permissions)
	}
}

func TestManagerUnknownSessionIsReported(t *testing.T) {
	manager := NewManager("node-1", testLogger())
	fake := newFakeAdapter("fake", Session{ID: "s1"})
	if err := manager.Add(Instance{ID: "a1", Adapter: fake}); err != nil {
		t.Fatal(err)
	}
	startManager(t, manager)
	waitFor(t, 2*time.Second, "agent online", func() bool {
		return agentStatus(manager, "a1") == string(StatusOnline)
	})

	if _, err := manager.SendPrompt(context.Background(), "a1", "nope", "hi"); !errors.Is(err, ErrSessionNotFound) {
		t.Fatalf("expected ErrSessionNotFound, got %v", err)
	}
}

func TestManagerOfflineAgentIsUnavailable(t *testing.T) {
	manager := NewManager("node-1", testLogger())
	fake := newFakeAdapter("fake", Session{ID: "s1"})
	if err := manager.Add(Instance{ID: "a1", Adapter: fake}); err != nil {
		t.Fatal(err)
	}
	// No Start: the adapter was never connected.
	if _, err := manager.SendPrompt(context.Background(), "a1", "s1", "hi"); !errors.Is(err, ErrUnavailable) {
		t.Fatalf("expected ErrUnavailable, got %v", err)
	}
}

func TestManagerAppliesNormalizedEvents(t *testing.T) {
	manager := NewManager("node-1", testLogger())
	fake := newFakeAdapter("fake", Session{ID: "s1", Status: "idle"})
	if err := manager.Add(Instance{ID: "a1", Adapter: fake}); err != nil {
		t.Fatal(err)
	}
	startManager(t, manager)
	waitFor(t, 2*time.Second, "agent online", func() bool {
		return agentStatus(manager, "a1") == string(StatusOnline)
	})

	fake.push(Event{
		Type:      protocol.EventSessionCreated,
		SessionID: "s2",
		Timestamp: time.Now().UTC(),
		Payload:   SessionEventPayload{SessionID: "s2", Title: "New Session"},
	})
	waitFor(t, 2*time.Second, "session created", func() bool {
		return sessionStatus(manager, "a1", "s2") != ""
	})

	fake.push(Event{
		Type:      protocol.EventSessionStatusChange,
		SessionID: "s2",
		Timestamp: time.Now().UTC(),
		Status:    "running",
	})
	waitFor(t, 2*time.Second, "session running", func() bool {
		return sessionStatus(manager, "a1", "s2") == "running"
	})
	waitFor(t, 2*time.Second, "agent busy", func() bool {
		return agentStatus(manager, "a1") == string(StatusBusy)
	})

	fake.push(Event{
		Type:      protocol.EventSessionDeleted,
		SessionID: "s2",
		Timestamp: time.Now().UTC(),
		Payload:   SessionEventPayload{SessionID: "s2", Status: "deleted"},
	})
	waitFor(t, 2*time.Second, "session deleted", func() bool {
		for _, agent := range manager.Agents() {
			for _, session := range agent.Sessions {
				if session.SessionID == "s2" {
					return false
				}
			}
		}
		return true
	})
}

func TestManagerMergesStaticSessions(t *testing.T) {
	manager := NewManager("node-1", testLogger())
	fake := newFakeAdapter("fake", Session{ID: "live", Title: "Live", UpdatedAt: time.Now()})
	if err := manager.Add(Instance{
		ID:             "a1",
		Adapter:        fake,
		StaticSessions: []Session{{ID: "static", Title: "Static", Status: "idle"}},
	}); err != nil {
		t.Fatal(err)
	}
	startManager(t, manager)
	waitFor(t, 2*time.Second, "agent online", func() bool {
		return agentStatus(manager, "a1") == string(StatusOnline)
	})

	if got := sessionStatus(manager, "a1", "live"); got == "" {
		t.Fatal("live session missing")
	}
	if got := sessionStatus(manager, "a1", "static"); got == "" {
		t.Fatal("static session missing")
	}
	if _, err := manager.SendPrompt(context.Background(), "a1", "static", "hi"); err != nil {
		t.Fatalf("static session should be addressable: %v", err)
	}
}

func TestManagerRefreshEmitsSessionDiff(t *testing.T) {
	manager := NewManager("node-1", testLogger())
	fake := newFakeAdapter("fake", Session{ID: "s1", Status: "idle", UpdatedAt: time.Now()})
	if err := manager.Add(Instance{ID: "a1", Adapter: fake}); err != nil {
		t.Fatal(err)
	}
	startManager(t, manager)
	waitFor(t, 2*time.Second, "agent online", func() bool {
		return agentStatus(manager, "a1") == string(StatusOnline)
	})

	// Drain the initial events.
	for {
		select {
		case <-manager.Events():
			continue
		default:
		}
		break
	}

	fake.setSessions([]Session{
		{ID: "s1", Status: "running", UpdatedAt: time.Now()},
		{ID: "s2", Status: "idle", UpdatedAt: time.Now()},
	})
	if _, err := manager.ListSessions(context.Background(), "a1"); err != nil {
		t.Fatalf("ListSessions: %v", err)
	}

	seen := map[string]bool{}
	deadline := time.Now().Add(time.Second)
	for time.Now().Before(deadline) && (!seen[protocol.EventSessionCreated] || !seen[protocol.EventSessionStatusChange]) {
		select {
		case event := <-manager.Events():
			seen[event.EventType] = true
		case <-time.After(10 * time.Millisecond):
		}
	}
	if !seen[protocol.EventSessionCreated] {
		t.Fatal("missing session.created event for s2")
	}
	if !seen[protocol.EventSessionStatusChange] {
		t.Fatal("missing session.status_change event for s1")
	}
}

func TestManagerConnectRetries(t *testing.T) {
	manager := NewManager("node-1", testLogger())
	fake := newFakeAdapter("fake")
	fake.connectErr = errors.New("boom")
	if err := manager.Add(Instance{ID: "a1", Adapter: fake}); err != nil {
		t.Fatal(err)
	}
	startManager(t, manager)
	waitFor(t, 2*time.Second, "agent error", func() bool {
		return agentStatus(manager, "a1") == string(StatusError)
	})

	fake.mu.Lock()
	connects := fake.connects
	fake.mu.Unlock()
	if connects < 1 {
		t.Fatalf("expected at least one connect attempt, got %d", connects)
	}
}
