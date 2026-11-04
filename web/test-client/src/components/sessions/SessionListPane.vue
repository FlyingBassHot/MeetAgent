<script setup lang="ts">
import { computed } from 'vue'
import { SESSION_FILTERS, useWorkbenchStore } from '../../stores/workbench'
import { useInventoryStore } from '../../stores/inventory'
import { useConnectionStore } from '../../stores/connection'
import { formatRelative, idSuffix } from '../../lib/format'
import { sessionStatusLabel, sessionStatusTone } from '../../lib/status'
import EmptyState from '../ui/EmptyState.vue'
import Skeleton from '../ui/Skeleton.vue'

const workbench = useWorkbenchStore()
const inventory = useInventoryStore()
const connection = useConnectionStore()

const agentOptions = computed(() => inventory.selectedNodeAgents)

const total = computed(() => workbench.scopedSessions.length)
const totalAll = computed(() => inventory.allSessions.length)

function onNodeChange(event: Event): void {
  const nodeId = (event.target as HTMLSelectElement).value
  if (!nodeId) return
  if (!inventory.selectNodeById(nodeId)) return
  inventory.clearSession()
}

function onAgentChange(event: Event): void {
  const agentId = (event.target as HTMLSelectElement).value
  if (!agentId) return
  inventory.selectAgent(agentId)
  inventory.clearSession()
}

function sessionTitle(sessionId: string): string {
  const session = inventory.allSessions.find((item) => item.session_id === sessionId)
  return session?.title?.trim() || idSuffix(sessionId)
}
</script>

<template>
  <section class="session-list-pane" data-testid="session-list-pane" aria-label="Session 列表">
    <div class="pane-selectors">
      <label class="compact-field">
        <span>Node</span>
        <select :value="inventory.selectedNodeId" :disabled="inventory.nodes.length === 0" @change="onNodeChange">
          <option v-if="inventory.nodes.length === 0" value="">无 Node</option>
          <option v-for="node in inventory.nodes" :key="node.node_id" :value="node.node_id">
            {{ node.name || node.node_id }}
          </option>
        </select>
      </label>

      <label class="compact-field">
        <span>Agent</span>
        <select
          :value="inventory.selectedAgentId"
          :disabled="agentOptions.length === 0"
          @change="onAgentChange"
        >
          <option v-if="agentOptions.length === 0" value="">无 Agent</option>
          <option v-for="agent in agentOptions" :key="agent.agent_id" :value="agent.agent_id">
            {{ agent.name }}
          </option>
        </select>
      </label>
    </div>

    <div class="pane-count">
      <strong>{{ total }}</strong> Sessions
      <span v-if="workbench.scopedRunningCount" class="pane-count-running">
        · {{ workbench.scopedRunningCount }} 运行中
      </span>
    </div>

    <input
      v-model="workbench.search"
      class="session-search"
      data-testid="session-search"
      type="search"
      placeholder="搜索标题或 Session ID"
      aria-label="搜索 Session"
      spellcheck="false"
    />

    <div class="session-filter-row" role="group" aria-label="Session 状态筛选" data-testid="session-filter">
      <button
        v-for="option in SESSION_FILTERS"
        :key="option.value"
        type="button"
        class="filter-chip"
        :class="{ active: workbench.filter === option.value }"
        :aria-pressed="workbench.filter === option.value"
        :data-testid="`session-filter-${option.value}`"
        @click="workbench.filter = option.value"
      >
        {{ option.label }}
      </button>
    </div>

    <Skeleton v-if="connection.isConnecting && total === 0" :rows="6" :row-height="48" />
    <EmptyState
      v-else-if="total === 0"
      icon="◎"
      :title="totalAll > 0 ? '当前 Node / Agent 下没有 Session' : '还没有 Session'"
      :hint="totalAll > 0 ? '切换 Node / Agent，或确认该 Agent 已上报 Session' : 'Node 连接 OpenCode 后会自动上报'"
      compact
    />
    <EmptyState
      v-else-if="workbench.filteredSessions.length === 0"
      icon="⌕"
      title="没有匹配的 Session"
      hint="调整搜索关键词或状态筛选"
      compact
    />
    <ul v-else class="session-items">
      <li v-for="session in workbench.filteredSessions" :key="session.session_id">
        <button
          type="button"
          class="session-item"
          :class="{ selected: session.session_id === inventory.selectedSessionId }"
          data-testid="session-item"
          :data-session-id="session.session_id"
          :data-node-id="session.node_id"
          :data-agent-id="session.agent_id"
          :aria-pressed="session.session_id === inventory.selectedSessionId"
          @click="workbench.selectSession(session.session_id)"
        >
          <span class="session-status-dot" :class="sessionStatusTone(session.status)" aria-hidden="true"></span>
          <span class="session-item-main">
            <strong :title="session.title || session.session_id">
              {{ session.title?.trim() || idSuffix(session.session_id) }}
            </strong>
            <small>
              {{ formatRelative(session.updated_at) }} · {{ idSuffix(session.session_id) }}
            </small>
          </span>
          <span class="session-item-status">{{ sessionStatusLabel(session.status) }}</span>
        </button>
      </li>
    </ul>

    <p class="pane-hint">
      默认按更新时间倒序 · 不会自动选中 Session
      <span v-if="inventory.selectedSessionId">· 当前 {{ sessionTitle(inventory.selectedSessionId) }}</span>
    </p>
  </section>
</template>
