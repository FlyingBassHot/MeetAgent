/**
 * MeetAgent Cloud 传输层（纯 TypeScript，不 import 任何业务 store）。
 *
 * 职责：
 * - WebSocket 连接生命周期（连接 / 关闭 / 自动重连，指数退避 + 抖动）
 * - 信封收发与协议层校验（version / type）
 * - Device 注册（10s 注册超时，last_sequences 断线补发游标）
 * - 心跳（20s，nonce → RTT）
 * - 事件补发分页（REPLAY_PAGE_SIZE，最多 5 页；truncated 时回退到完整快照）
 * - 命令请求超时定时器（15s）
 *
 * 业务语义（快照落库、事件去重排序、pending 列表、Activity 日志等）
 * 全部通过 CloudClientHandlers 回调交给外层 store 实现，禁止在此 import store。
 */

import {
  isTerminalStatus,
  makeEnvelope,
  newMessageId,
  PROTOCOL_VERSION,
  type CommandResult,
  type Envelope,
  type ErrorPayload,
  type EventReplayResult,
  type HeartbeatPayload,
  type RegisterPayload,
  type RegisterResult,
  type Snapshot,
} from '../lib/protocol'
import { summarize, truncate } from '../lib/format'

export type ConnectionPhase = 'offline' | 'connecting' | 'registering' | 'online' | 'reconnecting'

export type LogDirection = 'in' | 'out' | 'info' | 'error'

export interface ConnectionConfig {
  url: string
  deviceId: string
  token: string
}

export interface RegisterSucceededContext {
  connectionId: string
  snapshot: Snapshot | undefined
  isResume: boolean
  resumeFrom: Record<string, number>
}

/** 外层（connection store 装配）实现的回调集合。所有方法都必须存在，缺省为 no-op。 */
export interface CloudClientHandlers {
  /** 读取最新连接配置（url / deviceId / token）。 */
  getConfig(): ConnectionConfig
  /** 读取断线补发游标（register.last_sequences 与 sync/events 默认值）。 */
  getLastSequences(): Record<string, number>
  /** 心跳日志开关（events store 的 showHeartbeats）。 */
  isHeartbeatLogEnabled(): boolean

  /** 阶段变化；reconnectAttempt 仅在 scheduleReconnect 时递增。 */
  onPhase(phase: ConnectionPhase, reconnectAttempt: number): void
  /** 心跳 RTT（ms），连接断开时传 null。 */
  onHeartbeatRtt(ms: number | null): void
  /** 连接级错误（展示在连接卡片上的 lastError）。 */
  onError(message: string): void
  /** Activity 日志。 */
  onLog(direction: LogDirection, type: string, title: string, detail?: string): void

  /** 注册成功（含可选 snapshot 与 isResume 信息）。快照应用由外层完成。 */
  onRegisterSucceeded(context: RegisterSucceededContext): void
  /** 非注册 response（命令响应）。result 解析与 pending 匹配由外层完成。 */
  onCommandResponse(envelope: Envelope): void
  /** 协议 error 信封。pending 清理与 lastError 设置由外层完成。 */
  onProtocolError(envelope: Envelope, payload: ErrorPayload | undefined): void
  /** sync/snapshot 信封。payload 未校验，外层自行校验。 */
  onSnapshot(payload: unknown): void
  /** sync/events 补发结果（已确定非 truncated）。合并/去重/日志由外层完成。 */
  onEventReplay(result: EventReplayResult): void
  /** 事件补发被 Node 截断：外层负责剔除非连续事件；client 随后自动回退快照。 */
  onEventsTruncated(nodeIds: string[]): void
  /** event 信封。payload 未校验，外层自行校验。 */
  onEvent(payload: unknown): void
  /** 在途请求 15s 超时。 */
  onPendingTimeout(requestId: string, action: string, nodeId: string): void
}

const HEARTBEAT_INTERVAL_MS = 20_000
const REQUEST_TIMEOUT_MS = 15_000
const REGISTER_TIMEOUT_MS = 10_000
const REPLAY_PAGE_SIZE = 1000
const MAX_REPLAY_PAGES = 5
const HEARTBEAT_NONCE_TTL_MS = 60_000

const noopHandlers: CloudClientHandlers = {
  getConfig: () => ({ url: '', deviceId: '', token: '' }),
  getLastSequences: () => ({}),
  isHeartbeatLogEnabled: () => false,
  onPhase: () => undefined,
  onHeartbeatRtt: () => undefined,
  onError: () => undefined,
  onLog: () => undefined,
  onRegisterSucceeded: () => undefined,
  onCommandResponse: () => undefined,
  onProtocolError: () => undefined,
  onSnapshot: () => undefined,
  onEventReplay: () => undefined,
  onEventsTruncated: () => undefined,
  onEvent: () => undefined,
  onPendingTimeout: () => undefined,
}

export class CloudClient {
  private handlers: CloudClientHandlers = noopHandlers

  private socket: WebSocket | null = null
  private phase: ConnectionPhase = 'offline'
  private shouldReconnect = false
  private reconnectAttempt = 0
  private reconnectTimer: number | undefined
  private heartbeatTimer: number | undefined
  private registerTimer: number | undefined
  private registerRequestId = ''
  private replayPages = 0
  private replayInFlight = false

  private readonly pendingTimers = new Map<string, number>()
  private readonly heartbeatSent = new Map<string, number>()

  /** 注册外层回调（connection store 在 setup 中调用一次）。 */
  setHandlers(handlers: CloudClientHandlers): void {
    this.handlers = handlers
  }

  getPhase(): ConnectionPhase {
    return this.phase
  }

  /** 是否可立即发送（socket OPEN 且已注册在线）。 */
  isOnline(): boolean {
    return this.phase === 'online' && this.socket?.readyState === WebSocket.OPEN
  }

  /** 发起连接。调用前外层应完成配置校验与业务态清理。 */
  connect(): void {
    this.shouldReconnect = true
    this.replayPages = 0
    this.replayInFlight = false
    this.clearReconnectTimer()
    this.closeSocket()
    this.openSocket(false)
  }

  /** 手动断开：停止重连、关闭 socket、清空所有请求定时器并复位在线态。 */
  disconnect(): void {
    this.shouldReconnect = false
    this.reconnectAttempt = 0
    this.clearReconnectTimer()
    this.closeSocket()
    this.discardPendingTimers()
    this.setPhase('offline')
    this.handlers.onHeartbeatRtt(null)
    this.handlers.onLog('info', 'connection', '已断开连接')
  }

  /** 发送信封；socket 未就绪时记录日志并返回 false。 */
  sendEnvelope(envelope: Envelope): boolean {
    if (!this.socket || this.socket.readyState !== WebSocket.OPEN) {
      this.handlers.onLog('error', envelope.type, '发送失败', 'WebSocket 未连接')
      return false
    }
    this.socket.send(JSON.stringify(envelope))
    return true
  }

  /** 为在途请求启动 15s 超时定时器。 */
  trackRequest(requestId: string, action: string, nodeId: string): void {
    const timer = window.setTimeout(() => {
      this.pendingTimers.delete(requestId)
      this.handlers.onPendingTimeout(requestId, action, nodeId)
    }, REQUEST_TIMEOUT_MS)
    this.pendingTimers.set(requestId, timer)
  }

  /** 取消在途请求的超时定时器（幂等）。 */
  cancelRequest(requestId: string): void {
    const timer = this.pendingTimers.get(requestId)
    if (timer !== undefined) {
      window.clearTimeout(timer)
      this.pendingTimers.delete(requestId)
    }
  }

  /** 清空全部请求定时器（不触发回调；列表由外层清空）。 */
  discardPendingTimers(): void {
    for (const timer of this.pendingTimers.values()) {
      window.clearTimeout(timer)
    }
    this.pendingTimers.clear()
  }

  /** 请求最新快照（sync/snapshot）。未连接时记录错误日志。 */
  requestSnapshot(): boolean {
    if (this.phase !== 'online') {
      this.handlers.onLog('error', 'sync', '获取快照失败', '尚未连接 Cloud')
      return false
    }
    // 旧版行为：完整快照落库后复位补发分页计数
    this.replayPages = 0
    const { deviceId } = this.handlers.getConfig()
    const envelope = makeEnvelope('sync', 'snapshot', { type: 'device', id: deviceId })
    envelope.request_id = newMessageId('req')
    if (!this.sendEnvelope(envelope)) {
      return false
    }
    this.handlers.onLog('out', 'sync · snapshot', '请求最新状态快照')
    return true
  }

  /** 请求事件补发（sync/events）。默认携带当前 lastSequences 游标。 */
  requestEventReplay(lastSequences?: Record<string, number>): void {
    if (this.phase !== 'online' || this.replayInFlight) {
      return
    }
    const sequences = lastSequences ?? this.handlers.getLastSequences()
    this.replayPages += 1
    this.replayInFlight = true
    const { deviceId } = this.handlers.getConfig()
    const envelope = makeEnvelope(
      'sync',
      'events',
      { type: 'device', id: deviceId },
      { last_sequences: { ...sequences }, limit: REPLAY_PAGE_SIZE },
    )
    envelope.request_id = newMessageId('req')
    if (!this.sendEnvelope(envelope)) {
      this.replayInFlight = false
      return
    }
    this.handlers.onLog('out', 'sync · events', '请求事件补发', summarize(sequences))
  }

  // ---------- 连接生命周期 ----------

  private openSocket(isReconnect: boolean): void {
    if (!this.shouldReconnect) {
      return
    }

    this.setPhase(isReconnect ? 'reconnecting' : 'connecting')
    const config = this.handlers.getConfig()
    this.handlers.onLog(
      'info',
      'connection',
      isReconnect ? `正在重连（第 ${this.reconnectAttempt} 次）` : '正在连接 Cloud',
      config.url,
    )

    let ws: WebSocket
    try {
      ws = new WebSocket(config.url)
    } catch (error) {
      this.handlers.onError(error instanceof Error ? error.message : String(error))
      this.scheduleReconnect()
      return
    }
    this.socket = ws

    ws.onopen = () => {
      if (this.socket !== ws) return
      this.setPhase('registering')
      this.handlers.onLog('info', 'connection', 'WebSocket 已建立，正在注册 Device')
      this.sendRegister(ws)
      this.registerTimer = window.setTimeout(() => {
        if (this.socket === ws && this.phase === 'registering') {
          this.handlers.onError('注册超时，Cloud 未返回注册结果')
          this.handlers.onLog('error', 'register', '注册超时')
          ws.close()
        }
      }, REGISTER_TIMEOUT_MS)
    }

    ws.onmessage = (event: MessageEvent) => {
      if (this.socket !== ws) return
      this.handleMessage(String(event.data))
    }

    ws.onerror = () => {
      if (this.socket !== ws) return
      this.handlers.onError('WebSocket 发生错误')
    }

    ws.onclose = (event: CloseEvent) => {
      if (this.socket !== ws) return
      this.socket = null
      this.stopHeartbeat()
      this.stopRegisterTimer()
      this.setPhase(this.shouldReconnect ? 'reconnecting' : 'offline')
      this.handlers.onHeartbeatRtt(null)
      if (this.shouldReconnect) {
        this.handlers.onLog('info', 'connection', `连接已关闭 (${event.code})，准备重连`)
        this.scheduleReconnect()
      } else {
        this.handlers.onLog('info', 'connection', `连接已关闭 (${event.code})`)
      }
    }
  }

  private closeSocket(): void {
    const current = this.socket
    this.socket = null
    this.stopHeartbeat()
    this.stopRegisterTimer()
    if (current) {
      current.close(1000, 'client closed')
    }
  }

  private scheduleReconnect(): void {
    if (!this.shouldReconnect) {
      return
    }
    this.clearReconnectTimer()
    this.reconnectAttempt += 1
    this.setPhase('reconnecting')
    const delay = this.backoffDelay(this.reconnectAttempt)
    this.handlers.onLog('info', 'connection', `将在 ${(delay / 1000).toFixed(1)}s 后重连`)
    this.reconnectTimer = window.setTimeout(() => {
      this.reconnectTimer = undefined
      this.openSocket(true)
    }, delay)
  }

  private backoffDelay(attempt: number): number {
    const base = Math.min(30_000, 1_000 * 2 ** Math.max(0, attempt - 1))
    const jitter = base * 0.2 * (Math.random() * 2 - 1)
    return Math.max(500, Math.round(base + jitter))
  }

  private clearReconnectTimer(): void {
    if (this.reconnectTimer !== undefined) {
      window.clearTimeout(this.reconnectTimer)
      this.reconnectTimer = undefined
    }
  }

  private setPhase(phase: ConnectionPhase): void {
    this.phase = phase
    this.handlers.onPhase(phase, this.reconnectAttempt)
  }

  // ---------- 注册 ----------

  private sendRegister(ws: WebSocket): void {
    this.registerRequestId = newMessageId('req')
    const { deviceId, token } = this.handlers.getConfig()
    const resumeFrom = this.handlers.getLastSequences()
    const payload: RegisterPayload = {
      token,
      name: 'Web Test Client',
      hostname: window.location.host,
      platform: 'web',
      version: '0.4.0-phase3',
    }
    if (Object.keys(resumeFrom).length > 0) {
      payload.last_sequences = { ...resumeFrom }
    }
    const envelope = makeEnvelope('register', 'register', { type: 'device', id: deviceId }, payload)
    envelope.request_id = this.registerRequestId
    ws.send(JSON.stringify(envelope))
    this.handlers.onLog('out', 'register', `注册 Device ${deviceId}`)
  }

  private handleRegisterResponse(envelope: Envelope): void {
    if (envelope.request_id && this.registerRequestId && envelope.request_id !== this.registerRequestId) {
      this.handlers.onLog('in', 'register', '忽略无关的注册响应', envelope.request_id)
      return
    }
    this.stopRegisterTimer()

    const result = envelope.payload as RegisterResult | undefined
    if (!result?.ok) {
      this.shouldReconnect = false
      this.setPhase('offline')
      this.handlers.onError(result?.error ?? '注册被拒绝')
      this.handlers.onLog(
        'error',
        'register',
        '注册被拒绝',
        `${result?.error_code ?? 'unknown'}: ${result?.error ?? 'Cloud 未返回原因'}`,
      )
      this.socket?.close()
      return
    }

    const resumeFrom = { ...this.handlers.getLastSequences() }
    const isResume = Object.keys(resumeFrom).length > 0

    this.replayPages = 0
    this.replayInFlight = false
    this.reconnectAttempt = 0
    this.setPhase('online')
    this.handlers.onRegisterSucceeded({
      connectionId: result.connection_id ?? '',
      snapshot: result.snapshot,
      isResume,
      resumeFrom,
    })
    this.startHeartbeat()
  }

  // ---------- 消息分发 ----------

  private handleMessage(raw: string): void {
    let envelope: Envelope
    try {
      envelope = JSON.parse(raw) as Envelope
    } catch {
      this.handlers.onLog('error', 'protocol', '收到无法解析的消息', truncate(raw, 500))
      return
    }

    if (!envelope || envelope.version !== PROTOCOL_VERSION || !envelope.type) {
      this.handlers.onLog('error', 'protocol', '收到不兼容的协议消息', truncate(raw, 500))
      return
    }

    switch (envelope.type) {
      case 'response':
        if (envelope.action === 'register') {
          this.handleRegisterResponse(envelope)
        } else {
          this.handleCommandResponse(envelope)
        }
        break
      case 'sync':
        this.handleSync(envelope)
        break
      case 'heartbeat':
        this.handleHeartbeat(envelope)
        break
      case 'event':
        this.handlers.onEvent(envelope.payload)
        break
      case 'error':
        this.handleProtocolError(envelope)
        break
      case 'register':
        this.handlers.onLog('in', 'register', '收到注册消息', summarize(envelope.payload))
        break
      default:
        this.handlers.onLog('in', envelope.type, '收到未处理的消息', summarize(envelope.payload))
    }
  }

  private handleSync(envelope: Envelope): void {
    switch (envelope.action) {
      case 'snapshot':
        this.handlers.onSnapshot(envelope.payload)
        break
      case 'events':
        this.handleEventReplay(envelope)
        break
      default:
        this.handlers.onLog('in', 'sync', `未知同步动作 ${envelope.action ?? ''}`, summarize(envelope.payload))
    }
  }

  private handleCommandResponse(envelope: Envelope): void {
    const result = envelope.payload as CommandResult | undefined
    const requestId = envelope.request_id
    if (result?.status && isTerminalStatus(result.status) && requestId) {
      this.cancelRequest(requestId)
    }
    this.handlers.onCommandResponse(envelope)
  }

  private handleEventReplay(envelope: Envelope): void {
    this.replayInFlight = false
    const result = envelope.payload as EventReplayResult | undefined
    if (!result || !Array.isArray(result.events)) {
      this.handlers.onLog('error', 'sync · events', '事件补发格式无效', summarize(envelope.payload))
      return
    }

    const truncated = Array.isArray(result.truncated) ? result.truncated : []
    if (truncated.length > 0) {
      this.handlers.onEventsTruncated(truncated)
      this.handlers.onLog(
        'error',
        'sync · events',
        '事件缓存已截断，回退到 Snapshot',
        `nodes=${truncated.join(', ')} · 将重新获取完整快照`,
      )
      this.requestSnapshot()
      return
    }

    this.handlers.onEventReplay(result)

    if (result.events.length >= REPLAY_PAGE_SIZE && this.replayPages < MAX_REPLAY_PAGES) {
      this.requestEventReplay()
    }
  }

  private handleHeartbeat(envelope: Envelope): void {
    const payload = envelope.payload as HeartbeatPayload | undefined
    let rtt: number | null = null
    if (payload?.nonce) {
      const sentAt = this.heartbeatSent.get(payload.nonce)
      if (sentAt !== undefined) {
        this.heartbeatSent.delete(payload.nonce)
        rtt = Math.max(1, Math.round(performance.now() - sentAt))
        this.handlers.onHeartbeatRtt(rtt)
      }
    }
    if (this.handlers.isHeartbeatLogEnabled()) {
      this.handlers.onLog('in', 'heartbeat', '心跳响应', rtt ? `${rtt}ms` : '')
    }
  }

  private handleProtocolError(envelope: Envelope): void {
    const payload = envelope.payload as ErrorPayload | undefined
    if (envelope.request_id) {
      this.cancelRequest(envelope.request_id)
    }
    this.handlers.onProtocolError(envelope, payload)
  }

  // ---------- 心跳 ----------

  private startHeartbeat(): void {
    this.stopHeartbeat()
    this.sendHeartbeat()
    this.heartbeatTimer = window.setInterval(() => this.sendHeartbeat(), HEARTBEAT_INTERVAL_MS)
  }

  private stopHeartbeat(): void {
    if (this.heartbeatTimer !== undefined) {
      window.clearInterval(this.heartbeatTimer)
      this.heartbeatTimer = undefined
    }
    this.heartbeatSent.clear()
  }

  private sendHeartbeat(): void {
    if (!this.socket || this.socket.readyState !== WebSocket.OPEN || this.phase !== 'online') {
      return
    }
    const nonce = newMessageId('hb')
    this.heartbeatSent.set(nonce, performance.now())
    const now = performance.now()
    for (const [key, sentAt] of this.heartbeatSent) {
      if (now - sentAt > HEARTBEAT_NONCE_TTL_MS) {
        this.heartbeatSent.delete(key)
      }
    }
    const { deviceId } = this.handlers.getConfig()
    const envelope = makeEnvelope(
      'heartbeat',
      '',
      { type: 'device', id: deviceId },
      { nonce },
    )
    if (this.sendEnvelope(envelope) && this.handlers.isHeartbeatLogEnabled()) {
      this.handlers.onLog('out', 'heartbeat', '心跳', nonce)
    }
  }

  private stopRegisterTimer(): void {
    if (this.registerTimer !== undefined) {
      window.clearTimeout(this.registerTimer)
      this.registerTimer = undefined
    }
  }
}

/** 全局唯一实例。业务 store 通过它发送/注册回调，不各自创建连接。 */
export const cloudClient = new CloudClient()
