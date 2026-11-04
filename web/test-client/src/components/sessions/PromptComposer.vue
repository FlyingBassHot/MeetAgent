<script setup lang="ts">
import { computed, ref } from 'vue'
import { useCommandsStore } from '../../stores/commands'
import { useEventsStore } from '../../stores/events'
import { useConnectionStore } from '../../stores/connection'
import { shortId } from '../../lib/format'

const props = defineProps<{
  sessionId: string
}>()

const commands = useCommandsStore()
const events = useEventsStore()
const connection = useConnectionStore()

const text = ref('')

const blockReason = computed(() => commands.promptBlockReason(props.sessionId, text.value))
const canSend = computed(() => blockReason.value === '')
const interruptReason = computed(() => commands.interruptBlockReason(props.sessionId))
const canInterrupt = computed(() => interruptReason.value === '')

const response = computed(() => events.lastResponse)

function submit(): void {
  if (!canSend.value) return
  if (commands.sendPromptTo(props.sessionId, text.value)) {
    text.value = ''
  }
}

function interrupt(): void {
  if (!canInterrupt.value) return
  commands.interruptSession(props.sessionId)
}

function onKeydown(event: KeyboardEvent): void {
  if ((event.ctrlKey || event.metaKey) && event.key === 'Enter') {
    event.preventDefault()
    submit()
  }
}
</script>

<template>
  <div class="prompt-composer">
    <div
      v-if="response"
      class="command-result"
      :class="`is-${response.status}`"
      role="status"
      data-testid="last-response"
    >
      <span class="eyebrow">LAST</span>
      <strong>{{ response.action }}</strong>
      <span class="command-result-status">{{ response.status }}</span>
      <small v-if="response.nodeId">{{ shortId(response.nodeId) }}</small>
      <span class="command-result-detail" :title="response.detail || ''">{{ response.detail || '—' }}</span>
      <time>{{ new Date(response.at).toLocaleTimeString('zh-CN', { hour12: false }) }}</time>
    </div>

    <label class="field grow">
      <span>Prompt</span>
      <textarea
        v-model="text"
        data-testid="prompt-input"
        rows="3"
        placeholder="输入发送给当前 Session 的 Prompt（Ctrl / Cmd + Enter 发送）"
        @keydown="onKeydown"
      ></textarea>
    </label>

    <div class="prompt-actions">
      <button
        type="button"
        class="button primary"
        data-testid="prompt-send"
        :disabled="!canSend"
        :title="blockReason || '发送 Prompt'"
        @click="submit"
      >
        {{ connection.isOnline ? '发送 Prompt' : 'Cloud 离线' }}
      </button>
      <button
        type="button"
        class="button danger"
        data-testid="session-interrupt"
        :disabled="!canInterrupt"
        :title="interruptReason || '中断当前执行'"
        @click="interrupt"
      >
        中断
      </button>
      <span v-if="!canSend" class="inline-warning">{{ blockReason }}</span>
      <span v-else class="prompt-hint">Ctrl / Cmd + Enter</span>
    </div>

    <p v-if="!canInterrupt" class="prompt-subtle">
      中断不可用：{{ interruptReason }}
    </p>
  </div>
</template>
