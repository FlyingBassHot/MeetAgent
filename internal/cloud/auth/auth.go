// Package auth validates static Node and Device credentials and enforces the
// Device -> Node access policy.
//
// Static tokens are an intentional MVP trade-off. A later phase replaces them
// with a pairing flow and per-device credentials, but the call sites can stay
// the same.
package auth

import (
	"crypto/subtle"
	"errors"
	"fmt"

	"github.com/FlyingBassHot/meetagent/internal/cloud/config"
	"github.com/FlyingBassHot/meetagent/pkg/protocol"
)

var (
	ErrUnknownIdentity = errors.New("unknown identity")
	ErrInvalidToken    = errors.New("invalid token")
	ErrNodeForbidden   = errors.New("device is not authorized for node")
)

type devicePolicy struct {
	token      string
	allowAll   bool
	allowed    map[string]bool
	allowedIDs []string
}

// Authenticator checks tokens and Device -> Node authorization.
type Authenticator struct {
	nodes   map[string]string
	devices map[string]devicePolicy
}

// New builds an Authenticator from configuration.
func New(cfg config.AuthConfig) *Authenticator {
	nodes := make(map[string]string, len(cfg.Nodes))
	for id, credential := range cfg.Nodes {
		nodes[id] = credential.Token
	}
	devices := make(map[string]devicePolicy, len(cfg.Devices))
	for id, credential := range cfg.Devices {
		policy := devicePolicy{
			token:      credential.Token,
			allowed:    make(map[string]bool, len(credential.AllowedNodes)),
			allowedIDs: append([]string(nil), credential.AllowedNodes...),
		}
		for _, nodeID := range credential.AllowedNodes {
			if nodeID == "*" {
				policy.allowAll = true
			}
			policy.allowed[nodeID] = true
		}
		devices[id] = policy
	}
	return &Authenticator{nodes: nodes, devices: devices}
}

// Authenticate checks the identity and token pair for the given peer type.
func (a *Authenticator) Authenticate(peerType protocol.PeerType, id, token string) error {
	if id == "" || token == "" {
		return ErrInvalidToken
	}

	var expected string
	var ok bool
	switch peerType {
	case protocol.PeerNode:
		expected, ok = a.nodes[id]
	case protocol.PeerDevice:
		policy, found := a.devices[id]
		expected, ok = policy.token, found
	default:
		return fmt.Errorf("unsupported peer type %q", peerType)
	}
	if !ok {
		return ErrUnknownIdentity
	}
	if subtle.ConstantTimeCompare([]byte(expected), []byte(token)) != 1 {
		return ErrInvalidToken
	}
	return nil
}

// CanAccessNode reports whether a Device may observe or command a Node. The
// default is deny, including for unknown Devices.
func (a *Authenticator) CanAccessNode(deviceID, nodeID string) bool {
	policy, ok := a.devices[deviceID]
	if !ok {
		return false
	}
	if policy.allowAll {
		return true
	}
	return policy.allowed[nodeID]
}

// AllowedNodes returns the configured ACL of a Device. "*" means every Node.
func (a *Authenticator) AllowedNodes(deviceID string) []string {
	policy, ok := a.devices[deviceID]
	if !ok {
		return nil
	}
	if policy.allowAll {
		return []string{"*"}
	}
	return append([]string(nil), policy.allowedIDs...)
}

// HasNode reports whether a Node identity is configured.
func (a *Authenticator) HasNode(id string) bool {
	_, ok := a.nodes[id]
	return ok
}

// HasDevice reports whether a Device identity is configured.
func (a *Authenticator) HasDevice(id string) bool {
	_, ok := a.devices[id]
	return ok
}
