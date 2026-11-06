package main

import (
	"testing"
	"time"

	"github.com/FlyingBassHot/meetagent/internal/node/config"
)

func TestServerOptionsMapsRateLimitAndTLS(t *testing.T) {
	cfg := config.Default()
	cfg.Server.Addr = "127.0.0.1:9999"
	cfg.Server.TLSCert = "cert.pem"
	cfg.Server.TLSKey = "key.pem"
	cfg.Server.RateLimitPerSecond = 42
	cfg.Server.RateLimitBurst = 84
	cfg.Server.SnapshotEvents = 7
	cfg.Server.CommandTimeout = config.Duration(3 * time.Second)
	cfg.Server.ReadTimeout = config.Duration(30 * time.Second)

	opts := serverOptions(cfg)
	if opts.Addr != "127.0.0.1:9999" || opts.TLSCert != "cert.pem" || opts.TLSKey != "key.pem" {
		t.Fatalf("listener options = %+v", opts)
	}
	if opts.RateLimitPerSecond != 42 || opts.RateLimitBurst != 84 {
		t.Fatalf("rate limit = %v/%d, want 42/84", opts.RateLimitPerSecond, opts.RateLimitBurst)
	}
	if opts.SnapshotEvents != 7 || opts.CommandTimeout != 3*time.Second || opts.ReadTimeout != 30*time.Second {
		t.Fatalf("timeouts/options = %+v", opts)
	}
}
