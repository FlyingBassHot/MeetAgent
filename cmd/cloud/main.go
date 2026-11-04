// Command meetagent-cloud runs the MeetAgent Cloud gateway.
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

	"github.com/FlyingBassHot/meetagent/internal/cloud/config"
	"github.com/FlyingBassHot/meetagent/internal/cloud/server"
)

func main() {
	if err := run(); err != nil {
		fmt.Fprintln(os.Stderr, "meetagent-cloud:", err)
		os.Exit(1)
	}
}

func run() error {
	configPath := flag.String("config", "configs/cloud.yaml", "path to Cloud YAML configuration")
	flag.Parse()

	cfg, err := config.Load(*configPath)
	if err != nil {
		return err
	}

	logger := newLogger(cfg.Log.Level)
	slog.SetDefault(logger)

	srv := server.New(cfg, logger)
	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	defer stop()

	return srv.Run(ctx)
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
