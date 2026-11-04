package auth

import (
	"errors"
	"testing"

	"github.com/FlyingBassHot/meetagent/internal/cloud/config"
	"github.com/FlyingBassHot/meetagent/pkg/protocol"
)

func testConfig() config.AuthConfig {
	return config.AuthConfig{
		Nodes: map[string]config.NodeCredential{
			"node-001": {Token: "node-token"},
			"node-002": {Token: "node-2-token"},
		},
		Devices: map[string]config.DeviceCredential{
			"web-all":     {Token: "all-token", AllowedNodes: []string{"*"}},
			"web-one":     {Token: "one-token", AllowedNodes: []string{"node-001"}},
			"web-unknown": {Token: "unknown-token", AllowedNodes: []string{"node-404"}},
		},
	}
}

func TestAuthenticate(t *testing.T) {
	auth := New(testConfig())

	tests := []struct {
		name     string
		peerType protocol.PeerType
		id       string
		token    string
		wantErr  error
	}{
		{name: "node ok", peerType: protocol.PeerNode, id: "node-001", token: "node-token"},
		{name: "device ok", peerType: protocol.PeerDevice, id: "web-one", token: "one-token"},
		{name: "wrong token", peerType: protocol.PeerNode, id: "node-001", token: "nope", wantErr: ErrInvalidToken},
		{name: "unknown node", peerType: protocol.PeerNode, id: "node-404", token: "node-token", wantErr: ErrUnknownIdentity},
		{name: "unknown device", peerType: protocol.PeerDevice, id: "web-404", token: "x", wantErr: ErrUnknownIdentity},
		{name: "empty token", peerType: protocol.PeerDevice, id: "web-one", token: "", wantErr: ErrInvalidToken},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			err := auth.Authenticate(test.peerType, test.id, test.token)
			if test.wantErr == nil {
				if err != nil {
					t.Fatalf("unexpected error: %v", err)
				}
				return
			}
			if !errors.Is(err, test.wantErr) {
				t.Fatalf("error = %v, want %v", err, test.wantErr)
			}
		})
	}
}

func TestCanAccessNodeDefaultsToDeny(t *testing.T) {
	auth := New(testConfig())

	cases := []struct {
		device string
		node   string
		want   bool
	}{
		{device: "web-all", node: "node-001", want: true},
		{device: "web-all", node: "node-002", want: true},
		{device: "web-all", node: "node-404", want: true},
		{device: "web-one", node: "node-001", want: true},
		{device: "web-one", node: "node-002", want: false},
		{device: "web-unknown", node: "node-404", want: true},
		{device: "web-unknown", node: "node-001", want: false},
		{device: "web-404", node: "node-001", want: false},
		{device: "", node: "node-001", want: false},
	}
	for _, test := range cases {
		if got := auth.CanAccessNode(test.device, test.node); got != test.want {
			t.Errorf("CanAccessNode(%q, %q) = %v, want %v", test.device, test.node, got, test.want)
		}
	}
}

func TestAllowedNodes(t *testing.T) {
	auth := New(testConfig())

	if got := auth.AllowedNodes("web-all"); len(got) != 1 || got[0] != "*" {
		t.Fatalf("allowed nodes for wildcard = %+v", got)
	}
	if got := auth.AllowedNodes("web-one"); len(got) != 1 || got[0] != "node-001" {
		t.Fatalf("allowed nodes for web-one = %+v", got)
	}
	if got := auth.AllowedNodes("missing"); got != nil {
		t.Fatalf("allowed nodes for missing device = %+v", got)
	}

	got := auth.AllowedNodes("web-one")
	got[0] = "mutated"
	if auth.AllowedNodes("web-one")[0] != "node-001" {
		t.Fatal("AllowedNodes returned an aliased slice")
	}
}
