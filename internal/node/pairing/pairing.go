// Package pairing implements Node-local client pairing and token storage.
//
// The Node prints a short-lived pairing code. A Mobile or Terminal client
// exchanges that code for a long-lived per-client token; only a SHA-256 hash of
// the token is persisted.
package pairing

import (
	"crypto/rand"
	"crypto/sha256"
	"crypto/subtle"
	"encoding/base64"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"time"

	"github.com/FlyingBassHot/meetagent/pkg/clientproto"
)

var (
	// ErrInvalidCode means the supplied pairing code is wrong.
	ErrInvalidCode = errors.New("invalid pairing code")
	// ErrExpiredCode means the pairing code has expired.
	ErrExpiredCode = errors.New("pairing code expired")
	// ErrUnauthorized means the token is unknown or revoked.
	ErrUnauthorized = errors.New("unknown or revoked client token")
	// ErrInvalidClient means the client declaration is invalid.
	ErrInvalidClient = errors.New("invalid client declaration")
)

const (
	defaultCodeTTL     = 10 * time.Minute
	maxCodeFailures    = 5
	defaultTokenLength = 32
)

// Client is one paired client identity.
type Client struct {
	ClientID     string     `json:"client_id"`
	Name         string     `json:"name,omitempty"`
	Type         string     `json:"client_type"`
	Version      string     `json:"client_version,omitempty"`
	Capabilities []string   `json:"capabilities,omitempty"`
	Permissions  []string   `json:"permissions"`
	TokenHash    string     `json:"token_hash"`
	CreatedAt    time.Time  `json:"created_at"`
	LastSeen     *time.Time `json:"last_seen,omitempty"`
}

// Manager stores paired clients and the current pairing code.
type Manager struct {
	mu       sync.Mutex
	path     string
	codeTTL  time.Duration
	now      func() time.Time
	code     string
	expires  time.Time
	failures int
	clients  map[string]*Client
	byToken  map[string]*Client
	defaults []string
}

type storeFile struct {
	Clients []Client `json:"clients"`
}

// New creates a pairing manager. path may be empty for memory-only mode.
func New(path string, codeTTL time.Duration, defaultPermissions []string) (*Manager, error) {
	if codeTTL <= 0 {
		codeTTL = defaultCodeTTL
	}
	if len(defaultPermissions) == 0 {
		defaultPermissions = []string{
			clientproto.PermissionSessionRead,
			clientproto.PermissionSessionControl,
			clientproto.PermissionPermissionRespond,
		}
	}
	m := &Manager{
		path:     strings.TrimSpace(path),
		codeTTL:  codeTTL,
		now:      time.Now,
		clients:  make(map[string]*Client),
		byToken:  make(map[string]*Client),
		defaults: append([]string(nil), defaultPermissions...),
	}
	if m.path != "" {
		if err := m.load(); err != nil {
			return nil, err
		}
	}
	m.rotateLocked()
	return m, nil
}

// CurrentCode returns the active pairing code, rotating it when expired.
func (m *Manager) CurrentCode() string {
	m.mu.Lock()
	defer m.mu.Unlock()
	if m.code == "" || !m.now().Before(m.expires) {
		m.rotateLocked()
	}
	return m.code
}

// CodeExpires returns the current code expiry.
func (m *Manager) CodeExpires() time.Time {
	m.mu.Lock()
	defer m.mu.Unlock()
	return m.expires
}

// SetCode sets a fixed code for tests and local development. It also resets
// the failure counter and expiry.
func (m *Manager) SetCode(code string) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.code = code
	m.expires = m.now().Add(m.codeTTL)
	m.failures = 0
}

// Pair exchanges a pairing code for a per-client token.
func (m *Manager) Pair(req clientproto.PairRequest) (Client, string, error) {
	if !clientproto.ValidClientType(req.ClientType) {
		return Client{}, "", fmt.Errorf("%w: unsupported client_type %q", ErrInvalidClient, req.ClientType)
	}
	if strings.TrimSpace(req.PairingCode) == "" {
		return Client{}, "", ErrInvalidCode
	}

	m.mu.Lock()
	defer m.mu.Unlock()

	if m.code == "" || !m.now().Before(m.expires) {
		return Client{}, "", ErrExpiredCode
	}
	if subtle.ConstantTimeCompare([]byte(req.PairingCode), []byte(m.code)) != 1 {
		m.failures++
		if m.failures >= maxCodeFailures {
			m.rotateLocked()
		}
		return Client{}, "", ErrInvalidCode
	}

	token, err := randomToken(defaultTokenLength)
	if err != nil {
		return Client{}, "", fmt.Errorf("generate client token: %w", err)
	}
	client := Client{
		ClientID:     "cli_" + randomHex(8),
		Name:         strings.TrimSpace(req.Name),
		Type:         req.ClientType,
		Version:      strings.TrimSpace(req.ClientVersion),
		Capabilities: normalizeList(req.Capabilities),
		Permissions:  append([]string(nil), m.defaults...),
		TokenHash:    tokenHash(token),
		CreatedAt:    m.now().UTC(),
	}
	if client.Name == "" {
		client.Name = client.Type
	}
	m.clients[client.ClientID] = &client
	m.byToken[client.TokenHash] = &client
	m.rotateLocked()
	if err := m.saveLocked(); err != nil {
		return Client{}, "", err
	}
	return client, token, nil
}

// Authenticate resolves a bearer token to a paired client.
func (m *Manager) Authenticate(token string) (*Client, error) {
	token = strings.TrimSpace(token)
	if token == "" {
		return nil, ErrUnauthorized
	}
	m.mu.Lock()
	defer m.mu.Unlock()
	client, ok := m.byToken[tokenHash(token)]
	if !ok {
		return nil, ErrUnauthorized
	}
	now := m.now().UTC()
	client.LastSeen = &now
	copy := *client
	return &copy, nil
}

// Revoke removes one client identity. Existing connections are closed by the
// Node server when it observes the revoke; this method only updates storage.
func (m *Manager) Revoke(clientID string) bool {
	m.mu.Lock()
	defer m.mu.Unlock()
	client, ok := m.clients[clientID]
	if !ok {
		return false
	}
	delete(m.clients, clientID)
	delete(m.byToken, client.TokenHash)
	_ = m.saveLocked()
	return true
}

// Clients returns a copy of the paired client list.
func (m *Manager) Clients() []Client {
	m.mu.Lock()
	defer m.mu.Unlock()
	out := make([]Client, 0, len(m.clients))
	for _, client := range m.clients {
		out = append(out, *client)
	}
	return out
}

func (m *Manager) rotateLocked() {
	m.code = randomCode()
	m.expires = m.now().Add(m.codeTTL)
	m.failures = 0
}

func (m *Manager) load() error {
	data, err := os.ReadFile(m.path)
	if err != nil {
		if errors.Is(err, os.ErrNotExist) {
			return nil
		}
		return fmt.Errorf("read pairing store: %w", err)
	}
	var stored storeFile
	if err := json.Unmarshal(data, &stored); err != nil {
		return fmt.Errorf("parse pairing store: %w", err)
	}
	for index := range stored.Clients {
		client := stored.Clients[index]
		if client.ClientID == "" || client.TokenHash == "" {
			continue
		}
		copy := client
		m.clients[copy.ClientID] = &copy
		m.byToken[copy.TokenHash] = &copy
	}
	return nil
}

func (m *Manager) saveLocked() error {
	if m.path == "" {
		return nil
	}
	file := storeFile{Clients: make([]Client, 0, len(m.clients))}
	for _, client := range m.clients {
		file.Clients = append(file.Clients, *client)
	}
	data, err := json.MarshalIndent(file, "", "  ")
	if err != nil {
		return fmt.Errorf("encode pairing store: %w", err)
	}
	if err := os.MkdirAll(filepath.Dir(m.path), 0o700); err != nil {
		return fmt.Errorf("create pairing store dir: %w", err)
	}
	temp, err := os.CreateTemp(filepath.Dir(m.path), ".clients-*.json")
	if err != nil {
		return fmt.Errorf("create pairing store temp: %w", err)
	}
	tempName := temp.Name()
	defer os.Remove(tempName)
	if err := temp.Chmod(0o600); err != nil {
		_ = temp.Close()
		return fmt.Errorf("chmod pairing store: %w", err)
	}
	if _, err := temp.Write(data); err != nil {
		_ = temp.Close()
		return fmt.Errorf("write pairing store: %w", err)
	}
	if err := temp.Close(); err != nil {
		return fmt.Errorf("close pairing store: %w", err)
	}
	if err := os.Rename(tempName, m.path); err != nil {
		return fmt.Errorf("replace pairing store: %w", err)
	}
	return nil
}

func randomCode() string {
	var buf [4]byte
	if _, err := rand.Read(buf[:]); err != nil {
		return fmt.Sprintf("%06d", time.Now().UnixNano()%1_000_000)
	}
	value := uint32(buf[0])<<24 | uint32(buf[1])<<16 | uint32(buf[2])<<8 | uint32(buf[3])
	return fmt.Sprintf("%06d", value%1_000_000)
}

func randomToken(length int) (string, error) {
	buf := make([]byte, length)
	if _, err := rand.Read(buf); err != nil {
		return "", err
	}
	return base64.RawURLEncoding.EncodeToString(buf), nil
}

func randomHex(length int) string {
	buf := make([]byte, length)
	if _, err := rand.Read(buf); err != nil {
		return fmt.Sprintf("%d", time.Now().UnixNano())
	}
	return hex.EncodeToString(buf)
}

func tokenHash(token string) string {
	sum := sha256.Sum256([]byte(token))
	return hex.EncodeToString(sum[:])
}

func normalizeList(values []string) []string {
	seen := make(map[string]bool, len(values))
	out := make([]string, 0, len(values))
	for _, value := range values {
		value = strings.TrimSpace(value)
		if value == "" || seen[value] {
			continue
		}
		seen[value] = true
		out = append(out, value)
	}
	return out
}
