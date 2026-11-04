/**
 * Inventory store：Node / Agent / Session 清单与选中态。
 * 不发起任何网络请求；快照应用由 connection store 调用 applySnapshotNodes()。
 */

import { defineStore } from 'pinia'
import { computed, ref } from 'vue'
import type { AgentView, NodeView, SessionView } from '../lib/protocol'

export const useInventoryStore = defineStore('inventory', () => {
  const nodes = ref<NodeView[]>([])
  const selectedNodeId = ref('')
  const selectedAgentId = ref('')
  const selectedSessionId = ref('')

  const onlineNodes = computed(() => nodes.value.filter((node) => node.status === 'online'))
  const onlineNodeCount = computed(() => onlineNodes.value.length)
  const onlineAgentCount = computed(() =>
    onlineNodes.value.reduce((sum, node) => sum + (node.agents?.length ?? 0), 0),
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

  /** 应用快照中的 nodes，并在选中项失效时回退到第一个在线 Node。 */
  function applyNodes(next: NodeView[]): void {
    nodes.value = next
    const selected = next.find((node) => node.node_id === selectedNodeId.value)
    if (!selected || selected.status !== 'online') {
      selectedNodeId.value = next.find((node) => node.status === 'online')?.node_id ?? ''
    }
    syncAgentSelection()
  }

  /** 点击 Node 卡片：仅允许选中在线 Node，并自动挑选 Agent/Session。 */
  function selectNode(node: NodeView): void {
    if (node.status !== 'online') return
    selectedNodeId.value = node.node_id
    const agent =
      node.agents?.find((item) => item.status === 'online') ?? node.agents?.[0]
    selectedAgentId.value = agent?.agent_id ?? ''
    selectedSessionId.value = agent?.sessions?.[0]?.session_id ?? ''
  }

  function selectAgent(agentId: string): void {
    selectedAgentId.value = agentId
    const agent = selectedNodeAgents.value.find((item) => item.agent_id === agentId)
    const sessions = agent?.sessions ?? []
    if (!sessions.some((session) => session.session_id === selectedSessionId.value)) {
      selectedSessionId.value = sessions[0]?.session_id ?? ''
    }
  }

  function selectSession(sessionId: string): void {
    selectedSessionId.value = sessionId
  }

  /** 清单刷新后，保证 selectedAgentId / selectedSessionId 仍然有效。 */
  function syncAgentSelection(): void {
    const agents = selectedNodeAgents.value
    if (!agents.some((agent) => agent.agent_id === selectedAgentId.value)) {
      selectedAgentId.value =
        agents.find((agent) => agent.status === 'online')?.agent_id ?? agents[0]?.agent_id ?? ''
    }
    const sessions = selectedAgentSessions.value
    if (!sessions.some((session) => session.session_id === selectedSessionId.value)) {
      selectedSessionId.value = sessions[0]?.session_id ?? ''
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
    selectedNode,
    selectedNodeAgents,
    selectedAgent,
    selectedAgentSessions,
    selectedSession,
    applyNodes,
    selectNode,
    selectAgent,
    selectSession,
  }
})
