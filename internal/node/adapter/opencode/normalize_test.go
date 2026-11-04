package opencode

import (
	"encoding/json"
	"testing"
	"time"

	"github.com/FlyingBassHot/meetagent/internal/node/adapter"
	"github.com/FlyingBassHot/meetagent/pkg/protocol"
)

func TestNormalizeEvent(t *testing.T) {
	created := int64(1793762636543)
	base := func(eventType string, data map[string]any) []byte {
		payload := map[string]any{
			"id":      "evt_test",
			"created": created,
			"type":    eventType,
			"data":    data,
			"location": map[string]any{
				"directory": "/tmp/project",
			},
		}
		raw, err := json.Marshal(payload)
		if err != nil {
			t.Fatalf("marshal event: %v", err)
		}
		return raw
	}

	tests := []struct {
		name       string
		raw        []byte
		wantOK     bool
		wantType   string
		wantStatus string
		check      func(t *testing.T, event adapter.Event)
	}{
		{
			name:     "session created",
			raw:      base("session.created", map[string]any{"sessionID": "ses_1", "title": "Fix bug"}),
			wantOK:   true,
			wantType: protocol.EventSessionCreated,
			check: func(t *testing.T, event adapter.Event) {
				payload, ok := event.Payload.(adapter.SessionEventPayload)
				if !ok {
					t.Fatalf("payload type = %T", event.Payload)
				}
				if payload.SessionID != "ses_1" || payload.Title != "Fix bug" {
					t.Fatalf("unexpected payload: %+v", payload)
				}
				if payload.WorkingDirectory != "/tmp/project" {
					t.Fatalf("working directory = %q", payload.WorkingDirectory)
				}
				if payload.Status != "idle" {
					t.Fatalf("status = %q, want idle", payload.Status)
				}
			},
		},
		{
			name:       "execution started",
			raw:        base("session.execution.started", map[string]any{"sessionID": "ses_1"}),
			wantOK:     true,
			wantType:   protocol.EventSessionStatusChange,
			wantStatus: "running",
		},
		{
			name:       "execution succeeded",
			raw:        base("session.execution.succeeded", map[string]any{"sessionID": "ses_1"}),
			wantOK:     true,
			wantType:   protocol.EventSessionStatusChange,
			wantStatus: "idle",
		},
		{
			name:       "execution interrupted",
			raw:        base("session.execution.interrupted", map[string]any{"sessionID": "ses_1"}),
			wantOK:     true,
			wantType:   protocol.EventSessionStatusChange,
			wantStatus: "idle",
		},
		{
			name:     "execution failed",
			raw:      base("session.execution.failed", map[string]any{"sessionID": "ses_1", "outcome": "model overloaded"}),
			wantOK:   true,
			wantType: protocol.EventAgentError,
			check: func(t *testing.T, event adapter.Event) {
				payload, ok := event.Payload.(map[string]any)
				if !ok {
					t.Fatalf("payload type = %T", event.Payload)
				}
				if payload["message"] != "model overloaded" {
					t.Fatalf("message = %v", payload["message"])
				}
			},
		},
		{
			name:     "status object",
			raw:      base("session.status", map[string]any{"sessionID": "ses_1", "status": map[string]any{"type": "busy"}}),
			wantOK:   true,
			wantType: protocol.EventSessionStatusChange,
			check: func(t *testing.T, event adapter.Event) {
				if event.Status != "running" {
					t.Fatalf("status = %q, want running", event.Status)
				}
			},
		},
		{
			name:     "text delta",
			raw:      base("session.text.delta", map[string]any{"sessionID": "ses_1", "assistantMessageID": "msg_1", "ordinal": 2, "delta": "hello"}),
			wantOK:   true,
			wantType: protocol.EventMessageDelta,
			check: func(t *testing.T, event adapter.Event) {
				payload := event.Payload.(adapter.MessageEventPayload)
				if payload.MessageID != "msg_1" || payload.Ordinal != 2 || payload.Delta != "hello" {
					t.Fatalf("unexpected payload: %+v", payload)
				}
			},
		},
		{
			name:     "text ended",
			raw:      base("session.text.ended", map[string]any{"sessionID": "ses_1", "assistantMessageID": "msg_1", "text": "full text", "finish": "stop"}),
			wantOK:   true,
			wantType: protocol.EventMessageCompleted,
			check: func(t *testing.T, event adapter.Event) {
				payload := event.Payload.(adapter.MessageEventPayload)
				if payload.Text != "full text" || payload.Finish != "stop" {
					t.Fatalf("unexpected payload: %+v", payload)
				}
			},
		},
		{
			name:     "tool called",
			raw:      base("session.tool.called", map[string]any{"sessionID": "ses_1", "id": "call_1", "input": map[string]any{"name": "read"}}),
			wantOK:   true,
			wantType: protocol.EventToolStarted,
			check: func(t *testing.T, event adapter.Event) {
				payload := event.Payload.(adapter.ToolEventPayload)
				if payload.CallID != "call_1" || payload.Name != "read" || payload.Status != "running" {
					t.Fatalf("unexpected payload: %+v", payload)
				}
			},
		},
		{
			name:     "tool success",
			raw:      base("session.tool.success", map[string]any{"sessionID": "ses_1", "id": "call_1"}),
			wantOK:   true,
			wantType: protocol.EventToolCompleted,
		},
		{
			name:     "tool failed",
			raw:      base("session.tool.failed", map[string]any{"sessionID": "ses_1", "id": "call_1"}),
			wantOK:   true,
			wantType: protocol.EventToolFailed,
		},
		{
			name: "permission asked",
			raw: base("permission.asked", map[string]any{
				"id":        "per_1",
				"sessionID": "ses_1",
				"action":    "read",
				"resources": []string{"secret.env"},
			}),
			wantOK:   true,
			wantType: protocol.EventPermissionRequested,
			check: func(t *testing.T, event adapter.Event) {
				payload := event.Payload.(protocol.PermissionEventPayload)
				if payload.PermissionID != "per_1" || payload.Action != "read" || payload.Status != protocol.PermissionPending {
					t.Fatalf("unexpected payload: %+v", payload)
				}
				if len(payload.Resources) != 1 || payload.Resources[0] != "secret.env" {
					t.Fatalf("resources = %v", payload.Resources)
				}
				if payload.Description == "" {
					t.Fatal("expected a human readable description")
				}
			},
		},
		{
			name:     "permission approved",
			raw:      base("permission.replied", map[string]any{"sessionID": "ses_1", "requestID": "per_1", "reply": "once"}),
			wantOK:   true,
			wantType: protocol.EventPermissionResolved,
			check: func(t *testing.T, event adapter.Event) {
				payload := event.Payload.(protocol.PermissionEventPayload)
				if payload.PermissionID != "per_1" || payload.Status != protocol.PermissionApproved || payload.Decision != "once" {
					t.Fatalf("unexpected payload: %+v", payload)
				}
			},
		},
		{
			name:     "permission rejected",
			raw:      base("permission.replied", map[string]any{"sessionID": "ses_1", "requestID": "per_1", "reply": "reject"}),
			wantOK:   true,
			wantType: protocol.EventPermissionResolved,
			check: func(t *testing.T, event adapter.Event) {
				payload := event.Payload.(protocol.PermissionEventPayload)
				if payload.Status != protocol.PermissionRejected {
					t.Fatalf("status = %q, want rejected", payload.Status)
				}
			},
		},
		{
			name:     "session deleted",
			raw:      base("session.deleted", map[string]any{"sessionID": "ses_1"}),
			wantOK:   true,
			wantType: protocol.EventSessionDeleted,
		},
		{
			name:   "unknown type",
			raw:    base("session.something.new", map[string]any{"sessionID": "ses_1"}),
			wantOK: false,
		},
		{
			name:   "missing session id",
			raw:    base("session.execution.started", map[string]any{}),
			wantOK: false,
		},
		{
			name:   "malformed json",
			raw:    []byte("{not json"),
			wantOK: false,
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			event, ok := normalizeEvent(tc.raw)
			if ok != tc.wantOK {
				t.Fatalf("ok = %v, want %v (event %+v)", ok, tc.wantOK, event)
			}
			if !tc.wantOK {
				return
			}
			if event.Type != tc.wantType {
				t.Fatalf("type = %q, want %q", event.Type, tc.wantType)
			}
			if tc.wantStatus != "" && event.Status != tc.wantStatus {
				t.Fatalf("status = %q, want %q", event.Status, tc.wantStatus)
			}
			if event.Timestamp.UnixMilli() != created {
				t.Fatalf("timestamp = %s, want %d ms", event.Timestamp, created)
			}
			if tc.check != nil {
				tc.check(t, event)
			}
		})
	}
}

func TestNormalizeEventFallsBackToNow(t *testing.T) {
	raw := []byte(`{"type":"session.execution.started","data":{"sessionID":"ses_1"}}`)
	event, ok := normalizeEvent(raw)
	if !ok {
		t.Fatal("expected event to normalize")
	}
	if time.Since(event.Timestamp) > time.Minute {
		t.Fatalf("timestamp = %s, expected close to now", event.Timestamp)
	}
}

func TestNormalizeDecision(t *testing.T) {
	tests := map[string]string{
		"once":     "once",
		"ALLOW":    "once",
		"approved": "once",
		"always":   "always",
		"persist":  "always",
		"reject":   "reject",
		"":         "reject",
		"whatever": "reject",
	}
	for input, want := range tests {
		if got := normalizeDecision(input); got != want {
			t.Fatalf("normalizeDecision(%q) = %q, want %q", input, got, want)
		}
	}
}

func TestNormalizeActiveState(t *testing.T) {
	tests := map[string]string{
		"running": "running",
		"busy":    "running",
		"working": "running",
		"retry":   "error",
		"error":   "error",
		"idle":    "idle",
		"":        "idle",
	}
	for input, want := range tests {
		if got := normalizeActiveState(activeState{Type: input}); got != want {
			t.Fatalf("normalizeActiveState(%q) = %q, want %q", input, got, want)
		}
	}
}
