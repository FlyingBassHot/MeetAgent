import type {
  AdapterInfo,
  AgentView,
  NodeInfo,
  PermissionView,
  ProtocolEvent,
  Snapshot,
} from '@/domain/protocol';
import { SCHEMA_VERSION } from '@/domain/protocol';

export interface MockState {
  node: NodeInfo;
  adapters: AdapterInfo[];
  agents: AgentView[];
  events: ProtocolEvent[];
  pendingPermissions: PermissionView[];
  lastSequence: number;
}

function minutesAgo(now: number, minutes: number): string {
  return new Date(now - minutes * 60_000).toISOString();
}

function seedEvent(
  sequence: number,
  type: string,
  occurredAt: string,
  payload: ProtocolEvent['payload'],
  sessionId?: string,
  agentId?: string,
): ProtocolEvent {
  const event: ProtocolEvent = {
    schema_version: SCHEMA_VERSION,
    event_id: `evt_mock_seed_${sequence}`,
    node_id: 'node-home',
    sequence,
    type,
    occurred_at: occurredAt,
    payload,
  };
  if (sessionId) event.session_id = sessionId;
  if (agentId) event.agent_id = agentId;
  return event;
}

export function createMockState(now: number = Date.now()): MockState {
  const node: NodeInfo = {
    node_id: 'node-home',
    name: 'Perch Workstation',
    hostname: 'perch-omarchy',
    platform: 'linux/x64',
    version: '2.1.0-mock',
    protocol_version: 1,
  };

  const adapters: AdapterInfo[] = [
    {
      id: 'opencode',
      name: 'OpenCode Adapter',
      version: '2.1.0',
      api_version: 1,
      runtime: 'local',
      capabilities: ['session.list', 'session.prompt', 'session.interrupt'],
      permissions: ['session.read', 'session.control'],
    },
    {
      id: 'mock-script',
      name: 'Mock Script Adapter',
      version: '0.1.0',
      api_version: 1,
      runtime: 'mock',
      capabilities: ['session.list', 'session.prompt'],
      permissions: ['session.read'],
    },
  ];

  const agents: AgentView[] = [
    {
      agent_id: 'opencode-main',
      node_id: node.node_id,
      type: 'opencode',
      name: 'OpenCode Main',
      status: 'busy',
      capabilities: { send_prompt: true, interrupt: true },
      sessions: [
        {
          session_id: 'ses_auth_retry',
          agent_id: 'opencode-main',
          node_id: node.node_id,
          title: '修复登录重试逻辑',
          working_directory: '/home/perch/Work/MeetAgent',
          status: 'running',
          updated_at: minutesAgo(now, 1),
        },
        {
          session_id: 'ses_protocol_docs',
          agent_id: 'opencode-main',
          node_id: node.node_id,
          title: '整理 V2.1 协议文档',
          working_directory: '/home/perch/Work/MeetAgent',
          status: 'waiting',
          updated_at: minutesAgo(now, 90),
        },
      ],
    },
    {
      agent_id: 'mock-reviewer',
      node_id: node.node_id,
      type: 'mock-script',
      name: 'Mock Reviewer',
      status: 'idle',
      capabilities: { send_prompt: true, interrupt: true },
      sessions: [
        {
          session_id: 'ses_event_cache',
          agent_id: 'mock-reviewer',
          node_id: node.node_id,
          title: '重构事件缓存',
          working_directory: '/home/perch/Work/MeetAgent',
          status: 'waiting',
          updated_at: minutesAgo(now, 3),
        },
        {
          session_id: 'ses_terminal_theme',
          agent_id: 'mock-reviewer',
          node_id: node.node_id,
          title: '终端 UI 主题校验',
          working_directory: '/home/perch/Work/MeetAgent',
          status: 'completed',
          updated_at: minutesAgo(now, 180),
        },
      ],
    },
  ];

  const pendingPermissions: PermissionView[] = [
    {
      permission_id: 'per_mock_test_run',
      node_id: node.node_id,
      agent_id: 'mock-reviewer',
      session_id: 'ses_event_cache',
      description: '运行 `go test ./...` 校验事件缓存（Mock 审批请求）',
      status: 'pending',
      created_at: minutesAgo(now, 3),
      expires_at: new Date(now + 7 * 60_000).toISOString(),
    },
  ];

  const seed = [
    seedEvent(
      101,
      'session.created',
      minutesAgo(now, 180),
      { session_id: 'ses_terminal_theme', title: '终端 UI 主题校验', status: 'completed' },
      'ses_terminal_theme',
      'mock-reviewer',
    ),
    seedEvent(
      102,
      'session.created',
      minutesAgo(now, 90),
      { session_id: 'ses_protocol_docs', title: '整理 V2.1 协议文档', status: 'waiting' },
      'ses_protocol_docs',
      'opencode-main',
    ),
    seedEvent(
      103,
      'session.status_changed',
      minutesAgo(now, 90),
      { status: 'waiting' },
      'ses_protocol_docs',
      'opencode-main',
    ),
    seedEvent(
      104,
      'session.created',
      minutesAgo(now, 60),
      { session_id: 'ses_event_cache', title: '重构事件缓存', status: 'idle' },
      'ses_event_cache',
      'mock-reviewer',
    ),
    seedEvent(
      105,
      'message.started',
      minutesAgo(now, 42),
      { message_id: 'msg_seed_cache', role: 'assistant' },
      'ses_event_cache',
      'mock-reviewer',
    ),
    seedEvent(
      106,
      'message.delta',
      minutesAgo(now, 42),
      { message_id: 'msg_seed_cache', delta: '审批通过后我会运行测试并汇总结果。' },
      'ses_event_cache',
      'mock-reviewer',
    ),
    seedEvent(
      107,
      'message.completed',
      minutesAgo(now, 41),
      { message_id: 'msg_seed_cache' },
      'ses_event_cache',
      'mock-reviewer',
    ),
    seedEvent(
      108,
      'agent.status_changed',
      minutesAgo(now, 40),
      { status: 'idle' },
      'ses_event_cache',
      'mock-reviewer',
    ),
    seedEvent(
      109,
      'session.created',
      minutesAgo(now, 24),
      { session_id: 'ses_auth_retry', title: '修复登录重试逻辑', status: 'running' },
      'ses_auth_retry',
      'opencode-main',
    ),
    seedEvent(
      110,
      'message.started',
      minutesAgo(now, 6),
      { message_id: 'msg_seed_auth', role: 'assistant' },
      'ses_auth_retry',
      'opencode-main',
    ),
    seedEvent(
      111,
      'message.delta',
      minutesAgo(now, 6),
      { message_id: 'msg_seed_auth', delta: '已定位登录重试失败的原因：' },
      'ses_auth_retry',
      'opencode-main',
    ),
    seedEvent(
      112,
      'message.delta',
      minutesAgo(now, 5),
      { message_id: 'msg_seed_auth', delta: '刷新 Token 时没有重置重试计数。' },
      'ses_auth_retry',
      'opencode-main',
    ),
    seedEvent(
      113,
      'message.completed',
      minutesAgo(now, 5),
      { message_id: 'msg_seed_auth' },
      'ses_auth_retry',
      'opencode-main',
    ),
    seedEvent(
      114,
      'tool.started',
      minutesAgo(now, 4),
      { tool: 'read', target: 'internal/auth/retry.go' },
      'ses_auth_retry',
      'opencode-main',
    ),
    seedEvent(
      115,
      'tool.completed',
      minutesAgo(now, 4),
      { tool: 'read', target: 'internal/auth/retry.go', duration_ms: 42 },
      'ses_auth_retry',
      'opencode-main',
    ),
    seedEvent(
      116,
      'permission.requested',
      minutesAgo(now, 3),
      {
        permission_id: 'per_mock_test_run',
        description: '运行 `go test ./...` 校验事件缓存（Mock 审批请求）',
        status: 'pending',
      },
      'ses_event_cache',
      'mock-reviewer',
    ),
    seedEvent(
      117,
      'session.status_changed',
      minutesAgo(now, 3),
      { status: 'waiting' },
      'ses_event_cache',
      'mock-reviewer',
    ),
    seedEvent(
      118,
      'session.status_changed',
      minutesAgo(now, 1),
      { status: 'running' },
      'ses_auth_retry',
      'opencode-main',
    ),
  ];

  return {
    node,
    adapters,
    agents,
    events: seed,
    pendingPermissions,
    lastSequence: 118,
  };
}

export function snapshotFromState(state: MockState): Snapshot {
  return {
    schema_version: SCHEMA_VERSION,
    node: structuredClone(state.node),
    adapters: structuredClone(state.adapters),
    agents: structuredClone(state.agents),
    events: structuredClone(state.events),
    last_sequence: state.lastSequence,
    pending_permissions: structuredClone(state.pendingPermissions),
    server_time: new Date().toISOString(),
  };
}
