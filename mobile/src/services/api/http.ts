import type { Info, PairRequest, PairResult } from '@/domain/protocol';
import { normalizeBaseUrl } from '@/services/storage/preferences';

export class ApiHttpError extends Error {
  readonly status: number;

  constructor(message: string, status: number) {
    super(message);
    this.name = 'ApiHttpError';
    this.status = status;
  }
}

function errorMessage(payload: unknown, fallback: string): string {
  if (typeof payload === 'object' && payload !== null) {
    const record = payload as Record<string, unknown>;
    const message = record['message'] ?? record['error'];
    if (typeof message === 'string' && message.length > 0) return message;
  }
  return fallback;
}

async function requestJson<T>(url: string, init?: RequestInit): Promise<T> {
  let response: Response;
  try {
    response = await fetch(url, {
      ...init,
      headers: { Accept: 'application/json', ...(init?.headers ?? {}) },
    });
  } catch {
    throw new ApiHttpError('无法访问 Node，请检查地址、局域网或 CORS 配置', 0);
  }
  const text = await response.text();
  let payload: unknown = null;
  if (text.length > 0) {
    try {
      payload = JSON.parse(text);
    } catch {
      payload = text;
    }
  }
  if (!response.ok) {
    throw new ApiHttpError(errorMessage(payload, `Node 返回 HTTP ${response.status}`), response.status);
  }
  return payload as T;
}

export async function fetchInfo(baseUrl: string, signal?: AbortSignal): Promise<Info> {
  const url = `${normalizeBaseUrl(baseUrl)}/api/v2/info`;
  return requestJson<Info>(url, { signal });
}

export async function pairClient(baseUrl: string, request: PairRequest): Promise<PairResult> {
  const url = `${normalizeBaseUrl(baseUrl)}/api/v2/pair`;
  return requestJson<PairResult>(url, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(request),
  });
}
