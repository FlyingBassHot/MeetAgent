<script setup lang="ts">
import { computed } from 'vue'
import BaseCard from '../components/ui/BaseCard.vue'
import StatItem from '../components/ui/StatItem.vue'
import ConnectionCard from '../components/connection/ConnectionCard.vue'
import NodeList from '../components/nodes/NodeList.vue'
import PermissionList from '../components/permissions/PermissionList.vue'
import PendingRequests from '../components/pending/PendingRequests.vue'
import { useCommandsStore } from '../stores/commands'
import { useConnectionStore } from '../stores/connection'
import { useEventsStore } from '../stores/events'
import { useInventoryStore } from '../stores/inventory'
import { PHASE_LABELS } from '../lib/status'

const commands = useCommandsStore()
const connection = useConnectionStore()
const events = useEventsStore()
const inventory = useInventoryStore()

const rttDisplay = computed(() =>
  connection.heartbeatRttMs === null ? '—' : `${connection.heartbeatRttMs}ms`,
)
const totalNodes = computed(() => inventory.nodes.length)
</script>

<template>
  <div class="page-stack">
    <header class="view-header">
      <h2>总览</h2>
      <p>Cloud 连接状态、指标概览与待审批事项的一站式视图。</p>
    </header>

    <ConnectionCard />

    <div class="stat-grid" aria-label="指标概览">
      <StatItem label="阶段" :value="PHASE_LABELS[connection.phase]" />
      <StatItem label="心跳 RTT" :value="rttDisplay" hint="20s 心跳" />
      <StatItem
        label="Nodes"
        :value="`${inventory.onlineNodeCount}/${totalNodes}`"
        hint="在线 / 已知"
      />
      <StatItem label="事件" :value="events.events.length" hint="本次会话累计" />
    </div>

    <BaseCard title="Nodes" :description="`${inventory.onlineNodeCount} 在线 · ${totalNodes} 已知`">
      <template #actions>
        <button type="button" class="icon-button" title="刷新快照" aria-label="刷新快照" @click="commands.requestSnapshot">
          ↻
        </button>
      </template>
      <NodeList />
    </BaseCard>

    <BaseCard title="待审批权限" :description="`${events.pendingPermissions.length} 个 Agent 权限请求`">
      <template #actions>
        <span class="chip">{{ events.pendingPermissions.length }}</span>
      </template>
      <PermissionList />
    </BaseCard>

    <BaseCard title="在途请求" :description="`${events.pending.length} 个等待最终结果 · 15s 超时`">
      <template #actions>
        <span class="chip">{{ events.pending.length }}</span>
      </template>
      <PendingRequests />
    </BaseCard>
  </div>
</template>
