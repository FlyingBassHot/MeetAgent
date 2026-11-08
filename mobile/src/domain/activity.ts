import type { ProtocolEvent } from './protocol';

// 主界面只展示需要人关注的事件；原始事件由开发者开关打开。
const IMPORTANT_TYPES: Record<string, true> = {
  'agent.connected': true,
  'agent.disconnected': true,
  'agent.error': true,
  'session.created': true,
  'session.deleted': true,
  'session.status_changed': true,
  'permission.requested': true,
  'permission.resolved': true,
  'tool.failed': true,
};

export function isImportantEvent(event: ProtocolEvent): boolean {
  return IMPORTANT_TYPES[event.type] === true;
}

export type DayGroupKey = 'today' | 'yesterday' | 'earlier';

export interface DayGroup {
  key: DayGroupKey;
  label: string;
  events: ProtocolEvent[];
}

export function groupEventsByDay(events: ProtocolEvent[], now = new Date()): DayGroup[] {
  const startOfToday = new Date(now.getFullYear(), now.getMonth(), now.getDate()).getTime();
  const startOfYesterday = startOfToday - 86_400_000;
  const groups: DayGroup[] = [
    { key: 'today', label: '今天', events: [] },
    { key: 'yesterday', label: '昨天', events: [] },
    { key: 'earlier', label: '更早', events: [] },
  ];
  for (const event of events) {
    const timestamp = Date.parse(event.occurred_at);
    if (timestamp >= startOfToday) groups[0]?.events.push(event);
    else if (timestamp >= startOfYesterday) groups[1]?.events.push(event);
    else groups[2]?.events.push(event);
  }
  return groups.filter((group) => group.events.length > 0);
}
