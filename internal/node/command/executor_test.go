package command

import (
	"context"
	"encoding/json"
	"log/slog"
	"testing"
	"time"

	"github.com/FlyingBassHot/meetagent/internal/node/adapter"
	"github.com/FlyingBassHot/meetagent/internal/node/config"
	"github.com/FlyingBassHot/meetagent/pkg/protocol"
)

// stubAdapter is a minimal AgentAdapter used to drive the Executor.
type stubAdapter struct {
	caps         adapter.Capabilities
	sessions     []adapter.Session
	prompts      []string
	interrupted  []string
	permissions  []string
	promptResult adapter.PromptResult
	promptErr    error
}

func newStubAdapter() *stubAdapter {
	return &stubAdapter{
		caps: adapter.Capabilities{
			adapter.CapabilityListSessions:       true,
			adapter.CapabilitySendPrompt:         true,
			adapter.CapabilityInterrupt:          true,
			adapter.CapabilitySubscribeEvents:    true,
			adapter.CapabilityPermissionResponse: true,
		},
		sessions:     []adapter.Session{{ID: "ses_1", Title: "Session", Status: "idle", UpdatedAt: time.Now()}},
		promptResult: adapter.PromptResult{MessageID: "msg_1"},
	}
}

func (s *stubAdapter) Type() string                         { return "stub" }
func (s *stubAdapter) Capabilities() adapter.Capabilities   { return s.caps.Clone() }
func (s *stubAdapter) Connect(ctx context.Context) error    { return nil }
func (s *stubAdapter) Disconnect(ctx context.Context) error { return nil }
func (s *stubAdapter) ListSessions(ctx context.Context) ([]adapter.Session, error) {
	return append([]adapter.Session(nil), s.sessions...), nil
}

func (s *stubAdapter) SendPrompt(ctx context.Context, sessionID, prompt string) (adapter.PromptResult, error) {
	if s.promptErr != nil {
		return adapter.PromptResult{}, s.promptErr
	}
	s.prompts = append(s.prompts, sessionID+":"+prompt)
	return s.promptResult, nil
}

func (s *stubAdapter) Interrupt(ctx context.Context, sessionID string) (adapter.InterruptResult, error) {
	s.interrupted = append(s.interrupted, sessionID)
	return adapter.InterruptResult{Interrupted: true}, nil
}

func (s *stubAdapter) RespondPermission(ctx context.Context, sessionID, permissionID, decision, message string) error {
	s.permissions = append(s.permissions, permissionID+":"+decision)
	return nil
}

func (s *stubAdapter) SubscribeEvents(ctx context.Context) (<-chan adapter.Event, error) {
	ch := make(chan adapter.Event)
	return ch, nil
}

func testManager(t *testing.T, stub *stubAdapter) *adapter.Manager {
	t.Helper()
	logger := slog.New(slog.NewTextHandler(discard{}, &slog.HandlerOptions{Level: slog.LevelError}))
	manager := adapter.NewManager("node-1", logger)
	if err := manager.Add(adapter.Instance{ID: "agent-1", Name: "Agent", Adapter: stub}); err != nil {
		t.Fatalf("add adapter: %v", err)
	}
	ctx, cancel := context.WithCancel(context.Background())
	manager.Start(ctx)
	t.Cleanup(func() {
		cancel()
		stopCtx, stopCancel := context.WithTimeout(context.Background(), time.Second)
		defer stopCancel()
		manager.Stop(stopCtx)
	})
	waitForStatus(t, manager, "agent-1", string(adapter.StatusOnline))
	return manager
}

type discard struct{}

func (discard) Write(p []byte) (int, error) { return len(p), nil }

func waitForStatus(t *testing.T, manager *adapter.Manager, agentID, want string) {
	t.Helper()
	deadline := time.Now().Add(2 * time.Second)
	for time.Now().Before(deadline) {
		for _, agent := range manager.Agents() {
			if agent.AgentID == agentID && agent.Status == want {
				return
			}
		}
		time.Sleep(5 * time.Millisecond)
	}
	t.Fatalf("agent %s did not reach status %s", agentID, want)
}

func testConfig(t *testing.T) config.Config {
	t.Helper()
	cfg := config.Default()
	cfg.Node.ID = "node-1"
	cfg.Node.Token = "token"
	return cfg
}

func envelope(t *testing.T, action string, target protocol.Target, payload any) *protocol.Envelope {
	t.Helper()
	env, err := protocol.NewWithPayload(protocol.TypeCommand, action, protocol.Source{
		Type: protocol.PeerDevice,
		ID:   "device-1",
	}, payload)
	if err != nil {
		t.Fatalf("new envelope: %v", err)
	}
	env.WithRequestID(protocol.NewID("req")).WithTarget(target)
	return env
}

func decodeData(t *testing.T, result protocol.CommandResult) map[string]any {
	t.Helper()
	if len(result.Data) == 0 {
		t.Fatalf("command result has no data: %+v", result)
	}
	var data map[string]any
	if err := json.Unmarshal(result.Data, &data); err != nil {
		t.Fatalf("decode command data: %v", err)
	}
	return data
}

func TestExecutorPingAndEcho(t *testing.T) {
	executor := New("node-1", "Node", testConfig(t), nil)

	ping := executor.Execute(context.Background(), envelope(t, protocol.ActionPing,
		protocol.Target{NodeID: "node-1"}, map[string]string{"nonce": "abc"}))
	if ping.Status != protocol.StatusCompleted {
		t.Fatalf("ping status = %s (%s)", ping.Status, ping.Error)
	}
	data := decodeData(t, ping)
	if data["pong"] != true || data["nonce"] != "abc" || data["node_id"] != "node-1" {
		t.Fatalf("unexpected ping data: %+v", data)
	}

	echo := executor.Execute(context.Background(), envelope(t, protocol.ActionEcho,
		protocol.Target{NodeID: "node-1"}, map[string]string{"text": "hello"}))
	if echo.Status != protocol.StatusCompleted {
		t.Fatalf("echo status = %s (%s)", echo.Status, echo.Error)
	}
	if got := decodeData(t, echo)["text"]; got != "hello" {
		t.Fatalf("echo text = %v", got)
	}

	status := executor.Execute(context.Background(), envelope(t, protocol.ActionGetStatus,
		protocol.Target{NodeID: "node-1"}, nil))
	if status.Status != protocol.StatusCompleted {
		t.Fatalf("status status = %s (%s)", status.Status, status.Error)
	}
	if got := decodeData(t, status)["node_version"]; got != Version {
		t.Fatalf("node_version = %v, want %s", got, Version)
	}
}

func TestExecutorRejectsInvalidTargetsAndActions(t *testing.T) {
	cfg := testConfig(t)
	executor := New("node-1", "Node", cfg, nil)

	tests := []struct {
		name   string
		action string
		target protocol.Target
		want   protocol.CommandStatus
	}{
		{
			name:   "wrong node",
			action: protocol.ActionPing,
			target: protocol.Target{NodeID: "node-2"},
			want:   protocol.StatusRejected,
		},
		{
			name:   "not whitelisted",
			action: "shell_exec",
			target: protocol.Target{NodeID: "node-1"},
			want:   protocol.StatusRejected,
		},
		{
			name:   "permission disabled by default policy",
			action: protocol.ActionPermission,
			target: protocol.Target{NodeID: "node-1"},
			want:   protocol.StatusRejected,
		},
	}
	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			result := executor.Execute(context.Background(), envelope(t, tc.action, tc.target, nil))
			if result.Status != tc.want {
				t.Fatalf("status = %s, want %s (%s)", result.Status, tc.want, result.Error)
			}
		})
	}
}

func TestExecutorHonoursNarrowedPolicy(t *testing.T) {
	cfg := testConfig(t)
	cfg.Node.Policy.AllowedActions = []string{protocol.ActionPing}
	executor := New("node-1", "Node", cfg, nil)

	result := executor.Execute(context.Background(), envelope(t, protocol.ActionGetStatus,
		protocol.Target{NodeID: "node-1"}, nil))
	if result.Status != protocol.StatusRejected {
		t.Fatalf("status = %s, want rejected", result.Status)
	}
}

func TestExecutorAgentActionsRequireManager(t *testing.T) {
	executor := New("node-1", "Node", testConfig(t), nil)
	result := executor.Execute(context.Background(), envelope(t, protocol.ActionSendPrompt,
		protocol.Target{NodeID: "node-1", AgentID: "agent-1", SessionID: "ses_1"},
		map[string]string{"text": "hi"}))
	if result.Status != protocol.StatusFailed {
		t.Fatalf("status = %s, want failed", result.Status)
	}
}

func TestExecutorSendPrompt(t *testing.T) {
	stub := newStubAdapter()
	manager := testManager(t, stub)
	cfg := testConfig(t)
	executor := New("node-1", "Node", cfg, manager)
	target := protocol.Target{NodeID: "node-1", AgentID: "agent-1", SessionID: "ses_1"}

	result := executor.Execute(context.Background(), envelope(t, protocol.ActionSendPrompt, target,
		map[string]string{"text": "fix the bug"}))
	if result.Status != protocol.StatusCompleted {
		t.Fatalf("status = %s (%s)", result.Status, result.Error)
	}
	if len(stub.prompts) != 1 || stub.prompts[0] != "ses_1:fix the bug" {
		t.Fatalf("prompts = %+v", stub.prompts)
	}
	data := decodeData(t, result)
	if data["message_id"] != "msg_1" {
		t.Fatalf("message_id = %v", data["message_id"])
	}
}

func TestExecutorSendPromptValidation(t *testing.T) {
	stub := newStubAdapter()
	manager := testManager(t, stub)

	cfg := testConfig(t)
	cfg.Node.Policy.MaxPromptBytes = 5
	executor := New("node-1", "Node", cfg, manager)
	target := protocol.Target{NodeID: "node-1", AgentID: "agent-1", SessionID: "ses_1"}

	cases := []struct {
		name    string
		payload any
		target  protocol.Target
	}{
		{name: "empty", payload: map[string]string{"text": "  "}, target: target},
		{name: "too long", payload: map[string]string{"text": "toolong"}, target: target},
		{name: "missing session", payload: map[string]string{"text": "hi"}, target: protocol.Target{NodeID: "node-1", AgentID: "agent-1"}},
		{name: "missing agent", payload: map[string]string{"text": "hi"}, target: protocol.Target{NodeID: "node-1", SessionID: "ses_1"}},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			result := executor.Execute(context.Background(), envelope(t, protocol.ActionSendPrompt, tc.target, tc.payload))
			if result.Status != protocol.StatusRejected {
				t.Fatalf("status = %s, want rejected (%s)", result.Status, result.Error)
			}
		})
	}
	if len(stub.prompts) != 0 {
		t.Fatalf("adapter received prompts for rejected commands: %+v", stub.prompts)
	}
}

func TestExecutorInterrupt(t *testing.T) {
	stub := newStubAdapter()
	manager := testManager(t, stub)
	executor := New("node-1", "Node", testConfig(t), manager)

	result := executor.Execute(context.Background(), envelope(t, protocol.ActionInterrupt,
		protocol.Target{NodeID: "node-1", AgentID: "agent-1", SessionID: "ses_1"}, nil))
	if result.Status != protocol.StatusCompleted {
		t.Fatalf("status = %s (%s)", result.Status, result.Error)
	}
	if len(stub.interrupted) != 1 || stub.interrupted[0] != "ses_1" {
		t.Fatalf("interrupted = %+v", stub.interrupted)
	}
}

func TestExecutorListSessions(t *testing.T) {
	stub := newStubAdapter()
	manager := testManager(t, stub)
	executor := New("node-1", "Node", testConfig(t), manager)

	result := executor.Execute(context.Background(), envelope(t, protocol.ActionListSessions,
		protocol.Target{NodeID: "node-1", AgentID: "agent-1"}, nil))
	if result.Status != protocol.StatusCompleted {
		t.Fatalf("status = %s (%s)", result.Status, result.Error)
	}
	var data struct {
		AgentID  string                 `json:"agent_id"`
		Sessions []protocol.SessionView `json:"sessions"`
	}
	if err := json.Unmarshal(result.Data, &data); err != nil {
		t.Fatalf("decode: %v", err)
	}
	if data.AgentID != "agent-1" || len(data.Sessions) != 1 || data.Sessions[0].SessionID != "ses_1" {
		t.Fatalf("unexpected list result: %+v", data)
	}
}

func TestExecutorPermissionRequiresExplicitPolicy(t *testing.T) {
	stub := newStubAdapter()
	manager := testManager(t, stub)
	target := protocol.Target{NodeID: "node-1", AgentID: "agent-1", SessionID: "ses_1"}
	payload := map[string]string{"permission_id": "per_1", "decision": "once"}

	denied := New("node-1", "Node", testConfig(t), manager)
	result := denied.Execute(context.Background(), envelope(t, protocol.ActionPermission, target, payload))
	if result.Status != protocol.StatusRejected {
		t.Fatalf("status = %s, want rejected by default policy", result.Status)
	}
	if len(stub.permissions) != 0 {
		t.Fatalf("permission forwarded despite policy: %+v", stub.permissions)
	}

	cfg := testConfig(t)
	cfg.Node.Policy.AllowedActions = append(cfg.Node.Policy.AllowedActions, protocol.ActionPermission)
	allowed := New("node-1", "Node", cfg, manager)
	result = allowed.Execute(context.Background(), envelope(t, protocol.ActionPermission, target, payload))
	if result.Status != protocol.StatusCompleted {
		t.Fatalf("status = %s (%s)", result.Status, result.Error)
	}
	if len(stub.permissions) != 1 || stub.permissions[0] != "per_1:once" {
		t.Fatalf("permissions = %+v", stub.permissions)
	}
}

func TestExecutorMapsAdapterErrors(t *testing.T) {
	stub := newStubAdapter()
	stub.promptErr = adapter.ErrSessionNotFound
	manager := testManager(t, stub)
	executor := New("node-1", "Node", testConfig(t), manager)

	result := executor.Execute(context.Background(), envelope(t, protocol.ActionSendPrompt,
		protocol.Target{NodeID: "node-1", AgentID: "agent-1", SessionID: "ses_1"},
		map[string]string{"text": "hi"}))
	if result.Status != protocol.StatusRejected {
		t.Fatalf("status = %s, want rejected", result.Status)
	}
	if result.Error == "" {
		t.Fatal("expected an error message")
	}
}
