import type { ProtocolEvent } from './protocol';
import { eventSummary, eventText } from './payload';

export function latestSummaryBySession(events: ProtocolEvent[]): Record<string, string> {
  const summaries: Record<string, string> = {};
  for (const event of events) {
    if (!event.session_id) continue;
    if (
      event.type === 'message.started' ||
      event.type === 'session.status_changed' ||
      event.type === 'agent.status_changed'
    ) {
      continue;
    }
    if (event.type === 'message.completed' && !eventText(event)) continue;
    const summary = eventSummary(event);
    if (summary && summary !== event.type) summaries[event.session_id] = summary;
  }
  return summaries;
}
