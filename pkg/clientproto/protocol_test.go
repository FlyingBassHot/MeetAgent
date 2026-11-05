package clientproto

import (
	"testing"
	"time"

	"github.com/FlyingBassHot/meetagent/pkg/protocol"
)

func TestHelloValidate(t *testing.T) {
	if err := (Hello{ClientType: ClientMobile, Token: "t"}).Validate(); err != nil {
		t.Fatalf("valid hello: %v", err)
	}
	if err := (Hello{ClientType: ClientMobile}).Validate(); err == nil {
		t.Fatal("expected error without token or pairing code")
	}
	if err := (Hello{ClientType: "toaster", Token: "t"}).Validate(); err == nil {
		t.Fatal("expected unsupported client type error")
	}
}

func TestEnvelopePayload(t *testing.T) {
	env, err := NewEnvelope(TypeHello, Hello{ClientType: ClientTerminal, PairingCode: "123456"})
	if err != nil {
		t.Fatal(err)
	}
	if err := env.Validate(); err != nil {
		t.Fatal(err)
	}
	var hello Hello
	if err := env.DecodePayload(&hello); err != nil {
		t.Fatal(err)
	}
	if hello.PairingCode != "123456" {
		t.Fatalf("pairing code = %q", hello.PairingCode)
	}
}

func TestCommandValidate(t *testing.T) {
	command := Command{
		SchemaVersion: Version,
		CommandID:     "cmd-1",
		Type:          CommandSessionPrompt,
	}
	if err := command.Validate(); err != nil {
		t.Fatal(err)
	}
	command.CommandID = ""
	if err := command.Validate(); err == nil {
		t.Fatal("expected command_id error")
	}
}

func TestEventFromAgent(t *testing.T) {
	now := time.Now().UTC()
	event := EventFromAgent("node-1", protocol.AgentEvent{
		EventID:     "evt-1",
		Sequence:    42,
		AgentID:     "agent-1",
		AdapterType: "mock",
		SessionID:   "session-1",
		EventType:   protocol.EventMessageCompleted,
		Timestamp:   now,
	})
	if event.SchemaVersion != Version || event.Sequence != 42 || event.Type != protocol.EventMessageCompleted {
		t.Fatalf("event = %+v", event)
	}
	if event.Source == nil || event.Source.AdapterID != "mock" {
		t.Fatalf("source = %+v", event.Source)
	}
}
