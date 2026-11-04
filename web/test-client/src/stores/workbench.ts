/** Sessions 工作台 UI 状态：搜索 / 筛选 / 当前 Tab（业务数据仍在 inventory）。 */

import { defineStore } from 'pinia'
import { computed, ref } from 'vue'
import { useInventoryStore } from './inventory'

export type SessionFilter = 'all' | 'running' | 'idle' | 'error'
export type SessionTab = 'events' | 'detail'

export const SESSION_FILTERS: ReadonlyArray<{ value: SessionFilter; label: string }> = [
  { value: 'all', label: '全部' },
  { value: 'running', label: '运行中' },
  { value: 'idle', label: '空闲' },
  { value: 'error', label: '错误' },
]

export const SESSION_TABS: ReadonlyArray<{ value: SessionTab; label: string }> = [
  { value: 'events', label: '实时事件' },
  { value: 'detail', label: '详情' },
]

export const useWorkbenchStore = defineStore('workbench', () => {
  const inventory = useInventoryStore()

  const search = ref('')
  const filter = ref<SessionFilter>('all')
  const tab = ref<SessionTab>('events')

  /** 当前 Node + Agent 作用域内的 Session（列表与计数共用）。 */
  const scopedSessions = computed(() =>
    inventory.allSessions.filter(
      (session) =>
        session.node_id === inventory.selectedNodeId &&
        session.agent_id === inventory.selectedAgentId,
    ),
  )

  const scopedRunningCount = computed(
    () =>
      scopedSessions.value.filter(
        (session) => session.status === 'running' || session.status === 'busy',
      ).length,
  )

  const filteredSessions = computed(() => {
    const term = search.value.trim().toLowerCase()
    return scopedSessions.value.filter((session) => {
      if (term) {
        const haystack = `${session.title ?? ''} ${session.session_id}`.toLowerCase()
        if (!haystack.includes(term)) return false
      }
      switch (filter.value) {
        case 'running':
          return session.status === 'running' || session.status === 'busy'
        case 'idle':
          return session.status === 'idle'
        case 'error':
          return session.status.includes('error') || session.status.includes('failed')
        default:
          return true
      }
    })
  })

  const selectedSession = computed(() => inventory.selectedSessionEntry)
  const detailOpen = computed(() => selectedSession.value !== null)

  function selectSession(sessionId: string): void {
    if (!inventory.selectSessionById(sessionId)) return
    tab.value = 'events'
  }

  function closeSession(): void {
    inventory.clearSession()
  }

  function setTab(next: SessionTab): void {
    tab.value = next
  }

  function resetFilters(): void {
    search.value = ''
    filter.value = 'all'
  }

  return {
    search,
    filter,
    tab,
    scopedSessions,
    scopedRunningCount,
    filteredSessions,
    selectedSession,
    detailOpen,
    selectSession,
    closeSession,
    setTab,
    resetFilters,
  }
})
