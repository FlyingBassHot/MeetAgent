package server

import (
	"context"
	"io"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"

	"github.com/FlyingBassHot/meetagent/internal/node/adapter"
	"github.com/FlyingBassHot/meetagent/internal/node/eventhub"
	"github.com/FlyingBassHot/meetagent/internal/node/pairing"
	"github.com/FlyingBassHot/meetagent/pkg/clientproto"
)

func TestMetricsEndpoint(t *testing.T) {
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	logger := slog.New(slog.NewTextHandler(io.Discard, nil))

	manager := adapter.NewManager("node-test", logger)
	hub := eventhub.New(NodeInfo("node-test", "Test"), manager, 20, time.Minute, logger)
	go hub.Run(ctx)
	pairingManager, _ := pairing.New("", time.Minute, nil)
	pairingManager.SetCode("123456")
	srv := New(Options{Addr: "127.0.0.1:0", MDNS: false, AdminToken: "admin-token"}, "node-test", "Test", manager, hub, pairingManager, logger)
	ts := httptest.NewServer(srv.Handler())
	defer ts.Close()

	body := getMetrics(t, ts.URL, "")
	for _, want := range []string{
		"meetagent_up 1",
		"meetagent_ws_clients 0",
		"meetagent_paired_clients 0",
		"meetagent_events_published_total 0",
		`meetagent_command_results_total{status="completed"} 0`,
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("metrics missing %q:\n%s", want, body)
		}
	}

	paired := pairHTTP(t, ts.URL, "123456")
	conn := connectWS(t, ts.URL, paired.Token)
	defer conn.Close()
	command := clientproto.Command{
		SchemaVersion: clientproto.Version,
		CommandID:     "cmd-metrics-ping",
		Type:          clientproto.CommandPing,
		CreatedAt:     time.Now().UTC(),
	}
	env, _ := clientproto.NewEnvelope(clientproto.TypeCommand, command)
	if err := conn.WriteJSON(env); err != nil {
		t.Fatal(err)
	}
	_ = conn.SetReadDeadline(time.Now().Add(3 * time.Second))
	for {
		var response clientproto.Envelope
		if err := conn.ReadJSON(&response); err != nil {
			t.Fatal(err)
		}
		if response.Type == clientproto.TypeCommandResult {
			break
		}
	}

	body = getMetrics(t, ts.URL, "")
	for _, want := range []string{
		"meetagent_ws_clients 1",
		"meetagent_paired_clients 1",
		`meetagent_command_results_total{status="completed"} 1`,
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("metrics missing %q:\n%s", want, body)
		}
	}

	// Non-loopback scrapes require the admin token.
	req := httptest.NewRequest(http.MethodGet, "/metrics", nil)
	req.RemoteAddr = "203.0.113.9:1234"
	rec := httptest.NewRecorder()
	srv.Handler().ServeHTTP(rec, req)
	if rec.Code != http.StatusUnauthorized {
		t.Fatalf("remote metrics status = %d, want 401", rec.Code)
	}
	req = httptest.NewRequest(http.MethodGet, "/metrics", nil)
	req.RemoteAddr = "203.0.113.9:1234"
	req.Header.Set("Authorization", "Bearer admin-token")
	rec = httptest.NewRecorder()
	srv.Handler().ServeHTTP(rec, req)
	if rec.Code != http.StatusOK {
		t.Fatalf("authorized remote metrics status = %d, want 200", rec.Code)
	}
}

func getMetrics(t *testing.T, baseURL, token string) string {
	t.Helper()
	req, _ := http.NewRequest(http.MethodGet, baseURL+"/metrics", nil)
	if token != "" {
		req.Header.Set("Authorization", "Bearer "+token)
	}
	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		t.Fatal(err)
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		t.Fatalf("metrics status = %d", resp.StatusCode)
	}
	data, err := io.ReadAll(resp.Body)
	if err != nil {
		t.Fatal(err)
	}
	return string(data)
}
