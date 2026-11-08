<script setup lang="ts">
import { computed } from 'vue';
import type { Tone } from '@/domain/labels';
import { useConnectionStore } from '@/stores/connection';
import StatusPill from './StatusPill.vue';

const connection = useConnectionStore();

const view = computed<{ label: string; tone: Tone }>(() => {
  switch (connection.phase) {
    case 'online':
      return { label: '已连接', tone: 'success' };
    case 'connecting':
      return { label: '连接中', tone: 'info' };
    case 'authenticating':
      return { label: '认证中', tone: 'info' };
    case 'reconnecting':
      return { label: '重连中', tone: 'warning' };
    case 'offline':
      return { label: '已断开', tone: 'neutral' };
    case 'error':
      return { label: '连接错误', tone: 'danger' };
    default:
      return { label: '未连接', tone: 'neutral' };
  }
});
</script>

<template>
  <span class="connection-badge" :title="connection.detail">
    <StatusPill :label="view.label" :tone="view.tone" />
  </span>
</template>

<style scoped>
.connection-badge {
  display: inline-flex;
  align-items: center;
  gap: 6px;
}
</style>
