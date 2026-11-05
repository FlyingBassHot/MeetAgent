package server

import (
	"context"
	"encoding/json"
	"io"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"

	"github.com/FlyingBassHot/meetagent/internal/node/adapter"
	"github.com/FlyingBassHot/meetagent/internal/node/adapter/mock"
	"github.com/FlyingBassHot/meetagent/internal/node/eventhub"
	"github.com/FlyingBassHot/meetagent/internal/node/pairing"
	"github.com/FlyingBassHot/meetagent/pkg/clientproto"
	"github.com/FlyingBassHot/meetagent/pkg/protocol"
	"github.com/gorilla/websocket"
)

func TestServerPermissionFlow(t *testing.T) {
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	logger := slog.New(slog.NewTextHandler(io.Discard, nil))

	manager := adapter.NewManager("node-test", logger)
	if err := manager.Add(adapter.Instance{
		ID:   "mock-1",
		Name: "Mock Agent",
		Adapter: mock.New(mock.Options{
			Sessions: []adapter.Session{{
				ID:     "session-1",
				Title:  "Session One",
				Status: "idle",
			}},
			Permissions: true,
		}),
	}); err != nil {
		t.Fatal(err)
	}
	manager.Start(ctx)
	hub := eventhub.New(NodeInfo("node-test", "Test"), manager, 200, time.Minute, logger)
	go hub.Run(ctx)

	pairingManager, _ := pairing.New("", time.Minute, nil)
	pairingManager.SetCode("123456")
	srv := New(Options{Addr: "127.0.0.1:0", PairingTTL: time.Minute, MDNS: false, CommandTimeout: 5 * time.Second}, "node-test", "Test", manager, hub, pairingManager, logger)
	ts := httptest.NewServer(srv.Handler())
	defer ts.Close()
	time.Sleep(100 * time.Millisecond)

	body := `{"client_type":"mobile","capabilities":["input.text"],"pairing_code":"123456"}`
	resp, err := http.Post(ts.URL+"/api/v2/pair", "application/json", strings.NewReader(body))
	if err != nil {
		t.Fatal(err)
	}
	defer resp.Body.Close()
	var paired clientproto.PairResult
	if err := json.NewDecoder(resp.Body).Decode(&paired); err != nil {
		t.Fatal(err)
	}

	wsURL := "ws" + strings.TrimPrefix(ts.URL, "http") + "/ws"
	conn, _, err := websocket.DefaultDialer.Dial(wsURL, nil)
	if err != nil {
		t.Fatal(err)
	}
	defer conn.Close()
	hello, _ := clientproto.NewEnvelope(clientproto.TypeHello, clientproto.Hello{
		ClientType:   clientproto.ClientMobile,
		Token:        paired.Token,
		Capabilities: []string{clientproto.CapabilityInputText},
	})
	if err := conn.WriteJSON(hello); err != nil {
		t.Fatal(err)
	}
	var welcomeEnv clientproto.Envelope
	_ = conn.SetReadDeadline(time.Now().Add(5 * time.Second))
	if err := conn.ReadJSON(&welcomeEnv); err != nil {
		t.Fatal(err)
	}

	prompt := clientproto.Command{
		SchemaVersion: clientproto.Version,
		CommandID:     "cmd-permission",
		AgentID:       "mock-1",
		SessionID:     "session-1",
		Type:          clientproto.CommandSessionPrompt,
		Payload:       json.RawMessage(`{"text":"please request permission"}`),
		CreatedAt:     time.Now().UTC(),
	}
	promptEnv, _ := clientproto.NewEnvelope(clientproto.TypeCommand, prompt)
	if err := conn.WriteJSON(promptEnv); err != nil {
		t.Fatal(err)
	}

	var permissionID string
	_ = conn.SetReadDeadline(time.Now().Add(5 * time.Second))
	for permissionID == "" {
		var env clientproto.Envelope
		if err := conn.ReadJSON(&env); err != nil {
			t.Fatal(err)
		}
		if env.Type != clientproto.TypeEvent {
			continue
		}
		var event clientproto.Event
		if err := env.DecodePayload(&event); err != nil {
			t.Fatal(err)
		}
		if event.Type != protocol.EventPermissionRequested {
			continue
		}
		var payload protocol.PermissionEventPayload
		if err := json.Unmarshal(event.Payload, &payload); err != nil {
			t.Fatal(err)
		}
		permissionID = payload.PermissionID
	}
	if permissionID == "" {
		t.Fatal("no permission id")
	}

	respond := clientproto.Command{
		SchemaVersion: clientproto.Version,
		CommandID:     "cmd-respond",
		AgentID:       "mock-1",
		SessionID:     "session-1",
		Type:          clientproto.CommandPermissionRespond,
		Payload:       mustJSON(clientproto.PermissionResponsePayload{PermissionID: permissionID, Decision: "once"}),
		CreatedAt:     time.Now().UTC(),
	}
	respondEnv, _ := clientproto.NewEnvelope(clientproto.TypeCommand, respond)
	if err := conn.WriteJSON(respondEnv); err != nil {
		t.Fatal(err)
	}

	resultOK := false
	completed := false
	_ = conn.SetReadDeadline(time.Now().Add(5 * time.Second))
	for !resultOK || !completed {
		var env clientproto.Envelope
		if err := conn.ReadJSON(&env); err != nil {
			t.Fatal(err)
		}
		switch env.Type {
		case clientproto.TypeCommandResult:
			var result clientproto.CommandResult
			_ = env.DecodePayload(&result)
			if result.CommandID == "cmd-respond" && result.Status == protocol.StatusCompleted {
				resultOK = true
			}
		case clientproto.TypeEvent:
			var event clientproto.Event
			_ = env.DecodePayload(&event)
			if event.Type == protocol.EventMessageCompleted {
				completed = true
			}
		}
	}
	if !resultOK || !completed {
		t.Fatalf("resultOK=%v completed=%v", resultOK, completed)
	}
}

func mustJSON(value any) json.RawMessage {
	raw, err := json.Marshal(value)
	if err != nil {
		return nil
	}
	return raw
}
