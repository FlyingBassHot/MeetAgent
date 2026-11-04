<script setup lang="ts">
import { useInventoryStore } from '../../stores/inventory'
import { useConnectionStore } from '../../stores/connection'
import EmptyState from '../ui/EmptyState.vue'
import Skeleton from '../ui/Skeleton.vue'
import AgentRow from './AgentRow.vue'

const inventory = useInventoryStore()
const connection = useConnectionStore()
</script>

<template>
  <Skeleton
    v-if="connection.isConnecting && inventory.selectedNodeAgents.length === 0"
    :rows="2"
    :row-height="150"
  />
  <EmptyState
    v-else-if="inventory.selectedNodeAgents.length === 0"
    icon="◎"
    title="当前 Node 没有声明 Agent"
    hint="Node 启动 Adapter 后会上报实时 Agent / Session 清单"
  />
  <ul v-else class="agent-list">
    <AgentRow
      v-for="agent in inventory.selectedNodeAgents"
      :key="agent.agent_id"
      :agent="agent"
      :selected="agent.agent_id === inventory.selectedAgentId"
      :selected-session-id="inventory.selectedSessionId"
      @select="inventory.selectAgent"
      @select-session="inventory.selectSession"
    />
  </ul>
</template>
