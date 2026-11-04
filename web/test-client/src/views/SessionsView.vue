<script setup lang="ts">
import { onMounted, watch } from 'vue'
import { useRoute, useRouter, type LocationQueryRaw } from 'vue-router'
import SessionListPane from '../components/sessions/SessionListPane.vue'
import SessionDetail from '../components/sessions/SessionDetail.vue'
import { useWorkbenchStore } from '../stores/workbench'
import { useInventoryStore } from '../stores/inventory'

const route = useRoute()
const router = useRouter()
const workbench = useWorkbenchStore()
const inventory = useInventoryStore()

let syncingFromUrl = false

function queryValue(value: unknown): string {
  return typeof value === 'string' ? value : ''
}

function applyFromUrl(): void {
  const node = queryValue(route.query.node)
  const agent = queryValue(route.query.agent)
  const session = queryValue(route.query.session)

  syncingFromUrl = true
  if (node && node !== inventory.selectedNodeId) {
    inventory.selectNodeById(node)
  }
  if (agent && agent !== inventory.selectedAgentId) {
    inventory.selectAgent(agent)
  }
  if (session && session !== inventory.selectedSessionId) {
    inventory.selectSessionById(session)
  }
  if (!node && !agent && !session) {
    // 裸 /sessions 入口只展示列表，Session 必须由用户点击或 URL 显式指定。
    inventory.clearSession()
  }
  syncingFromUrl = false
}

function currentQuery(): LocationQueryRaw {
  const query: LocationQueryRaw = {}
  if (inventory.selectedNodeId) query.node = inventory.selectedNodeId
  if (inventory.selectedAgentId) query.agent = inventory.selectedAgentId
  if (inventory.selectedSessionId) query.session = inventory.selectedSessionId
  return query
}

function sameQuery(left: LocationQueryRaw, right: LocationQueryRaw): boolean {
  const keys = new Set([...Object.keys(left), ...Object.keys(right)])
  for (const key of keys) {
    if (queryValue(left[key]) !== queryValue(right[key])) return false
  }
  return true
}

function syncUrl(): void {
  if (syncingFromUrl || route.name !== 'sessions') return
  const next = currentQuery()
  if (sameQuery(next, route.query)) return
  router.replace({ path: '/sessions', query: next })
}

function openInEvents(): void {
  const session = inventory.selectedSessionEntry
  if (!session) return
  router.push({
    path: '/events',
    query: { tab: 'events', node: session.node_id, agent: session.agent_id, session: session.session_id },
  })
}

function closeDetail(): void {
  workbench.closeSession()
  syncUrl()
}

onMounted(() => {
  applyFromUrl()
  syncUrl()
})

watch(() => route.query, () => applyFromUrl())
watch(
  [() => inventory.selectedNodeId, () => inventory.selectedAgentId, () => inventory.selectedSessionId],
  () => syncUrl(),
)
watch(
  () => inventory.nodes.length,
  () => {
    if (inventory.nodes.length > 0) applyFromUrl()
  },
)
</script>

<template>
  <div class="sessions-workbench" :class="{ 'detail-open': workbench.detailOpen }">
    <SessionListPane class="list-pane" />

    <SessionDetail
      v-if="workbench.selectedSession"
      class="detail-pane"
      :session="workbench.selectedSession"
      @close="closeDetail"
      @open-events="openInEvents"
    />
    <section v-else class="detail-pane detail-placeholder">
      <span class="empty-icon" aria-hidden="true">◎</span>
      <h2>选择一个 Session</h2>
      <p>左侧列表按更新时间倒序展示全部 Session；选择后可发送 Prompt、中断执行或查看实时事件。</p>
      <p class="muted">Session 不会自动选中；刷新后从 URL 恢复上次选择。</p>
    </section>
  </div>
</template>
