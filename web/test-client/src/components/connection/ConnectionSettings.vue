<script setup lang="ts">
import { useConnectionStore } from '../../stores/connection'
import ConnectionForm from './ConnectionForm.vue'

const connection = useConnectionStore()

function toggleAutoConnect(): void {
  connection.setAutoConnect(!connection.autoConnect)
}
</script>

<template>
  <div class="connection-settings">
    <ConnectionForm />

    <label class="toggle toggle-row">
      <input
        :checked="connection.autoConnect"
        data-testid="autoconnect-toggle"
        type="checkbox"
        @change="toggleAutoConnect"
      />
      <span>
        自动连接
        <small>保存过配置时，打开页面自动握手</small>
      </span>
    </label>

    <div class="connection-settings-actions">
      <button
        v-if="connection.phase === 'offline'"
        type="button"
        class="button primary"
        data-testid="connection-disconnect"
        @click="connection.retryNow()"
      >
        重新连接
      </button>
      <button
        v-else
        type="button"
        class="button danger"
        data-testid="connection-disconnect"
        @click="connection.disconnect()"
      >
        断开连接
      </button>
      <button
        type="button"
        class="button ghost"
        data-testid="connection-forget"
        @click="connection.forgetDevice()"
      >
        忘记此设备
      </button>
      <span class="muted connection-settings-hint">
        忘记此设备会清除本地 URL / Device / Token，并返回连接页。
      </span>
    </div>
  </div>
</template>
