package server

import (
	"encoding/json"
	"sync"
	"time"

	"github.com/FlyingBassHot/meetagent/pkg/clientproto"
	"github.com/gorilla/websocket"
)

type clientConn struct {
	server *Server
	id     string
	name   string
	kind   string
	caps   []string
	perms  []string

	conn *websocket.Conn
	send chan []byte
	done chan struct{}
	once sync.Once

	lastSeen time.Time
	limiter  *tokenBucket
}

func (c *clientConn) close() {
	c.once.Do(func() {
		close(c.done)
		_ = c.conn.Close()
	})
}

func (c *clientConn) sendEnvelope(env *clientproto.Envelope) bool {
	raw, err := json.Marshal(env)
	if err != nil {
		c.server.logger.Warn("encode client envelope", "client_id", c.id, "type", env.Type, "error", err)
		return false
	}
	select {
	case c.send <- raw:
		return true
	case <-c.done:
		return false
	default:
		c.server.logger.Warn("client send buffer full, closing", "client_id", c.id)
		c.close()
		return false
	}
}

func (c *clientConn) writePump() {
	ticker := time.NewTicker(25 * time.Second)
	defer ticker.Stop()
	for {
		select {
		case <-c.done:
			return
		case raw := <-c.send:
			_ = c.conn.SetWriteDeadline(time.Now().Add(10 * time.Second))
			if err := c.conn.WriteMessage(websocket.TextMessage, raw); err != nil {
				c.server.logger.Debug("client write failed", "client_id", c.id, "error", err)
				c.close()
				return
			}
		case <-ticker.C:
			_ = c.conn.SetWriteDeadline(time.Now().Add(5 * time.Second))
			if err := c.conn.WriteControl(websocket.PingMessage, nil, time.Now().Add(5*time.Second)); err != nil {
				c.close()
				return
			}
		}
	}
}

func (c *clientConn) eventPump(events <-chan clientproto.Event) {
	for {
		select {
		case <-c.done:
			return
		case event, ok := <-events:
			if !ok {
				return
			}
			env, err := clientproto.NewEnvelope(clientproto.TypeEvent, event)
			if err != nil {
				c.server.logger.Warn("encode event envelope", "client_id", c.id, "error", err)
				continue
			}
			if !c.sendEnvelope(env) {
				return
			}
		}
	}
}

func (c *clientConn) readPump() {
	for {
		_ = c.conn.SetReadDeadline(time.Now().Add(c.server.opts.ReadTimeout))
		_, raw, err := c.conn.ReadMessage()
		if err != nil {
			if websocket.IsUnexpectedCloseError(err, websocket.CloseNormalClosure, websocket.CloseGoingAway) {
				c.server.logger.Debug("client read closed", "client_id", c.id, "error", err)
			}
			return
		}
		c.lastSeen = time.Now().UTC()
		c.server.pairing.Touch(c.id, c.lastSeen)
		var env clientproto.Envelope
		if err := json.Unmarshal(raw, &env); err != nil {
			c.sendError("bad_message", "invalid JSON envelope")
			continue
		}
		if err := env.Validate(); err != nil {
			c.sendError("bad_message", err.Error())
			continue
		}
		if !c.limiter.allow(time.Now()) {
			c.sendError("rate_limited", "inbound message rate exceeded")
			continue
		}
		switch env.Type {
		case clientproto.TypeCommand:
			var cmd clientproto.Command
			if err := env.DecodePayload(&cmd); err != nil {
				c.sendError("bad_command", err.Error())
				continue
			}
			result := c.server.handleCommand(c, cmd)
			response, err := clientproto.NewEnvelope(clientproto.TypeCommandResult, result)
			if err != nil {
				c.sendError("encode_error", err.Error())
				continue
			}
			if !c.sendEnvelope(response) {
				return
			}
		case clientproto.TypeHeartbeat:
			var heartbeat struct {
				Nonce string `json:"nonce"`
			}
			_ = env.DecodePayload(&heartbeat)
			response, err := clientproto.NewEnvelope(clientproto.TypeHeartbeat, map[string]any{
				"nonce":       heartbeat.Nonce,
				"server_time": time.Now().UTC(),
			})
			if err == nil {
				c.sendEnvelope(response)
			}
		case clientproto.TypeSnapshot:
			response, err := clientproto.NewEnvelope(clientproto.TypeSnapshot, c.server.hub.Snapshot(c.server.opts.SnapshotEvents))
			if err == nil {
				c.sendEnvelope(response)
			}
		default:
			c.sendError("unsupported_type", "unsupported message type: "+env.Type)
		}
	}
}

func (c *clientConn) sendError(code, message string) {
	env, err := clientproto.NewEnvelope(clientproto.TypeError, clientproto.ErrorPayload{Code: code, Message: message})
	if err == nil {
		c.sendEnvelope(env)
	}
}
