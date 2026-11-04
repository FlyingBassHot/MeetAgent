<script setup lang="ts">
import { useConnectionStore } from '../../stores/connection'
import { PHASE_LABELS } from '../../lib/status'

const connection = useConnectionStore()
</script>

<template>
  <div v-if="connection.isConnecting" class="reconnect-banner" role="status" aria-live="polite">
    <span class="spinner" aria-hidden="true"></span>
    <span class="banner-text">
      {{ PHASE_LABELS[connection.phase] }}<template v-if="connection.phase === 'reconnecting'">（第 {{ connection.reconnectAttempt }} 次）</template>
      · 指数退避自动重试中，正在等待 Cloud 响应
    </span>
    <button type="button" class="banner-action" @click="connection.retryNow()">
      立即重试
    </button>
  </div>
</template>
