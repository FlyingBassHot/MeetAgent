import type { PlatformAdapter } from './types';
import { createWebPlatform } from './browser';
import { createCapacitorPlatform } from './capacitor';

function isCapacitorRuntime(): boolean {
  if (typeof window === 'undefined') return false;
  const candidate = window as unknown as { Capacitor?: { isNativePlatform?: () => boolean } };
  return candidate.Capacitor?.isNativePlatform?.() === true;
}

export const platform: PlatformAdapter = isCapacitorRuntime()
  ? createCapacitorPlatform()
  : createWebPlatform('browser');
