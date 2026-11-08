<script setup lang="ts">
import { computed, ref } from 'vue';
import { useConnectionStore } from '@/stores/connection';
import { useNodeStore } from '@/stores/node';
import PageHeader from '@/components/PageHeader.vue';
import SessionCard from '@/components/SessionCard.vue';
import EmptyState from '@/components/EmptyState.vue';
import BottomSheet from '@/components/BottomSheet.vue';
import PageSkeleton from '@/components/PageSkeleton.vue';

type SessionFilter = 'all' | 'active' | 'waiting' | 'idle';

const connection = useConnectionStore();
const nodeStore = useNodeStore();
const search = ref('');
const searchOpen = ref(false);
const filterOpen = ref(false);
const filter = ref<SessionFilter>('all');
const loading = computed(() => !nodeStore.hasSnapshot && connection.isBusy);

const filters: Array<{ key: SessionFilter; label: string }> = [
  { key: 'all', label: '全部会话' },
  { key: 'active', label: '运行中' },
  { key: 'waiting', label: '等待中' },
  { key: 'idle', label: '空闲 / 已完成' },
];

const visibleSessions = computed(() => {
  const query = search.value.trim().toLowerCase();
  const priority = (status: string): number => {
    if (status === 'running') return 0;
    if (status === 'waiting') return 1;
    return 2;
  };
  return nodeStore.sessions
    .filter((session) => {
      const agentName = nodeStore.agentById[session.agent_id]?.name ?? '';
      const haystack = `${session.title ?? ''} ${agentName}`.toLowerCase();
      if (query.length > 0 && !haystack.includes(query)) return false;
      if (filter.value === 'active') return session.status === 'running';
      if (filter.value === 'waiting') return session.status === 'waiting';
      if (filter.value === 'idle') {
        return session.status === 'idle' || session.status === 'completed';
      }
      return true;
    })
    .sort((left, right) => {
      const delta = priority(left.status) - priority(right.status);
      if (delta !== 0) return delta;
      return Date.parse(right.updated_at) - Date.parse(left.updated_at);
    });
});

const groups = computed(() =>
  [
    {
      key: 'running',
      label: '运行中',
      sessions: visibleSessions.value.filter((session) => session.status === 'running'),
    },
    {
      key: 'waiting',
      label: '等待中',
      sessions: visibleSessions.value.filter((session) => session.status === 'waiting'),
    },
    {
      key: 'idle',
      label: '空闲 / 已完成',
      sessions: visibleSessions.value.filter(
        (session) => session.status !== 'running' && session.status !== 'waiting',
      ),
    },
  ].filter((group) => group.sessions.length > 0),
);

function selectFilter(next: SessionFilter): void {
  filter.value = next;
  filterOpen.value = false;
}
</script>

<template>
  <div class="sessions">
    <PageHeader title="会话" :subtitle="`${nodeStore.sessions.length} 个会话`">
      <template #actions>
        <button
          type="button"
          class="sessions__action"
          :class="{ 'sessions__action--active': searchOpen }"
          @click="searchOpen = !searchOpen"
        >
          搜索
        </button>
        <button type="button" class="sessions__action" @click="filterOpen = true">
          筛选{{ filter === 'all' ? '' : ' · 1' }}
        </button>
      </template>
    </PageHeader>

    <PageSkeleton v-if="loading" :rows="5" :status="false" />
    <template v-else>
    <input
      v-if="searchOpen"
      v-model="search"
      class="sessions__search"
      type="search"
      placeholder="搜索会话或 Agent"
      aria-label="搜索会话"
    />

    <EmptyState
      v-if="visibleSessions.length === 0"
      title="没有匹配的会话"
      description="调整搜索或筛选条件后重试。"
      mark=">_"
    />
    <template v-else>
      <section v-for="group in groups" :key="group.key" class="sessions__group">
        <h2 class="sessions__group-title">
          <span>{{ group.label }}</span>
          <span>{{ group.sessions.length }}</span>
        </h2>
        <div class="sessions__group-list list--surface">
          <SessionCard
            v-for="session in group.sessions"
            :key="session.session_id"
            :session="session"
            :agent-name="nodeStore.agentById[session.agent_id]?.name"
            :pending-count="nodeStore.pendingFor(session.session_id).length"
          />
        </div>
      </section>
    </template>
    </template>

    <BottomSheet :open="filterOpen" title="筛选会话" @close="filterOpen = false">
      <div class="sessions__options">
        <button
          v-for="item in filters"
          :key="item.key"
          type="button"
          class="sessions__option"
          :class="{ 'sessions__option--active': filter === item.key }"
          @click="selectFilter(item.key)"
        >
          {{ item.label }}
        </button>
      </div>
    </BottomSheet>
  </div>
</template>

<style scoped src="./sessions.css"></style>
