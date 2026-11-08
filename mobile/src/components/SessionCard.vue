<script setup lang="ts">
import { computed } from 'vue';
import type { SessionView } from '@/domain/protocol';
import { formatRelative } from '@/domain/format';
import { sessionStatusLabel, sessionStatusTone } from '@/domain/labels';
import StatusPill from './StatusPill.vue';

const props = defineProps<{
  session: SessionView;
  agentName?: string;
  pendingCount?: number;
}>();

const title = computed(() => props.session.title?.trim() || '未命名会话');
</script>

<template>
  <RouterLink :to="`/sessions/${session.session_id}`" class="session-row">
    <div class="session-row__top">
      <span class="session-row__title">{{ title }}</span>
      <StatusPill
        :label="sessionStatusLabel(session.status)"
        :tone="sessionStatusTone(session.status)"
      />
    </div>
    <div class="session-row__meta">
      <span v-if="agentName" class="session-row__agent">{{ agentName }}</span>
      <span class="muted">{{ formatRelative(session.updated_at) }}</span>
      <span v-if="pendingCount" class="session-row__pending">{{ pendingCount }} 项待审批</span>
    </div>
  </RouterLink>
</template>

<style scoped>
.session-row {
  display: flex;
  flex-direction: column;
  justify-content: center;
  gap: 5px;
  min-height: 66px;
  padding: 11px 15px;
  color: inherit;
  transition: background 0.14s ease;
}

.session-row:hover {
  background: var(--surface-2);
}

.session-row:active {
  opacity: 1;
  background: var(--surface-2);
}

.session-row__top {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 10px;
  min-width: 0;
}

.session-row__title {
  flex: 1;
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  font-size: 15px;
  font-weight: 580;
  line-height: 1.35;
}

.session-row__meta {
  display: flex;
  align-items: center;
  gap: 8px;
  min-width: 0;
  color: var(--muted);
  font-size: 11px;
}

.session-row__agent {
  max-width: 45%;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  color: var(--muted);
}

.session-row__pending {
  margin-left: auto;
  color: var(--warning);
  font-size: 11px;
  white-space: nowrap;
}

@media (min-width: 760px) {
  .session-row {
    min-height: 68px;
    padding: 12px 16px;
  }
}
</style>
