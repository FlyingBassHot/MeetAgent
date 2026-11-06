package eventhub

import (
	"context"
	"io"
	"log/slog"
	"testing"
	"time"

	"github.com/FlyingBassHot/meetagent/pkg/clientproto"
	"github.com/FlyingBassHot/meetagent/pkg/protocol"
)

type fakeSource struct {
	events chan protocol.AgentEvent
	agents []protocol.AgentView
}

func (f *fakeSource) Events() <-chan protocol.AgentEvent { return f.events }
func (f *fakeSource) Agents() []protocol.AgentView       { return f.agents }

func TestHubSequenceSnapshotAndReplay(t *testing.T) {
	source := &fakeSource{
		events: make(chan protocol.AgentEvent, 10),
		agents: []protocol.AgentView{{AgentID: "mock-1", NodeID: "node-test", Status: "online"}},
	}
	logger := slog.New(slog.NewTextHandler(io.Discard, nil))
	hub := New(clientproto.NodeInfo{NodeID: "node-test", Name: "Test"}, source, 100, time.Minute, logger)
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	go hub.Run(ctx)

	_, _, live, unsubscribe := hub.Subscribe(0, 10)
	defer unsubscribe()

	for i := 1; i <= 3; i++ {
		source.events <- protocol.AgentEvent{EventType: protocol.EventAgentStatusChanged}
	}
	for i := 0; i < 3; i++ {
		select {
		case event := <-live:
			if event.Sequence != uint64(i+1) {
				t.Fatalf("sequence = %d, want %d", event.Sequence, i+1)
			}
		case <-time.After(time.Second):
			t.Fatal("timed out waiting for live event")
		}
	}

	snapshot := hub.Snapshot(10)
	if snapshot.LastSequence != 3 {
		t.Fatalf("snapshot last = %d, want 3", snapshot.LastSequence)
	}
	if len(snapshot.Events) != 3 {
		t.Fatalf("snapshot events = %d, want 3", len(snapshot.Events))
	}

	missed, truncated, _, unsubscribe2 := hub.Subscribe(1, 10)
	defer unsubscribe2()
	if truncated {
		t.Fatal("replay should not be truncated")
	}
	if len(missed) != 2 || missed[0].Sequence != 2 || missed[1].Sequence != 3 {
		t.Fatalf("missed = %+v", missed)
	}
}

func TestSubscribeReportsTruncatedReplay(t *testing.T) {
	source := &fakeSource{events: make(chan protocol.AgentEvent, 8)}
	logger := slog.New(slog.NewTextHandler(io.Discard, nil))
	hub := New(clientproto.NodeInfo{NodeID: "node-test", Name: "Test"}, source, 3, time.Minute, logger)
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	go hub.Run(ctx)

	for i := 0; i < 5; i++ {
		source.events <- protocol.AgentEvent{EventType: protocol.EventAgentStatusChanged}
	}
	deadline := time.Now().Add(time.Second)
	for hub.Snapshot(1).LastSequence < 5 && time.Now().Before(deadline) {
		time.Sleep(5 * time.Millisecond)
	}

	missed, truncated, _, unsubscribe := hub.Subscribe(1, 10)
	defer unsubscribe()
	if !truncated {
		t.Fatal("expected truncated replay after cache eviction")
	}
	if len(missed) != 0 {
		t.Fatalf("truncated replay should not return partial events: %+v", missed)
	}
}

func TestSubscribeReportsTruncatedAfterSequenceReset(t *testing.T) {
	source := &fakeSource{events: make(chan protocol.AgentEvent, 1)}
	logger := slog.New(slog.NewTextHandler(io.Discard, nil))
	hub := New(clientproto.NodeInfo{NodeID: "node-test", Name: "Test"}, source, 10, time.Minute, logger)

	// A fresh process has no events yet; a client still holding a sequence from
	// the previous process must fall back to the snapshot.
	missed, truncated, _, unsubscribe := hub.Subscribe(42, 10)
	defer unsubscribe()
	if !truncated || len(missed) != 0 {
		t.Fatalf("truncated=%v missed=%+v, want truncated with no replay", truncated, missed)
	}
}

func TestPermissionRequestGetsExpiryAndExpires(t *testing.T) {
	source := &fakeSource{events: make(chan protocol.AgentEvent, 4), agents: nil}
	logger := slog.New(slog.NewTextHandler(io.Discard, nil))
	hub := New(clientproto.NodeInfo{NodeID: "node-test", Name: "Test"}, source, 20, time.Minute, logger)
	hub.SetPermissionTTL(50 * time.Millisecond)
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	go hub.Run(ctx)
	time.Sleep(10 * time.Millisecond)

	source.events <- protocol.AgentEvent{
		EventType: protocol.EventPermissionRequested,
		Payload:   []byte(`{"permission_id":"per-1","description":"read secret.env"}`),
	}
	deadline := time.Now().Add(time.Second)
	for time.Now().Before(deadline) {
		snapshot := hub.Snapshot(10)
		if len(snapshot.PendingPermissions) == 1 && snapshot.PendingPermissions[0].ExpiresAt != nil {
			if !hub.HasPendingPermission("per-1") {
				t.Fatal("permission should be pending before expiry")
			}
			break
		}
		time.Sleep(10 * time.Millisecond)
	}
	time.Sleep(80 * time.Millisecond)
	if hub.HasPendingPermission("per-1") {
		t.Fatal("permission should expire")
	}
}
