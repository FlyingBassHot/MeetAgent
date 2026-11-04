<script setup lang="ts">
import { computed, onBeforeUnmount, ref } from 'vue'
import type { AgentEvent } from '../../lib/protocol'
import { formatPayload, formatTime } from '../../lib/format'
import { eventTone } from '../../lib/status'
import { copyText } from '../../lib/clipboard'

const props = withDefaults(
  defineProps<{
    event: AgentEvent
    /** 聚合组内事件数 */
    count?: number
    /** message.delta 累计文本 */
    text?: string
    compact?: boolean
  }>(),
  { count: 1, text: '', compact: false },
)

const expanded = ref(false)
const copied = ref('')
let copiedTimer: number | undefined

const tone = computed(() => eventTone(props.event.event_type))
const isDelta = computed(() => props.event.event_type === 'message.delta')

function flash(target: string): void {
  copied.value = target
  if (copiedTimer !== undefined) window.clearTimeout(copiedTimer)
  copiedTimer = window.setTimeout(() => {
    copied.value = ''
  }, 1400)
}

async function copyJson(): Promise<void> {
  if (await copyText(JSON.stringify(props.event, null, 2))) flash('json')
}

async function copyId(): Promise<void> {
  if (await copyText(props.event.event_id)) flash('id')
}

onBeforeUnmount(() => {
  if (copiedTimer !== undefined) window.clearTimeout(copiedTimer)
})
</script>

<template>
  <li
    class="event-row"
    :class="[tone, { compact }]"
    data-testid="event-item"
    :data-event-id="event.event_id"
  >
    <div class="event-top">
      <span class="event-seq">#{{ event.sequence }}</span>
      <strong>{{ event.event_type }}</strong>
      <span v-if="count > 1" class="event-count">×{{ count }}</span>
      <span class="event-time">{{ formatTime(event.timestamp) }}</span>
    </div>

    <div class="event-meta">
      <code>{{ event.node_id }}</code>
      <span v-if="event.agent_id">{{ event.agent_id }}</span>
      <span v-if="event.session_id" :title="event.session_id">{{ event.session_id }}</span>
    </div>

    <p v-if="isDelta && text" class="event-aggregate">{{ text }}</p>

    <div class="event-actions">
      <button
        v-if="event.payload !== undefined"
        type="button"
        class="payload-toggle"
        :aria-expanded="expanded"
        @click="expanded = !expanded"
      >
        {{ expanded ? '收起 payload' : '展开 payload' }}
      </button>
      <button type="button" class="payload-toggle" @click="copyJson">
        {{ copied === 'json' ? '已复制 JSON' : '复制 JSON' }}
      </button>
      <button type="button" class="payload-toggle" @click="copyId">
        {{ copied === 'id' ? '已复制 ID' : '复制 ID' }}
      </button>
    </div>

    <pre v-if="expanded && event.payload !== undefined" class="event-payload">{{ formatPayload(event.payload) }}</pre>
  </li>
</template>
