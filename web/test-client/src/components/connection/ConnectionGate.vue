<script setup lang="ts">
import { computed } from 'vue'
import { useConnectionStore } from '../../stores/connection'
import { PHASE_LABELS } from '../../lib/status'
import ConnectionForm from './ConnectionForm.vue'

const connection = useConnectionStore()

const isBusy = computed(() => connection.isConnecting)

const submitLabel = computed(() => {
  switch (connection.phase) {
    case 'connecting':
      return '连接中…'
    case 'registering':
      return '注册中…'
    case 'reconnecting':
      return `重连中（#${connection.reconnectAttempt}）…`
    default:
      return '连接 Cloud'
  }
})

const progressHint = computed(() => {
  switch (connection.phase) {
    case 'connecting':
      return '正在建立 WebSocket 连接…'
    case 'registering':
      return '已建立连接，正在注册 Device（10s 超时）'
    case 'reconnecting':
      return `连接已断开，正在自动重连（第 ${connection.reconnectAttempt} 次）`
    default:
      return ''
  }
})

function submit(): void {
  if (connection.phase === 'offline') {
    connection.connect()
    return
  }
  connection.disconnect()
}
</script>

<template>
  <div class="gate" data-testid="connection-gate">
    <section class="gate-card" aria-labelledby="gate-title">
      <header class="gate-brand">
        <span class="brand-mark" aria-hidden="true">M</span>
        <span>
          <strong>MeetAgent</strong>
          <small>Debug Console</small>
        </span>
      </header>

      <div class="gate-intro">
        <h1 id="gate-title">连接你的 MeetAgent Cloud</h1>
        <p>
          Device 通过出站 WSS 连接 Cloud，无需公网 IP。连接成功后即可查看 Node、Session、事件并发送白名单命令。
        </p>
      </div>

      <ConnectionForm />

      <button
        type="button"
        class="button primary block connect-toggle"
        data-testid="connection-submit"
        @click="submit"
      >
        <span v-if="isBusy" class="spinner spinner-on-accent" aria-hidden="true"></span>
        {{ submitLabel }}
        <span v-if="isBusy" class="connect-cancel-hint">点击取消</span>
      </button>

      <p v-if="isBusy" class="connect-status" role="status" aria-live="polite">
        <span class="spinner" aria-hidden="true"></span>
        {{ progressHint }}
      </p>

      <div v-if="connection.lastError" class="alert" role="alert" data-testid="connection-error">
        <strong>连接失败</strong>
        <span>{{ connection.lastError }}</span>
        <div class="alert-actions">
          <button
            v-if="connection.phase === 'offline'"
            type="button"
            class="button tiny danger"
            @click="connection.retryNow()"
          >
            重试连接
          </button>
        </div>
      </div>

      <p class="footnote">
        Token 只保存在本机 localStorage。公网部署请使用自己的 Device 凭据，并确保 Cloud 使用
        <code>wss://</code>；不要提交仓库里的开发 Token。
      </p>

      <p class="gate-phase">
        <span class="status-dot" :class="connection.isOnline ? 'online' : 'offline'" aria-hidden="true"></span>
        当前阶段：{{ PHASE_LABELS[connection.phase] }}
      </p>
    </section>
  </div>
</template>
