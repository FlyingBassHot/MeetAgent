<script setup lang="ts">
import type { LogEntry } from '../../stores/events'
import EmptyState from '../ui/EmptyState.vue'

defineProps<{
  entries: LogEntry[]
}>()

function directionSymbol(direction: LogEntry['direction']): string {
  switch (direction) {
    case 'in':
      return '↓'
    case 'out':
      return '↑'
    case 'error':
      return '!'
    default:
      return '·'
  }
}
</script>

<template>
  <EmptyState
    v-if="entries.length === 0"
    icon="⌁"
    title="没有匹配的通信日志"
    hint="连接 Cloud 后，注册、心跳、命令、响应和快照都会出现在这里"
  />
  <ol v-else class="log-list">
    <li
      v-for="entry in entries"
      :key="entry.id"
      class="log-entry"
      :class="entry.direction"
      data-testid="activity-item"
    >
      <span class="log-time">{{ entry.time }}</span>
      <span class="log-direction" aria-hidden="true">{{ directionSymbol(entry.direction) }}</span>
      <div class="log-body">
        <div class="log-title">
          <span class="log-type">{{ entry.type }}</span>
          <strong>{{ entry.title }}</strong>
        </div>
        <pre v-if="entry.detail" class="log-detail">{{ entry.detail }}</pre>
      </div>
    </li>
  </ol>
</template>
