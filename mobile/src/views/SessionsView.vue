<script setup lang="ts">
import { computed, ref } from 'vue';
import { latestSummaryBySession } from '@/domain/session';
import { useNodeStore } from '@/stores/node';
import PageHeader from '@/components/PageHeader.vue';
import SessionCard from '@/components/SessionCard.vue';
import EmptyState from '@/components/EmptyState.vue';

type SessionFilter = 'all' | 'active' | 'waiting' | 'idle';

const nodeStore = useNodeStore();
const search = ref('');
const filter = ref<SessionFilter>('all');

const summaries = computed(() => latestSummaryBySession(nodeStore.events));

const filterCounts = computed<Record<SessionFilter, number>>(() => ({
  all: nodeStore.sessions.length,
  active: nodeStore.runningSessions.length,
  waiting: nodeStore.waitingSessions.length,
  idle: nodeStore.sessions.filter(
    (session) => session.status === 'idle' || session.status === 'completed',
  ).length,
}));

const visibleSessions = computed(() => {
  const query = search.value.trim().toLowerCase();
  return nodeStore.sessions.filter((session) => {
    const agentName = nodeStore.agentById[session.agent_id]?.name ?? '';
    const haystack = `${session.title ?? ''} ${session.session_id} ${agentName}`.toLowerCase();
    if (query.length > 0 && !haystack.includes(query)) return false;
    switch (filter.value) {
      case 'active':
        return session.status === 'running';
      case 'waiting':
        return session.status === 'waiting';
      case 'idle':
        return session.status === 'idle' || session.status === 'completed';
      default:
        return true;
    }
  });
});

const filters: Array<{ key: SessionFilter; label: string }> = [
  { key: 'all', label: '全部' },
  { key: 'active', label: '运行中' },
  { key: 'waiting', label: '等待中' },
  { key: 'idle', label: '空闲/完成' },
];
</script>

<template>
  <div class="stack">
    <PageHeader title="Sessions" :subtitle="`${nodeStore.sessions.length} 个 Session · ${nodeStore.runningSessions.length} 个运行中`" />

    <div class="sessions__tools">
      <input
        v-model="search"
        class="input"
        type="search"
        placeholder="搜索标题、Session ID 或 Agent"
        aria-label="搜索 Session"
      />
      <div class="chip-row">
        <button
          v-for="item in filters"
          :key="item.key"
          type="button"
          class="chip"
          :class="{ 'chip--active': filter === item.key }"
          @click="filter = item.key"
        >
          {{ item.label }} <span class="chip__count">{{ filterCounts[item.key] }}</span>
        </button>
      </div>
    </div>

    <div v-if="visibleSessions.length === 0" class="card card--pad">
      <EmptyState
        title="没有匹配的 Session"
        description="调整搜索或筛选条件，或先在 Activity 查看事件。"
      />
    </div>
    <div v-else class="session-list">
      <SessionCard
        v-for="session in visibleSessions"
        :key="session.session_id"
        :session="session"
        :agent-name="nodeStore.agentById[session.agent_id]?.name"
        :pending-count="nodeStore.pendingFor(session.session_id).length"
        :latest-event="summaries[session.session_id]"
      />
    </div>
  </div>
</template>

<style scoped src="./sessions.css"></style>
