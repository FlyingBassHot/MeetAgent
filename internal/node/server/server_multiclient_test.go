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

func TestTwoClientsReceiveSamePromptEvents(t *testing.T) {
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	logger := slog.New(slog.NewTextHandler(io.Discard, nil))

	manager := adapter.NewManager("node-test", logger)
	if err := manager.Add(adapter.Instance{
		ID:   "mock-1",
		Name: "Mock Agent",
		Adapter: mock.New(mock.Options{
			Sessions:    []adapter.Session{{ID: "session-1", Title: "Session One", Status: "idle"}},
			ReplyDelay:  300 * time.Millisecond,
			Permissions: false,
		}),
	}); err != nil {
		t.Fatal(err)
	}
	manager.Start(ctx)
	hub := eventhub.New(NodeInfo("node-test", "Test"), manager, 200, time.Minute, logger)
	go hub.Run(ctx)

	pairingManager, _ := pairing.New("", time.Minute, nil)
	pairingManager.SetCode("123456")
	srv := New(Options{Addr: "127.0.0.1:0", MDNS: false, PairingTTL: time.Minute, CommandTimeout: 5 * time.Second}, "node-test", "Test", manager, hub, pairingManager, logger)
	ts := httptest.NewServer(srv.Handler())
	defer ts.Close()
	time.Sleep(100 * time.Millisecond)

	first := pairHTTP(t, ts.URL, "123456")
	pairingManager.SetCode("123456")
	second := pairHTTP(t, ts.URL, "123456")

	one := connectWS(t, ts.URL, first.Token)
	defer one.Close()
	two := connectWS(t, ts.URL, second.Token)
	defer two.Close()

	command := clientproto.Command{
		SchemaVersion: clientproto.Version,
		CommandID:     "cmd-multi",
		AgentID:       "mock-1",
		SessionID:     "session-1",
		Type:          clientproto.CommandSessionPrompt,
		Payload:       json.RawMessage(`{"text":"multi client"}`),
		CreatedAt:     time.Now().UTC(),
	}
	env, _ := clientproto.NewEnvelope(clientproto.TypeCommand, command)
	if err := one.WriteJSON(env); err != nil {
		t.Fatal(err)
	}

	firstEvent := readUntilEvent(t, one, "message.completed")
	secondEvent := readUntilEvent(t, two, "message.completed")
	if firstEvent.Sequence != secondEvent.Sequence {
		t.Fatalf("sequences differ: %d vs %d", firstEvent.Sequence, secondEvent.Sequence)
	}
	if firstEvent.Type != protocol.EventMessageCompleted {
		t.Fatalf("event = %s", firstEvent.Type)
	}
}

func pairHTTP(t *testing.T, baseURL, code string) clientproto.PairResult {
	t.Helper()
	body := `{"client_type":"mobile","capabilities":["input.text"],"pairing_code":"` + code + `"}`
	resp, err := http.Post(baseURL+"/api/v2/pair", "application/json", strings.NewReader(body))
	if err != nil {
		t.Fatal(err)
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		t.Fatalf("pair status = %d", resp.StatusCode)
	}
	var result clientproto.PairResult
	if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
		t.Fatal(err)
	}
	return result
}

func connectWS(t *testing.T, baseURL, token string) *websocket.Conn {
	t.Helper()
	wsURL := "ws" + strings.TrimPrefix(baseURL, "http") + "/ws"
	conn, _, err := websocket.DefaultDialer.Dial(wsURL, nil)
	if err != nil {
		t.Fatal(err)
	}
	hello, _ := clientproto.NewEnvelope(clientproto.TypeHello, clientproto.Hello{
		ClientType:   clientproto.ClientMobile,
		Token:        token,
		Capabilities: []string{clientproto.CapabilityInputText},
	})
	if err := conn.WriteJSON(hello); err != nil {
		t.Fatal(err)
	}
	var welcome clientproto.Envelope
	_ = conn.SetReadDeadline(time.Now().Add(5 * time.Second))
	if err := conn.ReadJSON(&welcome); err != nil {
		t.Fatal(err)
	}
	if welcome.Type != clientproto.TypeWelcome {
		t.Fatalf("welcome type = %s", welcome.Type)
	}
	return conn
}

func readUntilEvent(t *testing.T, conn *websocket.Conn, eventType string) clientproto.Event {
	t.Helper()
	deadline := time.Now().Add(5 * time.Second)
	for time.Now().Before(deadline) {
		_ = conn.SetReadDeadline(time.Now().Add(2 * time.Second))
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
		if event.Type == eventType {
			return event
		}
	}
	t.Fatalf("timed out waiting for %s", eventType)
	return clientproto.Event{}
}

func TestServerReplaysMissedEventsAfterReconnect(t *testing.T) {
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	logger := slog.New(slog.NewTextHandler(io.Discard, nil))

	manager := adapter.NewManager("node-test", logger)
	if err := manager.Add(adapter.Instance{
		ID:   "mock-1",
		Name: "Mock Agent",
		Adapter: mock.New(mock.Options{
			Sessions:   []adapter.Session{{ID: "session-1", Title: "Session One", Status: "idle"}},
			ReplyDelay: 100 * time.Millisecond,
		}),
	}); err != nil {
		t.Fatal(err)
	}
	manager.Start(ctx)
	hub := eventhub.New(NodeInfo("node-test", "Test"), manager, 200, time.Minute, logger)
	go hub.Run(ctx)

	pairingManager, _ := pairing.New("", time.Minute, nil)
	pairingManager.SetCode("123456")
	srv := New(Options{Addr: "127.0.0.1:0", MDNS: false, PairingTTL: time.Minute, CommandTimeout: 5 * time.Second}, "node-test", "Test", manager, hub, pairingManager, logger)
	ts := httptest.NewServer(srv.Handler())
	defer ts.Close()
	time.Sleep(100 * time.Millisecond)

	paired := pairHTTP(t, ts.URL, "123456")
	firstConn := connectWS(t, ts.URL, paired.Token)
	command := clientproto.Command{
		SchemaVersion: clientproto.Version,
		CommandID:     "cmd-replay-1",
		AgentID:       "mock-1",
		SessionID:     "session-1",
		Type:          clientproto.CommandSessionPrompt,
		Payload:       json.RawMessage(`{"text":"first"}`),
		CreatedAt:     time.Now().UTC(),
	}
	env, _ := clientproto.NewEnvelope(clientproto.TypeCommand, command)
	if err := firstConn.WriteJSON(env); err != nil {
		t.Fatal(err)
	}
	firstEvent := readUntilEvent(t, firstConn, "message.completed")
	firstConn.Close()

	if _, err := manager.SendPrompt(context.Background(), "mock-1", "session-1", "second"); err != nil {
		t.Fatal(err)
	}
	time.Sleep(400 * time.Millisecond)

	secondConn := connectWSWithLast(t, ts.URL, paired.Token, firstEvent.Sequence)
	defer secondConn.Close()
	_ = secondConn.SetReadDeadline(time.Now().Add(5 * time.Second))
	replayed := false
	for {
		var envelope clientproto.Envelope
		if err := secondConn.ReadJSON(&envelope); err != nil {
			t.Fatal(err)
		}
		if envelope.Type != clientproto.TypeEvent {
			continue
		}
		var event clientproto.Event
		if err := envelope.DecodePayload(&event); err != nil {
			t.Fatal(err)
		}
		if event.Type == protocol.EventMessageCompleted && event.Sequence > firstEvent.Sequence {
			replayed = true
			break
		}
	}
	if !replayed {
		t.Fatal("did not receive replayed message.completed")
	}
}

func connectWSWithLast(t *testing.T, baseURL, token string, last uint64) *websocket.Conn {
	t.Helper()
	wsURL := "ws" + strings.TrimPrefix(baseURL, "http") + "/ws"
	conn, _, err := websocket.DefaultDialer.Dial(wsURL, nil)
	if err != nil {
		t.Fatal(err)
	}
	hello, _ := clientproto.NewEnvelope(clientproto.TypeHello, clientproto.Hello{
		ClientType:   clientproto.ClientMobile,
		Token:        token,
		Capabilities: []string{clientproto.CapabilityInputText},
		LastSequence: last,
	})
	if err := conn.WriteJSON(hello); err != nil {
		t.Fatal(err)
	}
	var welcome clientproto.Envelope
	_ = conn.SetReadDeadline(time.Now().Add(5 * time.Second))
	if err := conn.ReadJSON(&welcome); err != nil {
		t.Fatal(err)
	}
	return conn
}
