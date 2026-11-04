/**
 * Events store：事件流、lastSequences 游标、权限审批、在途请求、Activity 日志。
 * 不直接碰 WebSocket；需要取消请求定时器时调用 cloudClient.cancelRequest()。
 */

import { defineStore } from 'pinia'
import { computed, ref } from 'vue'
import {
  type AgentEvent,
  type CommandResult,
  type CommandStatus,
  type Envelope,
  type EventReplayResult,
  type PermissionView,
  type Snapshot,
} from '../lib/protocol'
import { summarize } from '../lib/format'
import type { LogDirection } from '../api/cloud-client'
import { cloudClient } from '../api/cloud-client'
import { useInventoryStore } from './inventory'

export type { LogDirection }

export interface LogEntry {
  id: number
  time: string
  direction: LogDirection
  type: string
  title: string
  detail?: string
}

export interface PendingView {
  requestId: string
  action: string
  nodeId: string
  sentAt: number
  status: CommandStatus | 'sent'
}

const MAX_EVENTS = 300
const MAX_LOGS = 300

let logId = 0

export const useEventsStore = defineStore('events', () => {
  const events = ref<AgentEvent[]>([])
  const lastSequences = ref<Record<string, number>>({})
  const pendingPermissions = ref<PermissionView[]>([])
  const pending = ref<PendingView[]>([])
  const logs = ref<LogEntry[]>([])
  const showHeartbeats = ref(false)

  const inventory = useInventoryStore()

  const recentEvents = computed(() => [...events.value].reverse())
  const selectedNodePermissions = computed(() =>
    pendingPermissions.value.filter((item) => item.node_id === inventory.selectedNodeId),
  )

  // ---------- Activity 日志 ----------

  function addLog(direction: LogDirection, type: string, title: string, detail?: string): void {
    logs.value.unshift({
      id: ++logId,
      time: new Date().toLocaleTimeString('zh-CN', { hour12: false }),
      direction,
      type,
      title,
      detail: detail || undefined,
    })
    if (logs.value.length > MAX_LOGS) {
      logs.value.length = MAX_LOGS
    }
  }

  function clearLogs(): void {
    logs.value = []
  }

  // ---------- 在途请求 ----------

  /** 登记在途请求并启动 15s 超时定时器（由 commands store 调用）。 */
  function addPending(requestId: string, action: string, nodeId: string): void {
    pending.value.push({
      requestId,
      action,
      nodeId,
      sentAt: Date.now(),
      status: 'sent',
    })
    cloudClient.trackRequest(requestId, action, nodeId)
  }

  /** 移除在途请求并取消其超时定时器（幂等）。 */
  function removePending(requestId: string): void {
    cloudClient.cancelRequest(requestId)
    const index = pending.value.findIndex((item) => item.requestId === requestId)
    if (index >= 0) {
      pending.value.splice(index, 1)
    }
  }

  function clearPending(reason: string): void {
    cloudClient.discardPendingTimers()
    pending.value = []
    addLog('info', 'command', '已清理在途请求', reason)
  }

  /** response（非 register）信封：更新/移除 pending 并写日志。 */
  function handleCommandResponse(envelope: Envelope): void {
    const result = envelope.payload as CommandResult | undefined
    if (!result?.status) {
      addLog('error', 'response', '响应缺少 status', summarize(envelope.payload))
      return
    }

    const current = pending.value.find((item) => item.requestId === envelope.request_id)
    if (current) {
      current.status = result.status
      if (result.status === 'accepted' || result.status === 'executing') {
        addLog(
          'in',
          `response · ${result.status}`,
          `${current.action} @ ${current.nodeId}`,
          `request_id=${current.requestId}`,
        )
        return
      }
      removePending(current.requestId)
    }

    const title = `response · ${result.status}`
    const detail = [
      envelope.request_id ? `request_id=${envelope.request_id}` : '',
      result.error ? `error=${result.error}` : '',
      result.duration_ms !== undefined ? `duration=${result.duration_ms}ms` : '',
      result.data !== undefined ? summarize(result.data) : '',
    ]
      .filter(Boolean)
      .join('\n')

    addLog(
      result.status === 'completed' ? 'in' : 'error',
      title,
      current ? `${current.action} @ ${current.nodeId}` : '未匹配的响应',
      detail,
    )
  }

  /** 15s 超时（由 cloud-client 触发）。 */
  function handlePendingTimeout(requestId: string, action: string, nodeId: string): void {
    addLog('error', 'response · timeout', `${action} @ ${nodeId}`, '超过 15 秒未收到最终结果')
    removePending(requestId)
  }

  // ---------- 事件流 ----------

  /** 合并事件：按 node_id:sequence 去重，按时间排序，超出上限截断最旧事件。返回新增条数。 */
  function mergeEvents(incoming: AgentEvent[]): number {
    const seen = new Set(events.value.map((event) => `${event.node_id}:${event.sequence}`))
    let added = 0
    for (const event of incoming) {
      if (!event?.node_id || typeof event.sequence !== 'number') {
        continue
      }
      const key = `${event.node_id}:${event.sequence}`
      if (seen.has(key)) {
        continue
      }
      seen.add(key)
      events.value.push(event)
      added += 1
    }
    if (added === 0) {
      return 0
    }

    events.value.sort((left, right) => {
      const leftTime = Date.parse(left.timestamp) || 0
      const rightTime = Date.parse(right.timestamp) || 0
      if (leftTime === rightTime) {
        if (left.node_id === right.node_id) {
          return left.sequence - right.sequence
        }
        return left.node_id.localeCompare(right.node_id)
      }
      return leftTime - rightTime
    })
    if (events.value.length > MAX_EVENTS) {
      events.value.splice(0, events.value.length - MAX_EVENTS)
    }
    return added
  }

  function bumpSequence(event: AgentEvent): void {
    const current = lastSequences.value[event.node_id] ?? 0
    if (event.sequence > current) {
      lastSequences.value = { ...lastSequences.value, [event.node_id]: event.sequence }
    }
  }

  /** event 信封：校验 + 去重 + 游标推进 + 日志。 */
  function handleLiveEvent(payload: unknown): void {
    const event = payload as AgentEvent | undefined
    if (!event || !event.event_type || !event.node_id || typeof event.sequence !== 'number') {
      addLog('error', 'event', '事件格式无效', summarize(payload))
      return
    }
    const added = mergeEvents([event])
    if (added === 0) {
      return
    }
    bumpSequence(event)
    addLog(
      'in',
      `event · ${event.event_type}`,
      `${event.agent_id || 'agent'} @ ${event.node_id}`,
      [
        `sequence=${event.sequence}`,
        event.session_id ? `session=${event.session_id}` : '',
        event.payload !== undefined ? summarize(event.payload) : '',
      ]
        .filter(Boolean)
        .join('\n'),
    )
  }

  /** sync/events 非截断补发结果：合并 + 游标 + 日志。 */
  function handleEventReplay(result: EventReplayResult): void {
    const added = mergeEvents(result.events)
    for (const event of result.events) {
      bumpSequence(event)
    }
    addLog(
      'in',
      'sync · events',
      `补发 ${result.events.length} 条事件`,
      `新增 ${added} 条 · last=${summarize(result.last_sequences)}`,
    )
  }

  /** 补发被截断：剔除这些 Node 的事件，等待 client 回退到完整快照。 */
  function dropEventsOfNodes(nodeIds: string[]): void {
    events.value = events.value.filter((event) => !nodeIds.includes(event.node_id))
  }

  /** 应用快照中的事件/游标/权限部分（nodes 由 inventory 应用）。 */
  function applySnapshotData(snapshot: Snapshot): void {
    lastSequences.value = snapshot.last_sequences ? { ...snapshot.last_sequences } : {}
    if (Array.isArray(snapshot.events)) {
      mergeEvents(snapshot.events)
    }
    for (const event of snapshot.events ?? []) {
      bumpSequence(event)
    }
    pendingPermissions.value = Array.isArray(snapshot.pending_permissions)
      ? [...snapshot.pending_permissions]
      : []
  }

  // ---------- 生命周期 ----------

  /** 发起连接前清空依赖会话的数据（连接成功后由快照重建）。 */
  function resetForConnect(): void {
    events.value = []
    pendingPermissions.value = []
    lastSequences.value = {}
  }

  /** 手动断开：清空事件/权限/游标/在途请求并写日志。 */
  function clearForDisconnect(reason: string): void {
    resetForConnect()
    clearPending(reason)
  }

  return {
    events,
    lastSequences,
    pendingPermissions,
    pending,
    logs,
    showHeartbeats,
    recentEvents,
    selectedNodePermissions,
    addLog,
    clearLogs,
    addPending,
    removePending,
    clearPending,
    handleCommandResponse,
    handlePendingTimeout,
    handleLiveEvent,
    handleEventReplay,
    dropEventsOfNodes,
    applySnapshotData,
    resetForConnect,
    clearForDisconnect,
  }
})
