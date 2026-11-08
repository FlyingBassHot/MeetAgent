<script setup lang="ts">
import { computed } from 'vue';
import type { PermissionDecision, PermissionView } from '@/domain/protocol';
import { formatCountdown } from '@/domain/format';

const props = defineProps<{
  permission: PermissionView;
  agentName?: string;
  sessionTitle?: string;
  busy?: boolean;
}>();

const emit = defineEmits<{
  respond: [permissionId: string, decision: PermissionDecision];
}>();

const description = computed(
  () => props.permission.description?.trim() || 'Agent 未提供请求描述',
);
const expiry = computed(() => formatCountdown(props.permission.expires_at));

function respond(decision: PermissionDecision): void {
  emit('respond', props.permission.permission_id, decision);
}
</script>

<template>
  <article class="card card--pad permission">
    <div class="permission__head">
      <div class="permission__title">
        <span class="permission__kicker small">审批请求</span>
        <strong>{{ description }}</strong>
      </div>
      <span class="pill pill--warning">待处理</span>
    </div>
    <div class="permission__meta small muted">
      <span v-if="agentName">{{ agentName }}</span>
      <span v-if="sessionTitle">· {{ sessionTitle }}</span>
      <span>· {{ expiry }}</span>
    </div>
    <div class="permission__actions">
      <button
        type="button"
        class="btn btn--sm btn--primary"
        :disabled="busy"
        @click="respond('once')"
      >
        允许一次
      </button>
      <button type="button" class="btn btn--sm" :disabled="busy" @click="respond('always')">
        始终允许
      </button>
      <button
        type="button"
        class="btn btn--sm btn--danger"
        :disabled="busy"
        @click="respond('reject')"
      >
        拒绝
      </button>
      <span v-if="busy" class="small muted">提交中…</span>
    </div>
  </article>
</template>

<style scoped>
.permission {
  display: flex;
  flex-direction: column;
  gap: 9px;
  border-color: rgba(240, 180, 90, 0.28);
}

.permission__head {
  display: flex;
  align-items: flex-start;
  justify-content: space-between;
  gap: 12px;
}

.permission__title {
  display: flex;
  flex-direction: column;
  gap: 2px;
  min-width: 0;
}

.permission__title strong {
  font-size: 14px;
  line-height: 1.45;
  overflow-wrap: anywhere;
}

.permission__kicker {
  color: var(--warning);
  font-size: 10px;
  font-weight: 650;
  letter-spacing: 0.08em;
}

.permission__meta {
  display: flex;
  flex-wrap: wrap;
  gap: 5px;
}

.permission__actions {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 7px;
}
</style>
