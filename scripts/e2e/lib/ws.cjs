/*
 * Minimal `ws`-compatible shim over Node's built-in WebSocket.
 *
 * The E2E scripts in this directory were originally written against the
 * `ws` package. Node >= 22 ships a global WebSocket, so we keep the test
 * helpers dependency-free and only adapt the small API surface they use:
 * `on('open' | 'message' | 'error')` and the second constructor argument.
 */

const NativeWebSocket = globalThis.WebSocket

if (!NativeWebSocket) {
  throw new Error('This script requires Node.js >= 22 (global WebSocket)')
}

class CompatWebSocket extends NativeWebSocket {
  constructor(url) {
    // Node's WebSocket takes (url, protocols); perMessageDeflate is not needed
    // for local CDP traffic, so the extra options object is dropped.
    super(url)
  }

  on(event, handler) {
    if (event === 'message') {
      this.addEventListener('message', (messageEvent) => handler(messageEvent.data))
    } else {
      this.addEventListener(event, handler)
    }
    return this
  }
}

module.exports = CompatWebSocket
