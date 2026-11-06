package server

import (
	"context"
	"io"
	"log/slog"
	"net/http/httptest"
	"path/filepath"
	"strings"
	"testing"
	"time"

	"github.com/FlyingBassHot/meetagent/internal/node/adapter"
	"github.com/FlyingBassHot/meetagent/internal/node/adapter/mock"
	"github.com/FlyingBassHot/meetagent/internal/node/eventhub"
	"github.com/FlyingBassHot/meetagent/internal/node/pairing"
	"github.com/FlyingBassHot/meetagent/pkg/clientproto"
	"github.com/gorilla/websocket"
)

func TestServerRateLimitRejectsExcessInbound(t *testing.T) {
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	logger := slog.New(slog.NewTextHandler(io.Discard, nil))

	manager := adapter.NewManager("node-test", logger)
	hub := eventhub.New(NodeInfo("node-test", "Test"), manager, 20, time.Minute, logger)
	go hub.Run(ctx)

	pairingManager, _ := pairing.New("", time.Minute, nil)
	pairingManager.SetCode("123456")
	srv := New(Options{
		Addr:               "127.0.0.1:0",
		MDNS:               false,
		PairingTTL:         time.Minute,
		CommandTimeout:     5 * time.Second,
		RateLimitPerSecond: 1,
		RateLimitBurst:     1,
	}, "node-test", "Test", manager, hub, pairingManager, logger)
	ts := httptest.NewServer(srv.Handler())
	defer ts.Close()

	paired := pairHTTP(t, ts.URL, "123456")
	conn := connectWS(t, ts.URL, paired.Token)
	defer conn.Close()

	heartbeat, _ := clientproto.NewEnvelope(clientproto.TypeHeartbeat, map[string]any{"nonce": "one"})
	if err := conn.WriteJSON(heartbeat); err != nil {
		t.Fatal(err)
	}
	_ = conn.SetReadDeadline(time.Now().Add(3 * time.Second))
	for {
		var env clientproto.Envelope
		if err := conn.ReadJSON(&env); err != nil {
			t.Fatal(err)
		}
		if env.Type == clientproto.TypeHeartbeat {
			break
		}
	}

	// The burst is exhausted; the next inbound message must be shed without
	// being processed.
	second, _ := clientproto.NewEnvelope(clientproto.TypeHeartbeat, map[string]any{"nonce": "two"})
	if err := conn.WriteJSON(second); err != nil {
		t.Fatal(err)
	}
	_ = conn.SetReadDeadline(time.Now().Add(3 * time.Second))
	for {
		var env clientproto.Envelope
		if err := conn.ReadJSON(&env); err != nil {
			t.Fatal(err)
		}
		if env.Type != clientproto.TypeError {
			continue
		}
		var payload clientproto.ErrorPayload
		if err := env.DecodePayload(&payload); err != nil {
			t.Fatal(err)
		}
		if payload.Code != "rate_limited" {
			t.Fatalf("error code = %q, want rate_limited", payload.Code)
		}
		return
	}
}

func TestServerRecoversAfterRestartWithFreshSequence(t *testing.T) {
	logger := slog.New(slog.NewTextHandler(io.Discard, nil))
	store := filepath.Join(t.TempDir(), "pairings.json")

	firstCtx, firstCancel := context.WithCancel(context.Background())
	defer firstCancel()
	firstManager := newMockManager(t, firstCtx, logger)
	firstHub := eventhub.New(NodeInfo("node-test", "Test"), firstManager, 200, time.Minute, logger)
	go firstHub.Run(firstCtx)
	firstPairing, err := pairing.New(store, time.Minute, nil)
	if err != nil {
		t.Fatal(err)
	}
	firstPairing.SetCode("123456")
	firstServer := New(Options{Addr: "127.0.0.1:0", MDNS: false, PairingTTL: time.Minute, CommandTimeout: 5 * time.Second}, "node-test", "Test", firstManager, firstHub, firstPairing, logger)
	firstHTTP := httptest.NewServer(firstServer.Handler())

	paired := pairHTTP(t, firstHTTP.URL, "123456")
	for i := 0; i < 3; i++ {
		if _, err := firstManager.SendPrompt(firstCtx, "mock-1", "session-1", "before restart"); err != nil {
			t.Fatal(err)
		}
		time.Sleep(150 * time.Millisecond)
	}
	firstSnapshot := firstHub.Snapshot(100)
	if firstSnapshot.LastSequence < 3 {
		t.Fatalf("first sequence = %d, want at least 3", firstSnapshot.LastSequence)
	}
	oldSequence := firstSnapshot.LastSequence
	firstHTTP.Close()
	firstCancel()

	secondCtx, secondCancel := context.WithCancel(context.Background())
	defer secondCancel()
	secondManager := newMockManager(t, secondCtx, logger)
	secondHub := eventhub.New(NodeInfo("node-test", "Test"), secondManager, 200, time.Minute, logger)
	go secondHub.Run(secondCtx)
	secondPairing, err := pairing.New(store, time.Minute, nil)
	if err != nil {
		t.Fatal(err)
	}
	secondServer := New(Options{Addr: "127.0.0.1:0", MDNS: false, PairingTTL: time.Minute, CommandTimeout: 5 * time.Second}, "node-test", "Test", secondManager, secondHub, secondPairing, logger)
	secondHTTP := httptest.NewServer(secondServer.Handler())
	defer secondHTTP.Close()
	time.Sleep(100 * time.Millisecond)

	conn, welcome := connectWithLastWelcome(t, secondHTTP.URL, paired.Token, oldSequence)
	defer conn.Close()
	if !welcome.ReplayTruncated {
		t.Fatalf("welcome = %+v, want replay_truncated after restart", welcome)
	}
	if welcome.Snapshot.LastSequence >= oldSequence {
		t.Fatalf("snapshot sequence = %d, want a reset below %d", welcome.Snapshot.LastSequence, oldSequence)
	}

	// The new process keeps producing low, monotonic sequences and the client
	// can continue from the snapshot without a permanent gap.
	before := welcome.Snapshot.LastSequence
	if _, err := secondManager.SendPrompt(secondCtx, "mock-1", "session-1", "after restart"); err != nil {
		t.Fatal(err)
	}
	expected := before + 1
	_ = conn.SetReadDeadline(time.Now().Add(5 * time.Second))
	for {
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
		if event.Sequence != expected {
			t.Fatalf("event %s sequence = %d, want %d", event.Type, event.Sequence, expected)
		}
		expected++
		if event.Type == "message.completed" {
			break
		}
	}
}

func newMockManager(t *testing.T, ctx context.Context, logger *slog.Logger) *adapter.Manager {
	t.Helper()
	manager := adapter.NewManager("node-test", logger)
	if err := manager.Add(adapter.Instance{
		ID:   "mock-1",
		Name: "Mock Agent",
		Adapter: mock.New(mock.Options{
			Sessions:   []adapter.Session{{ID: "session-1", Title: "Session One", Status: "idle"}},
			ReplyDelay: 10 * time.Millisecond,
		}),
	}); err != nil {
		t.Fatal(err)
	}
	manager.Start(ctx)
	return manager
}

func connectWithLastWelcome(t *testing.T, baseURL, token string, last uint64) (*websocket.Conn, clientproto.Welcome) {
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
	var welcome clientproto.Welcome
	_ = conn.SetReadDeadline(time.Now().Add(3 * time.Second))
	for {
		var env clientproto.Envelope
		if err := conn.ReadJSON(&env); err != nil {
			t.Fatal(err)
		}
		if env.Type != clientproto.TypeWelcome {
			continue
		}
		if err := env.DecodePayload(&welcome); err != nil {
			t.Fatal(err)
		}
		break
	}
	return conn, welcome
}
