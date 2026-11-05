// Package eventcache stores a bounded, in-memory window of recent Agent
// events. Cloud assigns a monotonic sequence per Node so a reconnecting Device
// can ask for everything after its last confirmed sequence.
package eventcache

import (
	"encoding/json"
	"sort"
	"sync"
	"time"

	"github.com/FlyingBassHot/meetagent/pkg/protocol"
)

type eventRef struct {
	nodeID   string
	sequence uint64
}

type stream struct {
	events         []protocol.AgentEvent
	next           uint64
	droppedThrough uint64
}

// Cache is a bounded event store. It is safe for concurrent use.
type Cache struct {
	mu       sync.Mutex
	streams  map[string]*stream
	order    []eventRef
	total    int
	capacity int
	ttl      time.Duration
	now      func() time.Time
}

// New creates a cache with a global event capacity and time-to-live. A zero
// TTL disables time based expiry.
func New(capacity int, ttl time.Duration) *Cache {
	if capacity <= 0 {
		capacity = 5000
	}
	return &Cache{
		streams:  make(map[string]*stream),
		capacity: capacity,
		ttl:      ttl,
		now:      time.Now,
	}
}

// Append normalizes an event for nodeID and assigns the next sequence. The
// Node-supplied sequence (if any) is intentionally ignored: Cloud is the
// authority for replay ordering.
func (c *Cache) Append(nodeID string, event protocol.AgentEvent) protocol.AgentEvent {
	c.mu.Lock()
	defer c.mu.Unlock()

	now := c.now().UTC()
	s := c.streams[nodeID]
	if s == nil {
		s = &stream{next: 1}
		c.streams[nodeID] = s
	}

	event.NodeID = nodeID
	event.Sequence = s.next
	s.next++
	if event.EventID == "" {
		event.EventID = protocol.NewID("evt")
	}
	if event.Timestamp.IsZero() {
		event.Timestamp = now
	}

	s.events = append(s.events, event)
	c.order = append(c.order, eventRef{nodeID: nodeID, sequence: event.Sequence})
	c.total++
	c.pruneLocked(now)
	return event
}

// Since returns events after the supplied per-Node sequences. Nodes for which
// visible returns false are skipped. truncated lists Nodes where the requested
// sequence cannot be satisfied from the retained cache (or where Cloud has
// fewer events than the client claims, e.g. after a restart).
func (c *Cache) Since(last map[string]uint64, visible func(nodeID string) bool, limit int) ([]protocol.AgentEvent, map[string]uint64, []string) {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.pruneLocked(c.now().UTC())

	latest := make(map[string]uint64, len(c.streams))
	for nodeID, s := range c.streams {
		if visible != nil && !visible(nodeID) {
			continue
		}
		latest[nodeID] = s.next - 1
	}

	nodeIDs := make([]string, 0, len(c.streams))
	for nodeID := range c.streams {
		if visible != nil && !visible(nodeID) {
			continue
		}
		nodeIDs = append(nodeIDs, nodeID)
	}
	sort.Strings(nodeIDs)

	events := make([]protocol.AgentEvent, 0, 16)
	var truncated []string
	for _, nodeID := range nodeIDs {
		s := c.streams[nodeID]
		after := last[nodeID]
		latestSeq := s.next - 1

		if after > latestSeq || (after > 0 && after < s.droppedThrough) {
			truncated = append(truncated, nodeID)
		}

		start := sort.Search(len(s.events), func(i int) bool {
			return s.events[i].Sequence > after
		})
		for _, event := range s.events[start:] {
			if limit > 0 && len(events) >= limit {
				return events, latest, truncated
			}
			events = append(events, event)
		}
	}
	return events, latest, truncated
}

// LatestSequences returns the newest sequence for every visible Node.
func (c *Cache) LatestSequences(visible func(nodeID string) bool) map[string]uint64 {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.pruneLocked(c.now().UTC())

	result := make(map[string]uint64, len(c.streams))
	for nodeID, s := range c.streams {
		if visible != nil && !visible(nodeID) {
			continue
		}
		result[nodeID] = s.next - 1
	}
	return result
}

// Recent returns up to limit of the most recent visible events in timestamp
// order. It is used to seed a fresh Snapshot.
func (c *Cache) Recent(visible func(nodeID string) bool, limit int) []protocol.AgentEvent {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.pruneLocked(c.now().UTC())

	all := make([]protocol.AgentEvent, 0, c.total)
	for nodeID, s := range c.streams {
		if visible != nil && !visible(nodeID) {
			continue
		}
		all = append(all, s.events...)
	}
	sort.SliceStable(all, func(i, j int) bool {
		if all[i].Timestamp.Equal(all[j].Timestamp) {
			if all[i].NodeID == all[j].NodeID {
				return all[i].Sequence < all[j].Sequence
			}
			return all[i].NodeID < all[j].NodeID
		}
		return all[i].Timestamp.Before(all[j].Timestamp)
	})
	if limit > 0 && len(all) > limit {
		all = all[len(all)-limit:]
	}
	return all
}

// PendingPermissions derives the current pending permission set from the
// retained permission events. Resolved requests are removed.
func (c *Cache) PendingPermissions(visible func(nodeID string) bool) []protocol.PermissionView {
	c.mu.Lock()
	defer c.mu.Unlock()
	now := c.now().UTC()
	c.pruneLocked(now)

	type ordered struct {
		view protocol.PermissionView
	}
	pending := make(map[string]ordered)
	for nodeID, s := range c.streams {
		if visible != nil && !visible(nodeID) {
			continue
		}
		for _, event := range s.events {
			if !protocol.IsPermissionEvent(event.EventType) {
				continue
			}
			var payload protocol.PermissionEventPayload
			if err := json.Unmarshal(event.Payload, &payload); err != nil || payload.PermissionID == "" {
				continue
			}
			if event.EventType == protocol.EventPermissionResolved {
				delete(pending, payload.PermissionID)
				continue
			}
			view := protocol.PermissionView{
				PermissionID: payload.PermissionID,
				NodeID:       nodeID,
				AgentID:      firstNonEmpty(payload.AgentID, event.AgentID),
				SessionID:    firstNonEmpty(payload.SessionID, event.SessionID),
				Description:  payload.Description,
				Status:       protocol.PermissionPending,
				ExpiresAt:    payload.ExpiresAt,
				CreatedAt:    event.Timestamp,
			}
			pending[payload.PermissionID] = ordered{view: view}
		}
	}

	result := make([]protocol.PermissionView, 0, len(pending))
	for _, item := range pending {
		if item.view.ExpiresAt != nil && !item.view.ExpiresAt.After(now) {
			continue
		}
		result = append(result, item.view)
	}
	sort.Slice(result, func(i, j int) bool {
		if result[i].CreatedAt.Equal(result[j].CreatedAt) {
			return result[i].PermissionID < result[j].PermissionID
		}
		return result[i].CreatedAt.Before(result[j].CreatedAt)
	})
	return result
}

// Len returns the number of retained events after applying TTL expiry.
func (c *Cache) Len() int {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.pruneLocked(c.now().UTC())
	return c.total
}

// NodeSequences returns the latest sequence for a single Node.
func (c *Cache) NodeSequences(nodeID string) uint64 {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.pruneLocked(c.now().UTC())
	s := c.streams[nodeID]
	if s == nil {
		return 0
	}
	return s.next - 1
}

func (c *Cache) pruneLocked(now time.Time) {
	if c.ttl > 0 {
		cutoff := now.Add(-c.ttl)
		for _, s := range c.streams {
			remove := 0
			for remove < len(s.events) && s.events[remove].Timestamp.Before(cutoff) {
				remove++
			}
			if remove == 0 {
				continue
			}
			s.droppedThrough = s.events[remove-1].Sequence
			s.events = append([]protocol.AgentEvent(nil), s.events[remove:]...)
			c.total -= remove
		}
	}

	for c.total > c.capacity && len(c.order) > 0 {
		ref := c.order[0]
		c.order = c.order[1:]
		s := c.streams[ref.nodeID]
		if s == nil || ref.sequence <= s.droppedThrough || len(s.events) == 0 || s.events[0].Sequence != ref.sequence {
			continue
		}
		s.droppedThrough = ref.sequence
		s.events = s.events[1:]
		c.total--
	}

	// Drop stale order entries left behind by TTL expiry.
	for len(c.order) > 0 {
		ref := c.order[0]
		s := c.streams[ref.nodeID]
		if s != nil && ref.sequence > s.droppedThrough {
			break
		}
		c.order = c.order[1:]
	}
}

func firstNonEmpty(values ...string) string {
	for _, value := range values {
		if value != "" {
			return value
		}
	}
	return ""
}
