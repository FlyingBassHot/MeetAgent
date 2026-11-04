<script setup lang="ts">
import { ref } from 'vue'
import { useConnectionStore } from '../../stores/connection'

const connection = useConnectionStore()
const showToken = ref(false)
</script>

<template>
  <div class="form-grid">
    <label class="field">
      <span>Cloud URL</span>
      <input
        v-model="connection.url"
        data-testid="connection-url"
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
        data-testid="connection-device"
        type="text"
        spellcheck="false"
        autocomplete="off"
        placeholder="device-id"
        :disabled="connection.phase !== 'offline'"
      />
    </label>

    <label class="field">
      <span class="field-row">
        <span>Device Token</span>
        <button
          type="button"
          class="link-button"
          :disabled="connection.phase !== 'offline'"
          @click="showToken = !showToken"
        >
          {{ showToken ? '隐藏' : '显示' }}
        </button>
      </span>
      <input
        v-model="connection.token"
        data-testid="connection-token"
        :type="showToken ? 'text' : 'password'"
        spellcheck="false"
        autocomplete="off"
        :disabled="connection.phase !== 'offline'"
      />
    </label>
  </div>
</template>
