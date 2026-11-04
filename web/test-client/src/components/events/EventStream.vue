<script setup lang="ts">
import { computed } from 'vue'
import { useEventsStore } from '../../stores/events'
import { useConnectionStore } from '../../stores/connection'
import EmptyState from '../ui/EmptyState.vue'
import Skeleton from '../ui/Skeleton.vue'
import EventRow from './EventRow.vue'

const events = useEventsStore()
const connection = useConnectionStore()

/** 渲染最近 100 条（数据保留上限 300）。 */
const visibleEvents = computed(() => events.recentEvents.slice(0, 100))
</script>

<template>
  <Skeleton v-if="connection.isConnecting && events.events.length === 0" :rows="3" :row-height="96" />
  <EmptyState
    v-else-if="events.events.length === 0"
    icon="⌁"
    title="还没有 Agent 事件"
    hint="Node 上线、Agent 连接和权限事件都会进入 Event Cache"
  />
  <TransitionGroup v-else name="event-item" tag="ol" class="event-list">
    <EventRow v-for="event in visibleEvents" :key="`${event.node_id}:${event.sequence}`" :event="event" />
  </TransitionGroup>
</template>
