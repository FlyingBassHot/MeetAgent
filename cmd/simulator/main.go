// Command simulator is a minimal V2.1 client used to exercise the Node
// standalone server without Mobile or Terminal firmware.
package main

import (
	"context"
	"crypto/tls"
	"encoding/json"
	"flag"
	"fmt"
	"os"
	"strings"
	"time"

	"github.com/FlyingBassHot/meetagent/pkg/clientproto"
	"github.com/gorilla/websocket"
)

func main() {
	if err := run(); err != nil {
		fmt.Fprintln(os.Stderr, "meetagent-simulator:", err)
		os.Exit(1)
	}
}

func run() error {
	url := flag.String("url", "ws://127.0.0.1:8787/ws", "Node WebSocket URL")
	clientType := flag.String("client-type", clientproto.ClientTerminal, "client type: mobile|terminal|web|desktop")
	token := flag.String("token", "", "paired client token")
	pairCode := flag.String("pair-code", "", "pairing code for a first connection")
	capabilities := flag.String("capabilities", "ui.compact,input.text,input.button,notification.local", "comma-separated capabilities")
	prompt := flag.String("prompt", "", "optional prompt to send after connecting")
	agentID := flag.String("agent", "mock-1", "agent id for the prompt")
	sessionID := flag.String("session", "session-1", "session id for the prompt")
	lastSequence := flag.Uint64("last-sequence", 0, "resume after this sequence")
	timeout := flag.Duration("timeout", 30*time.Second, "maximum run time")
	insecure := flag.Bool("insecure", false, "skip TLS certificate verification (local self-signed only)")
	flag.Parse()

	if *token == "" && *pairCode == "" {
		return fmt.Errorf("provide -token or -pair-code")
	}

	dialer := *websocket.DefaultDialer
	if *insecure {
		dialer.TLSClientConfig = &tls.Config{InsecureSkipVerify: true}
	}
	conn, _, err := dialer.DialContext(context.Background(), *url, nil)
	if err != nil {
		return fmt.Errorf("dial %s: %w", *url, err)
	}
	defer conn.Close()

	hello, err := clientproto.NewEnvelope(clientproto.TypeHello, clientproto.Hello{
		ClientType:    *clientType,
		ClientVersion: "simulator",
		Name:          "MeetAgent Simulator",
		Token:         *token,
		PairingCode:   *pairCode,
		Capabilities:  splitList(*capabilities),
		LastSequence:  *lastSequence,
	})
	if err != nil {
		return err
	}
	if err := conn.WriteJSON(hello); err != nil {
		return fmt.Errorf("send hello: %w", err)
	}

	var welcomeEnv clientproto.Envelope
	_ = conn.SetReadDeadline(time.Now().Add(10 * time.Second))
	if err := conn.ReadJSON(&welcomeEnv); err != nil {
		return fmt.Errorf("read welcome: %w", err)
	}
	if welcomeEnv.Type == clientproto.TypeError {
		var payload clientproto.ErrorPayload
		_ = welcomeEnv.DecodePayload(&payload)
		return fmt.Errorf("handshake rejected: %s: %s", payload.Code, payload.Message)
	}
	if welcomeEnv.Type != clientproto.TypeWelcome {
		return fmt.Errorf("expected welcome, got %s", welcomeEnv.Type)
	}
	var welcome clientproto.Welcome
	if err := welcomeEnv.DecodePayload(&welcome); err != nil {
		return err
	}
	fmt.Printf("connected: client=%s node=%s version=%s agents=%d events=%d last_sequence=%d\n",
		welcome.ClientID, welcome.Node.Name, welcome.Node.Version,
		len(welcome.Snapshot.Agents), len(welcome.Snapshot.Events), welcome.Snapshot.LastSequence)
	if welcome.Token != "" {
		fmt.Printf("paired token: %s\n", welcome.Token)
	}

	if *prompt != "" {
		command := clientproto.Command{
			SchemaVersion: clientproto.Version,
			CommandID:     "cmd_" + time.Now().Format("20060102150405.000000"),
			NodeID:        welcome.Node.NodeID,
			AgentID:       *agentID,
			SessionID:     *sessionID,
			Type:          clientproto.CommandSessionPrompt,
			Payload:       mustRaw(clientproto.PromptPayload{Text: *prompt}),
			CreatedAt:     time.Now().UTC(),
		}
		env, err := clientproto.NewEnvelope(clientproto.TypeCommand, command)
		if err != nil {
			return err
		}
		if err := conn.WriteJSON(env); err != nil {
			return fmt.Errorf("send prompt: %w", err)
		}
		fmt.Printf("prompt sent: agent=%s session=%s\n", *agentID, *sessionID)
	}

	deadline := time.Now().Add(*timeout)
	_ = conn.SetReadDeadline(deadline)
	for time.Now().Before(deadline) {
		var env clientproto.Envelope
		if err := conn.ReadJSON(&env); err != nil {
			if strings.Contains(err.Error(), "timeout") {
				break
			}
			return err
		}
		switch env.Type {
		case clientproto.TypeEvent:
			var event clientproto.Event
			if err := env.DecodePayload(&event); err != nil {
				fmt.Println("event decode error:", err)
				continue
			}
			fmt.Printf("event #%d %s agent=%s session=%s\n", event.Sequence, event.Type, event.AgentID, event.SessionID)
			if event.Type == "message.completed" {
				var payload struct {
					Text string `json:"text"`
				}
				_ = json.Unmarshal(event.Payload, &payload)
				if payload.Text != "" {
					fmt.Printf("reply: %s\n", payload.Text)
				}
			}
		case clientproto.TypeCommandResult:
			var result clientproto.CommandResult
			_ = env.DecodePayload(&result)
			fmt.Printf("command result: %s status=%s error=%s\n", result.CommandID, result.Status, result.Error)
		case clientproto.TypeError:
			var payload clientproto.ErrorPayload
			_ = env.DecodePayload(&payload)
			fmt.Printf("error: %s: %s\n", payload.Code, payload.Message)
		default:
			fmt.Printf("message: %s\n", env.Type)
		}
	}
	return nil
}

func splitList(value string) []string {
	parts := strings.Split(value, ",")
	out := make([]string, 0, len(parts))
	for _, part := range parts {
		part = strings.TrimSpace(part)
		if part != "" {
			out = append(out, part)
		}
	}
	return out
}

func mustRaw(value any) json.RawMessage {
	raw, err := json.Marshal(value)
	if err != nil {
		return nil
	}
	return raw
}
