package server

import (
	"bytes"
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
	"github.com/gorilla/websocket"
)

func TestServerPairSnapshotAndPrompt(t *testing.T) {
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
	hub := eventhub.New(NodeInfo("node-test", "Test Node"), manager, 200, time.Minute, logger)
	go hub.Run(ctx)

	pairingManager, err := pairing.New("", time.Minute, nil)
	if err != nil {
		t.Fatal(err)
	}
	pairingManager.SetCode("123456")

	srv := New(Options{
		Addr:           "127.0.0.1:0",
		PairingTTL:     time.Minute,
		SnapshotEvents: 50,
		MDNS:           false,
		CommandTimeout: 5 * time.Second,
	}, "node-test", "Test Node", manager, hub, pairingManager, logger)
	ts := httptest.NewServer(srv.Handler())
	defer ts.Close()
	time.Sleep(200 * time.Millisecond)

	// Pair over HTTP.
	body := `{"client_type":"terminal","client_version":"0.1.0","name":"Test Terminal","capabilities":["input.text","input.button"],"pairing_code":"123456"}`
	resp, err := http.Post(ts.URL+"/api/v2/pair", "application/json", strings.NewReader(body))
	if err != nil {
		t.Fatal(err)
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		t.Fatalf("pair status = %d", resp.StatusCode)
	}
	var paired clientproto.PairResult
	if err := json.NewDecoder(resp.Body).Decode(&paired); err != nil {
		t.Fatal(err)
	}
	if paired.ClientID == "" || paired.Token == "" {
		t.Fatalf("pair result = %+v", paired)
	}

	// Snapshot over HTTP.
	req, _ := http.NewRequest(http.MethodGet, ts.URL+"/api/v2/snapshot", nil)
	req.Header.Set("Authorization", "Bearer "+paired.Token)
	snapshotResp, err := http.DefaultClient.Do(req)
	if err != nil {
		t.Fatal(err)
	}
	defer snapshotResp.Body.Close()
	if snapshotResp.StatusCode != http.StatusOK {
		t.Fatalf("snapshot status = %d", snapshotResp.StatusCode)
	}
	var snapshot clientproto.Snapshot
	if err := json.NewDecoder(snapshotResp.Body).Decode(&snapshot); err != nil {
		t.Fatal(err)
	}
	if len(snapshot.Agents) != 1 || len(snapshot.Agents[0].Sessions) != 1 {
		t.Fatalf("snapshot agents = %+v", snapshot.Agents)
	}

	// Connect WebSocket and handshake with the paired token.
	wsURL := "ws" + strings.TrimPrefix(ts.URL, "http") + "/ws"
	conn, _, err := websocket.DefaultDialer.Dial(wsURL, nil)
	if err != nil {
		t.Fatal(err)
	}
	defer conn.Close()
	hello, _ := clientproto.NewEnvelope(clientproto.TypeHello, clientproto.Hello{
		ClientType:    clientproto.ClientTerminal,
		ClientVersion: "0.1.0",
		Token:         paired.Token,
		Capabilities:  []string{clientproto.CapabilityInputText, clientproto.CapabilityInputButton},
	})
	if err := conn.WriteJSON(hello); err != nil {
		t.Fatal(err)
	}

	var welcomeEnv clientproto.Envelope
	_ = conn.SetReadDeadline(time.Now().Add(5 * time.Second))
	if err := conn.ReadJSON(&welcomeEnv); err != nil {
		t.Fatal(err)
	}
	if welcomeEnv.Type != clientproto.TypeWelcome {
		t.Fatalf("welcome type = %q", welcomeEnv.Type)
	}
	var welcome clientproto.Welcome
	if err := welcomeEnv.DecodePayload(&welcome); err != nil {
		t.Fatal(err)
	}
	if welcome.ClientID != paired.ClientID {
		t.Fatalf("welcome client = %q, want %q", welcome.ClientID, paired.ClientID)
	}
	if len(welcome.Snapshot.Agents) != 1 {
		t.Fatalf("welcome agents = %+v", welcome.Snapshot.Agents)
	}

	// Send a prompt and wait for the result plus message events.
	command := clientproto.Command{
		SchemaVersion: clientproto.Version,
		CommandID:     "cmd-1",
		AgentID:       "mock-1",
		SessionID:     "session-1",
		Type:          clientproto.CommandSessionPrompt,
		Payload:       json.RawMessage(`{"text":"hello from test"}`),
		CreatedAt:     time.Now().UTC(),
	}
	commandEnv, _ := clientproto.NewEnvelope(clientproto.TypeCommand, command)
	if err := conn.WriteJSON(commandEnv); err != nil {
		t.Fatal(err)
	}

	var gotResult bool
	var gotCompleted bool
	deadline := time.Now().Add(5 * time.Second)
	for time.Now().Before(deadline) && (!gotResult || !gotCompleted) {
		_ = conn.SetReadDeadline(time.Now().Add(2 * time.Second))
		var env clientproto.Envelope
		if err := conn.ReadJSON(&env); err != nil {
			if strings.Contains(err.Error(), "timeout") {
				break
			}
			t.Fatal(err)
		}
		switch env.Type {
		case clientproto.TypeCommandResult:
			var result clientproto.CommandResult
			if err := env.DecodePayload(&result); err != nil {
				t.Fatal(err)
			}
			if result.CommandID != "cmd-1" || result.Status != "completed" {
				t.Fatalf("command result = %+v", result)
			}
			gotResult = true
		case clientproto.TypeEvent:
			var event clientproto.Event
			if err := env.DecodePayload(&event); err != nil {
				t.Fatal(err)
			}
			if event.Type == "message.completed" {
				gotCompleted = true
			}
		}
	}
	if !gotResult || !gotCompleted {
		t.Fatalf("gotResult=%v gotCompleted=%v", gotResult, gotCompleted)
	}

	// Duplicate command returns the cached result and does not start a new prompt.
	if err := conn.WriteJSON(commandEnv); err != nil {
		t.Fatal(err)
	}
	_ = conn.SetReadDeadline(time.Now().Add(3 * time.Second))
	for {
		var env clientproto.Envelope
		if err := conn.ReadJSON(&env); err != nil {
			t.Fatal(err)
		}
		if env.Type == clientproto.TypeCommandResult {
			var result clientproto.CommandResult
			_ = env.DecodePayload(&result)
			if result.CommandID != "cmd-1" || result.Status != "completed" {
				t.Fatalf("duplicate result = %+v", result)
			}
			break
		}
	}
}

func TestHTTPSnapshotRequiresToken(t *testing.T) {
	logger := slog.New(slog.NewTextHandler(io.Discard, nil))
	manager := adapter.NewManager("node-test", logger)
	hub := eventhub.New(NodeInfo("node-test", "Test"), manager, 10, time.Minute, logger)
	pairingManager, _ := pairing.New("", time.Minute, nil)
	srv := New(Options{Addr: "127.0.0.1:0", MDNS: false}, "node-test", "Test", manager, hub, pairingManager, logger)
	ts := httptest.NewServer(srv.Handler())
	defer ts.Close()

	resp, err := http.Get(ts.URL + "/api/v2/snapshot")
	if err != nil {
		t.Fatal(err)
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusUnauthorized {
		t.Fatalf("status = %d, want 401", resp.StatusCode)
	}
}

func TestInfoEndpoint(t *testing.T) {
	logger := slog.New(slog.NewTextHandler(io.Discard, nil))
	manager := adapter.NewManager("node-test", logger)
	hub := eventhub.New(NodeInfo("node-test", "Test"), manager, 10, time.Minute, logger)
	pairingManager, _ := pairing.New("", time.Minute, nil)
	srv := New(Options{Addr: "127.0.0.1:0", MDNS: false}, "node-test", "Test Node", manager, hub, pairingManager, logger)
	ts := httptest.NewServer(srv.Handler())
	defer ts.Close()

	resp, err := http.Get(ts.URL + "/api/v2/info")
	if err != nil {
		t.Fatal(err)
	}
	defer resp.Body.Close()
	var info clientproto.Info
	if err := json.NewDecoder(resp.Body).Decode(&info); err != nil {
		t.Fatal(err)
	}
	if info.Node.NodeID != "node-test" || info.ProtocolVersion != clientproto.Version {
		t.Fatalf("info = %+v", info)
	}
}

func TestPairRejectsBadCode(t *testing.T) {
	logger := slog.New(slog.NewTextHandler(io.Discard, nil))
	manager := adapter.NewManager("node-test", logger)
	hub := eventhub.New(NodeInfo("node-test", "Test"), manager, 10, time.Minute, logger)
	pairingManager, _ := pairing.New("", time.Minute, nil)
	pairingManager.SetCode("123456")
	srv := New(Options{Addr: "127.0.0.1:0", MDNS: false}, "node-test", "Test", manager, hub, pairingManager, logger)
	ts := httptest.NewServer(srv.Handler())
	defer ts.Close()

	body := `{"client_type":"mobile","pairing_code":"000000"}`
	resp, err := http.Post(ts.URL+"/api/v2/pair", "application/json", bytes.NewBufferString(body))
	if err != nil {
		t.Fatal(err)
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusUnauthorized {
		t.Fatalf("status = %d, want 401", resp.StatusCode)
	}
}

func TestAdminClientListAndRevoke(t *testing.T) {
	logger := slog.New(slog.NewTextHandler(io.Discard, nil))
	manager := adapter.NewManager("node-test", logger)
	hub := eventhub.New(NodeInfo("node-test", "Test"), manager, 10, time.Minute, logger)
	pairingManager, _ := pairing.New("", time.Minute, nil)
	pairingManager.SetCode("123456")
	srv := New(Options{Addr: "127.0.0.1:0", MDNS: false, AdminToken: "admin-token"}, "node-test", "Test", manager, hub, pairingManager, logger)
	ts := httptest.NewServer(srv.Handler())
	defer ts.Close()

	body := `{"client_type":"terminal","pairing_code":"123456"}`
	resp, err := http.Post(ts.URL+"/api/v2/pair", "application/json", strings.NewReader(body))
	if err != nil {
		t.Fatal(err)
	}
	var paired clientproto.PairResult
	if err := json.NewDecoder(resp.Body).Decode(&paired); err != nil {
		t.Fatal(err)
	}
	resp.Body.Close()

	req, _ := http.NewRequest(http.MethodGet, ts.URL+"/api/v2/clients", nil)
	req.Header.Set("Authorization", "Bearer admin-token")
	listResp, err := http.DefaultClient.Do(req)
	if err != nil {
		t.Fatal(err)
	}
	defer listResp.Body.Close()
	if listResp.StatusCode != http.StatusOK {
		t.Fatalf("clients status = %d", listResp.StatusCode)
	}
	var list struct {
		Clients []clientproto.ClientInfo `json:"clients"`
	}
	if err := json.NewDecoder(listResp.Body).Decode(&list); err != nil {
		t.Fatal(err)
	}
	if len(list.Clients) != 1 || list.Clients[0].ClientID != paired.ClientID {
		t.Fatalf("clients = %+v", list.Clients)
	}

	delReq, _ := http.NewRequest(http.MethodDelete, ts.URL+"/api/v2/clients/"+paired.ClientID, nil)
	delReq.Header.Set("Authorization", "Bearer admin-token")
	delResp, err := http.DefaultClient.Do(delReq)
	if err != nil {
		t.Fatal(err)
	}
	delResp.Body.Close()
	if delResp.StatusCode != http.StatusOK {
		t.Fatalf("delete status = %d", delResp.StatusCode)
	}

	snapReq, _ := http.NewRequest(http.MethodGet, ts.URL+"/api/v2/snapshot", nil)
	snapReq.Header.Set("Authorization", "Bearer "+paired.Token)
	snapResp, err := http.DefaultClient.Do(snapReq)
	if err != nil {
		t.Fatal(err)
	}
	snapResp.Body.Close()
	if snapResp.StatusCode != http.StatusUnauthorized {
		t.Fatalf("revoked token snapshot status = %d, want 401", snapResp.StatusCode)
	}
}
