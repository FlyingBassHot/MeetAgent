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
	"github.com/FlyingBassHot/meetagent/internal/node/adapter/opencode"
	"github.com/FlyingBassHot/meetagent/internal/node/config"
	"github.com/FlyingBassHot/meetagent/internal/node/connection"
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
	manager.Start(ctx)
	defer func() {
		shutdownCtx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
		defer cancel()
		manager.Stop(shutdownCtx)
	}()

	client := connection.New(cfg, manager, logger)
	return client.Run(ctx)
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
