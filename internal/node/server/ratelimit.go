package server

import (
	"sync"
	"time"
)

// tokenBucket is a dependency-free per-connection inbound limiter. A nil bucket
// allows every message (limiter disabled).
type tokenBucket struct {
	mu     sync.Mutex
	rate   float64
	burst  float64
	tokens float64
	last   time.Time
}

func newTokenBucket(ratePerSecond float64, burst int) *tokenBucket {
	if ratePerSecond <= 0 {
		return nil
	}
	if burst <= 0 {
		burst = 1
	}
	return &tokenBucket{
		rate:   ratePerSecond,
		burst:  float64(burst),
		tokens: float64(burst),
		last:   time.Now(),
	}
}

func (b *tokenBucket) allow(now time.Time) bool {
	if b == nil {
		return true
	}
	b.mu.Lock()
	defer b.mu.Unlock()
	if b.last.IsZero() {
		b.last = now
	}
	elapsed := now.Sub(b.last).Seconds()
	if elapsed > 0 {
		b.tokens += elapsed * b.rate
		if b.tokens > b.burst {
			b.tokens = b.burst
		}
		b.last = now
	}
	if b.tokens < 1 {
		return false
	}
	b.tokens--
	return true
}
