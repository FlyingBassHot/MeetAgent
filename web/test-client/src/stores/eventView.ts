/**
 * Events 页 UI 状态：Tab / 过滤器 / 跟随 / 聚合模式 / 通信日志过滤。
 * 事件数据本体与游标仍由 events store 持有。
 */

import { defineStore } from 'pinia'
import { computed, ref } from 'vue'
import type { LocationQuery } from 'vue-router'
import type { LogDirection } from '../api/cloud-client'
import type { AgentEvent } from '../lib/protocol'
import { eventKey, eventMatchesSearch, groupEvents, latestEventAt } from '../lib/events'
import { useEventsStore } from './events'
import { useInventoryStore } from './inventory'

export type EventsTab = 'events' | 'activity'
export type ActivityDirection = 'all' | LogDirection

export const ACTIVITY_DIRECTIONS: ReadonlyArray<{ value: ActivityDirection; label: string }> = [
  { value: 'all', label: '全部' },
  { value: 'in', label: '入站' },
  { value: 'out', label: '出站' },
  { value: 'info', label: '信息' },
  { value: 'error', label: '错误' },
]

function queryString(value: LocationQuery[string]): string {
  if (Array.isArray(value)) return value[0] ?? ''
  return value ?? ''
}

export const useEventViewStore = defineStore('eventView', () => {
  const events = useEventsStore()
  const inventory = useInventoryStore()

  const tab = ref<EventsTab>('events')
  const nodeId = ref('')
  const agentId = ref('')
  const sessionId = ref('')
  const eventType = ref('')
  const search = ref('')
  const rawMode = ref(false)
  const follow = ref(true)
  const pausedAt = ref(0)
  /** 暂停时已展示的事件键；严格冻结列表，避免晚到的旧时间戳事件插入。 */
  const pausedKeys = ref<Set<string>>(new Set())
  const filtersOpen = ref(false)
  const activityDirection = ref<ActivityDirection>('all')
  const activitySearch = ref('')

  const nodeOptions = computed(() =>
    inventory.nodes.map((node) => ({ value: node.node_id, label: node.name || node.node_id })),
  )

  const agentOptions = computed(() => {
    const seen = new Map<string, string>()
    for (const session of inventory.allSessions) {
      if (!seen.has(session.agent_id)) {
        seen.set(session.agent_id, session.agentName || session.agent_id)
      }
    }
    return [...seen.entries()].map(([value, label]) => ({ value, label }))
  })

  const sessionOptions = computed(() =>
    inventory.allSessions.map((session) => ({
      value: session.session_id,
      label: session.title || session.session_id,
    })),
  )

  const eventTypeOptions = computed(() => {
    const types = new Set(events.events.map((event) => event.event_type))
    return [...types].sort()
  })

  /** 仅按过滤条件筛选（不受暂停影响），用于暂停计数。 */
  const matchingEvents = computed<AgentEvent[]>(() =>
    events.recentEvents.filter((event) => {
      if (nodeId.value && event.node_id !== nodeId.value) return false
      if (agentId.value && event.agent_id !== agentId.value) return false
      if (sessionId.value && event.session_id !== sessionId.value) return false
      if (eventType.value && event.event_type !== eventType.value) return false
      return eventMatchesSearch(event, search.value)
    }),
  )

  /** 实际展示的事件：暂停后严格冻结在暂停瞬间的集合。 */
  const filteredEvents = computed<AgentEvent[]>(() => {
    if (follow.value) return matchingEvents.value
    return matchingEvents.value.filter(
      (event) =>
        pausedKeys.value.has(eventKey(event)) &&
        (Date.parse(event.timestamp) || 0) <= pausedAt.value,
    )
  })

  const groupedEvents = computed(() =>
    rawMode.value
      ? filteredEvents.value.map((event) => ({
          key: `${event.node_id}:${event.sequence}`,
          kind: 'single' as const,
          event,
          count: 1,
          text: '',
        }))
      : groupEvents(filteredEvents.value),
  )

  /** 暂停跟随后，当前过滤条件下新增的事件数。 */
  const newSincePause = computed(() => {
    if (follow.value) return 0
    return matchingEvents.value.filter((event) => !pausedKeys.value.has(eventKey(event))).length
  })

  /** 当前生效的过滤条件数量（不含搜索与原始模式）。 */
  const activeFilterCount = computed(
    () =>
      [nodeId.value, agentId.value, sessionId.value, eventType.value].filter(Boolean).length,
  )

  const filteredLogs = computed(() => {
    const term = activitySearch.value.trim().toLowerCase()
    return events.logs.filter((entry) => {
      if (activityDirection.value !== 'all' && entry.direction !== activityDirection.value) return false
      if (!term) return true
      return `${entry.type} ${entry.title} ${entry.detail ?? ''}`.toLowerCase().includes(term)
    })
  })

  function setFollow(next: boolean): void {
    if (next === follow.value) return
    follow.value = next
    if (next) {
      pausedAt.value = 0
      pausedKeys.value = new Set()
      return
    }
    pausedAt.value = latestEventAt(matchingEvents.value) || Date.now()
    pausedKeys.value = new Set(matchingEvents.value.map((event) => eventKey(event)))
  }

  function resume(): void {
    setFollow(true)
  }

  function toggleFilters(): void {
    filtersOpen.value = !filtersOpen.value
  }

  function resetFilters(): void {
    nodeId.value = ''
    agentId.value = ''
    sessionId.value = ''
    eventType.value = ''
    search.value = ''
    filtersOpen.value = false
  }

  /**
   * 从 URL query 同步会话 / Tab 作用域（会话页「在事件页打开」使用）。
   * 不动 eventType / search，避免 Tab 切换的 router.replace 清空用户输入。
   */
  function applyQuery(query: LocationQuery): void {
    tab.value = queryString(query.tab) === 'activity' ? 'activity' : 'events'
    nodeId.value = queryString(query.node)
    agentId.value = queryString(query.agent)
    sessionId.value = queryString(query.session)
  }

  return {
    tab,
    nodeId,
    agentId,
    sessionId,
    eventType,
    search,
    rawMode,
    follow,
    filtersOpen,
    activityDirection,
    activitySearch,
    nodeOptions,
    agentOptions,
    sessionOptions,
    eventTypeOptions,
    filteredEvents,
    groupedEvents,
    newSincePause,
    activeFilterCount,
    filteredLogs,
    setFollow,
    resume,
    toggleFilters,
    resetFilters,
    applyQuery,
  }
})
