package opencode

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"net/http/httptest"
	"strings"
	"sync"
	"testing"
	"time"

	"github.com/FlyingBassHot/meetagent/internal/node/adapter"
	"github.com/FlyingBassHot/meetagent/pkg/protocol"
)

const testPassword = "secret"

type recordedRequest struct {
	Method    string
	Path      string
	Directory string
	BasicUser string
	BasicPass string
	Body      map[string]any
}

type fakeServer struct {
	mu       sync.Mutex
	requests []recordedRequest
	server   *httptest.Server
}

func newFakeServer(t *testing.T, mux *http.ServeMux) *fakeServer {
	t.Helper()
	fake := &fakeServer{}
	handler := http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		user, pass, _ := r.BasicAuth()
		var body map[string]any
		if r.Body != nil {
			raw, _ := io.ReadAll(io.LimitReader(r.Body, 1<<20))
			if len(raw) > 0 {
				_ = json.Unmarshal(raw, &body)
			}
		}
		fake.mu.Lock()
		fake.requests = append(fake.requests, recordedRequest{
			Method:    r.Method,
			Path:      r.URL.Path,
			Directory: r.Header.Get("x-opencode-directory"),
			BasicUser: user,
			BasicPass: pass,
			Body:      body,
		})
		fake.mu.Unlock()

		if user != "opencode" || pass != testPassword {
			w.WriteHeader(http.StatusUnauthorized)
			return
		}
		mux.ServeHTTP(w, r)
	})
	fake.server = httptest.NewServer(handler)
	t.Cleanup(fake.server.Close)
	return fake
}

func (f *fakeServer) lastRequest(t *testing.T) recordedRequest {
	t.Helper()
	f.mu.Lock()
	defer f.mu.Unlock()
	if len(f.requests) == 0 {
		t.Fatal("no requests recorded")
	}
	return f.requests[len(f.requests)-1]
}

func (f *fakeServer) options() Options {
	return Options{BaseURL: f.server.URL, Password: testPassword, Directory: "/tmp/project"}
}

func TestAdapterConnectAuthenticates(t *testing.T) {
	mux := http.NewServeMux()
	mux.HandleFunc("/api/info", func(w http.ResponseWriter, r *http.Request) {
		fmt.Fprint(w, `{"version":"2.2.5"}`)
	})
	fake := newFakeServer(t, mux)

	a := New(fake.options())
	if err := a.Connect(context.Background()); err != nil {
		t.Fatalf("Connect: %v", err)
	}
	if !a.isConnected() {
		t.Fatal("adapter should be connected")
	}
	req := fake.lastRequest(t)
	if req.BasicUser != "opencode" || req.BasicPass != testPassword {
		t.Fatalf("unexpected basic auth: %q/%q", req.BasicUser, req.BasicPass)
	}
	if req.Directory != "/tmp/project" {
		t.Fatalf("directory header = %q", req.Directory)
	}
}

func TestAdapterConnectRejectsBadCredentials(t *testing.T) {
	mux := http.NewServeMux()
	mux.HandleFunc("/api/info", func(w http.ResponseWriter, r *http.Request) {
		fmt.Fprint(w, `{"version":"2.2.5"}`)
	})
	fake := newFakeServer(t, mux)

	options := fake.options()
	options.Password = "wrong"
	a := New(options)
	err := a.Connect(context.Background())
	if !errors.Is(err, adapter.ErrUnauthorized) {
		t.Fatalf("expected ErrUnauthorized, got %v", err)
	}
	if a.isConnected() {
		t.Fatal("adapter must not be connected after auth failure")
	}
}

func TestAdapterListSessionsMergesActiveState(t *testing.T) {
	mux := http.NewServeMux()
	mux.HandleFunc("/api/info", func(w http.ResponseWriter, r *http.Request) {
		fmt.Fprint(w, `{"version":"2.2.5"}`)
	})
	mux.HandleFunc("/api/session", func(w http.ResponseWriter, r *http.Request) {
		if got := r.URL.Query().Get("order"); got != "desc" {
			t.Errorf("order = %q, want desc", got)
		}
		fmt.Fprint(w, `{"data":[
			{"id":"ses_1","title":"Busy one","time":{"created":1700000000000,"updated":1700000100000},"location":{"directory":"/tmp/project"}},
			{"id":"ses_2","title":"Idle one","time":{"created":1700000000000,"updated":1700000200000},"location":{"directory":"/tmp/project"}}
		]}`)
	})
	mux.HandleFunc("/api/session/active", func(w http.ResponseWriter, r *http.Request) {
		fmt.Fprint(w, `{"data":{"ses_1":{"type":"busy"}}}`)
	})
	fake := newFakeServer(t, mux)

	a := New(fake.options())
	if err := a.Connect(context.Background()); err != nil {
		t.Fatalf("Connect: %v", err)
	}
	sessions, err := a.ListSessions(context.Background())
	if err != nil {
		t.Fatalf("ListSessions: %v", err)
	}
	if len(sessions) != 2 {
		t.Fatalf("sessions = %d, want 2", len(sessions))
	}
	byID := map[string]adapter.Session{}
	for _, session := range sessions {
		byID[session.ID] = session
	}
	if byID["ses_1"].Status != "running" {
		t.Fatalf("ses_1 status = %q, want running", byID["ses_1"].Status)
	}
	if byID["ses_2"].Status != "idle" {
		t.Fatalf("ses_2 status = %q, want idle", byID["ses_2"].Status)
	}
	if byID["ses_1"].Title != "Busy one" || byID["ses_1"].WorkingDirectory != "/tmp/project" {
		t.Fatalf("unexpected session: %+v", byID["ses_1"])
	}
	if byID["ses_1"].UpdatedAt.UnixMilli() != 1700000100000 {
		t.Fatalf("updated = %s", byID["ses_1"].UpdatedAt)
	}
}

func TestAdapterListSessionsToleratesActiveFailure(t *testing.T) {
	mux := http.NewServeMux()
	mux.HandleFunc("/api/info", func(w http.ResponseWriter, r *http.Request) {})
	mux.HandleFunc("/api/session", func(w http.ResponseWriter, r *http.Request) {
		fmt.Fprint(w, `{"data":[{"id":"ses_1","title":"One"}]}`)
	})
	mux.HandleFunc("/api/session/active", func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusInternalServerError)
	})
	fake := newFakeServer(t, mux)

	a := New(fake.options())
	if err := a.Connect(context.Background()); err != nil {
		t.Fatalf("Connect: %v", err)
	}
	sessions, err := a.ListSessions(context.Background())
	if err != nil {
		t.Fatalf("ListSessions should tolerate active failure: %v", err)
	}
	if len(sessions) != 1 || sessions[0].Status != "idle" {
		t.Fatalf("unexpected sessions: %+v", sessions)
	}
}

func TestAdapterSendPromptInterruptAndPermission(t *testing.T) {
	mux := http.NewServeMux()
	mux.HandleFunc("/api/info", func(w http.ResponseWriter, r *http.Request) {})
	mux.HandleFunc("/api/session/ses_1/prompt", func(w http.ResponseWriter, r *http.Request) {
		if r.Method != http.MethodPost {
			t.Errorf("method = %s, want POST", r.Method)
		}
		fmt.Fprint(w, `{"data":{"id":"msg_1"}}`)
	})
	mux.HandleFunc("/api/session/ses_1/interrupt", func(w http.ResponseWriter, r *http.Request) {
		fmt.Fprint(w, `{"interrupted":true}`)
	})
	mux.HandleFunc("/api/session/ses_1/permission/per_1/reply", func(w http.ResponseWriter, r *http.Request) {
		fmt.Fprint(w, `{}`)
	})
	fake := newFakeServer(t, mux)

	a := New(fake.options())
	if err := a.Connect(context.Background()); err != nil {
		t.Fatalf("Connect: %v", err)
	}

	result, err := a.SendPrompt(context.Background(), "ses_1", "do it")
	if err != nil {
		t.Fatalf("SendPrompt: %v", err)
	}
	if result.MessageID != "msg_1" {
		t.Fatalf("message id = %q", result.MessageID)
	}
	req := fake.lastRequest(t)
	if req.Body["text"] != "do it" {
		t.Fatalf("prompt body = %+v", req.Body)
	}

	interrupt, err := a.Interrupt(context.Background(), "ses_1")
	if err != nil {
		t.Fatalf("Interrupt: %v", err)
	}
	if !interrupt.Interrupted {
		t.Fatal("expected interrupted")
	}

	if err := a.RespondPermission(context.Background(), "ses_1", "per_1", "allow", "go ahead"); err != nil {
		t.Fatalf("RespondPermission: %v", err)
	}
	req = fake.lastRequest(t)
	if req.Body["decision"] != "once" {
		t.Fatalf("decision = %v, want once", req.Body["decision"])
	}
	if req.Body["message"] != "go ahead" {
		t.Fatalf("message = %v", req.Body["message"])
	}
}

func TestAdapterMapsNotFoundToSessionNotFound(t *testing.T) {
	mux := http.NewServeMux()
	mux.HandleFunc("/api/info", func(w http.ResponseWriter, r *http.Request) {})
	mux.HandleFunc("/api/session/missing/prompt", func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusNotFound)
		fmt.Fprint(w, `{"error":"no such session"}`)
	})
	fake := newFakeServer(t, mux)

	a := New(fake.options())
	if err := a.Connect(context.Background()); err != nil {
		t.Fatalf("Connect: %v", err)
	}
	if _, err := a.SendPrompt(context.Background(), "missing", "hi"); !errors.Is(err, adapter.ErrSessionNotFound) {
		t.Fatalf("expected ErrSessionNotFound, got %v", err)
	}
}

func TestAdapterOperationsRequireConnection(t *testing.T) {
	a := New(Options{BaseURL: "http://127.0.0.1:1", Password: "x"})
	if _, err := a.ListSessions(context.Background()); err == nil {
		t.Fatal("expected ListSessions to fail when disconnected")
	}
	if _, err := a.SendPrompt(context.Background(), "ses_1", "hi"); err == nil {
		t.Fatal("expected SendPrompt to fail when disconnected")
	}
	if _, err := a.Interrupt(context.Background(), "ses_1"); err == nil {
		t.Fatal("expected Interrupt to fail when disconnected")
	}
	if _, err := a.SubscribeEvents(context.Background()); err == nil {
		t.Fatal("expected SubscribeEvents to fail when disconnected")
	}
}

func TestAdapterSubscribeEvents(t *testing.T) {
	mux := http.NewServeMux()
	mux.HandleFunc("/api/info", func(w http.ResponseWriter, r *http.Request) {})
	mux.HandleFunc("/api/event", func(w http.ResponseWriter, r *http.Request) {
		if got := r.Header.Get("Accept"); !strings.Contains(got, "text/event-stream") {
			t.Errorf("Accept = %q", got)
		}
		w.Header().Set("Content-Type", "text/event-stream")
		w.WriteHeader(http.StatusOK)
		flusher, _ := w.(http.Flusher)
		events := []string{
			`{"id":"evt_1","created":1793762636543,"type":"session.execution.started","data":{"sessionID":"ses_1"}}`,
			`{"id":"evt_2","created":1793762636544,"type":"session.text.delta","data":{"sessionID":"ses_1","assistantMessageID":"msg_1","delta":"hi"}}`,
			`{"id":"evt_3","created":1793762636545,"type":"unknown.event","data":{"sessionID":"ses_1"}}`,
		}
		for _, event := range events {
			fmt.Fprintf(w, "data: %s\n\n", event)
			if flusher != nil {
				flusher.Flush()
			}
		}
		<-r.Context().Done()
	})
	fake := newFakeServer(t, mux)

	a := New(fake.options())
	if err := a.Connect(context.Background()); err != nil {
		t.Fatalf("Connect: %v", err)
	}

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	events, err := a.SubscribeEvents(ctx)
	if err != nil {
		t.Fatalf("SubscribeEvents: %v", err)
	}

	first := nextEvent(t, events)
	if first.Type != protocol.EventSessionStatusChange || first.Status != "running" || first.SessionID != "ses_1" {
		t.Fatalf("unexpected first event: %+v", first)
	}
	second := nextEvent(t, events)
	if second.Type != protocol.EventMessageDelta {
		t.Fatalf("unexpected second event: %+v", second)
	}
	payload := second.Payload.(adapter.MessageEventPayload)
	if payload.Delta != "hi" {
		t.Fatalf("delta = %q", payload.Delta)
	}
	// The unknown event must be dropped rather than forwarded.
	select {
	case event := <-events:
		t.Fatalf("unexpected extra event: %+v", event)
	case <-time.After(200 * time.Millisecond):
	}

	cancel()
	select {
	case _, ok := <-events:
		if ok {
			// Drain a possible buffered event before the channel closes.
			<-events
		}
	case <-time.After(3 * time.Second):
		t.Fatal("event channel was not closed after cancel")
	}
}

func nextEvent(t *testing.T, events <-chan adapter.Event) adapter.Event {
	t.Helper()
	select {
	case event, ok := <-events:
		if !ok {
			t.Fatal("event channel closed unexpectedly")
		}
		return event
	case <-time.After(3 * time.Second):
		t.Fatal("timed out waiting for event")
		return adapter.Event{}
	}
}
