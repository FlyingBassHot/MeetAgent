<script setup lang="ts">
import { ref } from 'vue'
import type { AgentEvent } from '../../lib/protocol'
import { formatPayload, formatTime } from '../../lib/format'
import { eventTone } from '../../lib/status'

defineProps<{
  event: AgentEvent
}>()

const expanded = ref(true)
</script>

<template>
  <li class="event-row" :class="eventTone(event.event_type)">
    <div class="event-top">
      <span class="event-seq">#{{ event.sequence }}</span>
      <strong>{{ event.event_type }}</strong>
      <span class="event-time">{{ formatTime(event.timestamp) }}</span>
    </div>
    <div class="event-meta">
      <code>{{ event.node_id }}</code>
      <span v-if="event.agent_id">{{ event.agent_id }}</span>
      <span v-if="event.session_id">{{ event.session_id }}</span>
    </div>
    <template v-if="event.payload !== undefined">
      <button
        type="button"
        class="payload-toggle"
        :aria-expanded="expanded"
        @click="expanded = !expanded"
      >
        {{ expanded ? '收起 payload' : '展开 payload' }}
      </button>
      <pre v-if="expanded" class="event-payload">{{ formatPayload(event.payload) }}</pre>
    </template>
  </li>
</template>
