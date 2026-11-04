// Package opencode implements the MeetAgent Adapter for the OpenCode v2 HTTP
// and SSE API. It translates OpenCode sessions, prompts, interrupts, permission
// requests and streaming events into the adapter-neutral model.
package opencode

import (
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"strings"
	"time"
)

const (
	defaultTimeout = 15 * time.Second
	maxBodyBytes   = 8 << 20
	sessionLimit   = 200
)

// Options configures one OpenCode connection.
type Options struct {
	BaseURL  string
	Username string
	Password string
	// Directory selects the OpenCode project directory when the server hosts
	// more than one. It is sent as the x-opencode-directory header.
	Directory string
	Timeout   time.Duration
}

func (o Options) withDefaults() Options {
	o.BaseURL = strings.TrimRight(strings.TrimSpace(o.BaseURL), "/")
	if o.Username == "" {
		o.Username = "opencode"
	}
	if o.Timeout <= 0 {
		o.Timeout = defaultTimeout
	}
	return o
}

// client is a small authenticated OpenCode HTTP client.
type client struct {
	baseURL   string
	username  string
	password  string
	directory string
	http      *http.Client
}

func newClient(options Options) *client {
	options = options.withDefaults()
	return &client{
		baseURL:   options.BaseURL,
		username:  options.Username,
		password:  options.Password,
		directory: options.Directory,
		http:      &http.Client{Timeout: options.Timeout},
	}
}

// streamClient has no overall timeout: SSE connections live until canceled.
func (c *client) streamClient() *http.Client {
	return &http.Client{Transport: c.http.Transport}
}

func (c *client) newRequest(ctx context.Context, method, path string, body any) (*http.Request, error) {
	var reader io.Reader
	if body != nil {
		raw, err := json.Marshal(body)
		if err != nil {
			return nil, err
		}
		reader = strings.NewReader(string(raw))
	}
	req, err := http.NewRequestWithContext(ctx, method, c.baseURL+path, reader)
	if err != nil {
		return nil, err
	}
	if c.username != "" || c.password != "" {
		req.SetBasicAuth(c.username, c.password)
	}
	if body != nil {
		req.Header.Set("Content-Type", "application/json")
	}
	req.Header.Set("Accept", "application/json")
	if c.directory != "" {
		req.Header.Set("x-opencode-directory", c.directory)
	}
	return req, nil
}

func (c *client) do(ctx context.Context, method, path string, body, out any) error {
	req, err := c.newRequest(ctx, method, path, body)
	if err != nil {
		return err
	}
	resp, err := c.http.Do(req)
	if err != nil {
		return fmt.Errorf("opencode request %s %s: %w", method, path, err)
	}
	defer resp.Body.Close()

	raw, err := io.ReadAll(io.LimitReader(resp.Body, maxBodyBytes))
	if err != nil {
		return fmt.Errorf("opencode read %s %s: %w", method, path, err)
	}
	if resp.StatusCode == http.StatusUnauthorized || resp.StatusCode == http.StatusForbidden {
		return fmt.Errorf("%w: status %d", errUnauthorized, resp.StatusCode)
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return &apiError{Status: resp.StatusCode, Method: method, Path: path, Body: truncate(string(raw), 300)}
	}
	if out == nil || len(raw) == 0 {
		return nil
	}
	if err := json.Unmarshal(raw, out); err != nil {
		return fmt.Errorf("opencode decode %s %s: %w", method, path, err)
	}
	return nil
}

// apiError carries a non-2xx OpenCode response.
type apiError struct {
	Status int
	Method string
	Path   string
	Body   string
}

func (e *apiError) Error() string {
	return fmt.Sprintf("opencode %s %s returned %d: %s", e.Method, e.Path, e.Status, e.Body)
}

func (e *apiError) NotFound() bool { return e.Status == http.StatusNotFound }

func truncate(value string, limit int) string {
	if len(value) <= limit {
		return value
	}
	return value[:limit] + "…"
}

func escapeSession(sessionID string) string {
	return url.PathEscape(sessionID)
}

func escapePermission(permissionID string) string {
	return url.PathEscape(permissionID)
}
