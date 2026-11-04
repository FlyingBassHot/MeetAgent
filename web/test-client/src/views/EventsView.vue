<script setup lang="ts">
import { onMounted, watch } from 'vue'
import { useRoute, useRouter, type LocationQueryRaw } from 'vue-router'
import EventFilters from '../components/events/EventFilters.vue'
import EventList from '../components/events/EventList.vue'
import ActivityTab from '../components/events/ActivityTab.vue'
import { useEventViewStore, type EventsTab } from '../stores/eventView'

const route = useRoute()
const router = useRouter()
const view = useEventViewStore()

function buildQuery(): LocationQueryRaw {
  const query: LocationQueryRaw = {}
  if (view.nodeId) query.node = view.nodeId
  if (view.agentId) query.agent = view.agentId
  if (view.sessionId) query.session = view.sessionId
  if (view.tab === 'activity') query.tab = 'activity'
  return query
}

function setTab(tab: EventsTab): void {
  view.tab = tab
  router.replace({ path: '/events', query: buildQuery() })
}

onMounted(() => view.applyQuery(route.query))

// 已停留在 /events 时再次带 query 进入也要应用；不触碰搜索 / 类型。
watch(
  () => route.query,
  (query) => {
    if (route.name === 'events') view.applyQuery(query)
  },
)
</script>

<template>
  <div class="page-stack events-page">
    <header class="view-header">
      <h2>事件</h2>
      <p>Agent 事件流与通信日志。事件按 node_id:sequence 去重，客户端最多保留 300 条。</p>
    </header>

    <div class="tab-bar" role="tablist" aria-label="事件视图">
      <button
        type="button"
        role="tab"
        class="detail-tab"
        :class="{ active: view.tab === 'events' }"
        :aria-selected="view.tab === 'events'"
        data-testid="events-tab"
        @click="setTab('events')"
      >
        Agent 事件
        <span class="tab-badge">{{ view.filteredEvents.length }}</span>
      </button>
      <button
        type="button"
        role="tab"
        class="detail-tab"
        :class="{ active: view.tab === 'activity' }"
        :aria-selected="view.tab === 'activity'"
        data-testid="activity-tab"
        @click="setTab('activity')"
      >
        通信日志
        <span class="tab-badge">{{ view.filteredLogs.length }}</span>
      </button>
    </div>

    <template v-if="view.tab === 'events'">
      <EventFilters />
      <EventList />
    </template>
    <ActivityTab v-else />
  </div>
</template>
