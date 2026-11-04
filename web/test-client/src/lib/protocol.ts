export const PROTOCOL_VERSION = '1'

export type MessageType =
  | 'register'
  | 'heartbeat'
  | 'command'
  | 'response'
  | 'event'
  | 'sync'
  | 'error'

export type PeerType = 'node' | 'device' | 'cloud'

export interface Source {
  type: PeerType
  id: string
}

export interface Target {
  node_id?: string
  agent_id?: string
  session_id?: string
}

export interface Envelope {
  version: string
  message_id: string
  type: MessageType
  action?: string
  request_id?: string
  source: Source
  target?: Target
  timestamp: string
  payload?: unknown
}

export interface RegisterPayload {
  token: string
  name?: string
  hostname?: string
  platform?: string
  version?: string
  agents?: AgentView[]
  last_sequences?: Record<string, number>
}

export interface RegisterResult {
  ok: boolean
  connection_id?: string
  identity?: Source
  server_time?: string
  snapshot?: Snapshot
  error?: string
  error_code?: string
}

export interface SessionView {
  session_id: string
  agent_id: string
  node_id: string
  title?: string
  working_directory?: string
  status: string
  updated_at: string
}

export interface AgentView {
  agent_id: string
  node_id: string
  type: string
  name: string
  status: string
  capabilities?: Record<string, boolean>
  sessions?: SessionView[]
}

export interface NodeView {
  node_id: string
  name: string
  hostname?: string
  platform?: string
  version?: string
  status: 'online' | 'offline' | string
  connected_at?: string
  last_seen?: string
  remote_addr?: string
  agents: AgentView[]
}

export type PermissionStatus = 'pending' | 'approved' | 'rejected' | 'expired'

export interface PermissionView {
  permission_id: string
  node_id: string
  agent_id?: string
  session_id?: string
  description?: string
  status: PermissionStatus
  expires_at?: string
  created_at: string
}

export interface AgentEvent {
  event_id: string
  sequence: number
  node_id: string
  agent_id?: string
  session_id?: string
  event_type: string
  timestamp: string
  payload?: unknown
}

export interface Snapshot {
  nodes: NodeView[]
  events: AgentEvent[]
  last_sequences: Record<string, number>
  pending_permissions: PermissionView[]
  server_time: string
}

export interface EventReplayRequest {
  last_sequences: Record<string, number>
  limit?: number
}

export interface EventReplayResult {
  events: AgentEvent[]
  last_sequences: Record<string, number>
  truncated: string[]
  server_time: string
}

export type CommandStatus =
  | 'accepted'
  | 'executing'
  | 'completed'
  | 'failed'
  | 'rejected'
  | 'timeout'

export interface CommandResult {
  status: CommandStatus
  data?: unknown
  error?: string
  duration_ms?: number
}

export interface HeartbeatPayload {
  nonce?: string
  server_time?: string
}

export interface ErrorPayload {
  code: string
  message: string
  detail?: string
}

const TERMINAL_STATUSES: ReadonlySet<string> = new Set([
  'completed',
  'failed',
  'rejected',
  'timeout',
])

export function isTerminalStatus(status: string): boolean {
  return TERMINAL_STATUSES.has(status)
}

export function newMessageId(prefix: string): string {
  if (typeof crypto !== 'undefined' && typeof crypto.randomUUID === 'function') {
    return `${prefix}_${crypto.randomUUID().replaceAll('-', '').slice(0, 16)}`
  }
  return `${prefix}_${Date.now().toString(36)}${Math.random().toString(36).slice(2, 10)}`
}

export function makeEnvelope(
  type: MessageType,
  action: string,
  source: Source,
  payload?: unknown,
): Envelope {
  return {
    version: PROTOCOL_VERSION,
    message_id: newMessageId('msg'),
    type,
    action: action || undefined,
    source,
    timestamp: new Date().toISOString(),
    payload,
  }
}