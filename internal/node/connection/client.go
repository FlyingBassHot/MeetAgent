// Package connection implements the Node's outbound Cloud client, including
// registration, heartbeat, command execution and exponential reconnect.
package connection

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"log/slog"
	"math/rand"
	"os"
	"runtime"
	"sync"
	"time"

	"github.com/FlyingBassHot/meetagent/internal/node/adapter"
	"github.com/FlyingBassHot/meetagent/internal/node/command"
	"github.com/FlyingBassHot/meetagent/internal/node/config"
	"github.com/FlyingBassHot/meetagent/pkg/protocol"
	"github.com/gorilla/websocket"
)

const writeTimeout = 10 * time.Second

// Client maintains a resilient WebSocket session to Cloud.
type Client struct {
	cfg      config.Config
	logger   *slog.Logger
	executor *command.Executor
	manager  *adapter.Manager
	hostname string
	platform string

	writeMu sync.Mutex
}

// New creates a Node Cloud client backed by the local Adapter Manager.
func New(cfg config.Config, manager *adapter.Manager, logger *slog.Logger) *Client {
	hostname, _ := os.Hostname()
	return &Client{
		cfg:      cfg,
		logger:   logger,
		executor: command.New(cfg.Node.ID, cfg.Node.Name, cfg, manager),
		manager:  manager,
		hostname: hostname,
		platform: runtime.GOOS + "/" + runtime.GOARCH,
	}
}

// Run connects and reconnects until ctx is canceled.
func (c *Client) Run(ctx context.Context) error {
	delay := c.cfg.Cloud.Reconnect.InitialDelay.Std()

	for {
		started := time.Now()
		err := c.runSession(ctx)
		if ctx.Err() != nil {
			c.logger.Info("node client stopped")
			return nil
		}
		if err == nil {
			err = errors.New("connection closed by cloud")
		}
		c.logger.Warn("cloud session ended", "error", err)

		// A long-lived session indicates a healthy path; start backoff over.
		if time.Since(started) >= 3*c.cfg.Cloud.HeartbeatInterval.Std() {
			delay = c.cfg.Cloud.Reconnect.InitialDelay.Std()
		}
		wait := jittered(delay, c.cfg.Cloud.Reconnect.Jitter)
		c.logger.Info("reconnecting to cloud", "delay", wait.Round(time.Millisecond))

		select {
		case <-ctx.Done():
			return nil
		case <-time.After(wait):
		}

		delay = time.Duration(float64(delay) * c.cfg.Cloud.Reconnect.Multiplier)
		if delay > c.cfg.Cloud.Reconnect.MaxDelay.Std() {
			delay = c.cfg.Cloud.Reconnect.MaxDelay.Std()
		}
	}
}

func (c *Client) runSession(ctx context.Context) error {
	dialer := *websocket.DefaultDialer
	dialer.HandshakeTimeout = c.cfg.Cloud.RegisterTimeout.Std()

	c.logger.Info("connecting to cloud", "url", c.cfg.Cloud.URL, "node_id", c.cfg.Node.ID)
	conn, resp, err := dialer.DialContext(ctx, c.cfg.Cloud.URL, nil)
	if resp != nil && resp.Body != nil {
		_ = resp.Body.Close()
	}
	if err != nil {
		return fmt.Errorf("dial cloud: %w", err)
	}
	defer conn.Close()

	conn.SetReadLimit(protocol.MaxMessageSize)

	sessionCtx, cancel := context.WithCancel(ctx)
	defer cancel()
	finished := make(chan struct{})
	defer close(finished)
	go func() {
		select {
		case <-sessionCtx.Done():
			_ = conn.Close()
		case <-finished:
		}
	}()

	conn.SetPingHandler(func(appData string) error {
		_ = conn.SetReadDeadline(time.Now().Add(c.cfg.Cloud.ReadTimeout.Std()))
		return conn.WriteControl(websocket.PongMessage, []byte(appData), time.Now().Add(writeTimeout))
	})

	online, err := c.register(conn)
	if err != nil {
		return err
	}
	c.logger.Info("node online", "node_id", c.cfg.Node.ID, "connection_id", online.ConnectionID)
	_ = conn.SetReadDeadline(time.Now().Add(c.cfg.Cloud.ReadTimeout.Std()))

	if err := c.announceState(conn); err != nil {
		return fmt.Errorf("announce agent state: %w", err)
	}

	var wg sync.WaitGroup
	wg.Add(3)
	go func() {
		defer wg.Done()
		c.heartbeatLoop(sessionCtx, conn)
	}()
	go func() {
		defer wg.Done()
		c.eventLoop(sessionCtx, conn)
	}()
	go func() {
		defer wg.Done()
		c.stateLoop(sessionCtx, conn)
	}()
	defer func() {
		cancel()
		wg.Wait()
	}()

	return c.readLoop(sessionCtx, conn)
}

func (c *Client) register(conn *websocket.Conn) (protocol.RegisterResult, error) {
	env, err := protocol.NewWithPayload(protocol.TypeRegister, protocol.ActionRegister, protocol.Source{
		Type: protocol.PeerNode,
		ID:   c.cfg.Node.ID,
	}, protocol.RegisterPayload{
		Token:    c.cfg.Node.Token,
		Name:     c.cfg.Node.Name,
		Hostname: c.hostname,
		Platform: c.platform,
		Version:  command.Version,
		Agents:   c.agents(),
	})
	if err != nil {
		return protocol.RegisterResult{}, err
	}
	env.WithRequestID(protocol.NewID("req"))
	requestID := env.RequestID

	_ = conn.SetReadDeadline(time.Now().Add(c.cfg.Cloud.RegisterTimeout.Std()))
	if err := c.write(conn, env); err != nil {
		return protocol.RegisterResult{}, fmt.Errorf("send register: %w", err)
	}

	for {
		_, raw, err := conn.ReadMessage()
		if err != nil {
			return protocol.RegisterResult{}, fmt.Errorf("read register response: %w", err)
		}

		var reply protocol.Envelope
		if err := json.Unmarshal(raw, &reply); err != nil {
			return protocol.RegisterResult{}, fmt.Errorf("decode register response: %w", err)
		}
		if err := reply.Validate(); err != nil {
			return protocol.RegisterResult{}, fmt.Errorf("invalid register response: %w", err)
		}
		if reply.Type == protocol.TypeError {
			var payload protocol.ErrorPayload
			_ = reply.DecodePayload(&payload)
			return protocol.RegisterResult{}, fmt.Errorf("registration error: %s (%s)", payload.Message, payload.Code)
		}
		if reply.Type != protocol.TypeResponse || reply.Action != protocol.ActionRegister {
			c.logger.Debug("ignoring message while registering", "type", reply.Type, "action", reply.Action)
			continue
		}
		if reply.RequestID != "" && reply.RequestID != requestID {
			c.logger.Debug("ignoring unrelated register response", "request_id", reply.RequestID, "expected", requestID)
			continue
		}

		var result protocol.RegisterResult
		if err := reply.DecodePayload(&result); err != nil {
			return protocol.RegisterResult{}, fmt.Errorf("decode register result: %w", err)
		}
		if !result.OK {
			return protocol.RegisterResult{}, fmt.Errorf("registration rejected: %s (%s)", result.Error, result.ErrorCode)
		}
		if result.Identity.ID != "" && result.Identity.ID != c.cfg.Node.ID {
			return protocol.RegisterResult{}, fmt.Errorf("cloud registered unexpected identity %q", result.Identity.ID)
		}
		return result, nil
	}
}

// agents returns the live adapter inventory, falling back to configured agents
// when no Manager is present (tests and Phase 1 compatibility).
func (c *Client) agents() []protocol.AgentView {
	if c.manager != nil {
		return c.manager.Agents()
	}
	return c.cfg.ProtocolAgents()
}

// announceState publishes the current Agent/Session inventory right after
// registration so Cloud can replace any stale view from a previous session.
func (c *Client) announceState(conn *websocket.Conn) error {
	return c.sendAgentState(conn)
}

// eventLoop forwards normalized Adapter events to Cloud until the session ends.
func (c *Client) eventLoop(ctx context.Context, conn *websocket.Conn) {
	if c.manager == nil {
		return
	}
	for {
		select {
		case <-ctx.Done():
			return
		case event, ok := <-c.manager.Events():
			if !ok {
				return
			}
			env, err := protocol.NewWithPayload(protocol.TypeEvent, event.EventType, protocol.Source{
				Type: protocol.PeerNode,
				ID:   c.cfg.Node.ID,
			}, event)
			if err != nil {
				c.logger.Warn("encode agent event", "event_type", event.EventType, "error", err)
				continue
			}
			target := protocol.Target{NodeID: c.cfg.Node.ID, AgentID: event.AgentID, SessionID: event.SessionID}
			env.WithTarget(target)
			if err := c.write(conn, env); err != nil {
				c.logger.Warn("forward agent event failed, closing session", "error", err)
				_ = conn.Close()
				return
			}
			c.logger.Debug("agent event forwarded", "event_type", event.EventType, "agent_id", event.AgentID, "session_id", event.SessionID)
		}
	}
}

// stateLoop pushes coalesced inventory updates whenever an adapter changes.
func (c *Client) stateLoop(ctx context.Context, conn *websocket.Conn) {
	if c.manager == nil {
		<-ctx.Done()
		return
	}
	for {
		select {
		case <-ctx.Done():
			return
		case <-c.manager.Updates():
			// Coalesce bursts (for example a session list refresh) into one sync.
			select {
			case <-ctx.Done():
				return
			case <-time.After(150 * time.Millisecond):
			}
			if err := c.sendAgentState(conn); err != nil {
				c.logger.Warn("send agent state failed, closing session", "error", err)
				_ = conn.Close()
				return
			}
		}
	}
}

func (c *Client) sendAgentState(conn *websocket.Conn) error {
	env, err := protocol.NewWithPayload(protocol.TypeSync, protocol.ActionAgentState, protocol.Source{
		Type: protocol.PeerNode,
		ID:   c.cfg.Node.ID,
	}, protocol.AgentStatePayload{Agents: c.agents()})
	if err != nil {
		return err
	}
	env.WithTarget(protocol.Target{NodeID: c.cfg.Node.ID})
	return c.write(conn, env)
}

func (c *Client) readLoop(ctx context.Context, conn *websocket.Conn) error {
	for {
		_, raw, err := conn.ReadMessage()
		if err != nil {
			return fmt.Errorf("read cloud message: %w", err)
		}
		_ = conn.SetReadDeadline(time.Now().Add(c.cfg.Cloud.ReadTimeout.Std()))

		var env protocol.Envelope
		if err := json.Unmarshal(raw, &env); err != nil {
			c.logger.Warn("received malformed cloud message", "error", err)
			continue
		}
		if err := env.Validate(); err != nil {
			c.logger.Warn("received invalid cloud envelope", "error", err)
			continue
		}

		switch env.Type {
		case protocol.TypeHeartbeat:
			c.handleHeartbeatAck(&env)
		case protocol.TypeCommand:
			if err := c.handleCommand(ctx, conn, &env); err != nil {
				return err
			}
		case protocol.TypeResponse:
			c.logger.Debug("received response", "action", env.Action, "request_id", env.RequestID)
		case protocol.TypeError:
			var payload protocol.ErrorPayload
			_ = env.DecodePayload(&payload)
			c.logger.Warn("cloud reported error", "code", payload.Code, "message", payload.Message, "detail", payload.Detail)
		case protocol.TypeSync:
			c.logger.Debug("received sync", "action", env.Action)
		case protocol.TypeRegister:
			c.logger.Debug("received duplicate register")
		default:
			c.logger.Debug("ignoring cloud message", "type", env.Type)
		}
	}
}

func (c *Client) handleCommand(ctx context.Context, conn *websocket.Conn, env *protocol.Envelope) error {
	c.logger.Info("command received", "action", env.Action, "request_id", env.RequestID, "session_id", targetSession(env))

	accepted := protocol.CommandResult{Status: protocol.StatusAccepted}
	acceptedEnv, err := c.responseEnvelope(env, accepted)
	if err != nil {
		return err
	}
	if err := c.write(conn, acceptedEnv); err != nil {
		return fmt.Errorf("send accepted response: %w", err)
	}

	result := c.executor.Execute(ctx, env)
	reply, err := c.responseEnvelope(env, result)
	if err != nil {
		return err
	}
	if err := c.write(conn, reply); err != nil {
		return fmt.Errorf("send command response: %w", err)
	}

	c.logger.Info("command completed", "action", env.Action, "request_id", env.RequestID, "status", result.Status, "duration_ms", result.DurationMS)
	return nil
}

func (c *Client) responseEnvelope(command *protocol.Envelope, result protocol.CommandResult) (*protocol.Envelope, error) {
	env, err := protocol.NewWithPayload(protocol.TypeResponse, command.Action, protocol.Source{
		Type: protocol.PeerNode,
		ID:   c.cfg.Node.ID,
	}, result)
	if err != nil {
		return nil, err
	}
	env.WithRequestID(command.RequestID)
	if command.Target != nil {
		env.WithTarget(*command.Target)
	}
	return env, nil
}

func (c *Client) heartbeatLoop(ctx context.Context, conn *websocket.Conn) {
	ticker := time.NewTicker(c.cfg.Cloud.HeartbeatInterval.Std())
	defer ticker.Stop()

	sent := make(map[string]time.Time)
	heartbeat := func() {
		nonce := protocol.NewID("hb")
		env, err := protocol.NewWithPayload(protocol.TypeHeartbeat, "", protocol.Source{
			Type: protocol.PeerNode,
			ID:   c.cfg.Node.ID,
		}, protocol.HeartbeatPayload{Nonce: nonce})
		if err != nil {
			return
		}
		env.WithRequestID(protocol.NewID("req"))
		sent[nonce] = time.Now()
		for key, at := range sent {
			if time.Since(at) > 3*c.cfg.Cloud.HeartbeatInterval.Std() {
				delete(sent, key)
			}
		}
		if err := c.write(conn, env); err != nil {
			c.logger.Warn("heartbeat failed, closing session", "error", err)
			_ = conn.Close()
		}
	}

	heartbeat()
	for {
		select {
		case <-ctx.Done():
			return
		case <-ticker.C:
			heartbeat()
		}
	}
}

func (c *Client) handleHeartbeatAck(env *protocol.Envelope) {
	var payload protocol.HeartbeatPayload
	if err := env.DecodePayload(&payload); err != nil {
		return
	}
	c.logger.Debug("heartbeat acknowledged", "nonce", payload.Nonce)
}

func (c *Client) write(conn *websocket.Conn, env *protocol.Envelope) error {
	raw, err := json.Marshal(env)
	if err != nil {
		return err
	}
	c.writeMu.Lock()
	defer c.writeMu.Unlock()
	_ = conn.SetWriteDeadline(time.Now().Add(writeTimeout))
	return conn.WriteMessage(websocket.TextMessage, raw)
}

func jittered(base time.Duration, jitter float64) time.Duration {
	if jitter <= 0 {
		return base
	}
	factor := 1 + jitter*(rand.Float64()*2-1)
	delay := time.Duration(float64(base) * factor)
	if delay < time.Millisecond {
		return time.Millisecond
	}
	return delay
}

func targetSession(env *protocol.Envelope) string {
	if env.Target == nil {
		return ""
	}
	return env.Target.SessionID
}
