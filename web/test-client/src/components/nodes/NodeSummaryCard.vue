<script setup lang="ts">
import { computed } from 'vue'
import type { NodeView } from '../../lib/protocol'
import { formatRelative } from '../../lib/format'

const props = defineProps<{
  node: NodeView
}>()

const emit = defineEmits<{
  open: [nodeId: string, agentId: string]
}>()

const agents = computed(() => props.node.agents ?? [])
const sessionCount = computed(() =>
  agents.value.reduce((sum, agent) => sum + (agent.sessions?.length ?? 0), 0),
)
const online = computed(() => props.node.status === 'online')

function open(): void {
  if (!online.value) return
  const agent = agents.value.find((item) => item.status === 'online') ?? agents.value[0]
  emit('open', props.node.node_id, agent?.agent_id ?? '')
}
</script>

<template>
  <button
    type="button"
    class="node-summary"
    :class="{ offline: !online }"
    data-testid="node-summary"
    :data-node-id="node.node_id"
    :disabled="!online"
    :aria-label="`查看 Node ${node.name} 的 Session`"
    @click="open"
  >
    <span class="node-summary-top">
      <strong :title="node.name">{{ node.name || node.node_id }}</strong>
      <span class="node-status" :class="node.status">
        <span class="status-dot" aria-hidden="true"></span>
        {{ online ? '在线' : '离线' }}
      </span>
    </span>

    <code :title="node.node_id">{{ node.node_id }}</code>

    <span class="node-summary-meta">
      <span>{{ agents.length }} Agent</span>
      <span>{{ sessionCount }} Session</span>
      <span v-if="node.version">v{{ node.version }}</span>
    </span>

    <span class="node-summary-agents">
      <span v-for="agent in agents" :key="agent.agent_id" class="agent-chip">
        {{ agent.name }} · {{ agent.status }}
      </span>
      <span v-if="!agents.length" class="muted">未上报 Agent</span>
    </span>

    <span class="node-summary-foot">
      <span v-if="node.platform">{{ node.platform }}</span>
      <span>seen {{ formatRelative(node.last_seen) }}</span>
      <span class="node-summary-cta">查看 Session →</span>
    </span>
  </button>
</template>
