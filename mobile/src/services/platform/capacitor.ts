import type { PlatformAdapter } from './types';
import { createWebPlatform } from './browser';

// Capacitor WebView 提供与浏览器一致的 Notification / localStorage。
// M2 只提供运行时选择；M5 打包时在此接入 Capacitor 插件实现。
export function createCapacitorPlatform(): PlatformAdapter {
  return createWebPlatform('capacitor');
}
