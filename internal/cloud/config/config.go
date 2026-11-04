// Package config loads Cloud runtime configuration.
package config

import (
	"fmt"
	"os"
	"strings"
	"time"

	"gopkg.in/yaml.v3"
)

// Duration is a time.Duration that accepts strings such as "20s" in YAML.
type Duration time.Duration

// UnmarshalYAML implements yaml.Unmarshaler.
func (d *Duration) UnmarshalYAML(value *yaml.Node) error {
	var raw string
	if err := value.Decode(&raw); err != nil {
		return err
	}
	parsed, err := time.ParseDuration(raw)
	if err != nil {
		return fmt.Errorf("invalid duration %q: %w", raw, err)
	}
	*d = Duration(parsed)
	return nil
}

// Std returns the standard library duration.
func (d Duration) Std() time.Duration { return time.Duration(d) }

// Config is the root Cloud configuration.
type Config struct {
	Server     ServerConfig     `yaml:"server"`
	Auth       AuthConfig       `yaml:"auth"`
	EventCache EventCacheConfig `yaml:"event_cache"`
	Log        LogConfig        `yaml:"log"`
}

// ServerConfig contains transport and connection tuning.
type ServerConfig struct {
	Addr            string   `yaml:"addr"`
	WSPath          string   `yaml:"ws_path"`
	AllowedOrigins  []string `yaml:"allowed_origins"`
	ReadTimeout     Duration `yaml:"read_timeout"`
	PingInterval    Duration `yaml:"ping_interval"`
	RegisterTimeout Duration `yaml:"register_timeout"`
	PendingTTL      Duration `yaml:"pending_ttl"`
	MaxPending      int      `yaml:"max_pending"`

	// AdminToken guards the HTTP management endpoints (/api/v1/status and
	// /api/v1/snapshot). It is required so a public deployment cannot leak the
	// Node/Device inventory by accident.
	AdminToken string `yaml:"admin_token"`

	// RateLimitPerSecond and RateLimitBurst bound inbound messages per
	// connection. Zero disables the limiter.
	RateLimitPerSecond float64 `yaml:"rate_limit_per_second"`
	RateLimitBurst     int     `yaml:"rate_limit_burst"`
}

// NodeCredential is the static credential of one Node identity.
type NodeCredential struct {
	Token string `yaml:"token"`
}

// DeviceCredential is the static credential and Node ACL of one Device.
type DeviceCredential struct {
	Token        string   `yaml:"token"`
	AllowedNodes []string `yaml:"allowed_nodes"`
}

// AuthConfig maps identity IDs to static credentials. Devices must explicitly
// list the Nodes they may access; there is no implicit access.
type AuthConfig struct {
	Nodes   map[string]NodeCredential   `yaml:"nodes"`
	Devices map[string]DeviceCredential `yaml:"devices"`
}

// EventCacheConfig bounds the in-memory event cache.
type EventCacheConfig struct {
	Capacity       int      `yaml:"capacity"`
	TTL            Duration `yaml:"ttl"`
	SnapshotEvents int      `yaml:"snapshot_events"`
	ReplayLimit    int      `yaml:"replay_limit"`
}

// LogConfig controls structured logging.
type LogConfig struct {
	Level string `yaml:"level"`
}

// Default returns a usable local-development configuration.
func Default() Config {
	return Config{
		Server: ServerConfig{
			Addr:               "127.0.0.1:8080",
			WSPath:             "/ws",
			AllowedOrigins:     []string{"http://localhost:5173", "http://127.0.0.1:5173", "http://localhost:4173", "http://127.0.0.1:4173"},
			ReadTimeout:        Duration(90 * time.Second),
			PingInterval:       Duration(25 * time.Second),
			RegisterTimeout:    Duration(10 * time.Second),
			PendingTTL:         Duration(2 * time.Minute),
			MaxPending:         10000,
			AdminToken:         "dev-admin-token-change-me",
			RateLimitPerSecond: 100,
			RateLimitBurst:     200,
		},
		Auth: AuthConfig{
			Nodes:   map[string]NodeCredential{},
			Devices: map[string]DeviceCredential{},
		},
		EventCache: EventCacheConfig{
			Capacity:       5000,
			TTL:            Duration(10 * time.Minute),
			SnapshotEvents: 50,
			ReplayLimit:    1000,
		},
		Log: LogConfig{Level: "info"},
	}
}

// Load reads YAML from path on top of the default configuration.
func Load(path string) (Config, error) {
	cfg := Default()
	data, err := os.ReadFile(path)
	if err != nil {
		return Config{}, fmt.Errorf("read config %s: %w", path, err)
	}
	if err := yaml.Unmarshal(data, &cfg); err != nil {
		return Config{}, fmt.Errorf("parse config %s: %w", path, err)
	}
	if err := cfg.Validate(); err != nil {
		return Config{}, err
	}
	return cfg, nil
}

// Validate checks required fields and normalizes values.
func (c *Config) Validate() error {
	c.Server.Addr = strings.TrimSpace(c.Server.Addr)
	if c.Server.Addr == "" {
		return fmt.Errorf("server.addr is required")
	}
	c.Server.WSPath = strings.TrimSpace(c.Server.WSPath)
	if c.Server.WSPath == "" {
		c.Server.WSPath = "/ws"
	}
	if !strings.HasPrefix(c.Server.WSPath, "/") {
		return fmt.Errorf("server.ws_path must start with /")
	}
	if c.Server.ReadTimeout.Std() <= 0 {
		return fmt.Errorf("server.read_timeout must be positive")
	}
	if c.Server.PingInterval.Std() <= 0 {
		return fmt.Errorf("server.ping_interval must be positive")
	}
	if c.Server.RegisterTimeout.Std() <= 0 {
		return fmt.Errorf("server.register_timeout must be positive")
	}
	if c.Server.PendingTTL.Std() <= 0 {
		return fmt.Errorf("server.pending_ttl must be positive")
	}
	if c.Server.MaxPending <= 0 {
		return fmt.Errorf("server.max_pending must be positive")
	}
	if strings.TrimSpace(c.Server.AdminToken) == "" {
		return fmt.Errorf("server.admin_token is required")
	}
	if c.Server.RateLimitPerSecond < 0 {
		return fmt.Errorf("server.rate_limit_per_second must be >= 0")
	}
	if c.Server.RateLimitPerSecond > 0 && c.Server.RateLimitBurst <= 0 {
		return fmt.Errorf("server.rate_limit_burst must be positive when rate limiting is enabled")
	}
	if len(c.Server.AllowedOrigins) == 0 {
		return fmt.Errorf("server.allowed_origins must not be empty")
	}
	if c.Auth.Nodes == nil {
		c.Auth.Nodes = map[string]NodeCredential{}
	}
	if c.Auth.Devices == nil {
		c.Auth.Devices = map[string]DeviceCredential{}
	}
	if len(c.Auth.Nodes) == 0 && len(c.Auth.Devices) == 0 {
		return fmt.Errorf("at least one node or device identity must be configured")
	}
	for id, node := range c.Auth.Nodes {
		if strings.TrimSpace(id) == "" {
			return fmt.Errorf("auth.nodes contains an empty identity")
		}
		if strings.TrimSpace(node.Token) == "" {
			return fmt.Errorf("auth.nodes.%s.token is required", id)
		}
	}
	for id, device := range c.Auth.Devices {
		if strings.TrimSpace(id) == "" {
			return fmt.Errorf("auth.devices contains an empty identity")
		}
		if strings.TrimSpace(device.Token) == "" {
			return fmt.Errorf("auth.devices.%s.token is required", id)
		}
		allowed, err := normalizeAllowedNodes(device.AllowedNodes)
		if err != nil {
			return fmt.Errorf("auth.devices.%s.allowed_nodes: %w", id, err)
		}
		device.AllowedNodes = allowed
		c.Auth.Devices[id] = device
	}
	if c.EventCache.Capacity <= 0 {
		return fmt.Errorf("event_cache.capacity must be positive")
	}
	if c.EventCache.TTL.Std() <= 0 {
		return fmt.Errorf("event_cache.ttl must be positive")
	}
	if c.EventCache.SnapshotEvents < 0 {
		return fmt.Errorf("event_cache.snapshot_events must be >= 0")
	}
	if c.EventCache.ReplayLimit <= 0 {
		return fmt.Errorf("event_cache.replay_limit must be positive")
	}
	return nil
}

func normalizeAllowedNodes(values []string) ([]string, error) {
	if len(values) == 0 {
		return nil, fmt.Errorf("must list at least one node (use \"*\" for all)")
	}
	seen := make(map[string]bool, len(values))
	result := make([]string, 0, len(values))
	for _, value := range values {
		value = strings.TrimSpace(value)
		if value == "" {
			return nil, fmt.Errorf("contains an empty node id")
		}
		if seen[value] {
			continue
		}
		seen[value] = true
		result = append(result, value)
	}
	return result, nil
}
