/**
 * 单连接守卫：同一浏览器 Origin 内只允许一个标签页持有 Node WebSocket。
 *
 * Web Locks 保证锁随标签页关闭自动释放；BroadcastChannel 用于“接管”通知，
 * 让持有者主动断开后由新标签页获取锁。Capacitor 只有一个 WebView，不受影响。
 */

const LOCK_NAME = 'meetagent.mobile.node-connection';
const CHANNEL_NAME = 'meetagent.mobile.tab-guard';
const TAKEOVER_RETRY_MS = 150;

type TakeoverListener = () => void;

function delay(ms: number): Promise<void> {
  const { promise, resolve } = Promise.withResolvers<void>();
  window.setTimeout(resolve, ms);
  return promise;
}

export class TabGuard {
  private channel: BroadcastChannel | null = null;
  private initialized = false;
  private holding = false;
  private releaseLock: (() => void) | null = null;
  private readonly takeoverListeners = new Set<TakeoverListener>();

  init(): void {
    if (this.initialized) return;
    this.initialized = true;
    if (typeof BroadcastChannel !== 'undefined') {
      this.channel = new BroadcastChannel(CHANNEL_NAME);
      this.channel.onmessage = (message: MessageEvent) => {
        if (message.data !== 'takeover') return;
        for (const listener of this.takeoverListeners) listener();
      };
    }
    window.addEventListener('pagehide', () => this.release());
  }

  isHolding(): boolean {
    return this.holding;
  }

  onTakeover(listener: TakeoverListener): () => void {
    this.takeoverListeners.add(listener);
    return () => {
      this.takeoverListeners.delete(listener);
    };
  }

  requestTakeover(): void {
    this.channel?.postMessage('takeover');
  }

  async acquire(): Promise<boolean> {
    this.init();
    if (this.holding) return true;
    const locks = navigator.locks;
    if (!locks) {
      this.holding = true;
      return true;
    }
    const { promise, resolve } = Promise.withResolvers<boolean>();
    locks
      .request(LOCK_NAME, { ifAvailable: true }, (lock) => {
        if (!lock) {
          resolve(false);
          return;
        }
        this.holding = true;
        resolve(true);
        return new Promise<void>((release) => {
          this.releaseLock = release;
        });
      })
      .catch(() => {
        this.holding = false;
        resolve(false);
      });
    return promise;
  }

  async acquireWithRetry(timeoutMs: number): Promise<boolean> {
    const deadline = Date.now() + timeoutMs;
    while (Date.now() < deadline) {
      if (await this.acquire()) return true;
      await delay(TAKEOVER_RETRY_MS);
    }
    return false;
  }

  release(): void {
    if (!this.holding && !this.releaseLock) return;
    this.holding = false;
    const release = this.releaseLock;
    this.releaseLock = null;
    release?.();
  }
}

export const tabGuard = new TabGuard();
