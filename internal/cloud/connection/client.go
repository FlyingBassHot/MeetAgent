// Package connection owns Cloud-side WebSocket connections and their
// in-memory registries.
package connection

import (
	"encoding/json"
	"errors"
	"log/slog"
	"sync"
	"time"

	"github.com/FlyingBassHot/meetagent/pkg/protocol"
	"github.com/gorilla/websocket"
)

var (
	// ErrClosed is returned when sending on a closed connection.
	ErrClosed = errors.New("connection closed")
	// ErrSendQueueFull is returned when a peer cannot keep up.
	ErrSendQueueFull = errors.New("connection send queue full")
)

// Options configures a Client.
type Options struct {
	ID              string
	Conn            *websocket.Conn
	Logger          *slog.Logger
	SendQueue       int
	ReadTimeout     time.Duration
	RegisterTimeout time.Duration
	PingInterval    time.Duration
	WriteTimeout    time.Duration
	RemoteAddr      string
	RatePerSecond   float64
	RateBurst       int
}

// Client is one WebSocket peer. Writes go through a single writer goroutine;
// Close may be called from any goroutine.
type Client struct {
	ID           string
	conn         *websocket.Conn
	logger       *slog.Logger
	send         chan []byte
	done         chan struct{}
	closeOnce    sync.Once
	readTimeout  time.Duration
	regTimeout   time.Duration
	pingInterval time.Duration
	writeTimeout time.Duration
	remoteAddr   string
	limiter      *tokenBucket

	mu          sync.RWMutex
	identity    protocol.Source
	registered  bool
	lastSeen    time.Time
	connectedAt time.Time
	rateNotice  time.Time
}

// New creates a Client. Call Serve to start its read and write loops.
func New(opts Options) *Client {
	if opts.SendQueue <= 0 {
		opts.SendQueue = 64
	}
	if opts.WriteTimeout <= 0 {
		opts.WriteTimeout = 10 * time.Second
	}
	return &Client{
		ID:           opts.ID,
		conn:         opts.Conn,
		logger:       opts.Logger,
		send:         make(chan []byte, opts.SendQueue),
		done:         make(chan struct{}),
		readTimeout:  opts.ReadTimeout,
		regTimeout:   opts.RegisterTimeout,
		pingInterval: opts.PingInterval,
		writeTimeout: opts.WriteTimeout,
		remoteAddr:   opts.RemoteAddr,
		limiter:      newTokenBucket(opts.RatePerSecond, opts.RateBurst),
		lastSeen:     time.Now().UTC(),
		connectedAt:  time.Now().UTC(),
	}
}

// Serve runs the read loop and starts the writer loop. It blocks until the
// connection closes, then calls onClose exactly once.
func (c *Client) Serve(handler func(*Client, *protocol.Envelope), onClose func(*Client)) {
	go c.writePump()

	defer func() {
		if onClose != nil {
			onClose(c)
		}
		c.Close(websocket.CloseNormalClosure, "")
	}()

	c.conn.SetReadLimit(protocol.MaxMessageSize)
	c.conn.SetPongHandler(func(string) error {
		c.resetReadDeadline()
		return nil
	})
	c.resetReadDeadline()

	for {
		_, raw, err := c.conn.ReadMessage()
		if err != nil {
			if !websocket.IsCloseError(err, websocket.CloseNormalClosure, websocket.CloseGoingAway) {
				c.logger.Debug("read loop ended", "connection_id", c.ID, "error", err)
			}
			return
		}

		c.Touch()

		if !c.limiter.allow(time.Now()) {
			c.notifyRateLimited()
			continue
		}

		var env protocol.Envelope
		if err := json.Unmarshal(raw, &env); err != nil {
			c.sendError("", "malformed_json", "message is not valid JSON", err.Error())
			continue
		}
		if err := env.Validate(); err != nil {
			c.sendError(env.RequestID, "invalid_envelope", "message failed validation", err.Error())
			continue
		}

		if handler != nil {
			handler(c, &env)
		}
		c.resetReadDeadline()
	}
}

// Send encodes and queues an envelope.
func (c *Client) Send(env *protocol.Envelope) error {
	raw, err := json.Marshal(env)
	if err != nil {
		return err
	}
	return c.SendRaw(raw)
}

// SendRaw queues an already encoded envelope.
func (c *Client) SendRaw(raw []byte) error {
	select {
	case <-c.done:
		return ErrClosed
	default:
	}

	select {
	case c.send <- raw:
		return nil
	case <-c.done:
		return ErrClosed
	default:
		c.logger.Warn("send queue full, closing connection", "connection_id", c.ID)
		go c.Close(websocket.ClosePolicyViolation, "send queue full")
		return ErrSendQueueFull
	}
}

// Close stops the client. It is safe to call multiple times.
func (c *Client) Close(code int, reason string) {
	c.closeOnce.Do(func() {
		close(c.done)
		_ = c.conn.WriteControl(
			websocket.CloseMessage,
			websocket.FormatCloseMessage(code, reason),
			time.Now().Add(time.Second),
		)
		_ = c.conn.Close()
	})
}

// Done returns a channel closed when the client is closed.
func (c *Client) Done() <-chan struct{} { return c.done }

// MarkRegistered records the authenticated identity.
func (c *Client) MarkRegistered(identity protocol.Source) {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.identity = identity
	c.registered = true
}

// Registered returns the authenticated identity, if any.
func (c *Client) Registered() (protocol.Source, bool) {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.identity, c.registered
}

// Touch updates the last activity timestamp.
func (c *Client) Touch() {
	c.mu.Lock()
	c.lastSeen = time.Now().UTC()
	c.mu.Unlock()
}

// LastSeen returns the last activity timestamp.
func (c *Client) LastSeen() time.Time {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.lastSeen
}

// ConnectedAt returns the connection creation time.
func (c *Client) ConnectedAt() time.Time {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.connectedAt
}

// RemoteAddr returns the peer address recorded at upgrade time.
func (c *Client) RemoteAddr() string { return c.remoteAddr }

func (c *Client) resetReadDeadline() {
	timeout := c.readTimeout
	if _, ok := c.Registered(); !ok {
		timeout = c.regTimeout
	}
	_ = c.conn.SetReadDeadline(time.Now().Add(timeout))
}

func (c *Client) writePump() {
	ticker := time.NewTicker(c.pingInterval)
	defer ticker.Stop()

	for {
		select {
		case raw := <-c.send:
			_ = c.conn.SetWriteDeadline(time.Now().Add(c.writeTimeout))
			if err := c.conn.WriteMessage(websocket.TextMessage, raw); err != nil {
				c.logger.Debug("write failed", "connection_id", c.ID, "error", err)
				c.Close(websocket.CloseGoingAway, "write failed")
				return
			}
		case <-ticker.C:
			_ = c.conn.SetWriteDeadline(time.Now().Add(c.writeTimeout))
			if err := c.conn.WriteMessage(websocket.PingMessage, nil); err != nil {
				c.logger.Debug("ping failed", "connection_id", c.ID, "error", err)
				c.Close(websocket.CloseGoingAway, "ping failed")
				return
			}
		case <-c.done:
			return
		}
	}
}

// notifyRateLimited tells a peer that its messages are being dropped, but at
// most once per second so the error itself cannot amplify the flood.
func (c *Client) notifyRateLimited() {
	now := time.Now()
	c.mu.Lock()
	if !c.rateNotice.IsZero() && now.Sub(c.rateNotice) < time.Second {
		c.mu.Unlock()
		return
	}
	c.rateNotice = now
	c.mu.Unlock()

	c.sendError("", "rate_limited", "message rate limit exceeded", "")
	c.logger.Warn("message rate limit exceeded", "connection_id", c.ID)
}

func (c *Client) sendError(requestID, code, message, detail string) {
	env, err := protocol.NewWithPayload(protocol.TypeError, "", protocol.Source{Type: protocol.PeerCloud, ID: "cloud"}, protocol.ErrorPayload{
		Code:    code,
		Message: message,
		Detail:  detail,
	})
	if err != nil {
		return
	}
	if requestID != "" {
		env.WithRequestID(requestID)
	}
	_ = c.Send(env)
}
