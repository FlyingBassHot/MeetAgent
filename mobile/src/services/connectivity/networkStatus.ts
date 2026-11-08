import { Capacitor } from '@capacitor/core';
import { Network } from '@capacitor/network';

type NetworkListener = (online: boolean) => void;

const listeners = new Set<NetworkListener>();
let online = typeof navigator === 'undefined' ? true : navigator.onLine;
let initialized = false;

function setOnline(value: boolean): void {
  if (online === value) return;
  online = value;
  for (const listener of listeners) listener(value);
}

export function isNetworkOnline(): boolean {
  return online;
}

export function subscribeNetwork(listener: NetworkListener): () => void {
  listeners.add(listener);
  listener(online);
  return () => {
    listeners.delete(listener);
  };
}

function bindWebEvents(): void {
  const sync = (): void => setOnline(navigator.onLine);
  window.addEventListener('online', sync);
  window.addEventListener('offline', sync);
  setOnline(navigator.onLine);
}

export async function initNetworkStatus(): Promise<void> {
  if (initialized) return;
  initialized = true;
  if (Capacitor.isNativePlatform()) {
    try {
      const refresh = async (): Promise<void> => {
        try {
          const status = await Network.getStatus();
          setOnline(status.connected);
        } catch {
          // 保持最后一次状态，由 WebSocket 重连结果兜底。
        }
      };
      await refresh();
      await Network.addListener('networkStatusChange', (next) => {
        setOnline(next.connected);
      });
      window.setInterval(() => {
        void refresh();
      }, 4000);
      document.addEventListener('visibilitychange', () => {
        if (document.visibilityState === 'visible') void refresh();
      });
      return;
    } catch {
      // 插件不可用时退回浏览器事件；不会阻断连接流程。
    }
  }
  bindWebEvents();
}
