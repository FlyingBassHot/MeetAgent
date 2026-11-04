<script setup lang="ts">
import { computed } from 'vue'
import { useRouter } from 'vue-router'
import BaseCard from '../components/ui/BaseCard.vue'
import StatItem from '../components/ui/StatItem.vue'
import EmptyState from '../components/ui/EmptyState.vue'
import ConnectionStatusBar from '../components/connection/ConnectionStatusBar.vue'
import NodeSummaryCard from '../components/nodes/NodeSummaryCard.vue'
import PermissionList from '../components/permissions/PermissionList.vue'
import PendingRequests from '../components/pending/PendingRequests.vue'
import RecentIssues from '../components/overview/RecentIssues.vue'
import { useCommandsStore } from '../stores/commands'
import { useEventsStore } from '../stores/events'
import { useInventoryStore } from '../stores/inventory'
import { formatRelative } from '../lib/format'

const router = useRouter()
const commands = useCommandsStore()
const events = useEventsStore()
const inventory = useInventoryStore()

const lastEventLabel = computed(() =>
  events.lastEventAt ? formatRelative(events.lastEventAt) : '—',
)

function openNode(nodeId: string, agentId: string): void {
  router.push({ path: '/sessions', query: { node: nodeId, agent: agentId } })
}
</script>

<template>
  <div class="page-stack overview-page">
    <header class="view-header">
      <h2>总览</h2>
      <p>连接健康、实时指标、待审批权限与最近异常。</p>
    </header>

    <ConnectionStatusBar @settings="router.push('/settings')" />

    <div class="stat-grid overview-metrics" aria-label="指标概览">
      <StatItem
        label="Nodes"
        :value="`${inventory.onlineNodeCount}/${inventory.nodes.length}`"
        hint="在线 / 总数"
      />
      <StatItem
        label="Agents"
        :value="`${inventory.onlineAgentCount}/${inventory.totalAgentCount}`"
        hint="健康 / 总数"
      />
      <StatItem
        label="Sessions"
        :value="inventory.allSessions.length"
        :hint="`${inventory.runningSessionCount} 运行中`"
      />
      <StatItem label="待审批" :value="events.pendingPermissions.length" hint="权限请求" />
      <StatItem label="最后事件" :value="lastEventLabel" hint="客户端缓存 300 条" />
    </div>

    <BaseCard
      v-if="events.pendingPermissions.length > 0"
      title="待审批权限"
      :description="`${events.pendingPermissions.length} 个 Agent 权限请求`"
    >
      <template #actions>
        <span class="chip warn">{{ events.pendingPermissions.length }}</span>
      </template>
      <PermissionList />
    </BaseCard>

    <BaseCard title="最近异常" description="Agent error、命令失败 / 超时与通信错误，最多 5 条">
      <RecentIssues />
    </BaseCard>

    <BaseCard
      title="Nodes"
      :description="`${inventory.onlineNodeCount} 在线 · ${inventory.nodes.length} 已知`"
    >
      <template #actions>
        <button
          type="button"
          class="icon-button"
          title="刷新快照"
          aria-label="刷新快照"
          @click="commands.requestSnapshot"
        >
          ↻
        </button>
      </template>

      <div v-if="inventory.nodes.length === 0" class="node-summary-grid">
        <EmptyState
          icon="◇"
          title="还没有 Node 注册"
          hint="启动 meetagent-node 后这里会自动出现"
          compact
        />
      </div>
      <div v-else class="node-summary-grid">
        <NodeSummaryCard
          v-for="node in inventory.nodes"
          :key="node.node_id"
          :node="node"
          @open="openNode"
        />
      </div>
    </BaseCard>

    <BaseCard
      title="在途请求"
      :description="`${events.pending.length} 个等待最终结果 · 15s 超时`"
    >
      <template #actions>
        <span class="chip">{{ events.pending.length }}</span>
      </template>
      <PendingRequests />
    </BaseCard>
  </div>
</template>
