import type { JsonObject, JsonValue, ProtocolEvent } from './protocol';
import { isRecord } from './protocol';

export function parseEventPayload(event: ProtocolEvent): JsonObject {
  return isRecord(event.payload) ? (event.payload as JsonObject) : {};
}

export function textField(payload: JsonObject | undefined, key: string): string | undefined {
  const value = payload?.[key];
  return typeof value === 'string' ? value : undefined;
}

export function numberField(payload: JsonObject | undefined, key: string): number | undefined {
  const value = payload?.[key];
  return typeof value === 'number' ? value : undefined;
}

export function eventText(event: ProtocolEvent): string | undefined {
  const payload = parseEventPayload(event);
  return (
    textField(payload, 'delta') ??
    textField(payload, 'text') ??
    textField(payload, 'content') ??
    textField(payload, 'description') ??
    textField(payload, 'title') ??
    textField(payload, 'message') ??
    textField(payload, 'target') ??
    textField(payload, 'tool')
  );
}

export function eventSummary(event: ProtocolEvent): string {
  const payload = parseEventPayload(event);
  const pieces: string[] = [];
  const permissionId = textField(payload, 'permission_id');
  const messageId = textField(payload, 'message_id');
  const status = textField(payload, 'status');
  const decision = textField(payload, 'decision');
  if (event.type === 'permission.requested' || event.type === 'permission.resolved') {
    if (textField(payload, 'description')) pieces.push(String(textField(payload, 'description')));
    if (decision) pieces.push(`决策 ${decision}`);
    else if (permissionId) pieces.push(permissionId);
  } else if (event.type === 'message.delta' || event.type === 'message.completed') {
    const text = eventText(event);
    if (text) pieces.push(text);
    else if (event.type === 'message.delta' && messageId) pieces.push(messageId);
    else if (event.type === 'message.completed') pieces.push('已完成');
  } else if (event.type.startsWith('tool.')) {
    const tool = textField(payload, 'tool');
    const target = textField(payload, 'target');
    if (tool) pieces.push(target ? `${tool} ${target}` : tool);
    if (status) pieces.push(status);
  } else if (event.type === 'session.created' || event.type === 'session.updated') {
    pieces.push(textField(payload, 'title') ?? event.session_id ?? event.type);
  } else {
    const text = eventText(event);
    if (text) pieces.push(text);
    if (status) pieces.push(`状态 ${status}`);
  }
  return pieces.length > 0 ? pieces.join(' · ') : event.type;
}

export function stringifyValue(value: JsonValue): string {
  if (typeof value === 'string') return value;
  if (value === null) return 'null';
  return JSON.stringify(value);
}
