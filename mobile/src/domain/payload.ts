import type { JsonObject, JsonValue, ProtocolEvent } from './protocol';
import { isRecord } from './protocol';
import {
  agentStatusLabel,
  decisionLabel,
  sessionStatusLabel,
  toolStatusLabel,
} from './labels';

export interface EventSummaryContext {
  sessionTitle?: string;
  agentName?: string;
}

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

export function eventSummary(event: ProtocolEvent, context: EventSummaryContext = {}): string {
  const payload = parseEventPayload(event);
  const status = textField(payload, 'status');
  const decision = textField(payload, 'decision');
  const sessionTitle =
    context.sessionTitle?.trim() || textField(payload, 'title') || '当前会话';
  const agentName = context.agentName?.trim() || 'Agent';

  if (event.type === 'permission.requested') {
    return textField(payload, 'description') ?? '新的审批请求';
  }

  if (event.type === 'permission.resolved') {
    const result = decision ? decisionLabel(decision) : '已处理';
    return `${sessionTitle} · ${result}`;
  }

  if (event.type === 'message.delta' || event.type === 'message.completed') {
    return eventText(event) ?? (event.type === 'message.completed' ? '消息已完成' : '消息更新');
  }

  if (event.type.startsWith('tool.')) {
    const tool = textField(payload, 'tool');
    const target = textField(payload, 'target');
    const subject = tool ? (target ? `${tool} ${target}` : tool) : '工具调用';
    return status ? `${subject} · ${toolStatusLabel(status)}` : subject;
  }

  if (event.type === 'session.status_changed') {
    return status ? `${sessionTitle} · ${sessionStatusLabel(status)}` : sessionTitle;
  }

  if (event.type === 'session.created' || event.type === 'session.updated') {
    return sessionTitle;
  }

  if (event.type === 'session.deleted') {
    return `${sessionTitle} · 已删除`;
  }

  if (event.type === 'agent.status_changed') {
    return status ? `${agentName} · ${agentStatusLabel(status)}` : agentName;
  }

  if (event.type === 'agent.connected') {
    return `${agentName} · 已连接`;
  }

  if (event.type === 'agent.disconnected') {
    return `${agentName} · 已断开`;
  }

  if (event.type === 'agent.error') {
    return `${agentName} · ${eventText(event) ?? '异常'}`;
  }

  return eventText(event) ?? event.type;
}

export function stringifyValue(value: JsonValue): string {
  if (typeof value === 'string') return value;
  if (value === null) return 'null';
  return JSON.stringify(value);
}
