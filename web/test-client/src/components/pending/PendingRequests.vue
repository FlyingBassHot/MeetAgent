<script setup lang="ts">
import { useEventsStore } from '../../stores/events'
import { useConnectionStore } from '../../stores/connection'
import { pendingStatusLabel } from '../../lib/status'
import EmptyState from '../ui/EmptyState.vue'
import Skeleton from '../ui/Skeleton.vue'

const events = useEventsStore()
const connection = useConnectionStore()
</script>

<template>
  <Skeleton v-if="connection.isConnecting && events.pending.length === 0" :rows="2" :row-height="64" />
  <EmptyState
    v-else-if="events.pending.length === 0"
    icon="✓"
    title="没有在途请求"
    compact
  />
  <ul v-else class="pending-list">
    <li v-for="item in events.pending" :key="item.requestId">
      <div>
        <strong>{{ item.action }}</strong>
        <code>{{ item.nodeId }}</code>
      </div>
      <span class="pending-status" :class="item.status">
        {{ pendingStatusLabel(item.status) }}
      </span>
      <small>{{ item.requestId }}</small>
    </li>
  </ul>
</template>
