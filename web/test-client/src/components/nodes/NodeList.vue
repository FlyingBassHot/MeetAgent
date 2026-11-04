<script setup lang="ts">
import { useInventoryStore } from '../../stores/inventory'
import { useConnectionStore } from '../../stores/connection'
import EmptyState from '../ui/EmptyState.vue'
import Skeleton from '../ui/Skeleton.vue'
import NodeCard from './NodeCard.vue'

const inventory = useInventoryStore()
const connection = useConnectionStore()
</script>

<template>
  <Skeleton v-if="connection.isConnecting && inventory.nodes.length === 0" :rows="3" :row-height="112" />
  <EmptyState
    v-else-if="inventory.nodes.length === 0"
    icon="◇"
    title="还没有 Node 注册"
    hint="启动 meetagent-node 后这里会自动出现"
  />
  <ul v-else class="node-list">
    <NodeCard
      v-for="node in inventory.nodes"
      :key="node.node_id"
      :node="node"
      :selected="node.node_id === inventory.selectedNodeId"
      @select="inventory.selectNode"
    />
  </ul>
</template>
