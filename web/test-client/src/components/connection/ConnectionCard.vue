<script setup lang="ts">
import { computed, ref } from 'vue'
import { useRouter } from 'vue-router'
import { useConnectionStore } from '../../stores/connection'

const connection = useConnectionStore()
const router = useRouter()
const showToken = ref(false)

const isBusy = computed(() => connection.isConnecting)

const connectLabel = computed(() => {
  switch (connection.phase) {
    case 'connecting':
      return '连接中…'
    case 'registering':
      return '注册中…'
    case 'reconnecting':
      return `重连中（#${connection.reconnectAttempt}）…`
    case 'online':
      return '断开连接'
    default:
      return '连接 Cloud'
  }
})

const busyHint = computed(() => {
  switch (connection.phase) {
    case 'connecting':
      return '正在建立 WebSocket 连接…'
    case 'registering':
      return '已建立连接，正在注册 Device（10s 超时）'
    case 'reconnecting':
      return `连接已断开，正在重连（第 ${connection.reconnectAttempt} 次）· 指数退避 + 抖动`
    default:
      return ''
  }
})

function toggleConnection(): void {
  if (connection.phase === 'offline') {
    connection.connect()
  } else {
    connection.disconnect()
  }
}
</script>

<template>
  <section class="card" aria-labelledby="connection-card-title">
    <div class="card-heading">
      <div>
        <h2 id="connection-card-title">Cloud 连接</h2>
        <p>Device 出站 WSS，无需公网 IP</p>
      </div>
      <span class="chip">{{ connection.connectionId ? connection.connectionId.slice(-8) : '未连接' }}</span>
    </div>

    <div class="form-grid">
      <label class="field">
        <span>Cloud URL</span>
        <input
          v-model="connection.url"
          type="text"
          spellcheck="false"
          autocomplete="off"
          placeholder="ws://127.0.0.1:8080/ws"
          :disabled="connection.phase !== 'offline'"
        />
      </label>

      <label class="field">
        <span>Device ID</span>
        <input
          v-model="connection.deviceId"
          type="text"
          spellcheck="false"
          autocomplete="off"
          placeholder="web-001"
          :disabled="connection.phase !== 'offline'"
        />
      </label>

      <label class="field">
        <span class="field-row">
          <span>Device Token</span>
          <button type="button" class="link-button" @click="showToken = !showToken">
            {{ showToken ? '隐藏' : '显示' }}
          </button>
        </span>
        <input
          v-model="connection.token"
          :type="showToken ? 'text' : 'password'"
          spellcheck="false"
          autocomplete="off"
          :disabled="connection.phase !== 'offline'"
        />
      </label>
    </div>

    <button
      type="button"
      class="button primary block connect-toggle"
      :class="{ danger: connection.phase !== 'offline' }"
      @click="toggleConnection"
    >
      <span v-if="isBusy" class="spinner spinner-on-accent" aria-hidden="true"></span>
      {{ connectLabel }}
      <span v-if="isBusy" class="connect-cancel-hint">点击取消</span>
    </button>

    <p v-if="isBusy" class="connect-status" role="status" aria-live="polite">
      <span class="spinner" aria-hidden="true"></span>
      {{ busyHint }}
    </p>

    <div v-if="connection.lastError" class="alert" role="alert">
      <strong>连接失败</strong>
      <span>{{ connection.lastError }}</span>
      <div class="alert-actions">
        <button v-if="connection.phase === 'offline'" type="button" class="button tiny danger" @click="connection.retryNow()">
          重试连接
        </button>
        <button type="button" class="button tiny ghost" @click="router.push('/activity')">
          查看活动日志
        </button>
      </div>
    </div>

    <p class="footnote">
      Token 仅保存在本机 localStorage。公网部署必须使用 wss:// 并替换开发 Token。
    </p>
  </section>
</template>
