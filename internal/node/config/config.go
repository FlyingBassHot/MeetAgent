// Package config loads Node runtime configuration.
package config

import (
	"fmt"
	"net/url"
	"os"
	"strings"
	"time"

	"github.com/FlyingBassHot/meetagent/pkg/protocol"
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

// Config is the root Node configuration.
type Config struct {
	Node  NodeConfig  `yaml:"node"`
	Cloud CloudConfig `yaml:"cloud"`
	Log   LogConfig   `yaml:"log"`
}

// NodeConfig describes this Node and its Cloud credential.
type NodeConfig struct {
	ID     string        `yaml:"id"`
	Name   string        `yaml:"name"`
	Token  string        `yaml:"token"`
	Policy PolicyConfig  `yaml:"policy"`
	Agents []AgentConfig `yaml:"agents"`
}

// AgentConfig declares one Agent instance hosted by this Node.
type AgentConfig struct {
	ID           string           `yaml:"id"`
	Type         string           `yaml:"type"`
	Name         string           `yaml:"name"`
	Enabled      *bool            `yaml:"enabled"`
	Status       string           `yaml:"status"`
	Capabilities map[string]bool  `yaml:"capabilities"`
	Connection   ConnectionConfig `yaml:"connection"`
	Policy       PolicyConfig     `yaml:"policy"`
	Sessions     []SessionConfig  `yaml:"sessions"`
}

// IsEnabled reports whether the Agent should be started. Adapters are enabled
// by default; an explicit enabled: false keeps the declaration for later.
func (a AgentConfig) IsEnabled() bool {
	return a.Enabled == nil || *a.Enabled
}

// ConnectionConfig describes how to reach a local Agent product.
type ConnectionConfig struct {
	BaseURL     string   `yaml:"base_url"`
	Username    string   `yaml:"username"`
	Password    string   `yaml:"password"`
	PasswordEnv string   `yaml:"password_env"`
	Directory   string   `yaml:"directory"`
	Timeout     Duration `yaml:"timeout"`
}

// PolicyConfig is the Node-side allowlist for remote Agent actions. It can only
// narrow the protocol whitelist, never extend it.
type PolicyConfig struct {
	AllowedActions []string `yaml:"allowed_actions"`
	MaxPromptBytes int      `yaml:"max_prompt_bytes"`
}

// DefaultAllowedActions are the actions a Node accepts unless configured
// otherwise. permission_response must be explicitly enabled because it grants
// local execution privileges.
var DefaultAllowedActions = []string{
	protocol.ActionPing,
	protocol.ActionEcho,
	protocol.ActionGetStatus,
	protocol.ActionSendPrompt,
	protocol.ActionInterrupt,
	protocol.ActionListSessions,
}

const defaultMaxPromptBytes = 32768

// SessionConfig declares a known Agent session.
type SessionConfig struct {
	ID               string `yaml:"id"`
	Title            string `yaml:"title"`
	WorkingDirectory string `yaml:"working_directory"`
	Status           string `yaml:"status"`
}

// CloudConfig controls the outbound Cloud connection.
type CloudConfig struct {
	URL               string          `yaml:"url"`
	RegisterTimeout   Duration        `yaml:"register_timeout"`
	HeartbeatInterval Duration        `yaml:"heartbeat_interval"`
	ReadTimeout       Duration        `yaml:"read_timeout"`
	RequestTimeout    Duration        `yaml:"request_timeout"`
	Reconnect         ReconnectConfig `yaml:"reconnect"`
}

// ReconnectConfig controls exponential backoff.
type ReconnectConfig struct {
	InitialDelay Duration `yaml:"initial_delay"`
	MaxDelay     Duration `yaml:"max_delay"`
	Multiplier   float64  `yaml:"multiplier"`
	Jitter       float64  `yaml:"jitter"`
}

// LogConfig controls structured logging.
type LogConfig struct {
	Level string `yaml:"level"`
}

// Default returns a usable local-development configuration.
func Default() Config {
	return Config{
		Node: NodeConfig{
			ID:    "node-001",
			Name:  "developer-machine",
			Token: "",
			Policy: PolicyConfig{
				AllowedActions: append([]string(nil), DefaultAllowedActions...),
				MaxPromptBytes: defaultMaxPromptBytes,
			},
		},
		Cloud: CloudConfig{
			URL:               "ws://127.0.0.1:8080/ws",
			RegisterTimeout:   Duration(10 * time.Second),
			HeartbeatInterval: Duration(20 * time.Second),
			ReadTimeout:       Duration(60 * time.Second),
			RequestTimeout:    Duration(15 * time.Second),
			Reconnect: ReconnectConfig{
				InitialDelay: Duration(time.Second),
				MaxDelay:     Duration(30 * time.Second),
				Multiplier:   2,
				Jitter:       0.2,
			},
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
	c.Node.ID = strings.TrimSpace(c.Node.ID)
	c.Node.Name = strings.TrimSpace(c.Node.Name)
	if c.Node.ID == "" {
		return fmt.Errorf("node.id is required")
	}
	if c.Node.Token == "" {
		return fmt.Errorf("node.token is required")
	}
	if c.Node.Name == "" {
		c.Node.Name = c.Node.ID
	}
	if err := c.validateAgents(); err != nil {
		return err
	}

	parsed, err := url.Parse(c.Cloud.URL)
	if err != nil {
		return fmt.Errorf("cloud.url is invalid: %w", err)
	}
	if parsed.Scheme != "ws" && parsed.Scheme != "wss" {
		return fmt.Errorf("cloud.url must use ws:// or wss://")
	}
	if parsed.Host == "" {
		return fmt.Errorf("cloud.url must include a host")
	}
	if c.Cloud.RegisterTimeout.Std() <= 0 {
		return fmt.Errorf("cloud.register_timeout must be positive")
	}
	if c.Cloud.HeartbeatInterval.Std() <= 0 {
		return fmt.Errorf("cloud.heartbeat_interval must be positive")
	}
	if c.Cloud.ReadTimeout.Std() < 2*c.Cloud.HeartbeatInterval.Std() {
		return fmt.Errorf("cloud.read_timeout must be at least twice heartbeat_interval")
	}
	if c.Cloud.RequestTimeout.Std() <= 0 {
		return fmt.Errorf("cloud.request_timeout must be positive")
	}
	if c.Cloud.Reconnect.InitialDelay.Std() <= 0 {
		return fmt.Errorf("cloud.reconnect.initial_delay must be positive")
	}
	if c.Cloud.Reconnect.MaxDelay.Std() < c.Cloud.Reconnect.InitialDelay.Std() {
		return fmt.Errorf("cloud.reconnect.max_delay must be >= initial_delay")
	}
	if c.Cloud.Reconnect.Multiplier < 1 {
		return fmt.Errorf("cloud.reconnect.multiplier must be >= 1")
	}
	if c.Cloud.Reconnect.Jitter < 0 || c.Cloud.Reconnect.Jitter > 1 {
		return fmt.Errorf("cloud.reconnect.jitter must be between 0 and 1")
	}
	return nil
}

func (c *Config) validateAgents() error {
	if err := validatePolicy("node.policy", &c.Node.Policy); err != nil {
		return err
	}
	seen := make(map[string]bool, len(c.Node.Agents))
	for i := range c.Node.Agents {
		agent := &c.Node.Agents[i]
		agent.ID = strings.TrimSpace(agent.ID)
		if agent.ID == "" {
			return fmt.Errorf("node.agents[%d].id is required", i)
		}
		if seen[agent.ID] {
			return fmt.Errorf("node.agents contains duplicate id %q", agent.ID)
		}
		seen[agent.ID] = true
		if agent.Type == "" {
			agent.Type = "unknown"
		}
		if agent.Name == "" {
			agent.Name = agent.ID
		}
		if agent.Status == "" {
			agent.Status = "online"
		}
		agent.Connection.BaseURL = strings.TrimSpace(agent.Connection.BaseURL)
		agent.Connection.Username = strings.TrimSpace(agent.Connection.Username)
		agent.Connection.Directory = strings.TrimSpace(agent.Connection.Directory)
		if agent.Connection.Username == "" {
			agent.Connection.Username = "opencode"
		}
		if agent.Connection.Timeout.Std() <= 0 {
			agent.Connection.Timeout = Duration(15 * time.Second)
		}
		if agent.Connection.Password == "" && agent.Connection.PasswordEnv != "" {
			if value, ok := os.LookupEnv(agent.Connection.PasswordEnv); ok {
				agent.Connection.Password = value
			}
		}
		if err := validateAgentConnection(i, agent); err != nil {
			return err
		}
		if err := validatePolicy(fmt.Sprintf("node.agents[%d].policy", i), &agent.Policy); err != nil {
			return err
		}
		sessionSeen := make(map[string]bool, len(agent.Sessions))
		for j := range agent.Sessions {
			session := &agent.Sessions[j]
			session.ID = strings.TrimSpace(session.ID)
			if session.ID == "" {
				return fmt.Errorf("node.agents[%d].sessions[%d].id is required", i, j)
			}
			if sessionSeen[session.ID] {
				return fmt.Errorf("node.agents[%d] contains duplicate session id %q", i, session.ID)
			}
			sessionSeen[session.ID] = true
			if session.Status == "" {
				session.Status = "unknown"
			}
		}
	}
	return nil
}

func validateAgentConnection(index int, agent *AgentConfig) error {
	switch strings.ToLower(agent.Type) {
	case "opencode":
		if !agent.IsEnabled() {
			return nil
		}
		if agent.Connection.BaseURL == "" {
			return fmt.Errorf("node.agents[%d].connection.base_url is required for opencode", index)
		}
		parsed, err := url.Parse(agent.Connection.BaseURL)
		if err != nil || (parsed.Scheme != "http" && parsed.Scheme != "https") || parsed.Host == "" {
			return fmt.Errorf("node.agents[%d].connection.base_url must be an http(s) URL", index)
		}
		if agent.Connection.Password == "" && agent.Connection.PasswordEnv == "" {
			return fmt.Errorf("node.agents[%d].connection.password or password_env is required for opencode", index)
		}
	}
	return nil
}

func validatePolicy(path string, policy *PolicyConfig) error {
	seen := make(map[string]bool, len(policy.AllowedActions))
	actions := make([]string, 0, len(policy.AllowedActions))
	for _, action := range policy.AllowedActions {
		action = strings.TrimSpace(action)
		if action == "" {
			continue
		}
		if !protocol.IsAllowedAction(action) {
			return fmt.Errorf("%s.allowed_actions contains unknown action %q", path, action)
		}
		if seen[action] {
			continue
		}
		seen[action] = true
		actions = append(actions, action)
	}
	policy.AllowedActions = actions
	if policy.MaxPromptBytes < 0 {
		return fmt.Errorf("%s.max_prompt_bytes must not be negative", path)
	}
	return nil
}

// PolicyFor resolves the effective local policy for one Agent. A declaration
// with an empty allowlist inherits the Node-level list; an empty Node-level
// list falls back to the protocol default.
func (c Config) PolicyFor(agentID string) PolicyConfig {
	policy := PolicyConfig{
		AllowedActions: append([]string(nil), DefaultAllowedActions...),
		MaxPromptBytes: defaultMaxPromptBytes,
	}
	if len(c.Node.Policy.AllowedActions) > 0 {
		policy.AllowedActions = append([]string(nil), c.Node.Policy.AllowedActions...)
	}
	if c.Node.Policy.MaxPromptBytes > 0 {
		policy.MaxPromptBytes = c.Node.Policy.MaxPromptBytes
	}
	for _, agent := range c.Node.Agents {
		if agent.ID != agentID {
			continue
		}
		if len(agent.Policy.AllowedActions) > 0 {
			policy.AllowedActions = append([]string(nil), agent.Policy.AllowedActions...)
		}
		if agent.Policy.MaxPromptBytes > 0 {
			policy.MaxPromptBytes = agent.Policy.MaxPromptBytes
		}
		break
	}
	return policy
}

// Allows reports whether action is permitted by the policy.
func (p PolicyConfig) Allows(action string) bool {
	for _, allowed := range p.AllowedActions {
		if allowed == action {
			return true
		}
	}
	return false
}

// ProtocolAgents converts configured Agents into the wire model.
func (c Config) ProtocolAgents() []protocol.AgentView {
	agents := make([]protocol.AgentView, 0, len(c.Node.Agents))
	for _, agent := range c.Node.Agents {
		view := protocol.AgentView{
			AgentID:      agent.ID,
			NodeID:       c.Node.ID,
			Type:         agent.Type,
			Name:         agent.Name,
			Status:       agent.Status,
			Capabilities: agent.Capabilities,
			Sessions:     make([]protocol.SessionView, 0, len(agent.Sessions)),
		}
		for _, session := range agent.Sessions {
			view.Sessions = append(view.Sessions, protocol.SessionView{
				SessionID:        session.ID,
				AgentID:          agent.ID,
				NodeID:           c.Node.ID,
				Title:            session.Title,
				WorkingDirectory: session.WorkingDirectory,
				Status:           session.Status,
				UpdatedAt:        time.Now().UTC(),
			})
		}
		agents = append(agents, view)
	}
	return agents
}
