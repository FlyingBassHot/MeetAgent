package connection

import (
	"context"
	"errors"
	"log/slog"
	"sort"
	"sync"
	"time"

	"github.com/FlyingBassHot/meetagent/pkg/protocol"
)

var (
	ErrNodeNotFound    = errors.New("node not found")
	ErrNodeOffline     = errors.New("node offline")
	ErrDeviceNotFound  = errors.New("device not found")
	ErrDeviceOffline   = errors.New("device offline")
	ErrDuplicateReq    = errors.New("duplicate request_id")
	ErrTooManyPending  = errors.New("too many pending requests")
	ErrUnknownRequest  = errors.New("unknown or expired request_id")
	ErrRequestNodeMiss = errors.New("request belongs to another node")
)

// PendingRequest tracks a command forwarded to a Node so the response can be
// routed back to the originating Device.
type PendingRequest struct {
	RequestID string    `json:"request_id"`
	DeviceID  string    `json:"device_id"`
	NodeID    string    `json:"node_id"`
	Action    string    `json:"action"`
	CreatedAt time.Time `json:"created_at"`
	ExpiresAt time.Time `json:"expires_at"`
}

type nodeEntry struct {
	client *Client
	view   protocol.NodeView
}

// Disconnect describes what was removed when a Client closed.
type Disconnect struct {
	Identity protocol.Source
	Node     *protocol.NodeView
}

// Manager maintains online Nodes, Devices and in-flight requests.
type Manager struct {
	mu          sync.RWMutex
	nodes       map[string]*nodeEntry
	nodeHistory map[string]protocol.NodeView
	devices     map[string]*Client
	pending     map[string]PendingRequest

	pendingTTL time.Duration
	maxPending int
	logger     *slog.Logger
}

// NewManager creates an empty connection manager.
func NewManager(pendingTTL time.Duration, maxPending int, logger *slog.Logger) *Manager {
	if pendingTTL <= 0 {
		pendingTTL = 2 * time.Minute
	}
	if maxPending <= 0 {
		maxPending = 10000
	}
	return &Manager{
		nodes:       make(map[string]*nodeEntry),
		nodeHistory: make(map[string]protocol.NodeView),
		devices:     make(map[string]*Client),
		pending:     make(map[string]PendingRequest),
		pendingTTL:  pendingTTL,
		maxPending:  maxPending,
		logger:      logger,
	}
}

// RegisterNode stores a Node connection and returns the replaced connection, if
// the same identity was already online.
func (m *Manager) RegisterNode(c *Client, info protocol.NodeView) *Client {
	now := time.Now().UTC()
	if info.Agents == nil {
		info.Agents = []protocol.AgentView{}
	}
	info.Status = "online"
	info.ConnectedAt = timePtr(c.ConnectedAt())
	info.LastSeen = timePtr(now)
	info.RemoteAddr = c.RemoteAddr()

	m.mu.Lock()
	old := m.nodes[info.NodeID]
	m.nodes[info.NodeID] = &nodeEntry{client: c, view: info}
	delete(m.nodeHistory, info.NodeID)
	m.mu.Unlock()

	if old != nil && old.client != c {
		m.logger.Warn("replacing existing node connection", "node_id", info.NodeID, "old_connection_id", old.client.ID, "new_connection_id", c.ID)
		old.client.Close(4001, "replaced by a newer connection")
	}
	return oldClient(old)
}

// RegisterDeviceIdentity stores a Device by its authenticated identity and
// returns the replaced connection, if any.
func (m *Manager) RegisterDeviceIdentity(c *Client, deviceID string) *Client {
	m.mu.Lock()
	old := m.devices[deviceID]
	m.devices[deviceID] = c
	m.mu.Unlock()

	if old != nil && old != c {
		m.logger.Warn("replacing existing device connection", "device_id", deviceID, "old_connection_id", old.ID, "new_connection_id", c.ID)
		old.Close(4001, "replaced by a newer connection")
	}
	return old
}

// Unregister removes a closed connection. It only removes the node entry if the
// identity still points at this exact connection, which prevents a replaced
// connection from removing its successor.
func (m *Manager) Unregister(c *Client) Disconnect {
	identity, ok := c.Registered()
	if !ok {
		return Disconnect{}
	}

	result := Disconnect{Identity: identity}
	m.mu.Lock()
	switch identity.Type {
	case protocol.PeerNode:
		entry, exists := m.nodes[identity.ID]
		if exists && entry.client == c {
			view := cloneView(entry.view)
			view.Status = "offline"
			view.LastSeen = timePtr(c.LastSeen())
			m.nodeHistory[identity.ID] = view
			delete(m.nodes, identity.ID)
			result.Node = &view
		}
	case protocol.PeerDevice:
		if current, exists := m.devices[identity.ID]; exists && current == c {
			delete(m.devices, identity.ID)
		}
	}
	m.mu.Unlock()
	return result
}

// UpdateNodeAgents replaces a live Node's Agent and Session inventory. It only
// affects the exact active connection so a stale sync cannot overwrite a newer
// registration. It returns the updated view.
func (m *Manager) UpdateNodeAgents(nodeID string, agents []protocol.AgentView) (protocol.NodeView, bool) {
	m.mu.Lock()
	defer m.mu.Unlock()
	entry, exists := m.nodes[nodeID]
	if !exists {
		return protocol.NodeView{}, false
	}
	entry.view.Agents = agents
	entry.view.LastSeen = timePtr(entry.client.LastSeen())
	return cloneView(entry.view), true
}

// Touch updates the last-seen time of a Node's snapshot entry.
func (m *Manager) Touch(c *Client) {
	identity, ok := c.Registered()
	if !ok || identity.Type != protocol.PeerNode {
		return
	}
	c.Touch()
	m.mu.Lock()
	if entry, exists := m.nodes[identity.ID]; exists && entry.client == c {
		entry.view.LastSeen = timePtr(c.LastSeen())
	}
	m.mu.Unlock()
}

// SendToNode queues an envelope to an online Node.
func (m *Manager) SendToNode(nodeID string, env *protocol.Envelope) error {
	m.mu.RLock()
	entry, ok := m.nodes[nodeID]
	_, known := m.nodeHistory[nodeID]
	m.mu.RUnlock()
	if !ok {
		if known {
			return ErrNodeOffline
		}
		return ErrNodeNotFound
	}
	return entry.client.Send(env)
}

// SendToDevice queues an envelope to an online Device.
func (m *Manager) SendToDevice(deviceID string, env *protocol.Envelope) error {
	m.mu.RLock()
	client, ok := m.devices[deviceID]
	m.mu.RUnlock()
	if !ok {
		return ErrDeviceOffline
	}
	return client.Send(env)
}

// BroadcastToDevices queues an envelope to every online Device and returns the
// number of devices it was sent to.
func (m *Manager) BroadcastToDevices(env *protocol.Envelope) int {
	return m.BroadcastToDevicesFiltered(env, nil)
}

// BroadcastToDevicesFiltered queues an envelope only to Devices accepted by
// allow. A nil allow means every online Device.
func (m *Manager) BroadcastToDevicesFiltered(env *protocol.Envelope, allow func(deviceID string) bool) int {
	m.mu.RLock()
	clients := make([]*Client, 0, len(m.devices))
	ids := make([]string, 0, len(m.devices))
	for id, client := range m.devices {
		if allow != nil && !allow(id) {
			continue
		}
		ids = append(ids, id)
		clients = append(clients, client)
	}
	m.mu.RUnlock()

	sent := 0
	for i, client := range clients {
		if err := client.Send(env); err == nil {
			sent++
		} else {
			m.logger.Debug("broadcast send failed", "device_id", ids[i], "error", err)
		}
	}
	return sent
}

// Snapshot returns a stable copy of all known Nodes, including offline history.
func (m *Manager) Snapshot() protocol.Snapshot {
	return m.SnapshotFor(nil)
}

// SnapshotFor returns a stable copy of known Nodes accepted by visible. A nil
// visible function returns every Node.
func (m *Manager) SnapshotFor(visible func(nodeID string) bool) protocol.Snapshot {
	m.mu.RLock()
	views := make([]protocol.NodeView, 0, len(m.nodes)+len(m.nodeHistory))
	for id, view := range m.nodeHistory {
		if visible == nil || visible(id) {
			views = append(views, cloneView(view))
		}
	}
	for id, entry := range m.nodes {
		if visible == nil || visible(id) {
			views = append(views, cloneView(entry.view))
		}
	}
	m.mu.RUnlock()

	sort.Slice(views, func(i, j int) bool {
		if views[i].Name == views[j].Name {
			return views[i].NodeID < views[j].NodeID
		}
		return views[i].Name < views[j].Name
	})

	return protocol.Snapshot{
		Nodes:      views,
		ServerTime: time.Now().UTC(),
	}
}

// NodeView returns a copy of the current or historical view of one Node.
func (m *Manager) NodeView(nodeID string) (protocol.NodeView, bool) {
	m.mu.RLock()
	defer m.mu.RUnlock()
	if entry, ok := m.nodes[nodeID]; ok {
		return cloneView(entry.view), true
	}
	if view, ok := m.nodeHistory[nodeID]; ok {
		return cloneView(view), true
	}
	return protocol.NodeView{}, false
}

// DeviceIDs returns the identities of all online Devices.
func (m *Manager) DeviceIDs() []string {
	m.mu.RLock()
	defer m.mu.RUnlock()
	ids := make([]string, 0, len(m.devices))
	for id := range m.devices {
		ids = append(ids, id)
	}
	sort.Strings(ids)
	return ids
}

// DeviceViews returns a copy of the online Device registry.
func (m *Manager) DeviceViews() []protocol.DeviceView {
	m.mu.RLock()
	defer m.mu.RUnlock()
	views := make([]protocol.DeviceView, 0, len(m.devices))
	for id, client := range m.devices {
		connectedAt := client.ConnectedAt()
		lastSeen := client.LastSeen()
		views = append(views, protocol.DeviceView{
			DeviceID:    id,
			Status:      "online",
			ConnectedAt: &connectedAt,
			LastSeen:    &lastSeen,
			RemoteAddr:  client.RemoteAddr(),
		})
	}
	sort.Slice(views, func(i, j int) bool { return views[i].DeviceID < views[j].DeviceID })
	return views
}

// AddPending reserves a request_id and associates it with the target Node.
func (m *Manager) AddPending(p PendingRequest) error {
	now := time.Now().UTC()
	if p.CreatedAt.IsZero() {
		p.CreatedAt = now
	}
	if p.ExpiresAt.IsZero() {
		p.ExpiresAt = now.Add(m.pendingTTL)
	}

	m.mu.Lock()
	defer m.mu.Unlock()

	if existing, ok := m.pending[p.RequestID]; ok {
		if existing.ExpiresAt.After(now) {
			return ErrDuplicateReq
		}
		delete(m.pending, p.RequestID)
	}
	if len(m.pending) >= m.maxPending {
		return ErrTooManyPending
	}
	m.pending[p.RequestID] = p
	return nil
}

// GetPending returns a non-expired pending request.
func (m *Manager) GetPending(requestID string) (PendingRequest, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	return m.getPendingLocked(requestID)
}

// ConsumePending returns and removes a pending request after validating that it
// belongs to the responding Node.
func (m *Manager) ConsumePending(requestID, nodeID string) (PendingRequest, error) {
	m.mu.Lock()
	defer m.mu.Unlock()

	pending, err := m.getPendingLocked(requestID)
	if err != nil {
		return PendingRequest{}, err
	}
	if pending.NodeID != nodeID {
		return PendingRequest{}, ErrRequestNodeMiss
	}
	delete(m.pending, requestID)
	return pending, nil
}

// DropPending removes a request, for example when forwarding failed.
func (m *Manager) DropPending(requestID string) {
	m.mu.Lock()
	delete(m.pending, requestID)
	m.mu.Unlock()
}

// TakePendingForNode removes and returns every in-flight request targeting a
// Node. It is used when that Node disconnects so Devices get a final status.
func (m *Manager) TakePendingForNode(nodeID string) []PendingRequest {
	m.mu.Lock()
	defer m.mu.Unlock()
	var result []PendingRequest
	for requestID, pending := range m.pending {
		if pending.NodeID != nodeID {
			continue
		}
		result = append(result, pending)
		delete(m.pending, requestID)
	}
	return result
}

// PendingCount returns the number of in-flight requests.
func (m *Manager) PendingCount() int {
	m.mu.RLock()
	defer m.mu.RUnlock()
	return len(m.pending)
}

// NodeCounts returns online and total known Node counts.
func (m *Manager) NodeCounts() (online int, total int) {
	m.mu.RLock()
	defer m.mu.RUnlock()
	return len(m.nodes), len(m.nodes) + len(m.nodeHistory)
}

// DeviceCount returns the number of online Devices.
func (m *Manager) DeviceCount() int {
	m.mu.RLock()
	defer m.mu.RUnlock()
	return len(m.devices)
}

// NodeOnline reports whether a Node identity is currently connected.
func (m *Manager) NodeOnline(nodeID string) bool {
	m.mu.RLock()
	defer m.mu.RUnlock()
	_, ok := m.nodes[nodeID]
	return ok
}

// StartJanitor periodically removes expired pending requests.
func (m *Manager) StartJanitor(ctx context.Context, interval time.Duration) {
	if interval <= 0 {
		interval = time.Minute
	}
	go func() {
		ticker := time.NewTicker(interval)
		defer ticker.Stop()
		for {
			select {
			case <-ctx.Done():
				return
			case <-ticker.C:
				removed := m.prunePending()
				if removed > 0 {
					m.logger.Debug("pruned expired pending requests", "count", removed)
				}
			}
		}
	}()
}

// CloseAll closes every connection. Used during graceful shutdown.
func (m *Manager) CloseAll() {
	m.mu.Lock()
	clients := make([]*Client, 0, len(m.nodes)+len(m.devices))
	for _, entry := range m.nodes {
		clients = append(clients, entry.client)
	}
	for _, client := range m.devices {
		clients = append(clients, client)
	}
	m.nodes = make(map[string]*nodeEntry)
	m.devices = make(map[string]*Client)
	m.mu.Unlock()

	for _, client := range clients {
		client.Close(1001, "server shutting down")
	}
}

func (m *Manager) getPendingLocked(requestID string) (PendingRequest, error) {
	pending, ok := m.pending[requestID]
	if !ok {
		return PendingRequest{}, ErrUnknownRequest
	}
	if time.Now().After(pending.ExpiresAt) {
		delete(m.pending, requestID)
		return PendingRequest{}, ErrUnknownRequest
	}
	return pending, nil
}

func (m *Manager) prunePending() int {
	now := time.Now()
	m.mu.Lock()
	defer m.mu.Unlock()
	removed := 0
	for id, pending := range m.pending {
		if now.After(pending.ExpiresAt) {
			delete(m.pending, id)
			removed++
		}
	}
	return removed
}

func timePtr(t time.Time) *time.Time { return &t }

func oldClient(entry *nodeEntry) *Client {
	if entry == nil {
		return nil
	}
	return entry.client
}

func cloneView(view protocol.NodeView) protocol.NodeView {
	agents := make([]protocol.AgentView, len(view.Agents))
	for i, agent := range view.Agents {
		agents[i] = cloneAgent(agent)
	}
	view.Agents = agents
	return view
}

func cloneAgent(agent protocol.AgentView) protocol.AgentView {
	if agent.Capabilities != nil {
		capabilities := make(map[string]bool, len(agent.Capabilities))
		for key, value := range agent.Capabilities {
			capabilities[key] = value
		}
		agent.Capabilities = capabilities
	}
	agent.Sessions = append([]protocol.SessionView(nil), agent.Sessions...)
	if agent.Sessions == nil {
		agent.Sessions = []protocol.SessionView{}
	}
	return agent
}
