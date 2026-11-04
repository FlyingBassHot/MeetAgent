<script setup lang="ts">
import { computed, ref } from 'vue'
import { useEventsStore } from '../../stores/events'
import { useConnectionStore } from '../../stores/connection'
import { eventKey, groupEvents } from '../../lib/events'
import EmptyState from '../ui/EmptyState.vue'
import Skeleton from '../ui/Skeleton.vue'
import EventRow from '../events/EventRow.vue'

const props = defineProps<{
  sessionId: string
}>()

const emit = defineEmits<{
  openEvents: []
}>()

const events = useEventsStore()
const connection = useConnectionStore()
const follow = ref(true)
const pausedKeys = ref<Set<string>>(new Set())

const sessionEvents = computed(() => events.sessionEvents(props.sessionId))
const visibleEvents = computed(() => {
  const list = sessionEvents.value
  return follow.value
    ? list
    : list.filter((event) => pausedKeys.value.has(eventKey(event)))
})
const newSincePause = computed(() => {
  if (follow.value) return 0
  return sessionEvents.value.filter((event) => !pausedKeys.value.has(eventKey(event))).length
})
const displayGroups = computed(() => groupEvents(visibleEvents.value))

function setFollow(next: boolean): void {
  if (next === follow.value) return
  follow.value = next
  pausedKeys.value = next
    ? new Set()
    : new Set(sessionEvents.value.map((event) => eventKey(event)))
}

function resume(): void {
  setFollow(true)
}
</script>

<template>
  <div class="session-live-events">
    <div class="live-events-toolbar">
      <label class="toggle">
        <input
          :checked="follow"
          type="checkbox"
          data-testid="session-live-follow"
          @change="setFollow(($event.target as HTMLInputElement).checked)"
        />
        <span>跟随最新</span>
      </label>
      <span class="muted live-events-count">{{ sessionEvents.length }} 条当前 Session 事件</span>
      <button type="button" class="button ghost tiny" data-testid="open-events" @click="emit('openEvents')">
        在事件页打开
      </button>
    </div>

    <div v-if="!follow" class="live-events-paused" role="status">
      <span>
        {{ newSincePause > 0 ? `${newSincePause} 条新事件` : '已暂停跟随' }}
      </span>
      <button type="button" class="button tiny primary" data-testid="session-live-resume" @click="resume">
        恢复跟随
      </button>
    </div>

    <Skeleton v-if="connection.isConnecting && visibleEvents.length === 0" :rows="3" :row-height="72" />
    <EmptyState
      v-else-if="visibleEvents.length === 0"
      icon="⌁"
      title="当前 Session 还没有事件"
      hint="发送 Prompt 后，message.* 与 tool.* 会实时出现在这里"
      compact
    />
    <ol v-else class="event-list live-event-list">
      <EventRow
        v-for="group in displayGroups.slice(0, 50)"
        :key="group.key"
        :event="group.event"
        :count="group.count"
        :text="group.text"
        compact
      />
    </ol>
  </div>
</template>
