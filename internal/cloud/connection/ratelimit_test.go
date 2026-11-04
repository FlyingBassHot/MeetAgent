package connection

import (
	"testing"
	"time"
)

func TestTokenBucketDropsExcessAndRefills(t *testing.T) {
	bucket := newTokenBucket(2, 2)
	now := time.Now()

	if !bucket.allow(now) || !bucket.allow(now) {
		t.Fatal("initial burst tokens should be available")
	}
	if bucket.allow(now) {
		t.Fatal("third message should be rate limited")
	}

	// 500ms at 2 tokens/s refills exactly one token.
	if !bucket.allow(now.Add(500 * time.Millisecond)) {
		t.Fatal("expected one refilled token")
	}
	if bucket.allow(now.Add(500 * time.Millisecond)) {
		t.Fatal("only one token should be available after refill")
	}
}

func TestTokenBucketDisabled(t *testing.T) {
	if bucket := newTokenBucket(0, 10); bucket != nil {
		t.Fatal("rate 0 should disable the limiter")
	}

	var bucket *tokenBucket
	for i := 0; i < 10; i++ {
		if !bucket.allow(time.Now()) {
			t.Fatal("nil bucket must allow every message")
		}
	}
}

func TestTokenBucketZeroBurstFallsBackToOne(t *testing.T) {
	bucket := newTokenBucket(1, 0)
	now := time.Now()
	if !bucket.allow(now) {
		t.Fatal("first message should be allowed")
	}
	if bucket.allow(now) {
		t.Fatal("zero burst should still allow only one immediate message")
	}
}
