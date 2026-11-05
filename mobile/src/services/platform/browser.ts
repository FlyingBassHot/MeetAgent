import type { PlatformAdapter, PlatformKind } from './types';

export function createWebPlatform(kind: PlatformKind): PlatformAdapter {
  const supported = typeof window !== 'undefined' && 'Notification' in window;

  async function requestNotificationPermission(): Promise<boolean> {
    if (!supported) return false;
    if (Notification.permission === 'granted') return true;
    if (Notification.permission === 'denied') return false;
    const permission = await Notification.requestPermission();
    return permission === 'granted';
  }

  function notify(title: string, body: string): void {
    if (!supported || Notification.permission !== 'granted') return;
    try {
      new Notification(title, { body });
    } catch {
      // 部分移动浏览器要求 Service Worker 才允许通知，静默降级。
    }
  }

  return {
    kind,
    notificationSupported: supported,
    requestNotificationPermission,
    notify,
  };
}
