package opencode

import (
	"time"

	"github.com/FlyingBassHot/meetagent/internal/node/adapter"
)

// sessionInfo mirrors the OpenCode v2 Session.Info payload.
type sessionInfo struct {
	ID       string `json:"id"`
	ParentID string `json:"parentID"`
	Title    string `json:"title"`
	Agent    string `json:"agent"`
	Outcome  string `json:"outcome"`
	Time     struct {
		Created int64 `json:"created"`
		Updated int64 `json:"updated"`
		Busy    int64 `json:"busy"`
		Idle    int64 `json:"idle"`
	} `json:"time"`
	Location struct {
		Directory string `json:"directory"`
	} `json:"location"`
}

func (s sessionInfo) session(status string) adapter.Session {
	updated := time.UnixMilli(s.Time.Updated)
	if s.Time.Updated <= 0 {
		updated = time.UnixMilli(s.Time.Created)
	}
	if updated.IsZero() || updated.Unix() <= 0 {
		updated = time.Now().UTC()
	}
	return adapter.Session{
		ID:               s.ID,
		Title:            s.Title,
		WorkingDirectory: s.Location.Directory,
		Status:           status,
		UpdatedAt:        updated.UTC(),
	}
}

// sseEvent is the raw envelope of one OpenCode server-sent event.
type sseEvent struct {
	ID      string `json:"id"`
	Created int64  `json:"created"`
	Type    string `json:"type"`
	Data    struct {
		SessionID          string   `json:"sessionID"`
		Title              string   `json:"title"`
		Slug               string   `json:"slug"`
		AssistantMessageID string   `json:"assistantMessageID"`
		MessageID          string   `json:"messageID"`
		Ordinal            int      `json:"ordinal"`
		Delta              string   `json:"delta"`
		Text               string   `json:"text"`
		Finish             string   `json:"finish"`
		Agent              string   `json:"agent"`
		Outcome            string   `json:"outcome"`
		ID                 string   `json:"id"`
		Name               string   `json:"name"`
		Action             string   `json:"action"`
		Resources          []string `json:"resources"`
		Save               []string `json:"save"`
		RequestID          string   `json:"requestID"`
		Reply              string   `json:"reply"`
		Executed           *bool    `json:"executed"`
		Status             any      `json:"status"`
		Location           struct {
			Directory string `json:"directory"`
		} `json:"location"`
		Input  map[string]any `json:"input"`
		Item   map[string]any `json:"item"`
		Source map[string]any `json:"source"`
	} `json:"data"`
	Location struct {
		Directory string `json:"directory"`
	} `json:"location"`
}

func (e sseEvent) timestamp() time.Time {
	if e.Created > 0 {
		return time.UnixMilli(e.Created).UTC()
	}
	return time.Now().UTC()
}

func (e sseEvent) directory() string {
	if e.Data.Location.Directory != "" {
		return e.Data.Location.Directory
	}
	return e.Location.Directory
}
