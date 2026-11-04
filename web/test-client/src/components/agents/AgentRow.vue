<script setup lang="ts">
import type { AgentView } from '../../lib/protocol'
import SessionList from './SessionList.vue'

defineProps<{
  agent: AgentView
  selected: boolean
  selectedSessionId: string
}>()

const emit = defineEmits<{
  select: [agentId: string]
  selectSession: [sessionId: string]
}>()

function capabilityList(agent: AgentView): string[] {
  return Object.entries(agent.capabilities ?? {})
    .filter(([, enabled]) => enabled)
    .map(([name]) => name)
}
</script>

<template>
  <li
    class="agent-row"
    :class="{ selected }"
    role="button"
    tabindex="0"
    :aria-pressed="selected"
    :aria-label="`选择 Agent ${agent.name}`"
    @click="emit('select', agent.agent_id)"
    @keydown.enter="emit('select', agent.agent_id)"
    @keydown.space.prevent="emit('select', agent.agent_id)"
  >
    <div class="agent-top">
      <strong>{{ agent.name }}</strong>
      <span class="agent-type">{{ agent.type }}</span>
      <span class="node-status" :class="agent.status">
        <span class="status-dot" aria-hidden="true"></span>
        {{ agent.status }}
      </span>
    </div>
    <code>{{ agent.agent_id }}</code>
    <div v-if="capabilityList(agent).length" class="tag-row">
      <span v-for="capability in capabilityList(agent)" :key="capability" class="tag">
        {{ capability }}
      </span>
    </div>
    <SessionList
      v-if="agent.sessions?.length"
      :sessions="agent.sessions"
      :selected-session-id="selectedSessionId"
      @pick="emit('selectSession', $event)"
    />
    <small v-else class="hint">暂无 Session · 可通过 OpenCode 创建，或点击列表刷新</small>
  </li>
</template>
