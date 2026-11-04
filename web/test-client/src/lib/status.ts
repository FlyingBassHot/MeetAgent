/** 阶段文案、状态色调、pending 文案（纯函数 + 常量表）。 */

import type { ConnectionPhase } from '../api/cloud-client'

/** 连接阶段对应的胶囊色调。 */
export function phaseTone(phase: ConnectionPhase): 'success' | 'busy' | 'idle' {
  if (phase === 'online') return 'success'
  if (phase === 'offline') return 'idle'
  return 'busy'
}

export const PHASE_LABELS: Record<ConnectionPhase, string> = {
  offline: '离线',
  connecting: '连接中',
  registering: '注册中',
  online: '在线',
  reconnecting: '重连中',
}

/** 在途请求状态的中文文案（未知状态原样返回）。 */
export function pendingStatusLabel(status: string): string {
  switch (status) {
    case 'sent':
      return '已发送'
    case 'accepted':
      return '已接收'
    case 'executing':
      return '执行中'
    case 'completed':
      return '已完成'
    case 'failed':
      return '失败'
    case 'rejected':
      return '已拒绝'
    case 'timeout':
      return '超时'
    default:
      return status
  }
}

/** Session 状态中文文案（未知状态原样返回）。 */
export function sessionStatusLabel(status: string): string {
  switch (status) {
    case 'running':
    case 'busy':
      return '运行中'
    case 'idle':
      return '空闲'
    case 'error':
    case 'failed':
      return '错误'
    default:
      return status || '未知'
  }
}

export type StatusTone = 'success' | 'busy' | 'danger' | 'idle'

/** Session 状态色调：运行中 / 空闲 / 错误 / 其他。 */
export function sessionStatusTone(status: string): StatusTone {
  if (status === 'running' || status === 'busy') return 'busy'
  if (status === 'error' || status === 'failed') return 'danger'
  if (status === 'idle') return 'success'
  return 'idle'
}

type EventTone = 'danger' | 'warn' | 'success' | 'muted' | ''

/** 根据事件类型推断色调（用于事件流左边线 / 标签）。 */
export function eventTone(eventType: string): EventTone {
  if (eventType.includes('error') || eventType.includes('failed')) return 'danger'
  if (eventType.includes('permission.requested')) return 'warn'
  if (
    eventType.endsWith('.online') ||
    eventType.endsWith('.connected') ||
    eventType.endsWith('.completed')
  ) {
    return 'success'
  }
  if (eventType.endsWith('.offline') || eventType.endsWith('.disconnected')) {
    return 'muted'
  }
  return ''
}
