package opencode

import (
	"bufio"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"strings"

	"github.com/FlyingBassHot/meetagent/internal/node/adapter"
	"github.com/FlyingBassHot/meetagent/pkg/protocol"
)

const maxSSELine = 2 << 20

// streamOnce reads one SSE connection until it fails or ctx is canceled.
func (a *Adapter) streamOnce(ctx context.Context, events chan<- adapter.Event) error {
	req, err := a.client.newRequest(ctx, http.MethodGet, "/api/event", nil)
	if err != nil {
		return err
	}
	req.Header.Set("Accept", "text/event-stream")
	req.Header.Del("Content-Type")

	resp, err := a.client.streamClient().Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()

	if resp.StatusCode == http.StatusUnauthorized || resp.StatusCode == http.StatusForbidden {
		return fmt.Errorf("%w: status %d", errUnauthorized, resp.StatusCode)
	}
	if resp.StatusCode != http.StatusOK {
		body, _ := io.ReadAll(io.LimitReader(resp.Body, 512))
		return &apiError{Status: resp.StatusCode, Method: http.MethodGet, Path: "/api/event", Body: string(body)}
	}

	scanner := bufio.NewScanner(resp.Body)
	scanner.Buffer(make([]byte, 0, 64*1024), maxSSELine)

	var data []string
	dispatch := func() {
		if len(data) == 0 {
			return
		}
		raw := strings.Join(data, "\n")
		data = data[:0]
		event, ok := normalizeEvent([]byte(raw))
		if !ok {
			return
		}
		select {
		case events <- event:
		case <-ctx.Done():
		}
	}

	for scanner.Scan() {
		line := scanner.Text()
		switch {
		case line == "":
			dispatch()
		case strings.HasPrefix(line, ":"):
			// Comment/heartbeat.
		case strings.HasPrefix(line, "data:"):
			value := strings.TrimPrefix(line, "data:")
			value = strings.TrimPrefix(value, " ")
			data = append(data, value)
		case strings.HasPrefix(line, "event:"):
			// OpenCode names the SSE frame "message"; the payload is authoritative.
		}
	}
	dispatch()
	if err := scanner.Err(); err != nil {
		return err
	}
	return errors.New("opencode event stream closed")
}

// normalizeEvent converts one OpenCode event to the shared model. Unknown
// event types are ignored so the adapter tolerates server upgrades.
func normalizeEvent(raw []byte) (adapter.Event, bool) {
	var event sseEvent
	if err := json.Unmarshal(raw, &event); err != nil {
		return adapter.Event{}, false
	}
	at := event.timestamp()
	sessionID := event.Data.SessionID
	if sessionID == "" {
		return adapter.Event{}, false
	}

	switch event.Type {
	case "session.created":
		return adapter.Event{
			Type:      protocol.EventSessionCreated,
			SessionID: sessionID,
			Timestamp: at,
			Payload: adapter.SessionEventPayload{
				SessionID:        sessionID,
				Title:            event.Data.Title,
				WorkingDirectory: event.directory(),
				Status:           "idle",
				UpdatedAt:        at,
			},
		}, true

	case "session.updated", "session.renamed":
		return adapter.Event{
			Type:      protocol.EventSessionUpdated,
			SessionID: sessionID,
			Timestamp: at,
			Payload: adapter.SessionEventPayload{
				SessionID:        sessionID,
				Title:            event.Data.Title,
				WorkingDirectory: event.directory(),
				UpdatedAt:        at,
			},
		}, true

	case "session.deleted", "session.removed":
		return adapter.Event{
			Type:      protocol.EventSessionDeleted,
			SessionID: sessionID,
			Timestamp: at,
			Payload: adapter.SessionEventPayload{
				SessionID: sessionID,
				Status:    "deleted",
				UpdatedAt: at,
			},
		}, true

	case "session.execution.started":
		return protocolStatus(sessionID, "running", at), true

	case "session.execution.succeeded", "session.execution.interrupted", "session.idle":
		return protocolStatus(sessionID, "idle", at), true

	case "session.execution.failed":
		return adapter.Event{
			Type:      protocol.EventAgentError,
			SessionID: sessionID,
			Timestamp: at,
			Status:    "idle",
			Payload: map[string]any{
				"session_id": sessionID,
				"message":    firstNonEmpty(event.Data.Outcome, "execution failed"),
			},
		}, true

	case "session.status":
		status, known := statusValue(event.Data.Status)
		if !known {
			return adapter.Event{}, false
		}
		return protocolStatus(sessionID, status, at), true

	case "session.text.started":
		return adapter.Event{
			Type:      protocol.EventMessageStarted,
			SessionID: sessionID,
			Timestamp: at,
			Payload: adapter.MessageEventPayload{
				MessageID: event.Data.AssistantMessageID,
				Ordinal:   event.Data.Ordinal,
			},
		}, true

	case "session.text.delta":
		return adapter.Event{
			Type:      protocol.EventMessageDelta,
			SessionID: sessionID,
			Timestamp: at,
			Payload: adapter.MessageEventPayload{
				MessageID: event.Data.AssistantMessageID,
				Ordinal:   event.Data.Ordinal,
				Delta:     event.Data.Delta,
			},
		}, true

	case "session.text.ended":
		return adapter.Event{
			Type:      protocol.EventMessageCompleted,
			SessionID: sessionID,
			Timestamp: at,
			Payload: adapter.MessageEventPayload{
				MessageID: event.Data.AssistantMessageID,
				Ordinal:   event.Data.Ordinal,
				Text:      event.Data.Text,
				Finish:    event.Data.Finish,
			},
		}, true

	case "session.tool.called":
		return adapter.Event{
			Type:      protocol.EventToolStarted,
			SessionID: sessionID,
			Timestamp: at,
			Payload: adapter.ToolEventPayload{
				CallID:   event.Data.ID,
				Name:     toolName(event.Data.Input),
				Status:   "running",
				Executed: event.Data.Executed,
			},
		}, true

	case "session.tool.success":
		return adapter.Event{
			Type:      protocol.EventToolCompleted,
			SessionID: sessionID,
			Timestamp: at,
			Payload: adapter.ToolEventPayload{
				CallID:   event.Data.ID,
				Status:   "completed",
				Executed: event.Data.Executed,
			},
		}, true

	case "session.tool.failed", "session.tool.error":
		return adapter.Event{
			Type:      protocol.EventToolFailed,
			SessionID: sessionID,
			Timestamp: at,
			Payload: adapter.ToolEventPayload{
				CallID:   event.Data.ID,
				Status:   "failed",
				Executed: event.Data.Executed,
			},
		}, true

	case "permission.asked":
		payload := protocol.PermissionEventPayload{
			PermissionID: event.Data.ID,
			SessionID:    sessionID,
			Action:       event.Data.Action,
			Resources:    event.Data.Resources,
			Status:       protocol.PermissionPending,
			Description:  permissionDescription(event.Data.Action, event.Data.Resources),
		}
		return adapter.Event{
			Type:      protocol.EventPermissionRequested,
			SessionID: sessionID,
			Timestamp: at,
			Payload:   payload,
		}, true

	case "permission.replied":
		decision := event.Data.Reply
		status := protocol.PermissionRejected
		if decision == "once" || decision == "always" {
			status = protocol.PermissionApproved
		}
		payload := protocol.PermissionEventPayload{
			PermissionID: event.Data.RequestID,
			SessionID:    sessionID,
			Status:       status,
			Decision:     decision,
		}
		return adapter.Event{
			Type:      protocol.EventPermissionResolved,
			SessionID: sessionID,
			Timestamp: at,
			Payload:   payload,
		}, true
	}

	return adapter.Event{}, false
}

// statusValue normalizes the many shapes OpenCode uses for status: a string or
// an object with a "type" field.
func statusValue(value any) (string, bool) {
	switch typed := value.(type) {
	case string:
		return normalizeStatusString(typed)
	case map[string]any:
		for _, key := range []string{"type", "state", "status"} {
			if raw, ok := typed[key].(string); ok {
				return normalizeStatusString(raw)
			}
		}
	}
	return "", false
}

func normalizeStatusString(value string) (string, bool) {
	switch strings.ToLower(value) {
	case "running", "busy", "working":
		return "running", true
	case "idle", "done", "completed":
		return "idle", true
	case "retry", "error", "failed":
		return "error", true
	case "interrupted":
		return "idle", true
	}
	return "", false
}

func toolName(input map[string]any) string {
	if name, ok := input["name"].(string); ok {
		return name
	}
	if tool, ok := input["tool"].(string); ok {
		return tool
	}
	return ""
}

func permissionDescription(action string, resources []string) string {
	if action == "" && len(resources) == 0 {
		return "permission requested"
	}
	if len(resources) == 0 {
		return action
	}
	return strings.TrimSpace(action + " " + strings.Join(resources, ", "))
}

func firstNonEmpty(values ...string) string {
	for _, value := range values {
		if value != "" {
			return value
		}
	}
	return ""
}
