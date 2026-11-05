// Package server wires the Cloud HTTP/WebSocket transport, authenticator,
// connection manager, event cache and router together.
package server

import (
	"context"
	"crypto/subtle"
	"encoding/json"
	"errors"
	"log/slog"
	"net"
	"net/http"
	"strings"
	"time"

	"github.com/FlyingBassHot/meetagent/internal/cloud/auth"
	"github.com/FlyingBassHot/meetagent/internal/cloud/config"
	"github.com/FlyingBassHot/meetagent/internal/cloud/connection"
	"github.com/FlyingBassHot/meetagent/internal/cloud/router"
	"github.com/FlyingBassHot/meetagent/internal/eventcache"
	"github.com/FlyingBassHot/meetagent/internal/version"
	"github.com/FlyingBassHot/meetagent/pkg/protocol"
	"github.com/go-chi/chi/v5"
	"github.com/go-chi/chi/v5/middleware"
	"github.com/gorilla/websocket"
)

// Version is the Cloud build version shown by the status endpoint.
const Version = version.Version

// Server is the MeetAgent Cloud process.
type Server struct {
	cfg      config.Config
	logger   *slog.Logger
	auth     *auth.Authenticator
	manager  *connection.Manager
	cache    *eventcache.Cache
	router   *router.Router
	upgrader websocket.Upgrader
	http     *http.Server
}

// New constructs a Cloud server. Call Handler for tests or Run to serve.
func New(cfg config.Config, logger *slog.Logger) *Server {
	if logger == nil {
		logger = slog.Default()
	}
	manager := connection.NewManager(cfg.Server.PendingTTL.Std(), cfg.Server.MaxPending, logger)
	authenticator := auth.New(cfg.Auth)
	cache := eventcache.New(cfg.EventCache.Capacity, cfg.EventCache.TTL.Std())
	messageRouter := router.New(
		manager,
		authenticator,
		cache,
		cfg.EventCache.SnapshotEvents,
		cfg.EventCache.ReplayLimit,
		logger,
	)

	s := &Server{
		cfg:     cfg,
		logger:  logger,
		auth:    authenticator,
		manager: manager,
		cache:   cache,
		router:  messageRouter,
	}
	s.upgrader = websocket.Upgrader{
		HandshakeTimeout: 10 * time.Second,
		ReadBufferSize:   4096,
		WriteBufferSize:  4096,
		CheckOrigin:      s.checkOrigin,
	}
	return s
}

// Handler returns the HTTP handler, useful for integration tests.
func (s *Server) Handler() http.Handler {
	r := chi.NewRouter()
	r.Use(middleware.RequestID)
	r.Use(middleware.RealIP)
	r.Use(middleware.Recoverer)

	r.Get("/healthz", s.handleHealth)
	r.Group(func(r chi.Router) {
		r.Use(s.requireAdminToken)
		r.Get("/api/v1/status", s.handleStatus)
		r.Get("/api/v1/snapshot", s.handleSnapshot)
	})
	r.Get(s.cfg.Server.WSPath, s.handleWebSocket)
	return r
}

// Close disconnects all peers. Useful in tests and during shutdown.
func (s *Server) Close() {
	s.manager.CloseAll()
}

// Run listens until ctx is canceled.
func (s *Server) Run(ctx context.Context) error {
	listener, err := net.Listen("tcp", s.cfg.Server.Addr)
	if err != nil {
		return err
	}

	s.http = &http.Server{
		Handler:           s.Handler(),
		ReadHeaderTimeout: 10 * time.Second,
		IdleTimeout:       120 * time.Second,
	}

	s.manager.StartJanitor(ctx, s.cfg.Server.PendingTTL.Std()/2)
	s.logger.Info("cloud listening", "addr", listener.Addr().String(), "ws_path", s.cfg.Server.WSPath, "version", Version)

	shutdownDone := make(chan struct{})
	go func() {
		defer close(shutdownDone)
		<-ctx.Done()
		shutdownCtx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
		defer cancel()
		_ = s.http.Shutdown(shutdownCtx)
		s.manager.CloseAll()
	}()

	err = s.http.Serve(listener)
	if errors.Is(err, http.ErrServerClosed) {
		<-shutdownDone
		return nil
	}
	return err
}

func (s *Server) handleWebSocket(w http.ResponseWriter, r *http.Request) {
	conn, err := s.upgrader.Upgrade(w, r, nil)
	if err != nil {
		s.logger.Debug("websocket upgrade failed", "remote_addr", r.RemoteAddr, "error", err)
		return
	}

	client := connection.New(connection.Options{
		ID:              protocol.NewID("conn"),
		Conn:            conn,
		Logger:          s.logger,
		SendQueue:       64,
		ReadTimeout:     s.cfg.Server.ReadTimeout.Std(),
		RegisterTimeout: s.cfg.Server.RegisterTimeout.Std(),
		PingInterval:    s.cfg.Server.PingInterval.Std(),
		WriteTimeout:    10 * time.Second,
		RemoteAddr:      r.RemoteAddr,
		RatePerSecond:   s.cfg.Server.RateLimitPerSecond,
		RateBurst:       s.cfg.Server.RateLimitBurst,
	})

	s.logger.Debug("websocket connected", "connection_id", client.ID, "remote_addr", r.RemoteAddr)
	client.Serve(
		s.router.Handle,
		func(c *connection.Client) {
			s.router.Disconnect(c)
			s.logger.Debug("websocket disconnected", "connection_id", c.ID)
		},
	)
}

func (s *Server) handleHealth(w http.ResponseWriter, _ *http.Request) {
	w.Header().Set("Content-Type", "text/plain; charset=utf-8")
	w.WriteHeader(http.StatusOK)
	_, _ = w.Write([]byte("ok\n"))
}

// requireAdminToken protects the management endpoints. The token is compared in
// constant time and never logged.
func (s *Server) requireAdminToken(next http.Handler) http.Handler {
	expected := []byte(s.cfg.Server.AdminToken)
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		header := r.Header.Get("Authorization")
		const prefix = "Bearer "
		if !strings.HasPrefix(header, prefix) ||
			subtle.ConstantTimeCompare([]byte(strings.TrimSpace(header[len(prefix):])), expected) != 1 {
			w.Header().Set("WWW-Authenticate", `Bearer realm="meetagent-admin"`)
			writeJSON(w, http.StatusUnauthorized, map[string]string{"error": "unauthorized"})
			return
		}
		next.ServeHTTP(w, r)
	})
}

func (s *Server) handleStatus(w http.ResponseWriter, _ *http.Request) {
	onlineNodes, totalNodes := s.manager.NodeCounts()
	devices := s.manager.DeviceViews()
	for i := range devices {
		devices[i].AllowedNodes = s.auth.AllowedNodes(devices[i].DeviceID)
	}
	writeJSON(w, http.StatusOK, map[string]any{
		"status":           "ok",
		"version":          Version,
		"server_time":      time.Now().UTC(),
		"nodes_online":     onlineNodes,
		"nodes_total":      totalNodes,
		"devices_online":   s.manager.DeviceCount(),
		"devices":          devices,
		"pending_requests": s.manager.PendingCount(),
		"cached_events":    s.cache.Len(),
	})
}

func (s *Server) handleSnapshot(w http.ResponseWriter, _ *http.Request) {
	snapshot := s.manager.Snapshot()
	snapshot.Events = s.cache.Recent(nil, s.cfg.EventCache.SnapshotEvents)
	snapshot.LastSequences = s.cache.LatestSequences(nil)
	snapshot.PendingPermissions = s.cache.PendingPermissions(nil)
	if snapshot.Events == nil {
		snapshot.Events = []protocol.AgentEvent{}
	}
	if snapshot.LastSequences == nil {
		snapshot.LastSequences = map[string]uint64{}
	}
	if snapshot.PendingPermissions == nil {
		snapshot.PendingPermissions = []protocol.PermissionView{}
	}
	writeJSON(w, http.StatusOK, snapshot)
}

func (s *Server) checkOrigin(r *http.Request) bool {
	origin := strings.TrimSpace(r.Header.Get("Origin"))
	if origin == "" {
		// Non-browser clients (Node) do not send Origin.
		return true
	}
	for _, allowed := range s.cfg.Server.AllowedOrigins {
		if allowed == "*" || strings.EqualFold(strings.TrimRight(allowed, "/"), strings.TrimRight(origin, "/")) {
			return true
		}
	}
	s.logger.Warn("rejected websocket origin", "origin", origin, "remote_addr", r.RemoteAddr)
	return false
}

func writeJSON(w http.ResponseWriter, status int, value any) {
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	w.WriteHeader(status)
	_ = json.NewEncoder(w).Encode(value)
}
