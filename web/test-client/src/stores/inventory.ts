/**
 * Inventory store：Node / Agent / Session 清单与选中态。
 * 不发起任何网络请求；快照应用由 connection store 调用 applyNodes()。
 * 选中态同时写入 localStorage，URL query 由 SessionsView 负责同步。
 */

import { defineStore } from 'pinia'
import { computed, ref, watch } from 'vue'
import type { AgentView, NodeView, SessionView } from '../lib/protocol'

const SELECTION_KEY = 'meetagent.test-client.selection.v1'

export interface SessionEntry extends SessionView {
  agentName: string
  agentType: string
  agentStatus: string
  capabilities: Record<string, boolean>
}

interface StoredSelection {
  nodeId: string
  agentId: string
  sessionId: string
}

function readSelection(): StoredSelection | null {
  try {
    const raw = window.localStorage.getItem(SELECTION_KEY)
    if (!raw) return null
    const parsed = JSON.parse(raw) as Partial<StoredSelection>
    return {
      nodeId: typeof parsed.nodeId === 'string' ? parsed.nodeId : '',
      agentId: typeof parsed.agentId === 'string' ? parsed.agentId : '',
      sessionId: typeof parsed.sessionId === 'string' ? parsed.sessionId : '',
    }
  } catch {
    return null
  }
}

export const useInventoryStore = defineStore('inventory', () => {
  const nodes = ref<NodeView[]>([])
  const selectedNodeId = ref('')
  const selectedAgentId = ref('')
  const selectedSessionId = ref('')

  const storedSelection = readSelection()
  let restorePending = storedSelection !== null

  const onlineNodes = computed(() => nodes.value.filter((node) => node.status === 'online'))
  const onlineNodeCount = computed(() => onlineNodes.value.length)
  /** 健康 Agent：在线 Node 上状态为 online / busy 的 Agent。 */
  const onlineAgentCount = computed(() =>
    onlineNodes.value.reduce(
      (sum, node) =>
        sum +
        (node.agents ?? []).filter(
          (agent) => agent.status === 'online' || agent.status === 'busy',
        ).length,
      0,
    ),
  )
  const totalAgentCount = computed(() =>
    nodes.value.reduce((sum, node) => sum + (node.agents?.length ?? 0), 0),
  )

  /** 全部 Session（跨 Node / Agent 扁平化，按更新时间倒序）。 */
  const allSessions = computed<SessionEntry[]>(() =>
    nodes.value
      .flatMap((node) =>
        (node.agents ?? []).flatMap((agent) =>
          (agent.sessions ?? []).map((session) => ({
            ...session,
            node_id: session.node_id || node.node_id,
            agent_id: session.agent_id || agent.agent_id,
            agentName: agent.name,
            agentType: agent.type,
            agentStatus: agent.status,
            capabilities: agent.capabilities ?? {},
          })),
        ),
      )
      .sort((left, right) => {
        const leftTime = Date.parse(left.updated_at) || 0
        const rightTime = Date.parse(right.updated_at) || 0
        return rightTime - leftTime
      }),
  )

  const runningSessionCount = computed(
    () =>
      allSessions.value.filter(
        (session) => session.status === 'running' || session.status === 'busy',
      ).length,
  )

  const selectedNode = computed<NodeView | null>(
    () => nodes.value.find((node) => node.node_id === selectedNodeId.value) ?? null,
  )
  const selectedNodeAgents = computed<AgentView[]>(() => selectedNode.value?.agents ?? [])
  const selectedAgent = computed<AgentView | null>(
    () => selectedNodeAgents.value.find((agent) => agent.agent_id === selectedAgentId.value) ?? null,
  )
  const selectedAgentSessions = computed<SessionView[]>(() => selectedAgent.value?.sessions ?? [])
  const selectedSession = computed<SessionView | null>(
    () =>
      selectedAgentSessions.value.find((session) => session.session_id === selectedSessionId.value) ??
      null,
  )
  const selectedSessionEntry = computed<SessionEntry | null>(
    () => allSessions.value.find((session) => session.session_id === selectedSessionId.value) ?? null,
  )

  watch([selectedNodeId, selectedAgentId, selectedSessionId], () => persistSelection())

  function persistSelection(): void {
    try {
      window.localStorage.setItem(
        SELECTION_KEY,
        JSON.stringify({
          nodeId: selectedNodeId.value,
          agentId: selectedAgentId.value,
          sessionId: selectedSessionId.value,
        }),
      )
    } catch {
      // localStorage 不可用时仅保留本次会话
    }
  }

  function clearSelection(): void {
    selectedNodeId.value = ''
    selectedAgentId.value = ''
    selectedSessionId.value = ''
    try {
      window.localStorage.removeItem(SELECTION_KEY)
    } catch {
      // 忽略
    }
  }

  /** 从 localStorage 恢复上次选择的 Node / Agent（Session 需要 URL 显式指定）。 */
  function restoreSelection(): void {
    if (!storedSelection) return
    const node = nodes.value.find((item) => item.node_id === storedSelection.nodeId)
    if (node) {
      selectedNodeId.value = node.node_id
      const agent = node.agents?.find((item) => item.agent_id === storedSelection.agentId)
      if (agent) {
        selectedAgentId.value = agent.agent_id
      }
    }
    restorePending = false
  }

  /** 应用快照中的 nodes，并在选中项失效时回退到第一个在线 Node。 */
  function applyNodes(next: NodeView[]): void {
    nodes.value = next
    if (restorePending && next.length > 0) {
      restoreSelection()
    }
    const selected = next.find((node) => node.node_id === selectedNodeId.value)
    if (!selected || selected.status !== 'online') {
      selectedNodeId.value = next.find((node) => node.status === 'online')?.node_id ?? ''
    }
    syncAgentSelection()
  }

  /** 点击 Node：仅允许选中在线 Node，并自动挑选 Agent（Session 由用户显式选择）。 */
  function selectNode(node: NodeView): void {
    if (node.status !== 'online') return
    selectedNodeId.value = node.node_id
    const agent = node.agents?.find((item) => item.status === 'online') ?? node.agents?.[0]
    selectedAgentId.value = agent?.agent_id ?? ''
    selectedSessionId.value = ''
  }

  /** 按 ID 选中 Node（总览下钻 / URL 恢复用）。 */
  function selectNodeById(nodeId: string): boolean {
    const node = nodes.value.find((item) => item.node_id === nodeId)
    if (!node) return false
    selectNode(node)
    return true
  }

  function selectAgent(agentId: string): void {
    selectedAgentId.value = agentId
    const agent = selectedNodeAgents.value.find((item) => item.agent_id === agentId)
    const sessions = agent?.sessions ?? []
    if (!sessions.some((session) => session.session_id === selectedSessionId.value)) {
      selectedSessionId.value = ''
    }
  }

  function selectSession(sessionId: string): void {
    selectedSessionId.value = sessionId
  }

  /** 按 ID 选中 Session，并同步 Node / Agent 选中态（URL / 事件页跳转用）。 */
  function selectSessionById(sessionId: string): boolean {
    const entry = allSessions.value.find((session) => session.session_id === sessionId)
    if (!entry) return false
    selectedNodeId.value = entry.node_id
    selectedAgentId.value = entry.agent_id
    selectedSessionId.value = entry.session_id
    return true
  }

  function clearSession(): void {
    selectedSessionId.value = ''
  }

  /** 清单刷新后，保证 selectedAgentId / selectedSessionId 仍然有效。 */
  function syncAgentSelection(): void {
    const agents = selectedNodeAgents.value
    if (!agents.some((agent) => agent.agent_id === selectedAgentId.value)) {
      selectedAgentId.value =
        agents.find((agent) => agent.status === 'online')?.agent_id ?? agents[0]?.agent_id ?? ''
      selectedSessionId.value = ''
    }
    const sessions = selectedAgentSessions.value
    if (!sessions.some((session) => session.session_id === selectedSessionId.value)) {
      selectedSessionId.value = ''
    }
  }

  return {
    nodes,
    selectedNodeId,
    selectedAgentId,
    selectedSessionId,
    onlineNodes,
    onlineNodeCount,
    onlineAgentCount,
    totalAgentCount,
    allSessions,
    runningSessionCount,
    selectedNode,
    selectedNodeAgents,
    selectedAgent,
    selectedAgentSessions,
    selectedSession,
    selectedSessionEntry,
    applyNodes,
    selectNode,
    selectNodeById,
    selectAgent,
    selectSession,
    selectSessionById,
    clearSession,
    clearSelection,
    restoreSelection,
  }
})
