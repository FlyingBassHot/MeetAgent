// 状态与事件的中文映射。未知值一律原样展示，方便协议扩展时诊断。

export type Tone = 'neutral' | 'info' | 'success' | 'warning' | 'danger';

const AGENT_STATUS: Record<string, string> = {
  busy: '忙碌',
  working: '忙碌',
  idle: '空闲',
  online: '在线',
  offline: '离线',
  starting: '启动中',
  error: '异常',
};

const SESSION_STATUS: Record<string, string> = {
  running: '运行中',
  busy: '运行中',
  waiting: '等待输入',
  idle: '空闲',
  paused: '已暂停',
  completed: '已完成',
  failed: '失败',
  error: '异常',
};

const PERMISSION_STATUS: Record<string, string> = {
  pending: '待审批',
  approved: '已允许',
  rejected: '已拒绝',
  expired: '已过期',
};

const COMMAND_STATUS: Record<string, string> = {
  accepted: '已接受',
  executing: '执行中',
  completed: '已完成',
  failed: '失败',
  rejected: '被拒绝',
  timeout: '超时',
};

export const EVENT_LABELS: Record<string, string> = {
  'agent.connected': 'Agent 已连接',
  'agent.disconnected': 'Agent 已断开',
  'agent.status_changed': 'Agent 状态变化',
  'agent.error': 'Agent 异常',
  'session.created': 'Session 创建',
  'session.updated': 'Session 更新',
  'session.deleted': 'Session 删除',
  'session.status_changed': 'Session 状态变化',
  'message.started': '消息开始',
  'message.delta': '消息增量',
  'message.completed': '消息完成',
  'tool.started': '工具调用开始',
  'tool.completed': '工具调用完成',
  'tool.failed': '工具调用失败',
  'permission.requested': '审批请求',
  'permission.resolved': '审批完成',
};

export type EventGroup = 'message' | 'tool' | 'permission' | 'session' | 'agent' | 'other';

export function eventGroup(type: string): EventGroup {
  if (type.startsWith('message.')) return 'message';
  if (type.startsWith('tool.')) return 'tool';
  if (type.startsWith('permission.')) return 'permission';
  if (type.startsWith('session.')) return 'session';
  if (type.startsWith('agent.')) return 'agent';
  return 'other';
}

export function eventTone(type: string): Tone {
  if (type === 'agent.error' || type === 'tool.failed' || type === 'session.deleted') return 'danger';
  if (type === 'permission.requested') return 'warning';
  if (type === 'permission.resolved' || type === 'tool.completed') return 'success';
  if (type.startsWith('message.')) return 'info';
  return 'neutral';
}

export function agentStatusLabel(status: string): string {
  return AGENT_STATUS[status] ?? status;
}

export function agentStatusTone(status: string): Tone {
  switch (status) {
    case 'busy':
    case 'working':
    case 'online':
      return 'success';
    case 'starting':
      return 'info';
    case 'error':
      return 'danger';
    case 'offline':
      return 'neutral';
    default:
      return 'neutral';
  }
}

export function sessionStatusLabel(status: string): string {
  return SESSION_STATUS[status] ?? status;
}

export function sessionStatusTone(status: string): Tone {
  switch (status) {
    case 'running':
    case 'busy':
      return 'success';
    case 'waiting':
      return 'warning';
    case 'failed':
    case 'error':
      return 'danger';
    case 'completed':
      return 'info';
    default:
      return 'neutral';
  }
}

export function permissionStatusLabel(status: string): string {
  return PERMISSION_STATUS[status] ?? status;
}

export function permissionStatusTone(status: string): Tone {
  switch (status) {
    case 'approved':
      return 'success';
    case 'pending':
      return 'warning';
    case 'rejected':
      return 'danger';
    case 'expired':
      return 'neutral';
    default:
      return 'neutral';
  }
}

export function commandStatusLabel(status: string): string {
  return COMMAND_STATUS[status] ?? status;
}

export function decisionLabel(decision: string): string {
  switch (decision) {
    case 'once':
      return '允许一次';
    case 'always':
      return '始终允许';
    case 'reject':
      return '拒绝';
    default:
      return decision;
  }
}
