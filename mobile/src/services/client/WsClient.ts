import type {
  Command,
  CommandResult,
  ConnectionPhase,
  ErrorPayload,
  HeartbeatPayload,
  ProtocolEvent,
  Snapshot,
  Welcome,
} from '@/domain/protocol';
import { CLIENT_TYPE, CLIENT_VERSION, SCHEMA_VERSION, isRecord, isTerminalStatus } from '@/domain/protocol';
import { parseEnvelope, commandEnvelope, heartbeatEnvelope, snapshotEnvelope, newCommandId, newMessageId } from './envelope';
import type {
  ClientConfig,
  ClientTransport,
  CommandRequest,
  TransportEvent,
  TransportListener,
} from './types';
import { toWebSocketUrl } from '@/services/storage/preferences';
import { isNetworkOnline, subscribeNetwork } from '@/services/connectivity/networkStatus';

const COMMAND_TIMEOUT_MS = 15_000;
const AUTH_TIMEOUT_MS = 10_000;
const HEARTBEAT_INTERVAL_MS = 10_000;
const MAX_RECONNECT_DELAY_MS = 15_000;
const SNAPSHOT_TIMEOUT_MS = 8_000;

interface SnapshotWaiter {
  resolve: () => void;
  reject: (error: Error) => void;
  timer: number;
}

const FATAL_ERROR_CODES: Record<string, true> = {
  unauthorized: true,
  auth_failed: true,
  token_invalid: true,
  token_revoked: true,
  pairing_failed: true,
  pairing_invalid: true,
  pairing_expired: true,
  unsupported_schema: true,
  protocol_version: true,
  version_mismatch: true,
};

interface PendingCommand {
  resolve: (result: CommandResult) => void;
  reject: (error: Error) => void;
  timer: number;
}

interface HelloPayload {
  client_type: string;
  client_version: string;
  name: string;
  capabilities: string[];
  token?: string;
  pairing_code?: string;
  last_sequence?: number;
}

export class WsClient implements ClientTransport {
  private socket: WebSocket | null = null;
  private listeners = new Set<TransportListener>();
  private pending = new Map<string, PendingCommand>();
  private snapshotWaiters = new Set<SnapshotWaiter>();
  private config: ClientConfig | null = null;
  private sequence = 0;
  private attempt = 0;
  private intentionalClose = true;
  private reconnectTimer: number | null = null;
  private authTimer: number | null = null;
  private heartbeatTimer: number | null = null;
  private beatNonce = '';
  private beatSentAt = 0;
  private networkUnsubscribe: (() => void) | null = null;
  private offlineSuspended = false;

  private readonly handleNetworkOffline = (): void => {
    this.offlineSuspended = true;
    this.clearTimers();
    this.rejectPending(new Error('网络离线'));
    const socket = this.socket;
    this.socket = null;
    if (socket) {
      socket.onopen = null;
      socket.onmessage = null;
      socket.onclose = null;
      socket.onerror = null;
      if (socket.readyState === WebSocket.OPEN || socket.readyState === WebSocket.CONNECTING) {
        socket.close(4002, 'network_offline');
      }
    }
    this.setPhase('offline', '网络离线');
  };

  private readonly handleNetworkOnline = (): void => {
    if (this.intentionalClose || !this.config || !this.offlineSuspended) return;
    this.offlineSuspended = false;
    this.clearTimers();
    this.attempt = 0;
    this.openSocket();
  };

  connect(config: ClientConfig): void {
    this.config = config;
    this.sequence = config.lastSequence;
    this.attempt = 0;
    this.intentionalClose = false;
    this.bindNetwork();
    if (!isNetworkOnline()) {
      this.offlineSuspended = true;
      this.setPhase('offline', '网络离线');
      return;
    }
    this.offlineSuspended = false;
    this.openSocket();
  }

  disconnect(): void {
    this.intentionalClose = true;
    this.offlineSuspended = false;
    this.unbindNetwork();
    this.clearTimers();
    this.rejectPending(new Error('连接已断开'));
    const socket = this.socket;
    this.socket = null;
    if (socket) {
      socket.onopen = null;
      socket.onmessage = null;
      socket.onclose = null;
      socket.onerror = null;
      if (socket.readyState === WebSocket.OPEN || socket.readyState === WebSocket.CONNECTING) {
        socket.close(1000, 'client_disconnect');
      }
    }
    this.setPhase('offline', '已主动断开');
  }

  setLastSequence(sequence: number): void {
    this.sequence = Math.max(this.sequence, sequence);
  }

  refreshSnapshot(): Promise<void> {
    const socket = this.socket;
    if (!socket || socket.readyState !== WebSocket.OPEN) {
      return Promise.reject(new Error('尚未连接 Node'));
    }
    const { promise, resolve, reject } = Promise.withResolvers<void>();
    const waiter: SnapshotWaiter = {
      resolve,
      reject,
      timer: window.setTimeout(() => {
        this.snapshotWaiters.delete(waiter);
        reject(new Error('刷新快照超时'));
      }, SNAPSHOT_TIMEOUT_MS),
    };
    this.snapshotWaiters.add(waiter);
    try {
      socket.send(JSON.stringify(snapshotEnvelope()));
    } catch (err) {
      window.clearTimeout(waiter.timer);
      this.snapshotWaiters.delete(waiter);
      reject(err instanceof Error ? err : new Error('刷新快照失败'));
    }
    return promise;
  }

  subscribe(listener: TransportListener): () => void {
    this.listeners.add(listener);
    return () => {
      this.listeners.delete(listener);
    };
  }

  send(command: CommandRequest): Promise<CommandResult> {
    const socket = this.socket;
    const config = this.config;
    if (!socket || socket.readyState !== WebSocket.OPEN || !config) {
      return Promise.reject(new Error('尚未连接 Node'));
    }
    const commandId = newCommandId();
    const body: Command = {
      schema_version: SCHEMA_VERSION,
      command_id: commandId,
      type: command.type,
      payload: command.payload,
      created_at: new Date().toISOString(),
    };
    if (command.agentId) body.agent_id = command.agentId;
    if (command.sessionId) body.session_id = command.sessionId;

    const { promise, resolve, reject } = Promise.withResolvers<CommandResult>();
    const timer = window.setTimeout(() => {
      this.pending.delete(commandId);
      reject(new Error(`命令超时（${COMMAND_TIMEOUT_MS / 1000} 秒）`));
    }, COMMAND_TIMEOUT_MS);
    this.pending.set(commandId, { resolve, reject, timer });
    socket.send(JSON.stringify(commandEnvelope(body)));
    return promise;
  }

  private openSocket(): void {
    const config = this.config;
    if (!config || this.intentionalClose) return;
    if (this.offlineSuspended || !isNetworkOnline()) {
      this.offlineSuspended = true;
      this.setPhase('offline', '网络离线');
      return;
    }
    const secure = config.baseUrl.trim().toLowerCase().startsWith('https');
    this.setPhase(this.attempt === 0 ? 'connecting' : 'reconnecting');

    let socket: WebSocket;
    try {
      socket = new WebSocket(toWebSocketUrl(config.baseUrl));
    } catch {
      this.emitError('connect_failed', 'Node 地址无效，无法建立 WebSocket', true);
      return;
    }
    this.socket = socket;

    socket.onopen = () => {
      if (this.socket !== socket) return;
      this.setPhase('authenticating', '已建立 WebSocket，正在握手');
      this.startAuthTimer();
      this.sendHello(config);
    };

    socket.onmessage = (message: MessageEvent) => {
      if (this.socket !== socket) return;
      const data = typeof message.data === 'string' ? message.data : '';
      if (data.length > 0) this.handleMessage(data);
    };

    socket.onerror = () => {
      if (this.socket !== socket) return;
      this.emitError(
        'socket_error',
        secure
          ? 'WebSocket 连接异常，请检查网络或证书信任'
          : 'WebSocket 连接异常',
        false,
      );
    };

    socket.onclose = (event: CloseEvent) => {
      if (this.socket !== socket) return;
      this.socket = null;
      this.clearHandshakeTimers();
      this.rejectPending(new Error('连接已断开'));
      if (this.intentionalClose) return;
      if (!isNetworkOnline()) {
        this.offlineSuspended = true;
        this.setPhase('offline', '网络离线');
        return;
      }
      const reason = event.reason ? `：${event.reason}` : '';
      const hint = secure ? '（若为自签证书，请先在系统中信任）' : '';
      this.scheduleReconnect(`连接中断${reason}${hint}`);
    };
  }

  private sendHello(config: ClientConfig): void {
    const payload: HelloPayload = {
      client_type: CLIENT_TYPE,
      client_version: CLIENT_VERSION,
      name: config.deviceName,
      capabilities: config.capabilities,
      last_sequence: this.sequence,
    };
    if (config.token) payload.token = config.token;
    if (config.pairingCode) payload.pairing_code = config.pairingCode;
    const socket = this.socket;
    if (!socket || socket.readyState !== WebSocket.OPEN) return;
    socket.send(
      JSON.stringify({
        schema_version: SCHEMA_VERSION,
        type: 'hello',
        message_id: newMessageId(),
        payload,
      }),
    );
  }

  private handleMessage(raw: string): void {
    const envelope = parseEnvelope(raw);
    if (!envelope) {
      this.emitError('decode_failed', '收到无法解析的 WebSocket 消息', false);
      return;
    }
    switch (envelope.type) {
      case 'welcome':
        this.handleWelcome(envelope.payload);
        break;
      case 'snapshot':
        this.handleSnapshot(envelope.payload);
        break;
      case 'event':
        this.handleEvent(envelope.payload);
        break;
      case 'command.result':
        this.handleCommandResult(envelope.payload);
        break;
      case 'heartbeat':
        this.handleHeartbeat(envelope.payload);
        break;
      case 'error':
        this.handleError(envelope.payload);
        break;
      default:
        // 未知事件类型必须忽略，保持向后兼容。
        break;
    }
  }

  private handleWelcome(payload: unknown): void {
    if (!isRecord(payload)) {
      this.emitError('bad_welcome', 'Node welcome 载荷格式错误', true);
      return;
    }
    const welcome = payload as unknown as Welcome;
    if (welcome.schema_version !== SCHEMA_VERSION) {
      this.fatal('unsupported_schema', `协议版本不兼容：Node v${String(welcome.schema_version)}`);
      return;
    }
    this.clearAuthTimer();
    this.attempt = 0;
    this.sequence = welcome.snapshot?.last_sequence ?? this.sequence;
    this.setPhase('online', '握手完成');
    this.startHeartbeat();
    this.emit({ kind: 'welcome', welcome });
  }

  private handleSnapshot(payload: unknown): void {
    if (!isRecord(payload)) {
      this.rejectSnapshotWaiters(new Error('Node 返回的 snapshot 格式错误'));
      return;
    }
    const snapshot = payload as unknown as Snapshot;
    this.sequence = Math.max(this.sequence, snapshot.last_sequence ?? 0);
    this.emit({ kind: 'snapshot', snapshot });
    for (const waiter of this.snapshotWaiters) {
      window.clearTimeout(waiter.timer);
      waiter.resolve();
    }
    this.snapshotWaiters.clear();
  }

  private handleEvent(payload: unknown): void {
    if (!isRecord(payload)) return;
    const event = payload as unknown as ProtocolEvent;
    if (typeof event.sequence !== 'number' || typeof event.type !== 'string') return;
    if (event.sequence > this.sequence) this.sequence = event.sequence;
    this.emit({ kind: 'event', event });
  }

  private handleCommandResult(payload: unknown): void {
    if (!isRecord(payload)) return;
    const result = payload as unknown as CommandResult;
    this.emit({ kind: 'command-result', result });
    if (!isTerminalStatus(result.status)) return;
    const pending = this.pending.get(result.command_id);
    if (!pending) return;
    window.clearTimeout(pending.timer);
    this.pending.delete(result.command_id);
    pending.resolve(result);
  }

  private handleHeartbeat(payload: unknown): void {
    if (!isRecord(payload)) return;
    const heartbeat = payload as unknown as HeartbeatPayload;
    let rttMs: number | undefined;
    if (heartbeat.nonce === this.beatNonce && this.beatSentAt > 0) {
      rttMs = Date.now() - this.beatSentAt;
    }
    if (rttMs === undefined) return;
    this.emit({ kind: 'heartbeat', rttMs, serverTime: heartbeat.server_time });
  }

  private handleError(payload: unknown): void {
    if (!isRecord(payload)) {
      this.emitError('node_error', 'Node 返回错误', false);
      return;
    }
    const error = payload as unknown as ErrorPayload;
    const code = typeof error.code === 'string' ? error.code : 'node_error';
    const message = typeof error.message === 'string' ? error.message : 'Node 返回错误';
    this.rejectSnapshotWaiters(new Error(message));
    if (FATAL_ERROR_CODES[code]) {
      this.fatal(code, message);
      return;
    }
    this.emitError(code, message, false);
  }

  private fatal(code: string, message: string): void {
    this.intentionalClose = true;
    this.offlineSuspended = false;
    this.unbindNetwork();
    this.clearTimers();
    this.rejectPending(new Error(message));
    this.emitError(code, message, true);
    this.setPhase('error', message);
    const socket = this.socket;
    this.socket = null;
    if (socket) {
      socket.onclose = null;
      socket.close(4001, 'fatal');
    }
  }

  private scheduleReconnect(detail: string): void {
    if (this.intentionalClose) return;
    if (!isNetworkOnline() || this.offlineSuspended) {
      this.offlineSuspended = true;
      this.setPhase('offline', '网络离线');
      return;
    }
    this.attempt += 1;
    const delay = Math.min(1000 * 2 ** (this.attempt - 1), MAX_RECONNECT_DELAY_MS);
    this.setPhase('reconnecting', `${detail}，${Math.round(delay / 1000)} 秒后重连`);
    this.reconnectTimer = window.setTimeout(() => {
      this.reconnectTimer = null;
      this.openSocket();
    }, delay);
  }

  private startAuthTimer(): void {
    this.clearAuthTimer();
    this.authTimer = window.setTimeout(() => {
      this.authTimer = null;
      this.fatal('auth_timeout', '握手超时：Node 未返回 welcome');
    }, AUTH_TIMEOUT_MS);
  }

  private startHeartbeat(): void {
    this.stopHeartbeat();
    this.sendHeartbeat();
    this.heartbeatTimer = window.setInterval(() => this.sendHeartbeat(), HEARTBEAT_INTERVAL_MS);
  }

  private sendHeartbeat(): void {
    const socket = this.socket;
    if (!socket || socket.readyState !== WebSocket.OPEN) return;
    this.beatNonce = `hb_${Date.now().toString(36)}`;
    this.beatSentAt = Date.now();
    socket.send(JSON.stringify(heartbeatEnvelope(this.beatNonce)));
  }

  private stopHeartbeat(): void {
    if (this.heartbeatTimer !== null) {
      window.clearInterval(this.heartbeatTimer);
      this.heartbeatTimer = null;
    }
  }

  private clearAuthTimer(): void {
    if (this.authTimer !== null) {
      window.clearTimeout(this.authTimer);
      this.authTimer = null;
    }
  }

  private clearHandshakeTimers(): void {
    this.clearAuthTimer();
    this.stopHeartbeat();
  }

  private clearTimers(): void {
    this.clearHandshakeTimers();
    if (this.reconnectTimer !== null) {
      window.clearTimeout(this.reconnectTimer);
      this.reconnectTimer = null;
    }
  }

  private bindNetwork(): void {
    if (this.networkUnsubscribe) return;
    this.networkUnsubscribe = subscribeNetwork((online) => {
      if (online) {
        this.handleNetworkOnline();
        return;
      }
      this.handleNetworkOffline();
    });
  }

  private unbindNetwork(): void {
    if (!this.networkUnsubscribe) return;
    this.networkUnsubscribe();
    this.networkUnsubscribe = null;
  }

  private rejectPending(error: Error): void {
    for (const pending of this.pending.values()) {
      window.clearTimeout(pending.timer);
      pending.reject(error);
    }
    this.pending.clear();
    this.rejectSnapshotWaiters(error);
  }

  private rejectSnapshotWaiters(error: Error): void {
    for (const waiter of this.snapshotWaiters) {
      window.clearTimeout(waiter.timer);
      waiter.reject(error);
    }
    this.snapshotWaiters.clear();
  }

  private setPhase(phase: ConnectionPhase, detail?: string): void {
    this.emit({ kind: 'phase', phase, detail });
  }

  private emitError(code: string, message: string, fatal: boolean): void {
    this.emit({ kind: 'error', code, message, fatal });
  }

  private emit(event: TransportEvent): void {
    for (const listener of this.listeners) listener(event);
  }
}
