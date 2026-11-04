package server_test

import (
	"encoding/json"
	"io"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"

	"github.com/FlyingBassHot/meetagent/internal/cloud/config"
	"github.com/FlyingBassHot/meetagent/internal/cloud/server"
	"github.com/FlyingBassHot/meetagent/pkg/protocol"
	"github.com/gorilla/websocket"
)

const (
	nodeID       = "node-001"
	nodeToken    = "node-token"
	node2ID      = "node-002"
	node2Token   = "node2-token"
	deviceID     = "web-001"
	deviceToken  = "device-token"
	device2ID    = "web-002"
	device2Token = "device2-token"
)

func TestRegistrationCommandRoutingAndOfflineStatus(t *testing.T) {
	wsURL, srv := newTestCloud(t)
	defer srv.Close()

	node := dial(t, wsURL)
	nodeResult := register(t, node, protocol.PeerNode, nodeID, nodeToken)
	if !nodeResult.OK {
		t.Fatalf("node registration failed: %+v", nodeResult)
	}

	device := dial(t, wsURL)
	deviceResult := register(t, device, protocol.PeerDevice, deviceID, deviceToken)
	if !deviceResult.OK {
		t.Fatalf("device registration failed: %+v", deviceResult)
	}
	if deviceResult.Snapshot == nil || len(deviceResult.Snapshot.Nodes) != 1 ||
		deviceResult.Snapshot.Nodes[0].NodeID != nodeID || deviceResult.Snapshot.Nodes[0].Status != "online" {
		t.Fatalf("device snapshot = %+v", deviceResult.Snapshot)
	}

	// Device -> Cloud -> Node
	command, err := protocol.NewWithPayload(protocol.TypeCommand, protocol.ActionPing, protocol.Source{
		Type: protocol.PeerDevice,
		ID:   deviceID,
	}, map[string]string{"nonce": "abc"})
	if err != nil {
		t.Fatal(err)
	}
	command.WithRequestID("req-ping-1").WithTarget(protocol.Target{NodeID: nodeID})
	writeEnvelope(t, device, command)

	forwarded := readUntil(t, node, func(env *protocol.Envelope) bool {
		return env.Type == protocol.TypeCommand && env.RequestID == "req-ping-1"
	})
	if forwarded.Source != command.Source {
		t.Fatalf("forwarded source = %+v, want %+v", forwarded.Source, command.Source)
	}

	// Node -> Cloud -> Device: accepted then completed.
	accepted := responseEnvelope(t, forwarded, protocol.CommandResult{Status: protocol.StatusAccepted}, nodeID)
	writeEnvelope(t, node, accepted)

	completed := responseEnvelope(t, forwarded, protocol.CommandResult{
		Status: protocol.StatusCompleted,
		Data:   json.RawMessage(`{"pong":true}`),
	}, nodeID)
	writeEnvelope(t, node, completed)

	first := readUntil(t, device, func(env *protocol.Envelope) bool {
		return env.Type == protocol.TypeResponse && env.RequestID == "req-ping-1"
	})
	firstResult := decodeResult(t, first)
	if firstResult.Status != protocol.StatusAccepted {
		t.Fatalf("first status = %q, want accepted", firstResult.Status)
	}

	second := readUntil(t, device, func(env *protocol.Envelope) bool {
		return env.Type == protocol.TypeResponse && env.RequestID == "req-ping-1"
	})
	secondResult := decodeResult(t, second)
	if secondResult.Status != protocol.StatusCompleted {
		t.Fatalf("second status = %q, want completed", secondResult.Status)
	}
	var data struct {
		Pong bool `json:"pong"`
	}
	if err := secondResult.DecodeData(&data); err != nil || !data.Pong {
		t.Fatalf("completed data = %+v, err = %v", data, err)
	}

	// A request_id that is still in flight must not be accepted twice.
	duplicateCommand, err := protocol.NewWithPayload(protocol.TypeCommand, protocol.ActionPing, protocol.Source{
		Type: protocol.PeerDevice,
		ID:   deviceID,
	}, map[string]string{"nonce": "dup"})
	if err != nil {
		t.Fatal(err)
	}
	duplicateCommand.WithRequestID("req-dup-1").WithTarget(protocol.Target{NodeID: nodeID})
	writeEnvelope(t, device, duplicateCommand)

	readUntil(t, node, func(env *protocol.Envelope) bool {
		return env.Type == protocol.TypeCommand && env.RequestID == "req-dup-1"
	})
	writeEnvelope(t, device, duplicateCommand)

	duplicate := readUntil(t, device, func(env *protocol.Envelope) bool {
		return env.Type == protocol.TypeResponse && env.RequestID == "req-dup-1"
	})
	if decodeResult(t, duplicate).Status != protocol.StatusRejected {
		t.Fatalf("duplicate request was not rejected: %+v", duplicate)
	}

	// Closing the Node must mark it offline and push a snapshot to Devices.
	_ = node.Close()
	snapshot := readUntil(t, device, func(env *protocol.Envelope) bool {
		if env.Type != protocol.TypeSync || env.Action != protocol.ActionSnapshot {
			return false
		}
		var snap protocol.Snapshot
		if err := env.DecodePayload(&snap); err != nil {
			return false
		}
		for _, node := range snap.Nodes {
			if node.NodeID == nodeID && node.Status == "offline" {
				return true
			}
		}
		return false
	})
	if snapshot.Type != protocol.TypeSync {
		t.Fatalf("expected offline snapshot, got %+v", snapshot)
	}
}

func TestRegistrationRejectsBadToken(t *testing.T) {
	wsURL, srv := newTestCloud(t)
	defer srv.Close()

	conn := dial(t, wsURL)
	result := register(t, conn, protocol.PeerDevice, deviceID, "wrong-token")
	if result.OK {
		t.Fatal("registration unexpectedly succeeded")
	}
	if result.ErrorCode != "invalid_credentials" {
		t.Fatalf("error_code = %q, want invalid_credentials", result.ErrorCode)
	}
}

func TestNodeConnectionReplacement(t *testing.T) {
	wsURL, srv := newTestCloud(t)
	defer srv.Close()

	first := dial(t, wsURL)
	register(t, first, protocol.PeerNode, nodeID, nodeToken)

	second := dial(t, wsURL)
	result := register(t, second, protocol.PeerNode, nodeID, nodeToken)
	if !result.OK {
		t.Fatalf("replacement registration failed: %+v", result)
	}

	_ = first.SetReadDeadline(time.Now().Add(2 * time.Second))
	for {
		_, _, err := first.ReadMessage()
		if err != nil {
			if websocket.IsCloseError(err, 4001) {
				return
			}
			// A TCP error also proves the old connection was removed.
			return
		}
	}
}

func TestNodeAuthorizationAndDeviceIsolation(t *testing.T) {
	wsURL, srv := newTestCloud(t)
	defer srv.Close()

	node1 := dial(t, wsURL)
	register(t, node1, protocol.PeerNode, nodeID, nodeToken)
	node2 := dial(t, wsURL)
	register(t, node2, protocol.PeerNode, node2ID, node2Token)

	allAccess := dial(t, wsURL)
	allSnapshot := register(t, allAccess, protocol.PeerDevice, deviceID, deviceToken)
	if allSnapshot.Snapshot == nil || len(allSnapshot.Snapshot.Nodes) != 2 {
		t.Fatalf("all-access snapshot = %+v", allSnapshot.Snapshot)
	}

	limited := dial(t, wsURL)
	limitedSnapshot := register(t, limited, protocol.PeerDevice, device2ID, device2Token)
	if limitedSnapshot.Snapshot == nil {
		t.Fatal("limited device received no snapshot")
	}
	if len(limitedSnapshot.Snapshot.Nodes) != 1 || limitedSnapshot.Snapshot.Nodes[0].NodeID != nodeID {
		t.Fatalf("limited snapshot nodes = %+v", limitedSnapshot.Snapshot.Nodes)
	}

	// Unauthorized Node is rejected before reaching the Node.
	forbidden, err := protocol.NewWithPayload(protocol.TypeCommand, protocol.ActionPing, protocol.Source{
		Type: protocol.PeerDevice,
		ID:   device2ID,
	}, map[string]string{"nonce": "forbidden"})
	if err != nil {
		t.Fatal(err)
	}
	forbidden.WithRequestID("req-forbidden").WithTarget(protocol.Target{NodeID: node2ID})
	writeEnvelope(t, limited, forbidden)

	rejected := readUntil(t, limited, func(env *protocol.Envelope) bool {
		return env.Type == protocol.TypeResponse && env.RequestID == "req-forbidden"
	})
	if decodeResult(t, rejected).Status != protocol.StatusRejected {
		t.Fatalf("unauthorized command was not rejected: %+v", rejected)
	}

	// Authorized command is routed normally.
	allowed, err := protocol.NewWithPayload(protocol.TypeCommand, protocol.ActionPing, protocol.Source{
		Type: protocol.PeerDevice,
		ID:   device2ID,
	}, map[string]string{"nonce": "allowed"})
	if err != nil {
		t.Fatal(err)
	}
	allowed.WithRequestID("req-allowed").WithTarget(protocol.Target{NodeID: nodeID})
	writeEnvelope(t, limited, allowed)

	forwarded := readUntil(t, node1, func(env *protocol.Envelope) bool {
		return env.Type == protocol.TypeCommand && env.RequestID == "req-allowed"
	})
	writeEnvelope(t, node1, responseEnvelope(t, forwarded, protocol.CommandResult{Status: protocol.StatusCompleted}, nodeID))

	completed := readUntil(t, limited, func(env *protocol.Envelope) bool {
		return env.Type == protocol.TypeResponse && env.RequestID == "req-allowed"
	})
	if decodeResult(t, completed).Status != protocol.StatusCompleted {
		t.Fatalf("authorized command failed: %+v", completed)
	}

	// The other Device must not receive this command response.
	expectNoMatch(t, allAccess, 500*time.Millisecond, func(env *protocol.Envelope) bool {
		return env.Type == protocol.TypeResponse && env.RequestID == "req-allowed"
	})
}

func TestEventIsolationReplayAndSequences(t *testing.T) {
	wsURL, srv := newTestCloud(t)
	defer srv.Close()

	node1 := dial(t, wsURL)
	register(t, node1, protocol.PeerNode, nodeID, nodeToken)
	node2 := dial(t, wsURL)
	register(t, node2, protocol.PeerNode, node2ID, node2Token)

	allAccess := dial(t, wsURL)
	register(t, allAccess, protocol.PeerDevice, deviceID, deviceToken)
	limited := dial(t, wsURL)
	register(t, limited, protocol.PeerDevice, device2ID, device2Token)

	publishEvent(t, node1, nodeID, protocol.EventToolStarted, "opencode-main", map[string]any{"tool_name": "read_file"})
	received := readUntil(t, limited, func(env *protocol.Envelope) bool {
		return env.Type == protocol.TypeEvent && env.Target != nil && env.Target.NodeID == nodeID
	})
	firstEvent := decodeEvent(t, received)
	if firstEvent.EventType != protocol.EventToolStarted || firstEvent.Sequence == 0 {
		t.Fatalf("first event = %+v", firstEvent)
	}

	publishEvent(t, node1, nodeID, protocol.EventToolCompleted, "opencode-main", map[string]any{"tool_name": "read_file"})
	next := readUntil(t, limited, func(env *protocol.Envelope) bool {
		return env.Type == protocol.TypeEvent && env.Target != nil && env.Target.NodeID == nodeID
	})
	nextEvent := decodeEvent(t, next)
	if nextEvent.Sequence != firstEvent.Sequence+1 {
		t.Fatalf("sequence did not increase by one: %d -> %d", firstEvent.Sequence, nextEvent.Sequence)
	}

	// node-002 is outside web-002's ACL.
	publishEvent(t, node2, node2ID, protocol.EventAgentError, "", map[string]any{"message": "boom"})
	readUntil(t, allAccess, func(env *protocol.Envelope) bool {
		return env.Type == protocol.TypeEvent && env.Target != nil && env.Target.NodeID == node2ID
	})
	expectNoMatch(t, limited, 500*time.Millisecond, func(env *protocol.Envelope) bool {
		return env.Type == protocol.TypeEvent && env.Target != nil && env.Target.NodeID == node2ID
	})

	// While web-002 is offline, node-001 emits more events.
	oldSequence := nextEvent.Sequence
	_ = limited.Close()
	time.Sleep(100 * time.Millisecond)
	publishEvent(t, node1, nodeID, protocol.EventSessionCreated, "opencode-main", map[string]any{"title": "restored"})
	publishEvent(t, node1, nodeID, protocol.EventSessionUpdated, "opencode-main", map[string]any{"title": "restored"})

	reconnected := dial(t, wsURL)
	result := register(t, reconnected, protocol.PeerDevice, device2ID, device2Token)
	if result.Snapshot == nil {
		t.Fatal("reconnected device received no snapshot")
	}
	if got := result.Snapshot.LastSequences[nodeID]; got <= oldSequence {
		t.Fatalf("snapshot last sequence = %d, want > %d", got, oldSequence)
	}

	replay := requestReplay(t, reconnected, device2ID, map[string]uint64{nodeID: oldSequence})
	if len(replay.Truncated) != 0 {
		t.Fatalf("unexpected truncation: %+v", replay.Truncated)
	}
	if len(replay.Events) != 2 {
		t.Fatalf("replay events = %d, want 2: %+v", len(replay.Events), replay.Events)
	}
	for i, event := range replay.Events {
		if event.Sequence != oldSequence+uint64(i)+1 {
			t.Fatalf("replay sequence[%d] = %d, want %d", i, event.Sequence, oldSequence+uint64(i)+1)
		}
		if event.NodeID != nodeID {
			t.Fatalf("replay leaked node %q", event.NodeID)
		}
	}
}

func TestEventCacheTruncationAndSnapshotBaseline(t *testing.T) {
	cfg := testConfig()
	cfg.EventCache.Capacity = 2
	wsURL, srv := newTestCloudWithConfig(t, cfg)
	defer srv.Close()

	node := dial(t, wsURL)
	register(t, node, protocol.PeerNode, nodeID, nodeToken)
	device := dial(t, wsURL)
	result := register(t, device, protocol.PeerDevice, deviceID, deviceToken)
	if result.Snapshot == nil {
		t.Fatal("device received no snapshot")
	}

	for i := 0; i < 4; i++ {
		publishEvent(t, node, nodeID, protocol.EventMessageDelta, "opencode-main", map[string]any{"index": i})
		index := i
		readUntil(t, device, func(env *protocol.Envelope) bool {
			if env.Type != protocol.TypeEvent || env.Action != protocol.EventMessageDelta {
				return false
			}
			var event protocol.AgentEvent
			if err := env.DecodePayload(&event); err != nil {
				return false
			}
			var payload struct {
				Index int `json:"index"`
			}
			if err := json.Unmarshal(event.Payload, &payload); err != nil {
				return false
			}
			return payload.Index == index
		})
	}

	replay := requestReplay(t, device, deviceID, map[string]uint64{nodeID: 1})
	if len(replay.Truncated) != 1 || replay.Truncated[0] != nodeID {
		t.Fatalf("truncated = %+v, want [%s]", replay.Truncated, nodeID)
	}
	if len(replay.Events) != 2 {
		t.Fatalf("bounded cache replay = %d events, want 2", len(replay.Events))
	}
	if replay.Events[0].Sequence <= 1 {
		t.Fatalf("replay started at sequence %d, expected retained tail", replay.Events[0].Sequence)
	}
}

func TestAgentStateSyncUpdatesDeviceSnapshot(t *testing.T) {
	wsURL, _ := newTestCloud(t)
	node := dial(t, wsURL)
	if result := register(t, node, protocol.PeerNode, nodeID, nodeToken); !result.OK {
		t.Fatalf("node registration failed: %+v", result)
	}

	device := dial(t, wsURL)
	deviceResult := register(t, device, protocol.PeerDevice, deviceID, deviceToken)
	if !deviceResult.OK {
		t.Fatalf("device registration failed: %+v", deviceResult)
	}
	if len(deviceResult.Snapshot.Nodes) != 1 || len(deviceResult.Snapshot.Nodes[0].Agents) != 0 {
		t.Fatalf("expected an empty agent inventory: %+v", deviceResult.Snapshot.Nodes)
	}

	updated := time.Now().UTC()
	agents := []protocol.AgentView{
		{
			AgentID:      "opencode-main",
			Type:         "opencode",
			Name:         "OpenCode Main",
			Status:       "online",
			Capabilities: map[string]bool{"send_prompt": true},
			Sessions: []protocol.SessionView{
				{
					SessionID: "ses_1",
					Title:     "Session One",
					Status:    "running",
					UpdatedAt: updated,
				},
			},
		},
	}
	env, err := protocol.NewWithPayload(protocol.TypeSync, protocol.ActionAgentState, protocol.Source{
		Type: protocol.PeerNode,
		ID:   nodeID,
	}, protocol.AgentStatePayload{Agents: agents})
	if err != nil {
		t.Fatal(err)
	}
	env.WithRequestID("req-state-1").WithTarget(protocol.Target{NodeID: nodeID})
	writeEnvelope(t, node, env)

	snapshotEnv := readUntil(t, device, func(candidate *protocol.Envelope) bool {
		if candidate.Type != protocol.TypeSync || candidate.Action != protocol.ActionSnapshot {
			return false
		}
		var snap protocol.Snapshot
		if err := candidate.DecodePayload(&snap); err != nil {
			return false
		}
		return len(snap.Nodes) == 1 && len(snap.Nodes[0].Agents) == 1
	})
	var snapshot protocol.Snapshot
	if err := snapshotEnv.DecodePayload(&snapshot); err != nil {
		t.Fatalf("decode snapshot: %v", err)
	}
	agent := snapshot.Nodes[0].Agents[0]
	if agent.AgentID != "opencode-main" || agent.NodeID != nodeID {
		t.Fatalf("unexpected agent: %+v", agent)
	}
	if len(agent.Sessions) != 1 || agent.Sessions[0].SessionID != "ses_1" ||
		agent.Sessions[0].AgentID != "opencode-main" || agent.Sessions[0].NodeID != nodeID {
		t.Fatalf("unexpected sessions: %+v", agent.Sessions)
	}

	// An invalid inventory must be rejected without touching the snapshot.
	bad, err := protocol.NewWithPayload(protocol.TypeSync, protocol.ActionAgentState, protocol.Source{
		Type: protocol.PeerNode,
		ID:   nodeID,
	}, protocol.AgentStatePayload{Agents: []protocol.AgentView{{AgentID: " "}}})
	if err != nil {
		t.Fatal(err)
	}
	bad.WithRequestID("req-state-2").WithTarget(protocol.Target{NodeID: nodeID})
	writeEnvelope(t, node, bad)

	errorEnv := readUntil(t, node, func(candidate *protocol.Envelope) bool {
		return candidate.Type == protocol.TypeError && candidate.RequestID == "req-state-2"
	})
	var errorPayload protocol.ErrorPayload
	if err := errorEnv.DecodePayload(&errorPayload); err != nil {
		t.Fatalf("decode error payload: %v", err)
	}
	if errorPayload.Code != "invalid_agents" {
		t.Fatalf("error code = %q, want invalid_agents", errorPayload.Code)
	}
}

func TestSyncAgentStateRequiresNode(t *testing.T) {
	wsURL, _ := newTestCloud(t)
	node := dial(t, wsURL)
	if result := register(t, node, protocol.PeerNode, nodeID, nodeToken); !result.OK {
		t.Fatalf("node registration failed: %+v", result)
	}
	device := dial(t, wsURL)
	if result := register(t, device, protocol.PeerDevice, deviceID, deviceToken); !result.OK {
		t.Fatalf("device registration failed: %+v", result)
	}

	// Devices may not push agent_state; doing so is a protocol error.
	env, err := protocol.NewWithPayload(protocol.TypeSync, protocol.ActionAgentState, protocol.Source{
		Type: protocol.PeerDevice,
		ID:   deviceID,
	}, protocol.AgentStatePayload{Agents: []protocol.AgentView{}})
	if err != nil {
		t.Fatal(err)
	}
	env.WithRequestID("req-device-state")
	writeEnvelope(t, device, env)

	reply := readUntil(t, device, func(candidate *protocol.Envelope) bool {
		return candidate.Type == protocol.TypeError && candidate.RequestID == "req-device-state"
	})
	var payload protocol.ErrorPayload
	if err := reply.DecodePayload(&payload); err != nil {
		t.Fatalf("decode error: %v", err)
	}
	if payload.Code != "forbidden" {
		t.Fatalf("error code = %q, want forbidden", payload.Code)
	}
}

func TestManagementEndpointsRequireAdminToken(t *testing.T) {
	cfg := testConfig()
	cfg.Server.AdminToken = "admin-secret"
	_, srv := newTestCloudWithConfig(t, cfg)
	handler := srv.Handler()

	for _, path := range []string{"/api/v1/status", "/api/v1/snapshot"} {
		for _, header := range []string{"", "Bearer wrong"} {
			request := httptest.NewRequest(http.MethodGet, path, nil)
			if header != "" {
				request.Header.Set("Authorization", header)
			}
			recorder := httptest.NewRecorder()
			handler.ServeHTTP(recorder, request)
			if recorder.Code != http.StatusUnauthorized {
				t.Fatalf("%s with %q = %d, want 401", path, header, recorder.Code)
			}
		}

		request := httptest.NewRequest(http.MethodGet, path, nil)
		request.Header.Set("Authorization", "Bearer admin-secret")
		recorder := httptest.NewRecorder()
		handler.ServeHTTP(recorder, request)
		if recorder.Code != http.StatusOK {
			t.Fatalf("%s with valid token = %d, want 200", path, recorder.Code)
		}
	}

	recorder := httptest.NewRecorder()
	handler.ServeHTTP(recorder, httptest.NewRequest(http.MethodGet, "/healthz", nil))
	if recorder.Code != http.StatusOK {
		t.Fatalf("/healthz = %d, want 200", recorder.Code)
	}
}

func TestNodeDisconnectFailsPendingRequests(t *testing.T) {
	wsURL, srv := newTestCloud(t)
	defer srv.Close()

	node := dial(t, wsURL)
	if result := register(t, node, protocol.PeerNode, nodeID, nodeToken); !result.OK {
		t.Fatalf("node registration failed: %+v", result)
	}
	device := dial(t, wsURL)
	if result := register(t, device, protocol.PeerDevice, deviceID, deviceToken); !result.OK {
		t.Fatalf("device registration failed: %+v", result)
	}

	command, err := protocol.NewWithPayload(protocol.TypeCommand, protocol.ActionPing, protocol.Source{
		Type: protocol.PeerDevice,
		ID:   deviceID,
	}, map[string]string{"nonce": "drop"})
	if err != nil {
		t.Fatal(err)
	}
	command.WithRequestID("req-node-drop").WithTarget(protocol.Target{NodeID: nodeID})
	writeEnvelope(t, device, command)

	readUntil(t, node, func(env *protocol.Envelope) bool {
		return env.Type == protocol.TypeCommand && env.RequestID == "req-node-drop"
	})

	// Drop the Node connection without a final response.
	_ = node.Close()

	failed := readUntil(t, device, func(env *protocol.Envelope) bool {
		return env.Type == protocol.TypeResponse && env.RequestID == "req-node-drop"
	})
	result := decodeResult(t, failed)
	if result.Status != protocol.StatusFailed {
		t.Fatalf("status = %q, want failed", result.Status)
	}
	if result.Error == "" {
		t.Fatal("expected an error message explaining the node disconnect")
	}
}

func TestRateLimitDropsExcessMessages(t *testing.T) {
	cfg := testConfig()
	cfg.Server.RateLimitPerSecond = 1
	cfg.Server.RateLimitBurst = 2
	wsURL, srv := newTestCloudWithConfig(t, cfg)
	defer srv.Close()

	device := dial(t, wsURL)
	if result := register(t, device, protocol.PeerDevice, deviceID, deviceToken); !result.OK {
		t.Fatalf("device registration failed: %+v", result)
	}

	for i := 0; i < 8; i++ {
		env, err := protocol.NewWithPayload(protocol.TypeHeartbeat, "", protocol.Source{
			Type: protocol.PeerDevice,
			ID:   deviceID,
		}, protocol.HeartbeatPayload{Nonce: protocol.NewID("hb")})
		if err != nil {
			t.Fatal(err)
		}
		writeEnvelope(t, device, env)
	}

	reply := readUntil(t, device, func(env *protocol.Envelope) bool {
		return env.Type == protocol.TypeError
	})
	var payload protocol.ErrorPayload
	if err := reply.DecodePayload(&payload); err != nil {
		t.Fatalf("decode error: %v", err)
	}
	if payload.Code != "rate_limited" {
		t.Fatalf("error code = %q, want rate_limited", payload.Code)
	}
}

func testConfig() config.Config {
	cfg := config.Default()
	cfg.Server.AllowedOrigins = []string{"*"}
	cfg.Server.ReadTimeout = config.Duration(5 * time.Second)
	cfg.Server.PingInterval = config.Duration(5 * time.Second)
	cfg.Server.RegisterTimeout = config.Duration(2 * time.Second)
	cfg.Server.PendingTTL = config.Duration(5 * time.Second)
	cfg.Auth.Nodes = map[string]config.NodeCredential{
		nodeID:  {Token: nodeToken},
		node2ID: {Token: node2Token},
	}
	cfg.Auth.Devices = map[string]config.DeviceCredential{
		deviceID:  {Token: deviceToken, AllowedNodes: []string{"*"}},
		device2ID: {Token: device2Token, AllowedNodes: []string{nodeID}},
	}
	return cfg
}

func newTestCloud(t *testing.T) (string, *server.Server) {
	t.Helper()
	return newTestCloudWithConfig(t, testConfig())
}

func newTestCloudWithConfig(t *testing.T, cfg config.Config) (string, *server.Server) {
	t.Helper()

	logger := slog.New(slog.NewTextHandler(io.Discard, nil))
	srv := server.New(cfg, logger)
	httpServer := httptest.NewServer(srv.Handler())
	t.Cleanup(httpServer.Close)

	wsURL := "ws" + strings.TrimPrefix(httpServer.URL, "http") + cfg.Server.WSPath
	return wsURL, srv
}

func dial(t *testing.T, wsURL string) *websocket.Conn {
	t.Helper()
	conn, resp, err := websocket.DefaultDialer.Dial(wsURL, nil)
	if err != nil {
		if resp != nil && resp.Body != nil {
			_ = resp.Body.Close()
		}
		t.Fatalf("dial %s: %v", wsURL, err)
	}
	t.Cleanup(func() { _ = conn.Close() })
	return conn
}

func register(t *testing.T, conn *websocket.Conn, peerType protocol.PeerType, id, token string) protocol.RegisterResult {
	t.Helper()

	env, err := protocol.NewWithPayload(protocol.TypeRegister, protocol.ActionRegister, protocol.Source{
		Type: peerType,
		ID:   id,
	}, protocol.RegisterPayload{Token: token, Name: id})
	if err != nil {
		t.Fatal(err)
	}
	env.WithRequestID(protocol.NewID("req"))
	writeEnvelope(t, conn, env)

	reply := readUntil(t, conn, func(candidate *protocol.Envelope) bool {
		return candidate.Type == protocol.TypeResponse && candidate.Action == protocol.ActionRegister && candidate.RequestID == env.RequestID
	})
	var result protocol.RegisterResult
	if err := reply.DecodePayload(&result); err != nil {
		t.Fatalf("decode register result: %v", err)
	}
	return result
}

func publishEvent(t *testing.T, conn *websocket.Conn, nodeID, eventType, agentID string, payload map[string]any) protocol.AgentEvent {
	t.Helper()
	raw, err := json.Marshal(payload)
	if err != nil {
		t.Fatal(err)
	}
	event := protocol.AgentEvent{
		EventID:   protocol.NewID("evt"),
		NodeID:    nodeID,
		AgentID:   agentID,
		EventType: eventType,
		Timestamp: time.Now().UTC(),
		Payload:   raw,
	}
	env, err := protocol.NewWithPayload(protocol.TypeEvent, eventType, protocol.Source{
		Type: protocol.PeerNode,
		ID:   nodeID,
	}, event)
	if err != nil {
		t.Fatal(err)
	}
	env.WithTarget(protocol.Target{NodeID: nodeID, AgentID: agentID})
	writeEnvelope(t, conn, env)
	return event
}

func requestReplay(t *testing.T, conn *websocket.Conn, deviceID string, last map[string]uint64) protocol.EventReplayResult {
	t.Helper()
	env, err := protocol.NewWithPayload(protocol.TypeSync, protocol.ActionEvents, protocol.Source{
		Type: protocol.PeerDevice,
		ID:   deviceID,
	}, protocol.EventReplayRequest{LastSequences: last})
	if err != nil {
		t.Fatal(err)
	}
	env.WithRequestID(protocol.NewID("req"))
	writeEnvelope(t, conn, env)

	reply := readUntil(t, conn, func(candidate *protocol.Envelope) bool {
		return candidate.Type == protocol.TypeSync && candidate.Action == protocol.ActionEvents && candidate.RequestID == env.RequestID
	})
	var result protocol.EventReplayResult
	if err := reply.DecodePayload(&result); err != nil {
		t.Fatalf("decode replay: %v", err)
	}
	return result
}

func responseEnvelope(t *testing.T, command *protocol.Envelope, result protocol.CommandResult, nodeID string) *protocol.Envelope {
	t.Helper()
	env, err := protocol.NewWithPayload(protocol.TypeResponse, command.Action, protocol.Source{
		Type: protocol.PeerNode,
		ID:   nodeID,
	}, result)
	if err != nil {
		t.Fatal(err)
	}
	env.WithRequestID(command.RequestID)
	if command.Target != nil {
		env.WithTarget(*command.Target)
	}
	return env
}

func decodeResult(t *testing.T, env *protocol.Envelope) protocol.CommandResult {
	t.Helper()
	var result protocol.CommandResult
	if err := env.DecodePayload(&result); err != nil {
		t.Fatalf("decode command result: %v", err)
	}
	return result
}

func decodeEvent(t *testing.T, env *protocol.Envelope) protocol.AgentEvent {
	t.Helper()
	var event protocol.AgentEvent
	if err := env.DecodePayload(&event); err != nil {
		t.Fatalf("decode event: %v", err)
	}
	return event
}

func expectNoMatch(t *testing.T, conn *websocket.Conn, wait time.Duration, predicate func(*protocol.Envelope) bool) {
	t.Helper()
	deadline := time.Now().Add(wait)
	for {
		_ = conn.SetReadDeadline(deadline)
		_, raw, err := conn.ReadMessage()
		if err != nil {
			// A read timeout means nothing matching arrived; that is success.
			return
		}
		var env protocol.Envelope
		if err := json.Unmarshal(raw, &env); err != nil {
			continue
		}
		if predicate(&env) {
			t.Fatalf("received message that should have been isolated: %+v", env)
		}
	}
}

func writeEnvelope(t *testing.T, conn *websocket.Conn, env *protocol.Envelope) {
	t.Helper()
	raw, err := json.Marshal(env)
	if err != nil {
		t.Fatal(err)
	}
	_ = conn.SetWriteDeadline(time.Now().Add(2 * time.Second))
	if err := conn.WriteMessage(websocket.TextMessage, raw); err != nil {
		t.Fatalf("write envelope: %v", err)
	}
}

func readUntil(t *testing.T, conn *websocket.Conn, predicate func(*protocol.Envelope) bool) *protocol.Envelope {
	t.Helper()
	deadline := time.Now().Add(4 * time.Second)
	for {
		_ = conn.SetReadDeadline(deadline)
		_, raw, err := conn.ReadMessage()
		if err != nil {
			t.Fatalf("read envelope: %v", err)
		}
		var env protocol.Envelope
		if err := json.Unmarshal(raw, &env); err != nil {
			t.Fatalf("decode envelope: %v", err)
		}
		if predicate(&env) {
			return &env
		}
	}
}
