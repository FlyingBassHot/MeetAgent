<script setup lang="ts">
import BaseCard from '../components/ui/BaseCard.vue'
import CommandPanel from '../components/commands/CommandPanel.vue'
import AgentList from '../components/agents/AgentList.vue'
import { useInventoryStore } from '../stores/inventory'

const inventory = useInventoryStore()
</script>

<template>
  <div class="page-stack">
    <header class="view-header">
      <h2>会话</h2>
      <p>向目标 Node 发送白名单命令，并选择要操作的 Agent 与 Session。</p>
    </header>

    <BaseCard
      title="Remote command"
      description="Phase 2 白名单：ping / echo / get_status / list_sessions / send_prompt / interrupt · Adapter 能力与本地策略双重校验"
    >
      <CommandPanel />
    </BaseCard>

    <BaseCard
      title="Agents & Sessions"
      :description="`${inventory.selectedNode ? inventory.selectedNode.name : '未选择 Node'} · ${inventory.selectedNodeAgents.length} 个 Agent`"
    >
      <template v-if="inventory.selectedNode" #actions>
        <code class="card-code">{{ inventory.selectedNode.node_id }}</code>
      </template>
      <AgentList />
    </BaseCard>
  </div>
</template>
