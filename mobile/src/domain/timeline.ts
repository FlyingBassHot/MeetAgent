import type { ProtocolEvent } from './protocol';
import { parseEventPayload, textField } from './payload';

export type TimelineEntry =
  | {
      kind: 'message';
      key: string;
      messageId: string;
      agentId?: string;
      text: string;
      streaming: boolean;
      interrupted: boolean;
      at: string;
    }
  | { kind: 'single'; key: string; event: ProtocolEvent };

const MAX_ENTRIES = 140;

export function buildTimeline(events: ProtocolEvent[]): TimelineEntry[] {
  const entries: TimelineEntry[] = [];
  const messageIndexes = new Map<string, number>();

  for (const event of events) {
    const payload = parseEventPayload(event);
    const messageId = textField(payload, 'message_id');

    if (event.type === 'message.started' && messageId) {
      if (messageIndexes.has(messageId)) continue;
      messageIndexes.set(messageId, entries.length);
      entries.push({
        kind: 'message',
        key: `msg:${messageId}`,
        messageId,
        agentId: event.agent_id,
        text: textField(payload, 'text') ?? '',
        streaming: true,
        interrupted: false,
        at: event.occurred_at,
      });
      continue;
    }

    if (event.type === 'message.delta' && messageId) {
      const index = messageIndexes.get(messageId);
      const delta = textField(payload, 'delta') ?? '';
      if (index !== undefined && entries[index]?.kind === 'message') {
        const entry = entries[index];
        if (entry.kind === 'message') {
          entry.text += delta;
          entry.streaming = true;
        }
      } else {
        messageIndexes.set(messageId, entries.length);
        entries.push({
          kind: 'message',
          key: `msg:${messageId}`,
          messageId,
          agentId: event.agent_id,
          text: delta,
          streaming: true,
          interrupted: false,
          at: event.occurred_at,
        });
      }
      continue;
    }

    if (event.type === 'message.completed' && messageId) {
      const index = messageIndexes.get(messageId);
      const finalText = textField(payload, 'text');
      if (index !== undefined && entries[index]?.kind === 'message') {
        const entry = entries[index];
        if (entry.kind === 'message') {
          if (finalText) entry.text = finalText;
          entry.streaming = false;
          entry.interrupted = payload['interrupted'] === true;
        }
      } else if (finalText) {
        messageIndexes.set(messageId, entries.length);
        entries.push({
          kind: 'message',
          key: `msg:${messageId}`,
          messageId,
          agentId: event.agent_id,
          text: finalText,
          streaming: false,
          interrupted: payload['interrupted'] === true,
          at: event.occurred_at,
        });
      }
      continue;
    }

    entries.push({ kind: 'single', key: event.event_id, event });
  }

  return entries.slice(-MAX_ENTRIES);
}
