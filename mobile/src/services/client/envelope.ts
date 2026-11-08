import type { Command, Envelope, JsonObject } from '@/domain/protocol';
import { SCHEMA_VERSION, isRecord } from '@/domain/protocol';

export function newMessageId(): string {
  return `cmsg_${randomToken()}`;
}

export function newCommandId(): string {
  return `cmd_${randomToken()}`;
}

function randomToken(): string {
  if (typeof crypto !== 'undefined' && typeof crypto.randomUUID === 'function') {
    return crypto.randomUUID().replace(/-/g, '').slice(0, 20);
  }
  return `${Date.now().toString(36)}${Math.random().toString(36).slice(2, 10)}`;
}

export function commandEnvelope(command: Command): Envelope {
  return {
    schema_version: SCHEMA_VERSION,
    type: 'command',
    message_id: newMessageId(),
    payload: command,
  };
}

export function heartbeatEnvelope(nonce: string): Envelope {
  return {
    schema_version: SCHEMA_VERSION,
    type: 'heartbeat',
    message_id: newMessageId(),
    payload: { nonce } satisfies JsonObject,
  };
}

export function snapshotEnvelope(): Envelope {
  return {
    schema_version: SCHEMA_VERSION,
    type: 'snapshot',
    message_id: newMessageId(),
  };
}

export function parseEnvelope(raw: string): Envelope | null {
  let parsed: unknown;
  try {
    parsed = JSON.parse(raw);
  } catch {
    return null;
  }
  if (!isRecord(parsed)) return null;
  const type = parsed['type'];
  if (typeof type !== 'string' || type.length === 0) return null;
  const schemaVersion = parsed['schema_version'];
  const messageId = parsed['message_id'];
  return {
    schema_version: typeof schemaVersion === 'number' ? schemaVersion : 0,
    type,
    message_id: typeof messageId === 'string' ? messageId : undefined,
    payload: parsed['payload'],
  };
}
