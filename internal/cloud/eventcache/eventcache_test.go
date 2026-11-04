package eventcache

import (
	"encoding/json"
	"testing"
	"time"

	"github.com/FlyingBassHot/meetagent/pkg/protocol"
)

func event(eventType string) protocol.AgentEvent {
	return protocol.AgentEvent{EventType: eventType, Timestamp: time.Now().UTC()}
}

func permissionPayload(t *testing.T, payload protocol.PermissionEventPayload) json.RawMessage {
	t.Helper()
	raw, err := json.Marshal(payload)
	if err != nil {
		t.Fatalf("marshal permission payload: %v", err)
	}
	return raw
}

func TestAppendAssignsMonotonicSequences(t *testing.T) {
	cache := New(100, 0)

	first := cache.Append("node-a", protocol.AgentEvent{EventType: "a", Sequence: 99, NodeID: "spoofed"})
	second := cache.Append("node-a", event("b"))
	other := cache.Append("node-b", event("c"))

	if first.Sequence != 1 || second.Sequence != 2 {
		t.Fatalf("node-a sequences = %d, %d, want 1, 2", first.Sequence, second.Sequence)
	}
	if other.Sequence != 1 {
		t.Fatalf("node-b sequence = %d, want 1", other.Sequence)
	}
	if first.NodeID != "node-a" || other.NodeID != "node-b" {
		t.Fatalf("node ids not normalized: %q %q", first.NodeID, other.NodeID)
	}
	if first.EventID == "" || second.EventID == "" {
		t.Fatal("append did not generate event ids")
	}
	if first.EventID == second.EventID {
		t.Fatal("generated event ids are not unique")
	}
	if first.Timestamp.IsZero() {
		t.Fatal("append did not assign a timestamp")
	}
	if cache.Len() != 3 {
		t.Fatalf("len = %d, want 3", cache.Len())
	}
	if got := cache.NodeSequences("node-a"); got != 2 {
		t.Fatalf("node-a latest = %d, want 2", got)
	}
}

func TestSinceFiltersLimitsAndVisibility(t *testing.T) {
	cache := New(100, 0)
	for i := 0; i < 3; i++ {
		cache.Append("node-a", event("a"))
	}
	cache.Append("node-b", event("b"))

	visibleOnlyA := func(nodeID string) bool { return nodeID == "node-a" }

	events, latest, truncated := cache.Since(map[string]uint64{"node-a": 1}, visibleOnlyA, 0)
	if len(events) != 2 || events[0].Sequence != 2 || events[1].Sequence != 3 {
		t.Fatalf("since events = %+v", events)
	}
	if len(truncated) != 0 {
		t.Fatalf("unexpected truncation: %+v", truncated)
	}
	if _, ok := latest["node-b"]; ok {
		t.Fatalf("visibility filter leaked node-b: %+v", latest)
	}
	if latest["node-a"] != 3 {
		t.Fatalf("latest node-a = %d, want 3", latest["node-a"])
	}

	limited, _, _ := cache.Since(map[string]uint64{"node-a": 0}, visibleOnlyA, 1)
	if len(limited) != 1 || limited[0].Sequence != 1 {
		t.Fatalf("limited events = %+v", limited)
	}
}

func TestCapacityEvictionMarksTruncated(t *testing.T) {
	cache := New(3, 0)
	for i := 0; i < 5; i++ {
		cache.Append("node-a", event("a"))
	}
	if cache.Len() != 3 {
		t.Fatalf("len after eviction = %d, want 3", cache.Len())
	}

	events, _, truncated := cache.Since(map[string]uint64{"node-a": 0}, nil, 0)
	if len(events) != 3 || events[0].Sequence != 3 {
		t.Fatalf("retained events = %+v", events)
	}
	if len(truncated) != 0 {
		t.Fatalf("request from cache start should not be truncated: %+v", truncated)
	}

	_, _, truncated = cache.Since(map[string]uint64{"node-a": 1}, nil, 0)
	if len(truncated) != 1 || truncated[0] != "node-a" {
		t.Fatalf("truncated = %+v, want [node-a]", truncated)
	}

	_, _, truncated = cache.Since(map[string]uint64{"node-a": 6}, nil, 0)
	if len(truncated) != 1 {
		t.Fatalf("future sequence should be truncated: %+v", truncated)
	}
}

func TestTTLExpiryMarksTruncated(t *testing.T) {
	cache := New(100, time.Minute)
	now := time.Now().UTC()
	cache.now = func() time.Time { return now }

	cache.Append("node-a", protocol.AgentEvent{EventType: "a", Timestamp: now})
	cache.Append("node-a", protocol.AgentEvent{EventType: "b", Timestamp: now})

	now = now.Add(2 * time.Minute)
	if got := cache.Len(); got != 0 {
		t.Fatalf("len after ttl = %d, want 0", got)
	}
	_, _, truncated := cache.Since(map[string]uint64{"node-a": 1}, nil, 0)
	if len(truncated) != 1 {
		t.Fatalf("expired request should be truncated: %+v", truncated)
	}
}

func TestPendingPermissionsDerivedFromEvents(t *testing.T) {
	cache := New(100, 0)
	now := time.Now().UTC()
	expires := now.Add(time.Hour)

	cache.Append("node-a", protocol.AgentEvent{
		EventType: protocol.EventPermissionRequested,
		Timestamp: now,
		Payload: permissionPayload(t, protocol.PermissionEventPayload{
			PermissionID: "perm-1",
			Description:  "run tests",
			AgentID:      "agent-1",
			ExpiresAt:    &expires,
		}),
	})
	cache.Append("node-a", protocol.AgentEvent{
		EventType: protocol.EventPermissionRequested,
		Timestamp: now,
		Payload: permissionPayload(t, protocol.PermissionEventPayload{
			PermissionID: "perm-2",
			AgentID:      "agent-1",
		}),
	})
	cache.Append("node-a", protocol.AgentEvent{
		EventType: protocol.EventPermissionRequested,
		Timestamp: now,
		Payload: permissionPayload(t, protocol.PermissionEventPayload{
			PermissionID: "perm-3",
			Description:  "expired",
			ExpiresAt:    ptr(now.Add(-time.Minute)),
		}),
	})
	cache.Append("node-a", protocol.AgentEvent{
		EventType: protocol.EventPermissionResolved,
		Timestamp: now,
		Payload: permissionPayload(t, protocol.PermissionEventPayload{
			PermissionID: "perm-2",
			Status:       protocol.PermissionApproved,
		}),
	})

	pending := cache.PendingPermissions(nil)
	if len(pending) != 1 || pending[0].PermissionID != "perm-1" {
		t.Fatalf("pending = %+v, want only perm-1", pending)
	}
	if pending[0].NodeID != "node-a" || pending[0].AgentID != "agent-1" {
		t.Fatalf("pending view = %+v", pending[0])
	}

	if got := cache.PendingPermissions(func(string) bool { return false }); len(got) != 0 {
		t.Fatalf("visibility=false pending = %+v", got)
	}
}

func TestRecentReturnsNewestEvents(t *testing.T) {
	cache := New(100, 0)
	base := time.Now().UTC()
	for i := 0; i < 5; i++ {
		cache.Append("node-a", protocol.AgentEvent{
			EventType: "a",
			Timestamp: base.Add(time.Duration(i) * time.Second),
		})
	}
	cache.Append("node-b", protocol.AgentEvent{
		EventType: "b",
		Timestamp: base.Add(10 * time.Second),
	})

	recent := cache.Recent(nil, 2)
	if len(recent) != 2 {
		t.Fatalf("recent len = %d, want 2", len(recent))
	}
	if recent[0].EventType != "a" || recent[0].Sequence != 5 || recent[1].EventType != "b" {
		t.Fatalf("recent = %+v", recent)
	}
}

func ptr[T any](value T) *T { return &value }
