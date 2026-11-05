// MeetAgent V2.1 客户端协议类型。
// 字段与 pkg/clientproto/protocol.go、pkg/protocol/payload.go 保持一致。
// 协议字段保持英文，界面文案在 domain/labels.ts 中映射。

export const SCHEMA_VERSION = 1;
export const CLIENT_VERSION = '0.1.0';
export const CLIENT_TYPE = 'mobile';
export const DEFAULT_CAPABILITIES = [
  'ui.full',
  'display.color',
  'display.touch',
  'input.text',
  'notification.local',
];

export type JsonPrimitive = string | number | boolean | null;
export type JsonValue = JsonPrimitive | JsonValue[] | { [key: string]: JsonValue };
export type JsonObject = { [key: string]: JsonValue };

export type ClientType = 'mobile' | 'terminal' | 'web' | 'desktop';
export type ConnectionPhase =
  | 'idle'
  | 'connecting'
  | 'authenticating'
  | 'online'
  | 'reconnecting'
  | 'offline'
  | 'error';

export type CommandStatus =
  | 'accepted'
  | 'executing'
  | 'completed'
  | 'failed'
  | 'rejected'
  | 'timeout';

export type PermissionStatus = 'pending' | 'approved' | 'rejected' | 'expired';
export type PermissionDecision = 'once' | 'always' | 'reject';

export interface NodeInfo {
  node_id: string;
  name: string;
  hostname?: string;
  platform?: string;
  version?: string;
  protocol_version: number;
}

export interface AdapterInfo {
  id: string;
  name: string;
  version?: string;
  api_version?: number;
  runtime?: string;
  capabilities?: string[];
  permissions?: string[];
}

export interface SessionView {
  session_id: string;
  agent_id: string;
  node_id: string;
  title?: string;
  working_directory?: string;
  status: string;
  updated_at: string;
}

export interface AgentView {
  agent_id: string;
  node_id: string;
  type: string;
  name: string;
  status: string;
  capabilities?: Record<string, boolean>;
  sessions: SessionView[];
}

export interface PermissionView {
  permission_id: string;
  node_id: string;
  agent_id?: string;
  session_id?: string;
  description?: string;
  status: PermissionStatus;
  expires_at?: string;
  created_at: string;
}

export interface EventSource {
  adapter_id?: string;
  native_event_type?: string;
}

export interface ProtocolEvent {
  schema_version: number;
  event_id: string;
  node_id: string;
  sequence: number;
  type: string;
  occurred_at: string;
  agent_id?: string;
  session_id?: string;
  payload?: JsonObject;
  source?: EventSource;
}

export interface Snapshot {
  schema_version: number;
  node: NodeInfo;
  adapters?: AdapterInfo[];
  agents: AgentView[];
  events?: ProtocolEvent[];
  last_sequence: number;
  pending_permissions?: PermissionView[];
  server_time: string;
}

export interface Welcome {
  schema_version: number;
  client_id: string;
  client_type?: ClientType;
  token?: string;
  node: NodeInfo;
  snapshot: Snapshot;
  server_time: string;
}

export interface Command {
  schema_version: number;
  command_id: string;
  node_id?: string;
  agent_id?: string;
  session_id?: string;
  type: string;
  payload?: JsonObject;
  created_at: string;
}

export interface CommandResult {
  schema_version: number;
  command_id: string;
  status: CommandStatus;
  data?: JsonValue;
  error?: string;
  duration_ms?: number;
  completed_at: string;
}

export interface ErrorPayload {
  code: string;
  message: string;
}

export interface PairRequest {
  client_type: ClientType;
  client_version?: string;
  name?: string;
  capabilities?: string[];
  pairing_code: string;
}

export interface PairResult {
  schema_version: number;
  client_id: string;
  token: string;
  permissions: string[];
  node: NodeInfo;
  server_time: string;
}

export interface Info {
  schema_version: number;
  protocol_version: number;
  node: NodeInfo;
  adapters?: AdapterInfo[];
  pairing_required: boolean;
  pairing_code_ttl_ms: number;
  ws_path: string;
}

export interface HeartbeatPayload {
  nonce?: string;
  server_time?: string;
}

export interface Envelope {
  schema_version: number;
  type: string;
  message_id?: string;
  payload?: unknown;
}

export function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}

export function isTerminalStatus(status: CommandStatus): boolean {
  return (
    status === 'completed' ||
    status === 'failed' ||
    status === 'rejected' ||
    status === 'timeout'
  );
}
