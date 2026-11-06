// Command meetagent-node runs the local MeetAgent Node.
package main

import (
	"context"
	"flag"
	"fmt"
	"log/slog"
	"os"
	"os/signal"
	"strings"
	"syscall"
	"time"

	"github.com/FlyingBassHot/meetagent/internal/node/adapter"
	"github.com/FlyingBassHot/meetagent/internal/node/adapter/mock"
	"github.com/FlyingBassHot/meetagent/internal/node/adapter/opencode"
	"github.com/FlyingBassHot/meetagent/internal/node/config"
	"github.com/FlyingBassHot/meetagent/internal/node/connection"
	"github.com/FlyingBassHot/meetagent/internal/node/eventhub"
	"github.com/FlyingBassHot/meetagent/internal/node/pairing"
	"github.com/FlyingBassHot/meetagent/internal/node/server"
)

func main() {
	if err := run(); err != nil {
		fmt.Fprintln(os.Stderr, "meetagent-node:", err)
		os.Exit(1)
	}
}

func run() error {
	configPath := flag.String("config", "configs/node.yaml", "path to Node YAML configuration")
	flag.Parse()

	cfg, err := config.Load(*configPath)
	if err != nil {
		return err
	}

	logger := newLogger(cfg.Log.Level)
	slog.SetDefault(logger)

	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	defer stop()

	manager, err := buildManager(cfg, logger)
	if err != nil {
		return err
	}

	nodeInfo := server.NodeInfo(cfg.Node.ID, cfg.Node.Name)
	hub := eventhub.New(nodeInfo, manager, cfg.Server.EventBuffer, cfg.Server.EventTTL.Std(), logger)
	hub.SetPermissionTTL(cfg.Server.PermissionTTL.Std())
	manager.Start(ctx)
	defer func() {
		shutdownCtx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
		defer cancel()
		manager.Stop(shutdownCtx)
	}()
	go hub.Run(ctx)

	if !cfg.Server.IsEnabled() && !cfg.Cloud.IsEnabled() {
		return fmt.Errorf("both server.enabled and cloud.enabled are false; nothing to run")
	}

	runCtx, cancel := context.WithCancel(ctx)
	defer cancel()
	errCh := make(chan error, 2)

	if cfg.Server.IsEnabled() {
		pairingManager, err := pairing.New(cfg.Server.PairingStore, cfg.Server.PairingTTL.Std(), cfg.Server.DefaultPermissions)
		if err != nil {
			return err
		}
		nodeServer := server.New(serverOptions(cfg), cfg.Node.ID, cfg.Node.Name, manager, hub, pairingManager, logger)
		go func() {
			errCh <- nodeServer.Run(runCtx)
		}()
	}

	if cfg.Cloud.IsEnabled() {
		events, unsubscribe := hub.SubscribeRaw(1024)
		defer unsubscribe()
		client := connection.New(cfg, manager, logger)
		client.SetEventSource(events)
		go func() {
			errCh <- client.Run(runCtx)
		}()
	}

	select {
	case <-ctx.Done():
		cancel()
		return nil
	case err := <-errCh:
		cancel()
		if err != nil {
			return err
		}
		return nil
	}
}

// serverOptions maps the shared config onto the local Node server. Keeping the
// mapping in one place prevents config fields silently not reaching the server.
func serverOptions(cfg config.Config) server.Options {
	return server.Options{
		Addr:               cfg.Server.Addr,
		TLSCert:            cfg.Server.TLSCert,
		TLSKey:             cfg.Server.TLSKey,
		AllowedOrigins:     cfg.Server.AllowedOrigins,
		DisallowLANOrigins: !cfg.Server.LANOriginAllowed(),
		SnapshotEvents:     cfg.Server.SnapshotEvents,
		PairingTTL:         cfg.Server.PairingTTL.Std(),
		AdminToken:         cfg.Server.AdminToken,
		MDNS:               cfg.Server.MDNSEnabled(),
		CommandTimeout:     cfg.Server.CommandTimeout.Std(),
		ReadTimeout:        cfg.Server.ReadTimeout.Std(),
		RateLimitPerSecond: float64(cfg.Server.RateLimitPerSecond),
		RateLimitBurst:     cfg.Server.RateLimitBurst,
	}
}

// buildManager instantiates one Adapter per enabled Agent declaration.
func buildManager(cfg config.Config, logger *slog.Logger) (*adapter.Manager, error) {
	manager := adapter.NewManager(cfg.Node.ID, logger)
	for _, agent := range cfg.Node.Agents {
		if !agent.IsEnabled() {
			logger.Info("agent disabled by configuration", "agent_id", agent.ID)
			continue
		}

		var impl adapter.AgentAdapter
		switch strings.ToLower(agent.Type) {
		case "opencode":
			impl = opencode.New(opencode.Options{
				BaseURL:   agent.Connection.BaseURL,
				Username:  agent.Connection.Username,
				Password:  agent.Connection.Password,
				Directory: agent.Connection.Directory,
				Timeout:   agent.Connection.Timeout.Std(),
			})
		case "mock":
			impl = mock.New(mock.Options{
				Sessions:    staticSessions(agent.Sessions),
				Permissions: true,
			})
		default:
			logger.Warn("unsupported agent type, skipping", "agent_id", agent.ID, "type", agent.Type)
			continue
		}

		if err := manager.Add(adapter.Instance{
			ID:                  agent.ID,
			Name:                agent.Name,
			Adapter:             impl,
			StaticSessions:      staticSessions(agent.Sessions),
			CapabilityOverrides: agent.Capabilities,
		}); err != nil {
			return nil, fmt.Errorf("configure agent %s: %w", agent.ID, err)
		}
		logger.Info("agent adapter configured", "agent_id", agent.ID, "type", agent.Type)
	}
	return manager, nil
}

func staticSessions(configured []config.SessionConfig) []adapter.Session {
	sessions := make([]adapter.Session, 0, len(configured))
	for _, session := range configured {
		sessions = append(sessions, adapter.Session{
			ID:               session.ID,
			Title:            session.Title,
			WorkingDirectory: session.WorkingDirectory,
			Status:           session.Status,
			UpdatedAt:        time.Now().UTC(),
		})
	}
	return sessions
}

func newLogger(level string) *slog.Logger {
	var slogLevel slog.Level
	switch strings.ToLower(level) {
	case "debug":
		slogLevel = slog.LevelDebug
	case "warn", "warning":
		slogLevel = slog.LevelWarn
	case "error":
		slogLevel = slog.LevelError
	default:
		slogLevel = slog.LevelInfo
	}
	return slog.New(slog.NewTextHandler(os.Stdout, &slog.HandlerOptions{Level: slogLevel}))
}
