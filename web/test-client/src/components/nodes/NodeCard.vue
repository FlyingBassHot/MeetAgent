<script setup lang="ts">
import type { NodeView } from '../../lib/protocol'
import { formatTime } from '../../lib/format'

defineProps<{
  node: NodeView
  selected: boolean
}>()

const emit = defineEmits<{
  select: [node: NodeView]
}>()

function onSelect(node: NodeView): void {
  if (node.status !== 'online') return
  emit('select', node)
}
</script>

<template>
  <li
    class="node-card"
    :class="{ selected, offline: node.status !== 'online' }"
    role="button"
    :tabindex="node.status === 'online' ? 0 : -1"
    :aria-pressed="selected"
    :aria-label="`选择 Node ${node.name}`"
    @click="onSelect(node)"
    @keydown.enter.prevent="onSelect(node)"
    @keydown.space.prevent="onSelect(node)"
  >
    <div class="node-top">
      <strong>{{ node.name }}</strong>
      <span class="node-status" :class="node.status">
        <span class="status-dot" aria-hidden="true"></span>
        {{ node.status === 'online' ? 'online' : 'offline' }}
      </span>
    </div>
    <code>{{ node.node_id }}</code>
    <div class="node-meta">
      <span v-if="node.platform">{{ node.platform }}</span>
      <span v-if="node.version">v{{ node.version }}</span>
      <span v-if="node.agents?.length">{{ node.agents.length }} agents</span>
    </div>
    <div class="node-meta muted">
      <span v-if="node.hostname">{{ node.hostname }}</span>
      <span>seen {{ formatTime(node.last_seen) }}</span>
    </div>
    <div v-if="node.agents?.length" class="agent-chips">
      <span v-for="agent in node.agents" :key="agent.agent_id" class="agent-chip">
        {{ agent.name }} <code>{{ agent.type }}</code>
      </span>
    </div>
  </li>
</template>
