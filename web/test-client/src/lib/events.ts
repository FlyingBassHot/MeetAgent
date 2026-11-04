/** 事件流聚合：message.delta 累计、tool.* 按 call_id 归组（纯函数）。 */

import type { AgentEvent } from './protocol'

export type EventGroupKind = 'single' | 'delta' | 'tool'

export interface EventGroup {
  key: string
  kind: EventGroupKind
  /** 代表事件（聚合组取最新一条）。 */
  event: AgentEvent
  /** 组内事件条数（single 恒为 1）。 */
  count: number
  /** message.delta 累计文本；其他组为空。 */
  text: string
}

function payloadRecord(event: AgentEvent): Record<string, unknown> {
  const payload = event.payload
  if (payload && typeof payload === 'object' && !Array.isArray(payload)) {
    return payload as Record<string, unknown>
  }
  return {}
}

function stringField(record: Record<string, unknown>, key: string): string {
  const value = record[key]
  return typeof value === 'string' ? value : ''
}

export function eventMessageId(event: AgentEvent): string {
  return stringField(payloadRecord(event), 'message_id')
}

export function eventCallId(event: AgentEvent): string {
  return stringField(payloadRecord(event), 'call_id')
}

export function eventDeltaText(event: AgentEvent): string {
  return stringField(payloadRecord(event), 'delta')
}

export function eventToolName(event: AgentEvent): string {
  return stringField(payloadRecord(event), 'name')
}

export function eventSessionKey(event: AgentEvent): string {
  return `${event.node_id}:${event.session_id ?? ''}`
}

/** 事件唯一键（与事件列表 / 聚合一致）。 */
export function eventKey(event: AgentEvent): string {
  return `${event.node_id}:${event.sequence}`
}

/**
 * 输入应为「最新在前」的事件列表，输出保持首次出现顺序。
 * - message.delta：同 node/session/message_id 合并，累计文本按时间正序拼接。
 * - tool.*：同 node/call_id 合并。
 */
export function groupEvents(list: AgentEvent[]): EventGroup[] {
  const groups: EventGroup[] = []
  const index = new Map<string, number>()

  for (const event of list) {
    if (event.event_type === 'message.delta') {
      const messageId = eventMessageId(event) || event.event_id
      const key = `delta:${eventSessionKey(event)}:${messageId}`
      const position = index.get(key)
      if (position === undefined) {
        index.set(key, groups.length)
        groups.push({ key, kind: 'delta', event, count: 1, text: eventDeltaText(event) })
      } else {
        const group = groups[position]
        group.count += 1
        group.text = eventDeltaText(event) + group.text
      }
      continue
    }

    if (event.event_type.startsWith('tool.')) {
      const callId = eventCallId(event)
      if (callId) {
        const key = `tool:${event.node_id}:${callId}`
        const position = index.get(key)
        if (position === undefined) {
          index.set(key, groups.length)
          groups.push({ key, kind: 'tool', event, count: 1, text: '' })
        } else {
          groups[position].count += 1
        }
        continue
      }
    }

    groups.push({ key: `${event.node_id}:${event.sequence}`, kind: 'single', event, count: 1, text: '' })
  }

  return groups
}

/** 事件列表中最大的时间戳（毫秒）；无有效时间返回 0。 */
export function latestEventAt(list: AgentEvent[]): number {
  let latest = 0
  for (const event of list) {
    const at = Date.parse(event.timestamp) || 0
    if (at > latest) latest = at
  }
  return latest
}

/** 搜索匹配：事件类型 / Node / Agent / Session / payload 文本。 */
export function eventMatchesSearch(event: AgentEvent, term: string): boolean {
  if (!term) return true
  const needle = term.toLowerCase()
  const haystack = [
    event.event_type,
    event.node_id,
    event.agent_id ?? '',
    event.session_id ?? '',
    typeof event.payload === 'string' ? event.payload : JSON.stringify(event.payload ?? ''),
  ]
    .join(' ')
    .toLowerCase()
  return haystack.includes(needle)
}
