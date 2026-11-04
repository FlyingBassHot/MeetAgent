<script setup lang="ts">
import { useEventsStore, type LogEntry } from '../../stores/events'
import { useConnectionStore } from '../../stores/connection'
import EmptyState from '../ui/EmptyState.vue'
import Skeleton from '../ui/Skeleton.vue'

const events = useEventsStore()
const connection = useConnectionStore()

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
  <Skeleton v-if="connection.isConnecting && events.logs.length === 0" :rows="5" :row-height="60" />
  <EmptyState
    v-else-if="events.logs.length === 0"
    icon="⌁"
    title="等待连接与消息"
    hint="连接 Cloud 后，注册、命令、响应和快照都会出现在这里"
  />
  <ol v-else class="log-list">
    <li v-for="entry in events.logs" :key="entry.id" class="log-entry" :class="entry.direction">
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
