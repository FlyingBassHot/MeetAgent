package adapter

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"log/slog"
	"sort"
	"strings"
	"sync"
	"time"

	"github.com/FlyingBassHot/meetagent/pkg/protocol"
)

const (
	defaultRefreshInterval = 20 * time.Second
	connectTimeout         = 20 * time.Second
	refreshTimeout         = 15 * time.Second
	eventBuffer            = 512
	maxReconnectDelay      = 30 * time.Second
)

// Instance wires one configured Agent identity to an Adapter implementation.
type Instance struct {
	ID                  string
	Name                string
	Adapter             AgentAdapter
	StaticSessions      []Session
	CapabilityOverrides map[string]bool
}

type instance struct {
	id        string
	name      string
	adapter   AgentAdapter
	static    []Session
	caps      Capabilities
	refresh   time.Duration
	mu        sync.RWMutex
	status    Status
	sessions  []Session
	lastErr   string
	connected bool
	failures  int
}

// Manager owns every local Agent instance and implements the Adapter lifecycle.
// A failing adapter never takes down the Node or other adapters.
type Manager struct {
	nodeID    string
	logger    *slog.Logger
	events    chan protocol.AgentEvent
	updates   chan struct{}
	mu        sync.RWMutex
	instances []*instance
	byID      map[string]*instance
	started   bool
}

// NewManager creates an empty Adapter Manager.
func NewManager(nodeID string, logger *slog.Logger) *Manager {
	if logger == nil {
		logger = slog.Default()
	}
	return &Manager{
		nodeID:  nodeID,
		logger:  logger,
		events:  make(chan protocol.AgentEvent, eventBuffer),
		updates: make(chan struct{}, 1),
		byID:    make(map[string]*instance),
	}
}

// Add registers one Agent instance.
func (m *Manager) Add(cfg Instance) error {
	cfg.ID = strings.TrimSpace(cfg.ID)
	if cfg.ID == "" {
		return errors.New("agent id is required")
	}
	if cfg.Adapter == nil {
		return fmt.Errorf("agent %q has no adapter", cfg.ID)
	}
	if cfg.Name == "" {
		cfg.Name = cfg.ID
	}

	m.mu.Lock()
	defer m.mu.Unlock()
	if _, exists := m.byID[cfg.ID]; exists {
		return fmt.Errorf("duplicate agent id %q", cfg.ID)
	}
	caps := cfg.Adapter.Capabilities().Clone()
	for name, allowed := range cfg.CapabilityOverrides {
		if !allowed {
			caps[name] = false
		}
	}
	inst := &instance{
		id:       cfg.ID,
		name:     cfg.Name,
		adapter:  cfg.Adapter,
		static:   append([]Session(nil), cfg.StaticSessions...),
		caps:     caps,
		refresh:  defaultRefreshInterval,
		status:   StatusOffline,
		sessions: append([]Session(nil), cfg.StaticSessions...),
	}
	m.instances = append(m.instances, inst)
	m.byID[cfg.ID] = inst
	return nil
}

// Events returns normalized events for the Node connection to forward.
func (m *Manager) Events() <-chan protocol.AgentEvent { return m.events }

// Updates is signaled whenever the public Agent/Session inventory changes.
func (m *Manager) Updates() <-chan struct{} { return m.updates }

// Start launches each adapter lifecycle in its own goroutine.
func (m *Manager) Start(ctx context.Context) {
	m.mu.Lock()
	if m.started {
		m.mu.Unlock()
		return
	}
	m.started = true
	instances := append([]*instance(nil), m.instances...)
	m.mu.Unlock()

	for _, inst := range instances {
		go m.runInstance(ctx, inst)
	}
}

// Stop disconnects every adapter. Safe to call after ctx cancellation.
func (m *Manager) Stop(ctx context.Context) {
	m.mu.RLock()
	instances := append([]*instance(nil), m.instances...)
	m.mu.RUnlock()
	for _, inst := range instances {
		_ = inst.adapter.Disconnect(ctx)
	}
}

// Agents returns the live inventory in wire format.
func (m *Manager) Agents() []protocol.AgentView {
	m.mu.RLock()
	instances := append([]*instance(nil), m.instances...)
	m.mu.RUnlock()

	views := make([]protocol.AgentView, 0, len(instances))
	for _, inst := range instances {
		inst.mu.RLock()
		status := inst.status
		sessions := append([]Session(nil), inst.sessions...)
		caps := inst.caps.Clone()
		inst.mu.RUnlock()

		views = append(views, protocol.AgentView{
			AgentID:      inst.id,
			NodeID:       m.nodeID,
			Type:         inst.adapter.Type(),
			Name:         inst.name,
			Status:       string(status),
			Capabilities: caps,
			Sessions:     protocolSessions(m.nodeID, inst.id, sessions),
		})
	}
	return views
}

// Capabilities returns a copy of one Agent's effective capabilities.
func (m *Manager) Capabilities(agentID string) (Capabilities, bool) {
	inst, ok := m.instance(agentID)
	if !ok {
		return nil, false
	}
	inst.mu.RLock()
	defer inst.mu.RUnlock()
	return inst.caps.Clone(), true
}

// ListSessions refreshes and returns the sessions of one Agent.
func (m *Manager) ListSessions(ctx context.Context, agentID string) ([]protocol.SessionView, error) {
	inst, ok := m.instance(agentID)
	if !ok {
		return nil, fmt.Errorf("unknown agent %q", agentID)
	}
	if err := m.refresh(ctx, inst); err != nil {
		return nil, err
	}
	inst.mu.RLock()
	defer inst.mu.RUnlock()
	return protocolSessions(m.nodeID, inst.id, inst.sessions), nil
}

// SendPrompt forwards a prompt after local capability and target checks.
func (m *Manager) SendPrompt(ctx context.Context, agentID, sessionID, text string) (PromptResult, error) {
	inst, err := m.readyInstance(agentID, CapabilitySendPrompt)
	if err != nil {
		return PromptResult{}, err
	}
	if err := m.requireSession(ctx, inst, sessionID); err != nil {
		return PromptResult{}, err
	}
	result, err := inst.adapter.SendPrompt(ctx, sessionID, text)
	if err != nil {
		return PromptResult{}, err
	}
	inst.mu.Lock()
	updateSessionLocked(inst, sessionID, func(session *Session) {
		session.Status = "running"
		session.UpdatedAt = time.Now().UTC()
	})
	inst.mu.Unlock()
	m.notifyUpdate()
	m.recomputeStatus(inst)
	return result, nil
}

// Interrupt asks an Agent to stop the active execution of one session.
func (m *Manager) Interrupt(ctx context.Context, agentID, sessionID string) (InterruptResult, error) {
	inst, err := m.readyInstance(agentID, CapabilityInterrupt)
	if err != nil {
		return InterruptResult{}, err
	}
	if err := m.requireSession(ctx, inst, sessionID); err != nil {
		return InterruptResult{}, err
	}
	return inst.adapter.Interrupt(ctx, sessionID)
}

// RespondPermission forwards a permission decision when the adapter supports
// the optional permission capability.
func (m *Manager) RespondPermission(ctx context.Context, agentID, sessionID, permissionID, decision, message string) error {
	inst, err := m.readyInstance(agentID, CapabilityPermissionResponse)
	if err != nil {
		return err
	}
	responder, ok := inst.adapter.(PermissionResponder)
	if !ok {
		return ErrNotSupported
	}
	if err := m.requireSession(ctx, inst, sessionID); err != nil {
		return err
	}
	return responder.RespondPermission(ctx, sessionID, permissionID, decision, message)
}

func (m *Manager) runInstance(ctx context.Context, inst *instance) {
	delay := time.Second
	for {
		if ctx.Err() != nil {
			m.shutdownInstance(inst)
			return
		}
		m.setStatus(inst, StatusConnecting, "")

		connectCtx, cancel := context.WithTimeout(ctx, connectTimeout)
		err := inst.adapter.Connect(connectCtx)
		cancel()
		if err != nil {
			m.setStatus(inst, StatusError, err.Error())
			m.logger.Warn("agent adapter connect failed", "agent_id", inst.id, "type", inst.adapter.Type(), "error", err)
			if !wait(ctx, delay) {
				m.shutdownInstance(inst)
				return
			}
			delay = nextDelay(delay)
			continue
		}

		inst.mu.Lock()
		inst.connected = true
		inst.failures = 0
		inst.lastErr = ""
		inst.mu.Unlock()
		delay = time.Second
		m.setStatus(inst, StatusOnline, "")
		m.logger.Info("agent adapter connected", "agent_id", inst.id, "type", inst.adapter.Type())
		m.publish(inst, Event{
			Type: protocol.EventAgentConnected,
			Payload: map[string]any{
				"name":         inst.name,
				"type":         inst.adapter.Type(),
				"capabilities": inst.caps.Clone(),
			},
		})
		m.notifyUpdate()

		if err := m.refresh(ctx, inst); err != nil {
			m.logger.Debug("initial agent session refresh failed", "agent_id", inst.id, "error", err)
		}

		events, err := inst.adapter.SubscribeEvents(ctx)
		if err != nil {
			m.logger.Warn("agent event subscribe failed", "agent_id", inst.id, "error", err)
			m.setStatus(inst, StatusError, err.Error())
			_ = inst.adapter.Disconnect(context.Background())
			m.markDisconnected(inst)
			if !wait(ctx, delay) {
				m.shutdownInstance(inst)
				return
			}
			delay = nextDelay(delay)
			continue
		}

		finished := m.consumeEvents(ctx, inst, events)
		if ctx.Err() != nil {
			m.shutdownInstance(inst)
			return
		}
		if finished {
			m.logger.Warn("agent event stream closed", "agent_id", inst.id)
		}
		m.setStatus(inst, StatusError, "agent event stream ended")
		_ = inst.adapter.Disconnect(context.Background())
		m.markDisconnected(inst)
		if !wait(ctx, delay) {
			m.shutdownInstance(inst)
			return
		}
		delay = nextDelay(delay)
	}
}

func (m *Manager) consumeEvents(ctx context.Context, inst *instance, events <-chan Event) bool {
	refreshed := inst.refresh
	if refreshed <= 0 {
		refreshed = defaultRefreshInterval
	}
	ticker := time.NewTicker(refreshed)
	defer ticker.Stop()

	for {
		select {
		case <-ctx.Done():
			return false
		case event, ok := <-events:
			if !ok {
				return true
			}
			m.handleEvent(inst, event)
		case <-ticker.C:
			if err := m.refresh(ctx, inst); err != nil {
				m.logger.Debug("agent session refresh failed", "agent_id", inst.id, "error", err)
			}
		}
	}
}

func (m *Manager) handleEvent(inst *instance, event Event) {
	if event.Timestamp.IsZero() {
		event.Timestamp = time.Now().UTC()
	}
	changed := m.applyEventState(inst, event)
	if event.Type != "" {
		m.publish(inst, event)
	}
	if changed {
		m.notifyUpdate()
	}
	m.recomputeStatus(inst)
}

func (m *Manager) applyEventState(inst *instance, event Event) bool {
	inst.mu.Lock()
	defer inst.mu.Unlock()
	changed := false

	sessionPayload, _ := event.Payload.(SessionEventPayload)
	sessionID := event.SessionID
	if sessionID == "" {
		sessionID = sessionPayload.SessionID
	}
	if sessionID == "" {
		return false
	}

	switch event.Type {
	case protocol.EventSessionCreated:
		if !hasSessionLocked(inst, sessionID) {
			inst.sessions = append(inst.sessions, Session{
				ID:               sessionID,
				Title:            sessionPayload.Title,
				WorkingDirectory: sessionPayload.WorkingDirectory,
				Status:           firstNonEmpty(sessionPayload.Status, event.Status, "idle"),
				UpdatedAt:        event.Timestamp,
			})
			changed = true
		}
	case protocol.EventSessionDeleted:
		filtered := inst.sessions[:0]
		for _, session := range inst.sessions {
			if session.ID != sessionID {
				filtered = append(filtered, session)
			}
		}
		changed = len(filtered) != len(inst.sessions)
		inst.sessions = filtered
	case protocol.EventSessionUpdated, protocol.EventSessionStatusChange:
		status := firstNonEmpty(event.Status, sessionPayload.Status)
		changed = updateSessionLocked(inst, sessionID, func(session *Session) {
			if sessionPayload.Title != "" {
				session.Title = sessionPayload.Title
			}
			if sessionPayload.WorkingDirectory != "" {
				session.WorkingDirectory = sessionPayload.WorkingDirectory
			}
			if status != "" && session.Status != status {
				session.Status = status
				session.UpdatedAt = event.Timestamp
				changed = true
			}
		})
	}

	return changed
}

func (m *Manager) refresh(ctx context.Context, inst *instance) error {
	refreshCtx, cancel := context.WithTimeout(ctx, refreshTimeout)
	sessions, err := inst.adapter.ListSessions(refreshCtx)
	cancel()
	if err != nil {
		inst.mu.Lock()
		inst.failures++
		failures := inst.failures
		connected := inst.connected
		inst.lastErr = err.Error()
		inst.mu.Unlock()
		if connected && failures >= 3 {
			m.setStatus(inst, StatusError, err.Error())
		}
		return err
	}

	inst.mu.Lock()
	old := append([]Session(nil), inst.sessions...)
	inst.sessions = mergeSessions(inst.static, sessions)
	inst.failures = 0
	inst.lastErr = ""
	connected := inst.connected
	newSessions := append([]Session(nil), inst.sessions...)
	inst.mu.Unlock()

	if connected {
		m.setStatus(inst, StatusOnline, "")
	}
	changed := emitSessionDiff(m, inst, old, newSessions)
	if changed {
		m.notifyUpdate()
	}
	m.recomputeStatus(inst)
	return nil
}

func emitSessionDiff(m *Manager, inst *instance, old, next []Session) bool {
	oldByID := make(map[string]Session, len(old))
	for _, session := range old {
		oldByID[session.ID] = session
	}
	nextByID := make(map[string]Session, len(next))
	changed := false
	now := time.Now().UTC()

	for _, session := range next {
		nextByID[session.ID] = session
		previous, existed := oldByID[session.ID]
		switch {
		case !existed:
			m.publish(inst, Event{
				Type:      protocol.EventSessionCreated,
				SessionID: session.ID,
				Timestamp: nonzeroTime(session.UpdatedAt, now),
				Payload: SessionEventPayload{
					SessionID:        session.ID,
					Title:            session.Title,
					WorkingDirectory: session.WorkingDirectory,
					Status:           session.Status,
					UpdatedAt:        session.UpdatedAt,
				},
			})
			changed = true
		case previous.Status != session.Status:
			m.publish(inst, Event{
				Type:      protocol.EventSessionStatusChange,
				SessionID: session.ID,
				Status:    session.Status,
				Timestamp: now,
				Payload: SessionEventPayload{
					SessionID: session.ID,
					Status:    session.Status,
				},
			})
			changed = true
		case previous.Title != session.Title || previous.WorkingDirectory != session.WorkingDirectory:
			m.publish(inst, Event{
				Type:      protocol.EventSessionUpdated,
				SessionID: session.ID,
				Timestamp: now,
				Payload: SessionEventPayload{
					SessionID:        session.ID,
					Title:            session.Title,
					WorkingDirectory: session.WorkingDirectory,
					Status:           session.Status,
				},
			})
			changed = true
		}
	}
	for _, session := range old {
		if _, exists := nextByID[session.ID]; !exists {
			m.publish(inst, Event{
				Type:      protocol.EventSessionDeleted,
				SessionID: session.ID,
				Timestamp: now,
				Payload: SessionEventPayload{
					SessionID: session.ID,
					Status:    "deleted",
				},
			})
			changed = true
		}
	}
	return changed
}

func (m *Manager) recomputeStatus(inst *instance) {
	inst.mu.RLock()
	connected := inst.connected
	busy := false
	for _, session := range inst.sessions {
		if session.Status == "running" || session.Status == "busy" {
			busy = true
			break
		}
	}
	inst.mu.RUnlock()
	if !connected {
		return
	}
	if busy {
		m.setStatus(inst, StatusBusy, "")
		return
	}
	m.setStatus(inst, StatusOnline, "")
}

func (m *Manager) setStatus(inst *instance, status Status, errMessage string) {
	inst.mu.Lock()
	previous := inst.status
	changed := previous != status
	inst.status = status
	if errMessage != "" {
		inst.lastErr = errMessage
	}
	inst.mu.Unlock()
	if !changed {
		return
	}
	m.notifyUpdate()
	payload := map[string]any{"status": string(status), "previous": string(previous)}
	if errMessage != "" {
		payload["error"] = errMessage
	}
	m.publish(inst, Event{
		Type:    protocol.EventAgentStatusChanged,
		Status:  string(status),
		Payload: payload,
	})
}

func (m *Manager) markDisconnected(inst *instance) {
	inst.mu.Lock()
	inst.connected = false
	inst.mu.Unlock()
	m.setStatus(inst, StatusError, "")
	m.publish(inst, Event{
		Type:    protocol.EventAgentDisconnected,
		Payload: map[string]any{"name": inst.name},
	})
	m.notifyUpdate()
}

func (m *Manager) shutdownInstance(inst *instance) {
	_ = inst.adapter.Disconnect(context.Background())
	inst.mu.Lock()
	inst.connected = false
	previous := inst.status
	inst.status = StatusOffline
	inst.mu.Unlock()
	if previous != StatusOffline {
		m.notifyUpdate()
		m.publish(inst, Event{
			Type:    protocol.EventAgentDisconnected,
			Payload: map[string]any{"name": inst.name},
		})
	}
}

func (m *Manager) publish(inst *instance, event Event) {
	if event.Type == "" {
		return
	}
	wire := protocol.AgentEvent{
		EventID:   protocol.NewID("evt"),
		NodeID:    m.nodeID,
		AgentID:   inst.id,
		SessionID: event.SessionID,
		EventType: event.Type,
		Timestamp: nonzeroTime(event.Timestamp, time.Now().UTC()),
	}
	if event.Payload != nil {
		raw, err := json.Marshal(event.Payload)
		if err != nil {
			m.logger.Warn("encode adapter event payload", "agent_id", inst.id, "event_type", event.Type, "error", err)
			return
		}
		wire.Payload = raw
	}
	select {
	case m.events <- wire:
	default:
		m.logger.Warn("adapter event dropped by full buffer", "agent_id", inst.id, "event_type", event.Type)
	}
}

func (m *Manager) notifyUpdate() {
	select {
	case m.updates <- struct{}{}:
	default:
	}
}

func (m *Manager) instance(agentID string) (*instance, bool) {
	m.mu.RLock()
	defer m.mu.RUnlock()
	inst, ok := m.byID[agentID]
	return inst, ok
}

func (m *Manager) readyInstance(agentID, capability string) (*instance, error) {
	inst, ok := m.instance(agentID)
	if !ok {
		return nil, fmt.Errorf("unknown agent %q", agentID)
	}
	inst.mu.RLock()
	connected := inst.connected
	enabled := inst.caps.Has(capability)
	inst.mu.RUnlock()
	if !connected {
		return nil, fmt.Errorf("%w: agent %q", ErrUnavailable, agentID)
	}
	if !enabled {
		return nil, fmt.Errorf("%w: agent %q does not support %s", ErrNotSupported, agentID, capability)
	}
	return inst, nil
}

func (m *Manager) requireSession(ctx context.Context, inst *instance, sessionID string) error {
	if sessionID == "" {
		return ErrSessionNotFound
	}
	if sessionKnown(inst, sessionID) {
		return nil
	}
	if err := m.refresh(ctx, inst); err == nil && sessionKnown(inst, sessionID) {
		return nil
	}
	return fmt.Errorf("%w: %s", ErrSessionNotFound, sessionID)
}

func sessionKnown(inst *instance, sessionID string) bool {
	inst.mu.RLock()
	defer inst.mu.RUnlock()
	return hasSessionLocked(inst, sessionID)
}

func hasSessionLocked(inst *instance, sessionID string) bool {
	for _, session := range inst.sessions {
		if session.ID == sessionID {
			return true
		}
	}
	return false
}

// updateSessionLocked mutates a session and reports whether it existed.
func updateSessionLocked(inst *instance, sessionID string, mutate func(*Session)) bool {
	for index := range inst.sessions {
		if inst.sessions[index].ID == sessionID {
			mutate(&inst.sessions[index])
			return true
		}
	}
	return false
}

func protocolSessions(nodeID, agentID string, sessions []Session) []protocol.SessionView {
	views := make([]protocol.SessionView, 0, len(sessions))
	ordered := append([]Session(nil), sessions...)
	sort.SliceStable(ordered, func(i, j int) bool {
		if ordered[i].UpdatedAt.Equal(ordered[j].UpdatedAt) {
			return ordered[i].ID < ordered[j].ID
		}
		return ordered[i].UpdatedAt.After(ordered[j].UpdatedAt)
	})
	for _, session := range ordered {
		views = append(views, ProtocolSession(nodeID, agentID, session))
	}
	return views
}

func mergeSessions(static, live []Session) []Session {
	if len(live) == 0 {
		return append([]Session(nil), static...)
	}
	seen := make(map[string]bool, len(live))
	result := make([]Session, 0, len(live)+len(static))
	for _, session := range live {
		if session.ID == "" || seen[session.ID] {
			continue
		}
		seen[session.ID] = true
		if session.Status == "" {
			session.Status = "unknown"
		}
		if session.UpdatedAt.IsZero() {
			session.UpdatedAt = time.Now().UTC()
		}
		result = append(result, session)
	}
	for _, session := range static {
		if _, exists := seen[session.ID]; !exists {
			result = append(result, session)
		}
	}
	return result
}

func wait(ctx context.Context, delay time.Duration) bool {
	timer := time.NewTimer(delay)
	defer timer.Stop()
	select {
	case <-ctx.Done():
		return false
	case <-timer.C:
		return true
	}
}

func nextDelay(current time.Duration) time.Duration {
	next := current * 2
	if next > maxReconnectDelay {
		return maxReconnectDelay
	}
	if next < time.Second {
		return time.Second
	}
	return next
}

func nonzeroTime(value time.Time, fallback time.Time) time.Time {
	if value.IsZero() {
		return fallback
	}
	return value
}

func firstNonEmpty(values ...string) string {
	for _, value := range values {
		if value != "" {
			return value
		}
	}
	return ""
}
