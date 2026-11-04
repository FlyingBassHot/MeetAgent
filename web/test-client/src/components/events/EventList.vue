<script setup lang="ts">
import { useEventViewStore } from '../../stores/eventView'
import { useEventsStore } from '../../stores/events'
import { useConnectionStore } from '../../stores/connection'
import EmptyState from '../ui/EmptyState.vue'
import Skeleton from '../ui/Skeleton.vue'
import EventRow from './EventRow.vue'

const view = useEventViewStore()
const events = useEventsStore()
const connection = useConnectionStore()
</script>

<template>
  <div class="event-stream-wrap">
    <Skeleton v-if="connection.isConnecting && events.events.length === 0" :rows="4" :row-height="88" />
    <EmptyState
      v-else-if="events.events.length === 0"
      icon="⌁"
      title="还没有 Agent 事件"
      hint="Node 上线、Agent 连接、Prompt 与工具调用都会写入客户端事件缓存（最多 300 条）"
    />
    <EmptyState
      v-else-if="view.filteredEvents.length === 0"
      icon="⌕"
      title="没有匹配的事件"
      hint="调整 Node / Agent / Session / 类型或搜索关键词"
      compact
    />
    <TransitionGroup v-else name="event-item" tag="ol" class="event-list event-scroll">
      <EventRow
        v-for="group in view.groupedEvents"
        :key="group.key"
        :event="group.event"
        :count="group.count"
        :text="group.text"
      />
    </TransitionGroup>

    <div v-if="view.newSincePause > 0" class="new-events-bar" role="status">
      <span>{{ view.newSincePause }} 条新事件</span>
      <button type="button" class="button tiny primary" @click="view.resume()">恢复跟随</button>
    </div>
  </div>
</template>
