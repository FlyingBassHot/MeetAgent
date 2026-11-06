package config

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"

	"github.com/FlyingBassHot/meetagent/pkg/protocol"
)

func writeConfig(t *testing.T, body string) string {
	t.Helper()
	path := filepath.Join(t.TempDir(), "node.yaml")
	if err := os.WriteFile(path, []byte(body), 0o600); err != nil {
		t.Fatalf("write config: %v", err)
	}
	return path
}

const minimalCloud = `
cloud:
  url: "ws://127.0.0.1:8080/ws"
`

func TestPolicyForUsesDefaults(t *testing.T) {
	cfg := Default()
	cfg.Node.Token = "token"

	policy := cfg.PolicyFor("anything")
	if policy.MaxPromptBytes != defaultMaxPromptBytes {
		t.Fatalf("MaxPromptBytes = %d, want %d", policy.MaxPromptBytes, defaultMaxPromptBytes)
	}
	if policy.Allows(protocol.ActionPermission) {
		t.Fatal("permission_response must not be enabled by default")
	}
	for _, action := range DefaultAllowedActions {
		if !policy.Allows(action) {
			t.Fatalf("default policy does not allow %q", action)
		}
	}
}

func TestPolicyForAgentOverridesNode(t *testing.T) {
	cfg := Default()
	cfg.Node.Token = "token"
	cfg.Node.Policy = PolicyConfig{
		AllowedActions: []string{protocol.ActionPing, protocol.ActionGetStatus},
		MaxPromptBytes: 1024,
	}
	cfg.Node.Agents = []AgentConfig{
		{
			ID: "a1",
			Policy: PolicyConfig{
				AllowedActions: []string{protocol.ActionSendPrompt},
				MaxPromptBytes: 2048,
			},
		},
		{ID: "a2"},
	}

	a1 := cfg.PolicyFor("a1")
	if len(a1.AllowedActions) != 1 || a1.AllowedActions[0] != protocol.ActionSendPrompt {
		t.Fatalf("agent policy did not replace node policy: %+v", a1.AllowedActions)
	}
	if a1.MaxPromptBytes != 2048 {
		t.Fatalf("agent max prompt bytes = %d, want 2048", a1.MaxPromptBytes)
	}

	a2 := cfg.PolicyFor("a2")
	if !a2.Allows(protocol.ActionPing) || a2.Allows(protocol.ActionSendPrompt) {
		t.Fatalf("agent without policy should inherit node policy: %+v", a2.AllowedActions)
	}
	if a2.MaxPromptBytes != 1024 {
		t.Fatalf("agent max prompt bytes = %d, want 1024", a2.MaxPromptBytes)
	}
}

func TestValidateRejectsUnknownAction(t *testing.T) {
	cfg := Default()
	cfg.Node.Token = "token"
	cfg.Node.Policy.AllowedActions = []string{protocol.ActionPing, "shell_exec"}

	err := cfg.Validate()
	if err == nil || !strings.Contains(err.Error(), "unknown action") {
		t.Fatalf("expected unknown action error, got %v", err)
	}
}

func TestValidateRejectsNegativeMaxPromptBytes(t *testing.T) {
	cfg := Default()
	cfg.Node.Token = "token"
	cfg.Node.Policy.MaxPromptBytes = -1

	if err := cfg.Validate(); err == nil {
		t.Fatal("expected error for negative max_prompt_bytes")
	}
}

func TestValidateDefaultsOpenCodeConnection(t *testing.T) {
	path := writeConfig(t, `
node:
  id: "node-test"
  token: "token"
  agents:
    - id: "oc"
      type: "opencode"
      connection:
        base_url: "http://127.0.0.1:4096/"
        password: "secret"
`+minimalCloud)

	cfg, err := Load(path)
	if err != nil {
		t.Fatalf("Load: %v", err)
	}
	agent := cfg.Node.Agents[0]
	if agent.Name != "oc" {
		t.Fatalf("agent name = %q, want oc", agent.Name)
	}
	if !agent.IsEnabled() {
		t.Fatal("agent should be enabled by default")
	}
	if agent.Connection.Username != "opencode" {
		t.Fatalf("username = %q, want opencode", agent.Connection.Username)
	}
	if agent.Connection.Timeout.Std() != 15*time.Second {
		t.Fatalf("timeout = %s, want 15s", agent.Connection.Timeout.Std())
	}
}

func TestLoadResolvesPasswordEnv(t *testing.T) {
	t.Setenv("MEETAGENT_TEST_OPENCODE_PASSWORD", "from-env")

	path := writeConfig(t, `
node:
  id: "node-test"
  token: "token"
  agents:
    - id: "oc"
      type: "opencode"
      connection:
        base_url: "http://127.0.0.1:4096"
        password_env: "MEETAGENT_TEST_OPENCODE_PASSWORD"
`+minimalCloud)

	cfg, err := Load(path)
	if err != nil {
		t.Fatalf("Load: %v", err)
	}
	if got := cfg.Node.Agents[0].Connection.Password; got != "from-env" {
		t.Fatalf("password = %q, want from-env", got)
	}
}

func TestLoadRejectsMissingOpenCodePassword(t *testing.T) {
	path := writeConfig(t, `
node:
  id: "node-test"
  token: "token"
  agents:
    - id: "oc"
      type: "opencode"
      connection:
        base_url: "http://127.0.0.1:4096"
`+minimalCloud)

	_, err := Load(path)
	if err == nil || !strings.Contains(err.Error(), "password or password_env") {
		t.Fatalf("expected password error, got %v", err)
	}
}

func TestLoadRejectsInvalidOpenCodeURL(t *testing.T) {
	path := writeConfig(t, `
node:
  id: "node-test"
  token: "token"
  agents:
    - id: "oc"
      type: "opencode"
      connection:
        base_url: "127.0.0.1:4096"
        password: "secret"
`+minimalCloud)

	_, err := Load(path)
	if err == nil || !strings.Contains(err.Error(), "must be an http(s) URL") {
		t.Fatalf("expected base_url error, got %v", err)
	}
}

func TestDisabledAgentSkipsConnectionValidation(t *testing.T) {
	path := writeConfig(t, `
node:
  id: "node-test"
  token: "token"
  agents:
    - id: "oc"
      type: "opencode"
      enabled: false
`+minimalCloud)

	cfg, err := Load(path)
	if err != nil {
		t.Fatalf("Load: %v", err)
	}
	if cfg.Node.Agents[0].IsEnabled() {
		t.Fatal("agent should be disabled")
	}
}

func TestValidateTrimsAndDeduplicatesActions(t *testing.T) {
	cfg := Default()
	cfg.Node.Token = "token"
	cfg.Node.Policy.AllowedActions = []string{" ping ", "ping", "", protocol.ActionEcho}

	if err := cfg.Validate(); err != nil {
		t.Fatalf("Validate: %v", err)
	}
	got := cfg.Node.Policy.AllowedActions
	want := []string{protocol.ActionPing, protocol.ActionEcho}
	if len(got) != len(want) {
		t.Fatalf("actions = %v, want %v", got, want)
	}
	for i := range want {
		if got[i] != want[i] {
			t.Fatalf("actions = %v, want %v", got, want)
		}
	}
}

func TestValidateTLSRequiresCertAndKey(t *testing.T) {
	for _, setup := range []struct {
		name string
		cert string
		key  string
		ok   bool
	}{
		{name: "plain", ok: true},
		{name: "pair", cert: "data/tls/cert.pem", key: "data/tls/key.pem", ok: true},
		{name: "cert only", cert: "data/tls/cert.pem"},
		{name: "key only", key: "data/tls/key.pem"},
	} {
		t.Run(setup.name, func(t *testing.T) {
			cfg := Default()
			cfg.Server.TLSCert = setup.cert
			cfg.Server.TLSKey = setup.key
			err := cfg.Validate()
			if setup.ok && err != nil {
				t.Fatalf("Validate: %v", err)
			}
			if !setup.ok && (err == nil || !strings.Contains(err.Error(), "must be set together")) {
				t.Fatalf("expected tls pair error, got %v", err)
			}
		})
	}
}

func TestValidateRejectsNegativeRateLimit(t *testing.T) {
	for _, setup := range []struct {
		name  string
		rate  int
		burst int
	}{
		{name: "negative rate", rate: -1},
		{name: "negative burst", burst: -1},
	} {
		t.Run(setup.name, func(t *testing.T) {
			cfg := Default()
			cfg.Server.RateLimitPerSecond = setup.rate
			cfg.Server.RateLimitBurst = setup.burst
			if err := cfg.Validate(); err == nil {
				t.Fatal("expected negative rate limit error")
			}
		})
	}
	if Default().Server.RateLimitPerSecond != 100 || Default().Server.RateLimitBurst != 200 {
		t.Fatalf("default rate limit = %d/%d, want 100/200", Default().Server.RateLimitPerSecond, Default().Server.RateLimitBurst)
	}
}

func TestValidateRequiresPositiveHeartbeat(t *testing.T) {
	cfg := Default()
	cfg.Node.Token = "token"
	cfg.Cloud.Enabled = boolPtr(true)
	cfg.Cloud.HeartbeatInterval = 0

	if err := cfg.Validate(); err == nil {
		t.Fatal("expected error for zero heartbeat interval")
	}
}
