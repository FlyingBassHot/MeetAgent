package config

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"
)

func writeConfig(t *testing.T, body string) string {
	t.Helper()
	path := filepath.Join(t.TempDir(), "cloud.yaml")
	if err := os.WriteFile(path, []byte(body), 0o600); err != nil {
		t.Fatalf("write config: %v", err)
	}
	return path
}

const validConfig = `
server:
  addr: "127.0.0.1:0"
  admin_token: "admin-token"
  allowed_origins:
    - "http://localhost:5173"

auth:
  nodes:
    node-001:
      token: "node-token"
  devices:
    web-001:
      token: "device-token"
      allowed_nodes:
        - " node-001 "
        - "node-001"
        - "node-002"

event_cache:
  capacity: 10
  ttl: 1m
  snapshot_events: 3
  replay_limit: 5
`

func TestLoadValidConfig(t *testing.T) {
	cfg, err := Load(writeConfig(t, validConfig))
	if err != nil {
		t.Fatalf("load: %v", err)
	}
	if cfg.Server.Addr != "127.0.0.1:0" {
		t.Fatalf("addr = %q", cfg.Server.Addr)
	}
	if cfg.Server.WSPath != "/ws" {
		t.Fatalf("default ws_path = %q, want /ws", cfg.Server.WSPath)
	}
	if cfg.EventCache.Capacity != 10 || cfg.EventCache.ReplayLimit != 5 {
		t.Fatalf("event cache = %+v", cfg.EventCache)
	}
	if cfg.EventCache.TTL.Std() != time.Minute {
		t.Fatalf("event cache ttl = %v", cfg.EventCache.TTL.Std())
	}

	device := cfg.Auth.Devices["web-001"]
	want := []string{"node-001", "node-002"}
	if len(device.AllowedNodes) != len(want) {
		t.Fatalf("allowed nodes = %+v, want %+v", device.AllowedNodes, want)
	}
	for i := range want {
		if device.AllowedNodes[i] != want[i] {
			t.Fatalf("allowed nodes = %+v, want %+v", device.AllowedNodes, want)
		}
	}
}

func TestLoadRejectsDeviceWithoutAllowedNodes(t *testing.T) {
	body := strings.Replace(validConfig, `      allowed_nodes:
        - " node-001 "
        - "node-001"
        - "node-002"`, "", 1)
	_, err := Load(writeConfig(t, body))
	if err == nil {
		t.Fatal("expected validation error for missing allowed_nodes")
	}
	if !strings.Contains(err.Error(), "allowed_nodes") {
		t.Fatalf("error = %v, want allowed_nodes detail", err)
	}
}

func TestLoadRejectsEmptyAllowedNodeID(t *testing.T) {
	body := strings.Replace(validConfig, `        - "node-002"`, `        - "  "`, 1)
	_, err := Load(writeConfig(t, body))
	if err == nil || !strings.Contains(err.Error(), "empty node id") {
		t.Fatalf("error = %v, want empty node id", err)
	}
}

func TestLoadRejectsMissingTokens(t *testing.T) {
	body := strings.Replace(validConfig, `      token: "node-token"`, `      token: ""`, 1)
	_, err := Load(writeConfig(t, body))
	if err == nil || !strings.Contains(err.Error(), "token is required") {
		t.Fatalf("error = %v, want token requirement", err)
	}
}

func TestLoadRejectsMissingAdminToken(t *testing.T) {
	body := strings.Replace(validConfig, `admin_token: "admin-token"`, `admin_token: ""`, 1)
	_, err := Load(writeConfig(t, body))
	if err == nil || !strings.Contains(err.Error(), "admin_token") {
		t.Fatalf("error = %v, want admin_token requirement", err)
	}
}

func TestValidateRejectsInvalidEventCache(t *testing.T) {
	cases := []struct {
		name   string
		mutate func(*Config)
		want   string
	}{
		{"capacity", func(c *Config) { c.EventCache.Capacity = 0 }, "event_cache.capacity"},
		{"ttl", func(c *Config) { c.EventCache.TTL = 0 }, "event_cache.ttl"},
		{"snapshot", func(c *Config) { c.EventCache.SnapshotEvents = -1 }, "event_cache.snapshot_events"},
		{"replay", func(c *Config) { c.EventCache.ReplayLimit = 0 }, "event_cache.replay_limit"},
	}
	for _, test := range cases {
		t.Run(test.name, func(t *testing.T) {
			cfg := Default()
			cfg.Auth.Nodes["node-001"] = NodeCredential{Token: "token"}
			test.mutate(&cfg)
			err := cfg.Validate()
			if err == nil || !strings.Contains(err.Error(), test.want) {
				t.Fatalf("error = %v, want %q", err, test.want)
			}
		})
	}
}

func TestValidateNormalizesWildcard(t *testing.T) {
	cfg := Default()
	cfg.Auth.Devices["web-001"] = DeviceCredential{
		Token:        "token",
		AllowedNodes: []string{"* ", "*"},
	}
	if err := cfg.Validate(); err != nil {
		t.Fatalf("validate: %v", err)
	}
	allowed := cfg.Auth.Devices["web-001"].AllowedNodes
	if len(allowed) != 1 || allowed[0] != "*" {
		t.Fatalf("allowed nodes = %+v, want [*]", allowed)
	}
}
