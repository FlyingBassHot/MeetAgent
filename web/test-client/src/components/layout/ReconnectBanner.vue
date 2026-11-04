<script setup lang="ts">
import { computed } from 'vue'
import { useConnectionStore } from '../../stores/connection'
import { PHASE_LABELS } from '../../lib/status'

const connection = useConnectionStore()

const offlineWithError = computed(
  () => connection.phase === 'offline' && connection.lastError !== '',
)
const visible = computed(
  () => connection.isConnecting || offlineWithError.value,
)
</script>

<template>
  <div v-if="visible" class="reconnect-banner" role="status" aria-live="polite">
    <span v-if="connection.isConnecting" class="spinner" aria-hidden="true"></span>
    <span v-else class="permission-banner-icon" aria-hidden="true">!</span>
    <span class="banner-text">
      <template v-if="connection.isConnecting">
        {{ PHASE_LABELS[connection.phase] }}<template v-if="connection.phase === 'reconnecting'">（第 {{ connection.reconnectAttempt }} 次）</template>
        · 指数退避自动重试中，正在等待 Cloud 响应
      </template>
      <template v-else>
        已离线：{{ connection.lastError }}
      </template>
    </span>
    <button
      v-if="connection.isConnecting || connection.phase === 'offline'"
      type="button"
      class="banner-action"
      @click="connection.retryNow()"
    >
      {{ connection.isConnecting ? '立即重试' : '重新连接' }}
    </button>
  </div>
</template>
