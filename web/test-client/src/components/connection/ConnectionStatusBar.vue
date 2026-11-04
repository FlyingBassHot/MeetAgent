<script setup lang="ts">
import { useConnectionStore } from '../../stores/connection'
import { PHASE_LABELS, phaseTone } from '../../lib/status'
import StatusPill from '../ui/StatusPill.vue'

const emit = defineEmits<{
  settings: []
}>()

const connection = useConnectionStore()
</script>

<template>
  <section class="connection-bar" data-testid="connection-status-bar" aria-label="连接状态">
    <StatusPill :tone="phaseTone(connection.phase)">{{ PHASE_LABELS[connection.phase] }}</StatusPill>

    <span class="connection-bar-url" :title="connection.url">
      <span class="eyebrow">Cloud</span>
      <code>{{ connection.url }}</code>
    </span>

    <span class="connection-bar-stat">
      <span class="eyebrow">RTT</span>
      <strong>{{ connection.heartbeatRttMs === null ? '—' : `${connection.heartbeatRttMs}ms` }}</strong>
    </span>

    <span v-if="connection.reconnectAttempt > 0" class="connection-bar-stat">
      <span class="eyebrow">重连</span>
      <strong>#{{ connection.reconnectAttempt }}</strong>
    </span>

    <span v-if="connection.connectionId" class="connection-bar-stat hide-sm">
      <span class="eyebrow">连接</span>
      <code>{{ connection.connectionId.slice(-8) }}</code>
    </span>

    <div class="connection-bar-actions">
      <button type="button" class="button tiny ghost" data-testid="connection-settings" @click="emit('settings')">
        设置
      </button>
      <button
        v-if="connection.phase === 'offline'"
        type="button"
        class="button tiny primary"
        @click="connection.retryNow()"
      >
        重新连接
      </button>
      <button v-else type="button" class="button tiny danger" @click="connection.disconnect()">
        断开
      </button>
    </div>
  </section>
</template>
