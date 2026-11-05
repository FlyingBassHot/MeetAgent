import type {
  AgentView,
  CommandResult,
  CommandStatus,
  JsonObject,
  JsonValue,
  PermissionDecision,
  ProtocolEvent,
  SessionView,
} from '@/domain/protocol';
import { SCHEMA_VERSION } from '@/domain/protocol';
import { textField } from '@/domain/payload';
import { createMockState, snapshotFromState, type MockState } from '@/services/mock/scenario';
import type {
  ClientConfig,
  ClientTransport,
  CommandRequest,
  TransportEvent,
  TransportListener,
} from './types';

const REPLY_PREFIX = '（Mock 回复）已收到 Prompt：';
const REPLY_SUFFIX = ' 这是本地脚本生成的流式回复，没有调用真实 Agent。';

function chunkText(text: string, size: number): string[] {
  const chunks: string[] = [];
  for (let index = 0; index < text.length; index += size) {
    chunks.push(text.slice(index, index + size));
  }
  return chunks;
}

function buildReply(prompt: string): string {
  return `${REPLY_PREFIX}“${prompt}”${REPLY_SUFFIX}`;
}

function commandResult(
  commandId: string,
  status: CommandStatus,
  data?: JsonValue,
  error?: string,
): CommandResult {
  const result: CommandResult = {
    schema_version: SCHEMA_VERSION,
    command_id: commandId,
    status,
    completed_at: new Date().toISOString(),
  };
  if (data !== undefined) result.data = data;
  if (error) result.error = error;
  return result;
}

export class MockClient implements ClientTransport {
  private listeners = new Set<TransportListener>();
  private state: MockState = createMockState();
  private timers = new Set<number>();
  private heartbeatTimer: number | null = null;
  private sessionVersion = new Map<string, number>();
  private activeMessages = new Map<string, string>();
  private messageCounter = 0;

  connect(_config: ClientConfig): void {
    this.emit({ kind: 'phase', phase: 'connecting', detail: '正在启动本地 Mock Node' });
    this.later(() => {
      this.emit({ kind: 'phase', phase: 'authenticating', detail: 'Mock 握手（无需配对码）' });
      this.later(() => {
        const snapshot = snapshotFromState(this.state);
        this.emit({
          kind: 'welcome',
          welcome: {
            schema_version: SCHEMA_VERSION,
            client_id: 'cli_mock_mobile',
            client_type: 'mobile',
            node: snapshot.node,
            snapshot,
            server_time: snapshot.server_time,
          },
        });
        this.emit({ kind: 'phase', phase: 'online', detail: 'Mock Node 已连接' });
        this.startHeartbeat();
      }, 180);
    }, 260);
  }

  disconnect(): void {
    this.clearTimers();
    this.emit({ kind: 'phase', phase: 'offline', detail: 'Mock 连接已断开' });
  }

  setLastSequence(sequence: number): void {
    this.state.lastSequence = Math.max(this.state.lastSequence, sequence);
  }

  subscribe(listener: TransportListener): () => void {
    this.listeners.add(listener);
    return () => {
      this.listeners.delete(listener);
    };
  }

  send(command: CommandRequest): Promise<CommandResult> {
    const { promise, resolve } = Promise.withResolvers<CommandResult>();
    const commandId = `cmd_mock_${Date.now().toString(36)}${Math.random().toString(36).slice(2, 6)}`;
    this.later(() => {
      const result = this.execute(commandId, command);
      this.emit({ kind: 'command-result', result });
      resolve(result);
    }, 90);
    return promise;
  }

  private execute(commandId: string, command: CommandRequest): CommandResult {
    switch (command.type) {
      case 'ping':
        return commandResult(commandId, 'completed', { pong: true });
      case 'node.status':
        return commandResult(commandId, 'completed', {
          node: this.state.node as unknown as JsonValue,
          agents: this.state.agents as unknown as JsonValue,
        });
      case 'session.list':
        return commandResult(commandId, 'completed', {
          sessions: this.state.agents.flatMap((agent) => agent.sessions) as unknown as JsonValue,
        });
      case 'session.prompt':
        return this.prompt(commandId, command);
      case 'session.interrupt':
        return this.interrupt(commandId, command);
      case 'permission.respond':
        return this.respondPermission(commandId, command);
      default:
        return commandResult(commandId, 'rejected', undefined, `不支持的 Mock 命令 ${command.type}`);
    }
  }

  private prompt(commandId: string, command: CommandRequest): CommandResult {
    const text = textField(command.payload, 'text')?.trim() ?? '';
    if (text.length === 0) {
      return commandResult(commandId, 'rejected', undefined, 'Prompt 不能为空');
    }
    const session = this.findSession(command.sessionId);
    if (!session) {
      return commandResult(commandId, 'rejected', undefined, 'Session 不存在或已被删除');
    }
    const agent = this.findAgent(session.agent_id);
    const version = this.bumpVersion(session.session_id);
    const now = new Date().toISOString();
    session.status = 'running';
    session.updated_at = now;
    if (agent) agent.status = 'busy';
    this.emitEvent(
      'session.status_changed',
      session.session_id,
      session.agent_id,
      { status: 'running' },
      now,
    );
    if (agent) {
      this.emitEvent('agent.status_changed', session.session_id, agent.agent_id, { status: 'busy' }, now);
    }

    this.messageCounter += 1;
    const messageId = `msg_mock_${this.messageCounter}`;
    this.activeMessages.set(session.session_id, messageId);
    this.emitEvent(
      'message.started',
      session.session_id,
      session.agent_id,
      { message_id: messageId, role: 'assistant' },
      now,
    );

    const chunks = chunkText(buildReply(text), 18);
    chunks.forEach((chunk, index) => {
      this.later(() => {
        if (this.sessionVersion.get(session.session_id) !== version) return;
        this.emitEvent('message.delta', session.session_id, session.agent_id, {
          message_id: messageId,
          delta: chunk,
        });
        if (index !== chunks.length - 1) return;
        this.later(() => {
          if (this.sessionVersion.get(session.session_id) !== version) return;
          this.finishReply(session, agent, messageId);
        }, 360);
      }, 420 * (index + 1));
    });

    return commandResult(commandId, 'completed', { message_id: messageId });
  }

  private finishReply(
    session: SessionView,
    agent: AgentView | undefined,
    messageId: string,
  ): void {
    const now = new Date().toISOString();
    this.activeMessages.delete(session.session_id);
    session.status = 'idle';
    session.updated_at = now;
    this.emitEvent(
      'message.completed',
      session.session_id,
      session.agent_id,
      { message_id: messageId },
      now,
    );
    this.emitEvent(
      'session.status_changed',
      session.session_id,
      session.agent_id,
      { status: 'idle' },
      now,
    );
    if (agent && !agent.sessions.some((item) => item.status === 'running')) {
      agent.status = 'idle';
      this.emitEvent('agent.status_changed', session.session_id, agent.agent_id, { status: 'idle' }, now);
    }
  }

  private interrupt(commandId: string, command: CommandRequest): CommandResult {
    const session = this.findSession(command.sessionId);
    if (!session) {
      return commandResult(commandId, 'rejected', undefined, 'Session 不存在或已被删除');
    }
    const wasActive = session.status === 'running' || session.status === 'waiting';
    this.bumpVersion(session.session_id);
    const now = new Date().toISOString();
    const activeMessageId = this.activeMessages.get(session.session_id);
    if (activeMessageId) {
      this.activeMessages.delete(session.session_id);
      this.emitEvent(
        'message.completed',
        session.session_id,
        session.agent_id,
        { message_id: activeMessageId, interrupted: true },
        now,
      );
    }
    session.status = 'idle';
    session.updated_at = now;
    this.emitEvent(
      'session.status_changed',
      session.session_id,
      session.agent_id,
      { status: 'idle', interrupted: wasActive },
      now,
    );
    const agent = this.findAgent(session.agent_id);
    if (agent && !agent.sessions.some((item) => item.status === 'running')) {
      agent.status = 'idle';
      this.emitEvent('agent.status_changed', session.session_id, agent.agent_id, { status: 'idle' }, now);
    }
    return commandResult(commandId, 'completed', { interrupted: wasActive });
  }

  private respondPermission(commandId: string, command: CommandRequest): CommandResult {
    const permissionId = textField(command.payload, 'permission_id') ?? '';
    const decision = textField(command.payload, 'decision') ?? '';
    if (!this.isDecision(decision)) {
      return commandResult(commandId, 'rejected', undefined, '无效的审批决策');
    }
    const index = this.state.pendingPermissions.findIndex(
      (permission) => permission.permission_id === permissionId,
    );
    if (index < 0) {
      return commandResult(commandId, 'rejected', undefined, '审批已处理或不存在');
    }
    const [permission] = this.state.pendingPermissions.splice(index, 1);
    const status = decision === 'reject' ? 'rejected' : 'approved';
    const now = new Date().toISOString();
    this.emitEvent(
      'permission.resolved',
      permission?.session_id,
      permission?.agent_id,
      { permission_id: permissionId, decision, status },
      now,
    );
    const session = this.findSession(permission?.session_id);
    if (session && session.status === 'waiting') {
      session.status = 'idle';
      session.updated_at = now;
      this.emitEvent('session.status_changed', session.session_id, session.agent_id, { status: 'idle' }, now);
    }
    return commandResult(commandId, 'completed', { permission_id: permissionId, decision });
  }

  private isDecision(value: string): value is PermissionDecision {
    return value === 'once' || value === 'always' || value === 'reject';
  }

  private findSession(sessionId: string | undefined): SessionView | undefined {
    if (!sessionId) return undefined;
    for (const agent of this.state.agents) {
      const session = agent.sessions.find((item) => item.session_id === sessionId);
      if (session) return session;
    }
    return undefined;
  }

  private findAgent(agentId: string): AgentView | undefined {
    return this.state.agents.find((agent) => agent.agent_id === agentId);
  }

  private bumpVersion(sessionId: string): number {
    const next = (this.sessionVersion.get(sessionId) ?? 0) + 1;
    this.sessionVersion.set(sessionId, next);
    return next;
  }

  private emitEvent(
    type: string,
    sessionId: string | undefined,
    agentId: string | undefined,
    payload: JsonObject,
    occurredAt: string = new Date().toISOString(),
  ): ProtocolEvent {
    this.state.lastSequence += 1;
    const event: ProtocolEvent = {
      schema_version: SCHEMA_VERSION,
      event_id: `evt_mock_${this.state.lastSequence}`,
      node_id: this.state.node.node_id,
      sequence: this.state.lastSequence,
      type,
      occurred_at: occurredAt,
      payload,
    };
    if (sessionId) event.session_id = sessionId;
    if (agentId) event.agent_id = agentId;
    this.state.events.push(event);
    if (this.state.events.length > 300) this.state.events.splice(0, this.state.events.length - 300);
    this.later(() => this.emit({ kind: 'event', event }), 40);
    return event;
  }

  private startHeartbeat(): void {
    this.stopHeartbeat();
    const beat = (): void => {
      this.emit({
        kind: 'heartbeat',
        rttMs: 18 + Math.round(Math.random() * 22),
        serverTime: new Date().toISOString(),
      });
    };
    beat();
    this.heartbeatTimer = window.setInterval(beat, 12_000);
  }

  private stopHeartbeat(): void {
    if (this.heartbeatTimer !== null) {
      window.clearInterval(this.heartbeatTimer);
      this.heartbeatTimer = null;
    }
  }

  private later(callback: () => void, delayMs: number): void {
    const timer = window.setTimeout(() => {
      this.timers.delete(timer);
      callback();
    }, delayMs);
    this.timers.add(timer);
  }

  private clearTimers(): void {
    this.stopHeartbeat();
    for (const timer of this.timers) window.clearTimeout(timer);
    this.timers.clear();
  }

  private emit(event: TransportEvent): void {
    for (const listener of this.listeners) listener(event);
  }
}
