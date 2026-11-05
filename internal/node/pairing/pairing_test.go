package pairing

import (
	"errors"
	"path/filepath"
	"testing"
	"time"

	"github.com/FlyingBassHot/meetagent/pkg/clientproto"
)

func TestPairAuthenticateRevoke(t *testing.T) {
	manager, err := New("", time.Minute, nil)
	if err != nil {
		t.Fatal(err)
	}
	manager.SetCode("123456")

	client, token, err := manager.Pair(clientproto.PairRequest{
		ClientType:   clientproto.ClientMobile,
		Name:         "Test Phone",
		Capabilities: []string{clientproto.CapabilityInputText},
		PairingCode:  "123456",
	})
	if err != nil {
		t.Fatalf("Pair: %v", err)
	}
	if client.ClientID == "" || token == "" {
		t.Fatal("expected client id and token")
	}
	if len(client.Permissions) == 0 {
		t.Fatal("expected default permissions")
	}

	authenticated, err := manager.Authenticate(token)
	if err != nil {
		t.Fatalf("Authenticate: %v", err)
	}
	if authenticated.ClientID != client.ClientID {
		t.Fatalf("client id = %q, want %q", authenticated.ClientID, client.ClientID)
	}
	if _, err := manager.Authenticate("wrong"); !errors.Is(err, ErrUnauthorized) {
		t.Fatalf("wrong token error = %v", err)
	}
	if !manager.Revoke(client.ClientID) {
		t.Fatal("Revoke returned false")
	}
	if _, err := manager.Authenticate(token); !errors.Is(err, ErrUnauthorized) {
		t.Fatalf("revoked token error = %v", err)
	}
}

func TestPairRejectsWrongCode(t *testing.T) {
	manager, err := New("", time.Minute, nil)
	if err != nil {
		t.Fatal(err)
	}
	manager.SetCode("123456")
	_, _, err = manager.Pair(clientproto.PairRequest{
		ClientType:  clientproto.ClientMobile,
		PairingCode: "999999",
	})
	if !errors.Is(err, ErrInvalidCode) {
		t.Fatalf("error = %v, want ErrInvalidCode", err)
	}
}

func TestPairRejectsUnsupportedClientType(t *testing.T) {
	manager, err := New("", time.Minute, nil)
	if err != nil {
		t.Fatal(err)
	}
	manager.SetCode("123456")
	_, _, err = manager.Pair(clientproto.PairRequest{
		ClientType:  "toaster",
		PairingCode: "123456",
	})
	if !errors.Is(err, ErrInvalidClient) {
		t.Fatalf("error = %v, want ErrInvalidClient", err)
	}
}

func TestPairPersistence(t *testing.T) {
	path := filepath.Join(t.TempDir(), "clients.json")
	manager, err := New(path, time.Minute, nil)
	if err != nil {
		t.Fatal(err)
	}
	manager.SetCode("123456")
	client, token, err := manager.Pair(clientproto.PairRequest{
		ClientType:  clientproto.ClientTerminal,
		PairingCode: "123456",
	})
	if err != nil {
		t.Fatal(err)
	}

	reloaded, err := New(path, time.Minute, nil)
	if err != nil {
		t.Fatal(err)
	}
	authenticated, err := reloaded.Authenticate(token)
	if err != nil {
		t.Fatalf("Authenticate after reload: %v", err)
	}
	if authenticated.ClientID != client.ClientID {
		t.Fatalf("reloaded client = %q, want %q", authenticated.ClientID, client.ClientID)
	}
}

func TestExpiredCode(t *testing.T) {
	manager, err := New("", time.Minute, nil)
	if err != nil {
		t.Fatal(err)
	}
	manager.SetCode("123456")
	manager.expires = time.Now().Add(-time.Second)
	_, _, err = manager.Pair(clientproto.PairRequest{
		ClientType:  clientproto.ClientMobile,
		PairingCode: "123456",
	})
	if !errors.Is(err, ErrExpiredCode) {
		t.Fatalf("error = %v, want ErrExpiredCode", err)
	}
}
