// Package eventhub is the Node-local event authority. It consumes normalized
// Adapter events once, assigns a monotonic Node sequence, keeps a bounded
// replay window and fans out to local clients and the optional Cloud relay.
package eventhub

import (
	"context"
	"encoding/json"
	"log/slog"
	"sync"
	"time"

	"github.com/FlyingBassHot/meetagent/internal/eventcache"
	"github.com/FlyingBassHot/meetagent/internal/node/adapter"
	"github.com/FlyingBassHot/meetagent/pkg/clientproto"
	"github.com/FlyingBassHot/meetagent/pkg/protocol"
)

// Source is the subset of adapter.Manager the hub needs.
type Source interface {
	Events() <-chan protocol.AgentEvent
	Agents() []protocol.AgentView
}

// ManifestSource is optional; managers expose adapter manifests for discovery.
type ManifestSource interface {
	Manifests() map[string]adapter.Manifest
}

// Hub owns the Node event sequence and subscriptions.
type Hub struct {
	nodeID        string
	nodeInfo      clientproto.NodeInfo
	source        Source
	cache         *eventcache.Cache
	logger        *slog.Logger
	permissionTTL time.Duration

	mu      sync.Mutex
	nextSub int
	v2      map[int]chan clientproto.Event
	raw     map[int]chan protocol.AgentEvent
}

// New creates a hub. The caller must run Run for the lifetime of the Node.
func New(nodeInfo clientproto.NodeInfo, source Source, capacity int, ttl time.Duration, logger *slog.Logger) *Hub {
	if logger == nil {
		logger = slog.Default()
	}
	if capacity <= 0 {
		capacity = 1000
	}
	return &Hub{
		nodeID:        nodeInfo.NodeID,
		nodeInfo:      nodeInfo,
		source:        source,
		cache:         eventcache.New(capacity, ttl),
		logger:        logger,
		permissionTTL: 10 * time.Minute,
		v2:            make(map[int]chan clientproto.Event),
		raw:           make(map[int]chan protocol.AgentEvent),
	}
}

// SetPermissionTTL configures the default expiry added to permission requests
// that do not provide one. Zero keeps the 10 minute default.
func (h *Hub) SetPermissionTTL(ttl time.Duration) {
	if ttl > 0 {
		h.permissionTTL = ttl
	}
}

// HasPendingPermission reports whether a permission ID is still pending and
// not expired in the current cache.
func (h *Hub) HasPendingPermission(permissionID string) bool {
	for _, permission := range h.cache.PendingPermissions(nil) {
		if permission.PermissionID == permissionID {
			return true
		}
	}
	return false
}

// Run consumes adapter events until ctx is canceled.
func (h *Hub) Run(ctx context.Context) {
	for {
		select {
		case <-ctx.Done():
			return
		case event, ok := <-h.source.Events():
			if !ok {
				return
			}
			h.publish(event)
		}
	}
}

func (h *Hub) publish(event protocol.AgentEvent) {
	event = h.stampPermission(event)
	event = h.cache.Append(h.nodeID, event)
	clientEvent := clientproto.EventFromAgent(h.nodeID, event)

	h.mu.Lock()
	defer h.mu.Unlock()
	for id, sub := range h.v2 {
		select {
		case sub <- clientEvent:
		default:
			h.logger.Debug("client event dropped by full buffer", "subscription", id, "event_id", event.EventID)
		}
	}
	for id, sub := range h.raw {
		select {
		case sub <- event:
		default:
			h.logger.Debug("raw event dropped by full buffer", "subscription", id, "event_id", event.EventID)
		}
	}
}

func (h *Hub) stampPermission(event protocol.AgentEvent) protocol.AgentEvent {
	if event.EventType != protocol.EventPermissionRequested || len(event.Payload) == 0 {
		return event
	}
	var payload map[string]any
	if err := json.Unmarshal(event.Payload, &payload); err != nil {
		return event
	}
	if value, ok := payload["expires_at"].(string); ok && value != "" {
		return event
	}
	payload["expires_at"] = time.Now().UTC().Add(h.permissionTTL).Format(time.RFC3339Nano)
	raw, err := json.Marshal(payload)
	if err != nil {
		return event
	}
	event.Payload = raw
	return event
}

// Snapshot builds the current full state.
func (h *Hub) Snapshot(snapshotEvents int) clientproto.Snapshot {
	events := h.cache.Recent(nil, snapshotEvents)
	clientEvents := make([]clientproto.Event, 0, len(events))
	for _, event := range events {
		clientEvents = append(clientEvents, clientproto.EventFromAgent(h.nodeID, event))
	}
	adapters := []clientproto.AdapterInfo(nil)
	if source, ok := h.source.(ManifestSource); ok {
		for _, manifest := range source.Manifests() {
			adapters = append(adapters, clientproto.AdapterInfo{
				ID:           manifest.ID,
				Name:         manifest.Name,
				Version:      manifest.Version,
				APIVersion:   manifest.APIVersion,
				Runtime:      manifest.Runtime,
				Capabilities: append([]string(nil), manifest.Capabilities...),
				Permissions:  append([]string(nil), manifest.Permissions...),
			})
		}
	}
	last := uint64(0)
	if sequences := h.cache.LatestSequences(nil); sequences != nil {
		last = sequences[h.nodeID]
	}
	return clientproto.Snapshot{
		SchemaVersion:      clientproto.Version,
		Node:               h.nodeInfo,
		Adapters:           adapters,
		Agents:             h.source.Agents(),
		Events:             clientEvents,
		LastSequence:       last,
		PendingPermissions: h.cache.PendingPermissions(nil),
		ServerTime:         time.Now().UTC(),
	}
}

// Subscribe registers a V2 event subscriber. It returns events missed after
// lastSequence (only when lastSequence > 0; a fresh client gets the Snapshot),
// a live channel and an unsubscribe function.
func (h *Hub) Subscribe(lastSequence uint64, buffer int) ([]clientproto.Event, <-chan clientproto.Event, func()) {
	if buffer <= 0 {
		buffer = 128
	}
	h.mu.Lock()
	defer h.mu.Unlock()

	channel := make(chan clientproto.Event, buffer)
	h.nextSub++
	id := h.nextSub
	h.v2[id] = channel

	var missed []clientproto.Event
	if lastSequence > 0 {
		events, _, _ := h.cache.Since(map[string]uint64{h.nodeID: lastSequence}, nil, 1000)
		missed = make([]clientproto.Event, 0, len(events))
		for _, event := range events {
			missed = append(missed, clientproto.EventFromAgent(h.nodeID, event))
		}
	}
	cancel := func() {
		h.mu.Lock()
		if current, ok := h.v2[id]; ok && current == channel {
			delete(h.v2, id)
			close(channel)
		}
		h.mu.Unlock()
	}
	return missed, channel, cancel
}

// SubscribeRaw registers a raw event subscriber for the optional Cloud relay.
func (h *Hub) SubscribeRaw(buffer int) (<-chan protocol.AgentEvent, func()) {
	if buffer <= 0 {
		buffer = 512
	}
	h.mu.Lock()
	defer h.mu.Unlock()
	channel := make(chan protocol.AgentEvent, buffer)
	h.nextSub++
	id := h.nextSub
	h.raw[id] = channel
	cancel := func() {
		h.mu.Lock()
		if current, ok := h.raw[id]; ok && current == channel {
			delete(h.raw, id)
			close(channel)
		}
		h.mu.Unlock()
	}
	return channel, cancel
}
