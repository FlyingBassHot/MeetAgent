<script setup lang="ts">
import { computed, ref } from 'vue'
import { useCommandsStore } from '../../stores/commands'
import { useInventoryStore } from '../../stores/inventory'
import { useConnectionStore } from '../../stores/connection'

const commands = useCommandsStore()
const inventory = useInventoryStore()
const connection = useConnectionStore()

const open = ref(false)
const echoText = ref('')
const echoMode = ref(false)

const disabled = computed(() => !connection.isOnline || !inventory.selectedNodeId)

function toggle(): void {
  if (disabled.value) return
  open.value = !open.value
  echoMode.value = false
}

function run(action: 'ping' | 'get_status' | 'list_sessions'): void {
  commands.sendNodeTool(action)
  open.value = false
}

function startEcho(): void {
  echoMode.value = true
}

function submitEcho(): void {
  if (!echoText.value.trim()) return
  commands.sendNodeTool('echo', echoText.value)
  echoText.value = ''
  echoMode.value = false
  open.value = false
}
</script>

<template>
  <div class="node-tools" @keydown.esc="open = false">
    <button
      type="button"
      class="button ghost tiny"
      data-testid="node-tools"
      :disabled="disabled"
      :aria-expanded="open"
      @click="toggle"
    >
      Node 工具 ▾
    </button>

    <div v-if="open" class="node-tools-menu" role="menu">
      <button type="button" role="menuitem" data-testid="node-tool-ping" @click="run('ping')">
        Ping
        <small>往返延迟</small>
      </button>
      <button type="button" role="menuitem" data-testid="node-tool-echo" @click="startEcho">
        Echo
        <small>文本回显</small>
      </button>
      <button type="button" role="menuitem" data-testid="node-tool-status" @click="run('get_status')">
        Get status
        <small>节点信息</small>
      </button>
      <button type="button" role="menuitem" data-testid="node-tool-sessions" @click="run('list_sessions')">
        List sessions
        <small>刷新 Session 清单</small>
      </button>

      <form v-if="echoMode" class="node-tools-echo" @submit.prevent="submitEcho">
        <input
          v-model="echoText"
          type="text"
          placeholder="要回显的文本"
          aria-label="Echo 文本"
          autofocus
        />
        <button type="submit" class="button tiny primary" :disabled="!echoText.trim()">发送</button>
      </form>
    </div>
  </div>
</template>
