<script setup lang="ts">
import { COMMAND_ACTIONS, useCommandsStore } from '../../stores/commands'
import { useInventoryStore } from '../../stores/inventory'
import { shortId } from '../../lib/format'
import ActionPicker from './ActionPicker.vue'

const commands = useCommandsStore()
const inventory = useInventoryStore()

function submitCommand(): void {
  if (!commands.canSend) return
  if (commands.action === 'echo') {
    commands.sendCommand('echo', commands.echoText)
    return
  }
  if (commands.action === 'send_prompt') {
    commands.sendCommand('send_prompt', commands.promptText)
    return
  }
  commands.sendCommand(commands.action)
}

function onTextareaKeydown(event: KeyboardEvent): void {
  if ((event.ctrlKey || event.metaKey) && event.key === 'Enter') {
    event.preventDefault()
    submitCommand()
  }
}
</script>

<template>
  <div class="target-summary">
    <span class="eyebrow">TARGET</span>
    <template v-if="inventory.selectedNode">
      <strong>{{ inventory.selectedNode.name }}</strong>
      <code>{{ inventory.selectedNode.node_id }}</code>
      <small v-if="inventory.selectedAgent" class="target-agent">
        {{ inventory.selectedAgent.name }}
        <template v-if="inventory.selectedSession">
          / {{ inventory.selectedSession.title || shortId(inventory.selectedSession.session_id) }}
        </template>
      </small>
    </template>
    <span v-else class="muted">未选择在线 Node</span>
  </div>

  <div class="command-grid">
    <ActionPicker v-model="commands.action" :actions="COMMAND_ACTIONS" />

    <label v-if="commands.action === 'echo'" class="field grow">
      <span>Echo text</span>
      <textarea
        v-model="commands.echoText"
        rows="3"
        placeholder="输入要回显的文本"
        @keydown="onTextareaKeydown"
      ></textarea>
      <small class="hint">Ctrl / Cmd + Enter 发送</small>
    </label>

    <label v-else-if="commands.action === 'send_prompt'" class="field grow">
      <span>Prompt</span>
      <textarea
        v-model="commands.promptText"
        rows="4"
        placeholder="输入发送给所选 Session 的 Prompt"
        @keydown="onTextareaKeydown"
      ></textarea>
      <small class="hint">
        {{
          inventory.selectedSession
            ? `→ ${inventory.selectedSession.title || inventory.selectedSession.session_id}`
            : '请先在下方选择 Session'
        }}
        · Ctrl / Cmd + Enter 发送
      </small>
    </label>

    <div v-else class="command-hint">
      <span class="hint-mark" aria-hidden="true">→</span>
      <p v-if="commands.action === 'ping'">向目标 Node 发送一次往返探测，验证双向路由。</p>
      <p v-else-if="commands.action === 'get_status'">读取目标 Node 的版本、平台、运行时长和实时 Agent 清单。</p>
      <p v-else-if="commands.action === 'list_sessions'">让 Node 通过 Adapter 重新拉取所选 Agent 的 Session 列表。</p>
      <p v-else-if="commands.action === 'interrupt'">中断所选 Session 当前正在执行的 Agent 任务。</p>
    </div>
  </div>

  <div class="command-actions">
    <button type="button" class="button primary" :disabled="!commands.canSend" @click="submitCommand">
      发送命令
    </button>
    <button type="button" class="button ghost" @click="commands.requestSnapshot">
      获取快照
    </button>
    <span v-if="!commands.canSend" class="inline-warning">
      {{
        !inventory.selectedNodeId
          ? '需要 Cloud 在线并选择目标 Node'
          : '该操作需要先选择 Agent 与 Session'
      }}
    </span>
  </div>
</template>
