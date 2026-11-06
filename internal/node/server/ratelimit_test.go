package server

import (
	"testing"
	"time"
)

func TestTokenBucket(t *testing.T) {
	bucket := newTokenBucket(1, 1)
	if !bucket.allow(time.Now()) {
		t.Fatal("first token should be allowed")
	}
	if bucket.allow(time.Now()) {
		t.Fatal("second immediate token should be rejected")
	}
	time.Sleep(1100 * time.Millisecond)
	if !bucket.allow(time.Now()) {
		t.Fatal("token should refill after one second")
	}
}
