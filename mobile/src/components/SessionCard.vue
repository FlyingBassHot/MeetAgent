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
    <div class="session-row__title">{{ title }}</div>
    <div class="session-row__meta">
      <StatusPill
        :label="sessionStatusLabel(session.status)"
        :tone="sessionStatusTone(session.status)"
      />
      <span v-if="agentName" class="session-row__agent">{{ agentName }}</span>
      <span class="muted">{{ formatRelative(session.updated_at) }}</span>
    </div>
    <div v-if="pendingCount" class="session-row__pending">
      <span class="pill pill--warning">{{ pendingCount }} 项待审批</span>
    </div>
  </RouterLink>
</template>

<style scoped>
.session-row {
  display: flex;
  flex-direction: column;
  gap: 5px;
  padding: 13px 14px;
  border: 1px solid var(--border);
  border-radius: 13px;
  background: var(--surface);
  color: inherit;
  transition: border-color 0.15s ease, background 0.15s ease;
}

.session-row:hover {
  border-color: var(--border-strong);
  background: var(--surface-2);
}

.session-row__title {
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  font-size: 15px;
  font-weight: 600;
  line-height: 1.35;
}

.session-row__meta {
  display: flex;
  align-items: center;
  flex-wrap: wrap;
  gap: 7px;
  min-width: 0;
  color: var(--muted);
  font-size: 11px;
}

.session-row__agent {
  max-width: 45%;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  color: var(--fg);
}

.session-row__pending {
  margin-top: 1px;
}

@media (min-width: 760px) {
  .session-row {
    padding: 14px 16px;
  }
}
</style>
