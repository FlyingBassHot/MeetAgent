<script setup lang="ts">
import { computed } from 'vue';
import type { SessionView } from '@/domain/protocol';
import { formatRelative, truncate } from '@/domain/format';
import { sessionStatusLabel, sessionStatusTone } from '@/domain/labels';
import StatusPill from './StatusPill.vue';

const props = defineProps<{
  session: SessionView;
  agentName?: string;
  pendingCount?: number;
  latestEvent?: string;
}>();

const title = computed(() => props.session.title?.trim() || props.session.session_id);
const summary = computed(() => {
  if (props.latestEvent) return truncate(props.latestEvent, 72);
  return props.session.working_directory ?? '暂无事件';
});
</script>

<template>
  <RouterLink
    :to="`/sessions/${session.session_id}`"
    class="card card--pad card--link session-card"
  >
    <div class="session-card__head">
      <div class="session-card__title">
        <strong>{{ title }}</strong>
        <span class="session-card__id mono small muted">{{ session.session_id }}</span>
      </div>
      <StatusPill
        :label="sessionStatusLabel(session.status)"
        :tone="sessionStatusTone(session.status)"
      />
    </div>
    <p class="session-card__summary small muted">{{ summary }}</p>
    <div class="session-card__meta small">
      <span v-if="agentName" class="session-card__agent">{{ agentName }}</span>
      <span class="muted">{{ formatRelative(session.updated_at) }}</span>
      <span v-if="pendingCount" class="pill pill--warning">{{ pendingCount }} 项待审批</span>
    </div>
  </RouterLink>
</template>

<style scoped>
.session-card {
  display: flex;
  flex-direction: column;
  gap: 7px;
}

.session-card__head {
  display: flex;
  align-items: flex-start;
  justify-content: space-between;
  gap: 10px;
}

.session-card__title {
  display: flex;
  flex-direction: column;
  min-width: 0;
}

.session-card__title strong {
  font-size: 15px;
  font-weight: 610;
  line-height: 1.35;
}

.session-card__id {
  font-size: 10px;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.session-card__summary {
  line-height: 1.45;
}

.session-card__meta {
  display: flex;
  align-items: center;
  flex-wrap: wrap;
  gap: 8px;
  color: var(--muted);
}

.session-card__agent {
  padding: 1px 7px;
  border-radius: 999px;
  background: var(--surface-3);
  color: var(--fg);
  font-size: 11px;
}
</style>
