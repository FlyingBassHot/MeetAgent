export type ClientMode = 'mock' | 'live';

export interface StoredPreferences {
  mode: ClientMode;
  baseUrl: string;
  token: string | null;
  deviceName: string;
  autoConnect: boolean;
  notifyEnabled: boolean;
  showRawEvents: boolean;
}

const STORAGE_KEY = 'meetagent.mobile.preferences.v1';

export const DEFAULT_BASE_URL = 'http://192.168.1.10:8787';

export const DEFAULT_PREFERENCES: StoredPreferences = {
  mode: 'mock',
  baseUrl: DEFAULT_BASE_URL,
  token: null,
  deviceName: 'Mobile Web',
  autoConnect: false,
  notifyEnabled: false,
  showRawEvents: false,
};

function readString(source: Record<string, unknown>, key: string): string | undefined {
  const value = source[key];
  return typeof value === 'string' ? value : undefined;
}

function readBoolean(source: Record<string, unknown>, key: string): boolean | undefined {
  const value = source[key];
  return typeof value === 'boolean' ? value : undefined;
}

function parsePreferences(raw: string): StoredPreferences | null {
  let parsed: unknown;
  try {
    parsed = JSON.parse(raw);
  } catch {
    return null;
  }
  if (typeof parsed !== 'object' || parsed === null || Array.isArray(parsed)) return null;
  const source = parsed as Record<string, unknown>;
  const mode = readString(source, 'mode');
  const baseUrl = readString(source, 'baseUrl');
  const deviceName = readString(source, 'deviceName');
  const tokenValue = source['token'];
  return {
    mode: mode === 'live' ? 'live' : 'mock',
    baseUrl: baseUrl && baseUrl.length > 0 ? baseUrl : DEFAULT_BASE_URL,
    token: typeof tokenValue === 'string' && tokenValue.length > 0 ? tokenValue : null,
    deviceName: deviceName && deviceName.length > 0 ? deviceName : DEFAULT_PREFERENCES.deviceName,
    autoConnect: readBoolean(source, 'autoConnect') ?? false,
    notifyEnabled: readBoolean(source, 'notifyEnabled') ?? false,
    showRawEvents: readBoolean(source, 'showRawEvents') ?? false,
  };
}

export function loadPreferences(): StoredPreferences {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    if (!raw) return { ...DEFAULT_PREFERENCES };
    return parsePreferences(raw) ?? { ...DEFAULT_PREFERENCES };
  } catch {
    return { ...DEFAULT_PREFERENCES };
  }
}

export function savePreferences(preferences: StoredPreferences): void {
  try {
    localStorage.setItem(STORAGE_KEY, JSON.stringify(preferences));
  } catch {
    // 存储不可用（隐私模式/配额）时保持内存态，不影响本次会话。
  }
}

export function clearPreferences(): void {
  try {
    localStorage.removeItem(STORAGE_KEY);
  } catch {
    // 同上。
  }
}

export function normalizeBaseUrl(input: string): string {
  const trimmed = input.trim();
  if (trimmed.length === 0) return DEFAULT_BASE_URL;
  const withScheme = /^[a-z]+:\/\//i.test(trimmed) ? trimmed : `http://${trimmed}`;
  try {
    const url = new URL(withScheme);
    return `${url.protocol}//${url.host}${url.pathname.replace(/\/+$/, '')}`;
  } catch {
    return withScheme.replace(/\/+$/, '');
  }
}

export function toWebSocketUrl(baseUrl: string): string {
  const normalized = normalizeBaseUrl(baseUrl);
  const url = new URL(normalized);
  url.protocol = url.protocol === 'https:' ? 'wss:' : 'ws:';
  url.pathname = `${url.pathname.replace(/\/+$/, '')}/ws`;
  return url.toString();
}
