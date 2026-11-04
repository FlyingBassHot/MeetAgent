package protocol_test

import (
	"encoding/json"
	"testing"
	"time"

	"github.com/FlyingBassHot/meetagent/pkg/protocol"
)

func TestEnvelopeRoundTrip(t *testing.T) {
	env, err := protocol.NewWithPayload(protocol.TypeCommand, protocol.ActionEcho, protocol.Source{
		Type: protocol.PeerDevice,
		ID:   "web-001",
	}, map[string]string{"text": "hello"})
	if err != nil {
		t.Fatalf("NewWithPayload: %v", err)
	}
	env.WithRequestID("req-1").WithTarget(protocol.Target{NodeID: "node-001"})

	raw, err := json.Marshal(env)
	if err != nil {
		t.Fatalf("marshal: %v", err)
	}
	var decoded protocol.Envelope
	if err := json.Unmarshal(raw, &decoded); err != nil {
		t.Fatalf("unmarshal: %v", err)
	}
	if err := decoded.Validate(); err != nil {
		t.Fatalf("Validate: %v", err)
	}
	if decoded.Source != env.Source || decoded.RequestID != "req-1" || decoded.Target.NodeID != "node-001" {
		t.Fatalf("round trip mismatch: %+v", decoded)
	}

	var payload struct {
		Text string `json:"text"`
	}
	if err := decoded.DecodePayload(&payload); err != nil {
		t.Fatalf("DecodePayload: %v", err)
	}
	if payload.Text != "hello" {
		t.Fatalf("payload text = %q", payload.Text)
	}
}

func TestEnvelopeValidation(t *testing.T) {
	valid := func() *protocol.Envelope {
		return &protocol.Envelope{
			Version:   protocol.Version,
			MessageID: "msg-1",
			Type:      protocol.TypeHeartbeat,
			Source:    protocol.Source{Type: protocol.PeerNode, ID: "node-001"},
			Timestamp: time.Now(),
		}
	}

	tests := []struct {
		name    string
		mutate  func(*protocol.Envelope)
		wantErr bool
	}{
		{name: "valid heartbeat", mutate: func(*protocol.Envelope) {}},
		{name: "wrong version", mutate: func(e *protocol.Envelope) { e.Version = "2" }, wantErr: true},
		{name: "missing message id", mutate: func(e *protocol.Envelope) { e.MessageID = "" }, wantErr: true},
		{name: "unknown type", mutate: func(e *protocol.Envelope) { e.Type = "mystery" }, wantErr: true},
		{name: "bad source", mutate: func(e *protocol.Envelope) { e.Source.Type = "robot" }, wantErr: true},
		{name: "command without target", mutate: func(e *protocol.Envelope) {
			e.Type = protocol.TypeCommand
			e.Action = protocol.ActionPing
			e.RequestID = "req-1"
		}, wantErr: true},
		{name: "command without request id", mutate: func(e *protocol.Envelope) {
			e.Type = protocol.TypeCommand
			e.Action = protocol.ActionPing
			e.Target = &protocol.Target{NodeID: "node-001"}
		}, wantErr: true},
		{name: "response without request id", mutate: func(e *protocol.Envelope) {
			e.Type = protocol.TypeResponse
		}, wantErr: true},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			env := valid()
			tt.mutate(env)
			err := env.Validate()
			if (err != nil) != tt.wantErr {
				t.Fatalf("Validate() error = %v, wantErr = %v", err, tt.wantErr)
			}
		})
	}
}

func TestCommandStatusTerminal(t *testing.T) {
	terminal := []protocol.CommandStatus{
		protocol.StatusCompleted,
		protocol.StatusFailed,
		protocol.StatusRejected,
		protocol.StatusTimeout,
	}
	for _, status := range terminal {
		if !status.IsTerminal() {
			t.Errorf("%s should be terminal", status)
		}
	}
	for _, status := range []protocol.CommandStatus{protocol.StatusAccepted, protocol.StatusExecuting} {
		if status.IsTerminal() {
			t.Errorf("%s should not be terminal", status)
		}
	}
}
