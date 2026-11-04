/**
 * Commands store：命令选择、echo/prompt 文本、输入校验与发送动作。
 * 只负责"用户想发什么"；在途请求与超时由 events store 登记，
 * WebSocket 收发由 cloudClient 完成。
 */

import { defineStore } from 'pinia'
import { computed, ref } from 'vue'
import { cloudClient } from '../api/cloud-client'
import { makeEnvelope, newMessageId } from '../lib/protocol'
import { summarize } from '../lib/format'
import { useConnectionStore } from './connection'
import { useEventsStore } from './events'
import { useInventoryStore } from './inventory'

export type TestAction =
  | 'ping'
  | 'echo'
  | 'get_status'
  | 'list_sessions'
  | 'send_prompt'
  | 'interrupt'
  | 'permission_response'

export interface CommandOptions {
  nodeId?: string
  agentId?: string
  sessionId?: string
  permissionId?: string
  decision?: string
}

export interface CommandActionMeta {
  value: TestAction
  label: string
  hint: string
}

export const COMMAND_ACTIONS: readonly CommandActionMeta[] = [
  { value: 'ping', label: 'Ping', hint: '往返测试' },
  { value: 'echo', label: 'Echo', hint: '文本回显' },
  { value: 'get_status', label: 'Get status', hint: '节点信息' },
  { value: 'list_sessions', label: 'List sessions', hint: '实时刷新 Session' },
  { value: 'send_prompt', label: 'Send prompt', hint: '发送 Prompt' },
  { value: 'interrupt', label: 'Interrupt', hint: '中断当前执行' },
]

function requiresAgent(action: TestAction): boolean {
  return action === 'send_prompt' || action === 'interrupt' || action === 'permission_response'
}

function requiresSession(action: TestAction): boolean {
  return action === 'send_prompt' || action === 'interrupt' || action === 'permission_response'
}

export const useCommandsStore = defineStore('commands', () => {
  const action = ref<TestAction>('ping')
  const echoText = ref('Hello from MeetAgent web client')
  const promptText = ref('Reply with exactly: pong')

  const connection = useConnectionStore()
  const inventory = useInventoryStore()
  const events = useEventsStore()

  const canSend = computed(() => {
    if (!connection.isOnline || !inventory.selectedNodeId) return false
    if (action.value === 'send_prompt' || action.value === 'interrupt') {
      return Boolean(inventory.selectedAgentId && inventory.selectedSessionId)
    }
    return true
  })

  /** 发送命令；校验失败时写 Activity 日志并中止。返回是否已发出。 */
  function sendCommand(
    targetAction: TestAction = action.value,
    text = '',
    options: CommandOptions = {},
  ): boolean {
    if (!connection.isOnline) {
      events.addLog('error', 'command', '发送失败', '尚未连接 Cloud')
      return false
    }
    const nodeId = options.nodeId ?? inventory.selectedNodeId
    if (!nodeId) {
      events.addLog('error', 'command', '发送失败', '请先选择一个在线 Node')
      return false
    }

    const agentId = options.agentId ?? inventory.selectedAgentId
    const sessionId = options.sessionId ?? inventory.selectedSessionId
    if (requiresAgent(targetAction) && !agentId) {
      events.addLog('error', 'command', '发送失败', '请先选择一个 Agent')
      return false
    }
    if (requiresSession(targetAction) && !sessionId) {
      events.addLog('error', 'command', '发送失败', '请先选择一个 Session')
      return false
    }
    if (targetAction === 'send_prompt' && !text.trim()) {
      events.addLog('error', 'command', '发送失败', 'Prompt 不能为空')
      return false
    }
    if (targetAction === 'permission_response' && !options.permissionId) {
      events.addLog('error', 'command', '发送失败', '缺少 permission_id')
      return false
    }

    const requestId = newMessageId('req')
    const payload: Record<string, unknown> = {}
    switch (targetAction) {
      case 'echo':
        payload.text = text
        break
      case 'ping':
        payload.nonce = requestId
        break
      case 'send_prompt':
        payload.text = text
        break
      case 'permission_response':
        payload.permission_id = options.permissionId
        payload.decision = options.decision ?? 'reject'
        break
    }

    const envelope = makeEnvelope(
      'command',
      targetAction,
      { type: 'device', id: connection.deviceId.trim() },
      payload,
    )
    envelope.request_id = requestId
    envelope.target = {
      node_id: nodeId,
      agent_id: agentId || undefined,
      session_id: sessionId || undefined,
    }

    events.addPending(requestId, targetAction, nodeId)
    if (!cloudClient.sendEnvelope(envelope)) {
      events.removePending(requestId)
      return false
    }
    events.addLog(
      'out',
      `command · ${targetAction}`,
      `→ ${nodeId}${sessionId ? ` / ${sessionId.slice(-8)}` : ''}`,
      Object.keys(payload).length ? summarize(payload) : '{}',
    )
    return true
  }

  /** Session 工作台：向指定 Session 发送 Prompt。 */
  function sendPromptTo(sessionId: string, text: string): boolean {
    const entry = inventory.allSessions.find((session) => session.session_id === sessionId)
    return sendCommand('send_prompt', text, {
      nodeId: entry?.node_id,
      agentId: entry?.agent_id,
      sessionId,
    })
  }

  /** Session 工作台：中断指定 Session。 */
  function interruptSession(sessionId: string): boolean {
    const entry = inventory.allSessions.find((session) => session.session_id === sessionId)
    return sendCommand('interrupt', '', {
      nodeId: entry?.node_id,
      agentId: entry?.agent_id,
      sessionId,
    })
  }

  /** Session 工作台：Node 级工具命令。 */
  function sendNodeTool(action: 'ping' | 'echo' | 'get_status' | 'list_sessions', text = ''): boolean {
    return sendCommand(action, text, { nodeId: inventory.selectedNodeId })
  }

  /** 发送 Prompt 前的禁用原因（空字符串表示可发送）。 */
  function promptBlockReason(sessionId: string, text: string): string {
    if (!connection.isOnline) return 'Cloud 离线'
    if (!inventory.selectedNodeId) return '未选择 Node'
    if (!sessionId) return '未选择 Session'
    if (!text.trim()) return 'Prompt 为空'
    return ''
  }

  /** 中断按钮的禁用原因（空字符串表示可中断）。 */
  function interruptBlockReason(sessionId: string): string {
    if (!connection.isOnline) return 'Cloud 离线'
    if (!sessionId) return '未选择 Session'
    const entry = inventory.allSessions.find((session) => session.session_id === sessionId)
    if (!entry) return 'Session 已失效'
    if (entry.status !== 'running' && entry.status !== 'busy') return 'Session 当前未运行'
    return ''
  }

  /** 响应权限审批：定位 permission 并把它的 node/agent/session 带上。 */
  function respondPermission(permissionId: string, decision: string): void {
    const permission = events.pendingPermissions.find(
      (item) => item.permission_id === permissionId,
    )
    sendCommand('permission_response', '', {
      nodeId: permission?.node_id,
      agentId: permission?.agent_id,
      sessionId: permission?.session_id,
      permissionId,
      decision,
    })
  }

  /** 请求最新快照（sync/snapshot）。 */
  function requestSnapshot(): void {
    cloudClient.requestSnapshot()
  }

  return {
    action,
    echoText,
    promptText,
    canSend,
    sendCommand,
    sendPromptTo,
    interruptSession,
    sendNodeTool,
    promptBlockReason,
    interruptBlockReason,
    respondPermission,
    requestSnapshot,
  }
})
