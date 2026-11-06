// Package server implements the Node standalone HTTP + WebSocket service used
// by Mobile and Terminal clients. It does not require Cloud.
package server

import (
	"context"
	"crypto/subtle"
	"crypto/tls"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"log/slog"
	"net"
	"net/http"
	"net/url"
	"os"
	"runtime"
	"strings"
	"sync"
	"time"

	"github.com/FlyingBassHot/meetagent/internal/node/adapter"
	"github.com/FlyingBassHot/meetagent/internal/node/eventhub"
	"github.com/FlyingBassHot/meetagent/internal/node/pairing"
	"github.com/FlyingBassHot/meetagent/internal/version"
	"github.com/FlyingBassHot/meetagent/pkg/clientproto"
	"github.com/gorilla/websocket"
	"github.com/grandcat/zeroconf"
)

const (
	helloTimeout   = 10 * time.Second
	defaultTimeout = 15 * time.Second
	defaultRead    = 90 * time.Second
	maxMessageSize = 1 << 20
)

// Options configures the local Node server.
type Options struct {
	Addr               string
	TLSCert            string
	TLSKey             string
	AllowedOrigins     []string
	DisallowLANOrigins bool
	SnapshotEvents     int
	PairingTTL         time.Duration
	AdminToken         string
	MDNS               bool
	CommandTimeout     time.Duration
	ReadTimeout        time.Duration
	RateLimitPerSecond float64
	RateLimitBurst     int
}

func (o Options) withDefaults() Options {
	if strings.TrimSpace(o.Addr) == "" {
		o.Addr = "0.0.0.0:8787"
	}
	if o.SnapshotEvents <= 0 {
		o.SnapshotEvents = 100
	}
	if o.PairingTTL <= 0 {
		o.PairingTTL = 10 * time.Minute
	}
	if o.CommandTimeout <= 0 {
		o.CommandTimeout = defaultTimeout
	}
	if o.ReadTimeout <= 0 {
		o.ReadTimeout = defaultRead
	}
	if len(o.AllowedOrigins) == 0 {
		o.AllowedOrigins = []string{
			"http://localhost:5173", "http://127.0.0.1:5173",
			"http://localhost:5174", "http://127.0.0.1:5174",
			"http://localhost:4173", "http://127.0.0.1:4173",
		}
	}
	return o
}

// Server owns the Node HTTP/WS listener, client registry and command gateway.
type Server struct {
	opts    Options
	node    clientproto.NodeInfo
	manager *adapter.Manager
	hub     *eventhub.Hub
	pairing *pairing.Manager
	logger  *slog.Logger

	httpServer *http.Server
	mdns       *zeroconf.Server
	upgrader   websocket.Upgrader

	clientsMu sync.Mutex
	clients   map[string]*clientConn
	commands  *commandCache

	permissionMu    sync.Mutex
	permissionState map[string]permissionState
}

// New creates a Node server.
func New(opts Options, nodeID, nodeName string, manager *adapter.Manager, hub *eventhub.Hub, pairingManager *pairing.Manager, logger *slog.Logger) *Server {
	if logger == nil {
		logger = slog.Default()
	}
	opts = opts.withDefaults()
	s := &Server{
		opts:            opts,
		node:            newNodeInfo(nodeID, nodeName),
		manager:         manager,
		hub:             hub,
		pairing:         pairingManager,
		logger:          logger,
		clients:         make(map[string]*clientConn),
		commands:        newCommandCache(5*time.Minute, 1000),
		permissionState: make(map[string]permissionState),
	}
	s.upgrader = websocket.Upgrader{
		ReadBufferSize:  4096,
		WriteBufferSize: 4096,
		CheckOrigin:     s.checkOrigin,
	}
	return s
}

// NodeInfo builds the public Node identity shared by the server and event hub.
func NodeInfo(nodeID, nodeName string) clientproto.NodeInfo {
	return newNodeInfo(nodeID, nodeName)
}

func newNodeInfo(nodeID, nodeName string) clientproto.NodeInfo {
	hostname, _ := os.Hostname()
	return clientproto.NodeInfo{
		NodeID:          nodeID,
		Name:            nodeName,
		Hostname:        hostname,
		Platform:        runtime.GOOS + "/" + runtime.GOARCH,
		Version:         version.Version,
		ProtocolVersion: clientproto.Version,
	}
}

// Handler returns the HTTP handler. It is exported for httptest-based tests.
func (s *Server) Handler() http.Handler {
	mux := http.NewServeMux()
	mux.HandleFunc("/healthz", s.handleHealth)
	mux.HandleFunc("/api/v2/info", s.handleInfo)
	mux.HandleFunc("/api/v2/pair", s.handlePair)
	mux.HandleFunc("/api/v2/snapshot", s.handleSnapshot)
	mux.HandleFunc("/api/v2/clients", s.handleClients)
	mux.HandleFunc("/api/v2/clients/", s.handleClientByID)
	mux.HandleFunc("/ws", s.handleWS)
	return s.withCORS(mux)
}

// Run starts the HTTP/WS listener and mDNS until ctx is canceled.
func (s *Server) Run(ctx context.Context) error {
	handler := s.Handler()
	ln, err := net.Listen("tcp", s.opts.Addr)
	if err != nil {
		return fmt.Errorf("listen %s: %w", s.opts.Addr, err)
	}
	if s.opts.TLSCert != "" || s.opts.TLSKey != "" {
		cert, err := tls.LoadX509KeyPair(s.opts.TLSCert, s.opts.TLSKey)
		if err != nil {
			_ = ln.Close()
			return fmt.Errorf("load TLS certificate: %w", err)
		}
		ln = tls.NewListener(ln, &tls.Config{Certificates: []tls.Certificate{cert}, MinVersion: tls.VersionTLS12})
	}

	s.httpServer = &http.Server{
		Handler:           handler,
		ReadHeaderTimeout: 5 * time.Second,
		ReadTimeout:       30 * time.Second,
		WriteTimeout:      30 * time.Second,
		IdleTimeout:       90 * time.Second,
	}

	if err := s.startMDNS(ln.Addr()); err != nil {
		s.logger.Warn("mDNS registration failed", "error", err)
	}
	s.logPairingCode()
	stopLog := make(chan struct{})
	go s.pairingCodeLogger(stopLog)

	errCh := make(chan error, 1)
	go func() {
		s.logger.Info("node server listening", "addr", ln.Addr().String(), "ws", "/ws", "version", version.Version)
		errCh <- s.httpServer.Serve(ln)
	}()

	select {
	case <-ctx.Done():
		shutdownCtx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
		defer cancel()
		_ = s.httpServer.Shutdown(shutdownCtx)
		close(stopLog)
		if s.mdns != nil {
			s.mdns.Shutdown()
		}
		return nil
	case err := <-errCh:
		close(stopLog)
		if s.mdns != nil {
			s.mdns.Shutdown()
		}
		if errors.Is(err, http.ErrServerClosed) {
			return nil
		}
		return err
	}
}

func (s *Server) startMDNS(addr net.Addr) error {
	if !s.opts.MDNS {
		return nil
	}
	_, portRaw, err := net.SplitHostPort(addr.String())
	if err != nil {
		return err
	}
	port := 0
	if _, err := fmt.Sscanf(portRaw, "%d", &port); err != nil || port == 0 {
		return fmt.Errorf("parse mDNS port %q", portRaw)
	}
	txt := []string{
		"node_id=" + s.node.NodeID,
		"name=" + s.node.Name,
		"protocol=2",
		"ws=/ws",
		"pairing=required",
	}
	if s.opts.TLSCert != "" {
		txt = append(txt, "tls=1")
	}
	mdns, err := zeroconf.Register(s.node.Name+" MeetAgent", "_meetagent._tcp", "local.", port, txt, nil)
	if err != nil {
		return err
	}
	s.mdns = mdns
	return nil
}

func (s *Server) logPairingCode() {
	code := s.pairing.CurrentCode()
	s.logger.Info("pairing code ready", "code", code, "expires_at", s.pairing.CodeExpires().Format(time.RFC3339))
}

func (s *Server) pairingCodeLogger(stop <-chan struct{}) {
	ticker := time.NewTicker(30 * time.Second)
	defer ticker.Stop()
	last := s.pairing.CurrentCode()
	for {
		select {
		case <-stop:
			return
		case <-ticker.C:
			code := s.pairing.CurrentCode()
			if code != last {
				last = code
				s.logger.Info("pairing code rotated", "code", code, "expires_at", s.pairing.CodeExpires().Format(time.RFC3339))
			}
		}
	}
}

func (s *Server) audit(event string, attrs ...any) {
	args := make([]any, 0, len(attrs)+2)
	args = append(args, "event", event)
	args = append(args, attrs...)
	s.logger.Info("audit", args...)
}

func (s *Server) handleHealth(w http.ResponseWriter, _ *http.Request) {
	writeJSON(w, http.StatusOK, map[string]any{
		"status":  "ok",
		"node_id": s.node.NodeID,
		"version": version.Version,
	})
}

func (s *Server) handleInfo(w http.ResponseWriter, _ *http.Request) {
	writeJSON(w, http.StatusOK, clientproto.Info{
		SchemaVersion:    clientproto.Version,
		ProtocolVersion:  clientproto.Version,
		Node:             s.node,
		Adapters:         s.adapterInfos(),
		PairingRequired:  true,
		PairingCodeTTLMS: s.opts.PairingTTL.Milliseconds(),
		WSPath:           "/ws",
	})
}

func (s *Server) handlePair(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}
	var req clientproto.PairRequest
	if err := decodeJSON(r, &req); err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}
	client, token, err := s.pairing.Pair(req)
	if err != nil {
		status := http.StatusInternalServerError
		switch {
		case errors.Is(err, pairing.ErrInvalidCode):
			status = http.StatusUnauthorized
		case errors.Is(err, pairing.ErrExpiredCode):
			status = http.StatusGone
		case errors.Is(err, pairing.ErrInvalidClient):
			status = http.StatusBadRequest
		}
		http.Error(w, err.Error(), status)
		return
	}
	s.audit("client.paired", "client_id", client.ClientID, "client_type", client.Type, "name", client.Name)
	writeJSON(w, http.StatusOK, clientproto.PairResult{
		SchemaVersion: clientproto.Version,
		ClientID:      client.ClientID,
		Token:         token,
		Permissions:   client.Permissions,
		Node:          s.node,
		ServerTime:    time.Now().UTC(),
	})
}

func (s *Server) handleSnapshot(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}
	if _, err := s.authenticateHeader(r); err != nil {
		http.Error(w, err.Error(), http.StatusUnauthorized)
		return
	}
	writeJSON(w, http.StatusOK, s.hub.Snapshot(s.opts.SnapshotEvents))
}

func (s *Server) handleClients(w http.ResponseWriter, r *http.Request) {
	if !s.adminAuthorized(r) {
		http.Error(w, "admin token required", http.StatusUnauthorized)
		return
	}
	if r.Method != http.MethodGet {
		http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}
	clients := s.pairing.Clients()
	out := make([]clientproto.ClientInfo, 0, len(clients))
	for _, client := range clients {
		out = append(out, clientproto.ClientInfo{
			ClientID:     client.ClientID,
			Name:         client.Name,
			ClientType:   client.Type,
			Version:      client.Version,
			Capabilities: append([]string(nil), client.Capabilities...),
			Permissions:  append([]string(nil), client.Permissions...),
			Online:       s.clientOnline(client.ClientID),
			CreatedAt:    client.CreatedAt,
			LastSeen:     client.LastSeen,
		})
	}
	writeJSON(w, http.StatusOK, map[string]any{"clients": out})
}

func (s *Server) handleClientByID(w http.ResponseWriter, r *http.Request) {
	if !s.adminAuthorized(r) {
		http.Error(w, "admin token required", http.StatusUnauthorized)
		return
	}
	clientID := strings.TrimSpace(strings.TrimPrefix(r.URL.Path, "/api/v2/clients/"))
	if clientID == "" {
		http.Error(w, "client id is required", http.StatusBadRequest)
		return
	}
	switch r.Method {
	case http.MethodDelete:
		if !s.pairing.Revoke(clientID) {
			http.Error(w, "unknown client", http.StatusNotFound)
			return
		}
		s.disconnectClient(clientID)
		s.audit("client.revoked", "client_id", clientID)
		writeJSON(w, http.StatusOK, map[string]any{"revoked": clientID})
	case http.MethodPatch:
		var body struct {
			Permissions []string `json:"permissions"`
		}
		if err := decodeJSON(r, &body); err != nil {
			http.Error(w, err.Error(), http.StatusBadRequest)
			return
		}
		client, err := s.pairing.SetPermissions(clientID, body.Permissions)
		if err != nil {
			status := http.StatusBadRequest
			if errors.Is(err, pairing.ErrUnknownClient) {
				status = http.StatusNotFound
			}
			http.Error(w, err.Error(), status)
			return
		}
		s.disconnectClient(clientID)
		s.audit("client.permissions_updated", "client_id", clientID, "permissions", strings.Join(client.Permissions, ","))
		writeJSON(w, http.StatusOK, clientproto.ClientInfo{
			ClientID:     client.ClientID,
			Name:         client.Name,
			ClientType:   client.Type,
			Version:      client.Version,
			Capabilities: client.Capabilities,
			Permissions:  client.Permissions,
			CreatedAt:    client.CreatedAt,
			LastSeen:     client.LastSeen,
		})
	default:
		http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
	}

}

func (s *Server) adminAuthorized(r *http.Request) bool {
	if strings.TrimSpace(s.opts.AdminToken) == "" {
		return false
	}
	value := strings.TrimSpace(r.Header.Get("Authorization"))
	if !strings.HasPrefix(strings.ToLower(value), "bearer ") {
		return false
	}
	return subtle.ConstantTimeCompare([]byte(strings.TrimSpace(value[7:])), []byte(s.opts.AdminToken)) == 1
}

func (s *Server) clientOnline(clientID string) bool {
	s.clientsMu.Lock()
	defer s.clientsMu.Unlock()
	_, ok := s.clients[clientID]
	return ok
}

func (s *Server) disconnectClient(clientID string) {
	s.clientsMu.Lock()
	cc := s.clients[clientID]
	delete(s.clients, clientID)
	s.clientsMu.Unlock()
	if cc != nil {
		cc.close()
	}
}

func (s *Server) authenticateHeader(r *http.Request) (*pairing.Client, error) {
	value := strings.TrimSpace(r.Header.Get("Authorization"))
	if !strings.HasPrefix(strings.ToLower(value), "bearer ") {
		return nil, errors.New("missing bearer token")
	}
	return s.pairing.Authenticate(strings.TrimSpace(value[7:]))
}

func (s *Server) handleWS(w http.ResponseWriter, r *http.Request) {
	conn, err := s.upgrader.Upgrade(w, r, nil)
	if err != nil {
		s.logger.Warn("websocket upgrade failed", "error", err)
		return
	}
	conn.SetReadLimit(maxMessageSize)
	_ = conn.SetReadDeadline(time.Now().Add(helloTimeout))

	hello, env, err := readHello(conn)
	if err != nil {
		s.writeAndClose(conn, clientproto.TypeError, clientproto.ErrorPayload{Code: "bad_hello", Message: err.Error()}, websocket.CloseProtocolError)
		return
	}

	client, issuedToken, err := s.authenticateHello(hello)
	if err != nil {
		s.logger.Warn("client authentication failed", "error", err, "client_type", hello.ClientType)
		s.audit("client.auth_failed", "client_type", hello.ClientType, "error", err.Error())
		code := "auth_failed"
		if errors.Is(err, pairing.ErrExpiredCode) {
			code = "pairing_expired"
		} else if errors.Is(err, pairing.ErrInvalidCode) {
			code = "invalid_pairing_code"
		}
		s.writeAndClose(conn, clientproto.TypeError, clientproto.ErrorPayload{Code: code, Message: err.Error()}, websocket.ClosePolicyViolation)
		return
	}

	cc := &clientConn{
		server:   s,
		id:       client.ClientID,
		name:     client.Name,
		kind:     client.Type,
		caps:     append([]string(nil), client.Capabilities...),
		perms:    append([]string(nil), client.Permissions...),
		conn:     conn,
		send:     make(chan []byte, 256),
		done:     make(chan struct{}),
		lastSeen: time.Now().UTC(),
		limiter:  newTokenBucket(s.opts.RateLimitPerSecond, s.opts.RateLimitBurst),
	}
	s.replaceClient(cc)

	snapshot := s.hub.Snapshot(s.opts.SnapshotEvents)
	welcome := clientproto.Welcome{
		SchemaVersion: clientproto.Version,
		ClientID:      client.ClientID,
		ClientType:    client.Type,
		Token:         issuedToken,
		Permissions:   append([]string(nil), client.Permissions...),
		Node:          s.node,
		Snapshot:      snapshot,
		ServerTime:    time.Now().UTC(),
	}
	if !s.writeDirect(cc, clientproto.TypeWelcome, welcome) {
		cc.close()
		return
	}

	missed, events, unsubscribe := s.hub.Subscribe(hello.LastSequence, 256)
	defer unsubscribe()
	for _, event := range missed {
		if !s.writeDirect(cc, clientproto.TypeEvent, event) {
			cc.close()
			return
		}
	}

	if env.MessageID != "" {
		s.logger.Debug("client connected", "client_id", client.ClientID, "client_type", client.Type, "message_id", env.MessageID)
	}
	go cc.writePump()
	go cc.eventPump(events)
	cc.readPump()
	cc.close()
	s.unregisterClient(cc)
	s.logger.Info("client disconnected", "client_id", client.ClientID)
	s.audit("client.disconnected", "client_id", client.ClientID)
}

func (s *Server) authenticateHello(hello clientproto.Hello) (pairing.Client, string, error) {
	if err := hello.Validate(); err != nil {
		return pairing.Client{}, "", err
	}
	if strings.TrimSpace(hello.Token) != "" {
		client, err := s.pairing.Authenticate(hello.Token)
		if err != nil {
			return pairing.Client{}, "", err
		}
		return *client, "", nil
	}
	return s.pairing.Pair(clientproto.PairRequest{
		ClientType:    hello.ClientType,
		ClientVersion: hello.ClientVersion,
		Name:          hello.Name,
		Capabilities:  hello.Capabilities,
		PairingCode:   hello.PairingCode,
	})
}

func readHello(conn *websocket.Conn) (clientproto.Hello, clientproto.Envelope, error) {
	_, raw, err := conn.ReadMessage()
	if err != nil {
		return clientproto.Hello{}, clientproto.Envelope{}, err
	}
	var env clientproto.Envelope
	if err := json.Unmarshal(raw, &env); err != nil {
		return clientproto.Hello{}, env, fmt.Errorf("decode hello envelope: %w", err)
	}
	if err := env.Validate(); err != nil {
		return clientproto.Hello{}, env, err
	}
	if env.Type != clientproto.TypeHello {
		return clientproto.Hello{}, env, fmt.Errorf("first message must be hello, got %s", env.Type)
	}
	var hello clientproto.Hello
	if err := env.DecodePayload(&hello); err != nil {
		return clientproto.Hello{}, env, err
	}
	return hello, env, nil
}

func (s *Server) writeAndClose(conn *websocket.Conn, messageType string, payload any, closeCode int) {
	env, err := clientproto.NewEnvelope(messageType, payload)
	if err == nil {
		_ = conn.SetWriteDeadline(time.Now().Add(5 * time.Second))
		_ = conn.WriteJSON(env)
	}
	_ = conn.WriteControl(websocket.CloseMessage, websocket.FormatCloseMessage(closeCode, ""), time.Now().Add(2*time.Second))
	_ = conn.Close()
}

func (s *Server) writeDirect(cc *clientConn, messageType string, payload any) bool {
	env, err := clientproto.NewEnvelope(messageType, payload)
	if err != nil {
		s.logger.Warn("encode direct message", "type", messageType, "error", err)
		return false
	}
	_ = cc.conn.SetWriteDeadline(time.Now().Add(10 * time.Second))
	if err := cc.conn.WriteJSON(env); err != nil {
		s.logger.Debug("write direct message failed", "client_id", cc.id, "error", err)
		return false
	}
	return true
}

func (s *Server) replaceClient(cc *clientConn) {
	s.clientsMu.Lock()
	previous := s.clients[cc.id]
	s.clients[cc.id] = cc
	s.clientsMu.Unlock()
	if previous != nil && previous != cc {
		s.logger.Info("replacing existing client connection", "client_id", cc.id)
		previous.close()
	}
}

func (s *Server) unregisterClient(cc *clientConn) {
	s.clientsMu.Lock()
	if current, ok := s.clients[cc.id]; ok && current == cc {
		delete(s.clients, cc.id)
	}
	s.clientsMu.Unlock()
}

type permissionState struct {
	resolved bool
	expires  time.Time
}

func (s *Server) beginPermission(permissionID string) bool {
	s.permissionMu.Lock()
	defer s.permissionMu.Unlock()
	now := time.Now()
	for id, state := range s.permissionState {
		if now.After(state.expires) {
			delete(s.permissionState, id)
		}
	}
	if _, exists := s.permissionState[permissionID]; exists {
		return false
	}
	s.permissionState[permissionID] = permissionState{expires: now.Add(10 * time.Minute)}
	return true
}

func (s *Server) finishPermission(permissionID string, resolved bool) {
	s.permissionMu.Lock()
	defer s.permissionMu.Unlock()
	if resolved {
		s.permissionState[permissionID] = permissionState{resolved: true, expires: time.Now().Add(10 * time.Minute)}
		return
	}
	delete(s.permissionState, permissionID)
}

func (s *Server) adapterInfos() []clientproto.AdapterInfo {
	if s.manager == nil {
		return nil
	}
	manifests := s.manager.Manifests()
	out := make([]clientproto.AdapterInfo, 0, len(manifests))
	for _, manifest := range manifests {
		out = append(out, clientproto.AdapterInfo{
			ID:           manifest.ID,
			Name:         manifest.Name,
			Version:      manifest.Version,
			APIVersion:   manifest.APIVersion,
			Runtime:      manifest.Runtime,
			Capabilities: append([]string(nil), manifest.Capabilities...),
			Permissions:  append([]string(nil), manifest.Permissions...),
		})
	}
	return out
}

func (s *Server) withCORS(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		origin := r.Header.Get("Origin")
		if origin != "" && s.originAllowed(r, origin) {
			w.Header().Set("Access-Control-Allow-Origin", origin)
			w.Header().Set("Vary", "Origin")
			w.Header().Set("Access-Control-Allow-Headers", "Authorization, Content-Type")
			w.Header().Set("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
		}
		if r.Method == http.MethodOptions {
			w.WriteHeader(http.StatusNoContent)
			return
		}
		next.ServeHTTP(w, r)
	})
}

func (s *Server) checkOrigin(r *http.Request) bool {
	origin := r.Header.Get("Origin")
	if origin == "" {
		return true
	}
	return s.originAllowed(r, origin)
}

func (s *Server) originAllowed(r *http.Request, origin string) bool {
	for _, allowed := range s.opts.AllowedOrigins {
		if allowed == "*" || allowed == origin {
			return true
		}
	}
	parsed, err := url.Parse(origin)
	if err != nil {
		return false
	}
	if parsed.Host == r.Host {
		return true
	}
	if s.opts.DisallowLANOrigins {
		return false
	}
	return isPrivateHost(parsed.Hostname())
}

func isPrivateHost(host string) bool {
	host = strings.ToLower(strings.TrimSpace(host))
	if host == "" {
		return false
	}
	if host == "localhost" || strings.HasSuffix(host, ".local") {
		return true
	}
	ip := net.ParseIP(host)
	if ip == nil {
		return false
	}
	if v4 := ip.To4(); v4 != nil && v4[0] == 100 && v4[1] >= 64 && v4[1] <= 127 {
		// Tailscale uses the 100.64.0.0/10 shared address range.
		return true
	}
	return ip.IsLoopback() || ip.IsPrivate() || ip.IsLinkLocalUnicast()
}

func decodeJSON(r *http.Request, dst any) error {
	defer r.Body.Close()
	decoder := json.NewDecoder(io.LimitReader(r.Body, maxMessageSize))
	decoder.DisallowUnknownFields()
	if err := decoder.Decode(dst); err != nil {
		return fmt.Errorf("invalid JSON body: %w", err)
	}
	return nil
}

func writeJSON(w http.ResponseWriter, status int, value any) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(status)
	_ = json.NewEncoder(w).Encode(value)
}

func jsonRaw(value any) json.RawMessage {
	raw, err := json.Marshal(value)
	if err != nil {
		return nil
	}
	return raw
}
