/**
 * Connection store：Cloud 连接配置、阶段、错误、重连次数、心跳 RTT。
 * 同时负责把 cloudClient 的回调装配到 inventory / events store（快照编排），
 * 不实现业务细节——节点落库在 inventory，事件落库在 events。
 */

import { defineStore } from 'pinia'
import { computed, nextTick, ref, watch } from 'vue'
import {
  cloudClient,
  type CloudClientHandlers,
  type ConnectionPhase,
} from '../api/cloud-client'
import { summarize } from '../lib/format'
import type { ErrorPayload, Snapshot } from '../lib/protocol'
import { useEventsStore } from './events'
import { useInventoryStore } from './inventory'

export type { ConnectionPhase }

const STORAGE_KEY = 'meetagent.test-client.v1'

interface SavedSettings {
  url: string
  deviceId: string
  token: string
  autoConnect?: boolean
}

function isLocalHost(): boolean {
  try {
    const hostname = window.location.hostname
    return (
      !hostname ||
      hostname === 'localhost' ||
      hostname === '127.0.0.1' ||
      hostname === '::1' ||
      hostname === '[::1]'
    )
  } catch {
    return true
  }
}

function defaultUrl(): string {
  if (isLocalHost()) return 'ws://127.0.0.1:8080/ws'
  try {
    // 公网部署走同域反向代理（wss）。
    return `wss://${window.location.host}/ws`
  } catch {
    return 'ws://127.0.0.1:8080/ws'
  }
}

function defaultDeviceId(): string {
  return isLocalHost() ? 'web-001' : ''
}

function defaultToken(): string {
  return isLocalHost() ? 'dev-device-token-change-me' : ''
}

const FALLBACK_SETTINGS: SavedSettings = {
  url: defaultUrl(),
  deviceId: defaultDeviceId(),
  token: defaultToken(),
  autoConnect: true,
}

function readStoredSettings(): SavedSettings | null {
  try {
    const raw = window.localStorage.getItem(STORAGE_KEY)
    if (!raw) return null
    const parsed = JSON.parse(raw) as Partial<SavedSettings>
    return {
      url: typeof parsed.url === 'string' ? parsed.url : FALLBACK_SETTINGS.url,
      deviceId: typeof parsed.deviceId === 'string' ? parsed.deviceId : FALLBACK_SETTINGS.deviceId,
      token: typeof parsed.token === 'string' ? parsed.token : FALLBACK_SETTINGS.token,
      autoConnect: parsed.autoConnect !== false,
    }
  } catch {
    return null
  }
}

// HMR / 重复 setup 时避免重复注册回调
let handlersRegistered = false

export const useConnectionStore = defineStore('connection', () => {
  const stored = readStoredSettings()
  const saved = stored ?? { ...FALLBACK_SETTINGS }

  const url = ref(saved.url)
  const deviceId = ref(saved.deviceId)
  const token = ref(saved.token)
  const autoConnect = ref(saved.autoConnect !== false)
  const phase = ref<ConnectionPhase>('offline')
  const lastError = ref('')
  const reconnectAttempt = ref(0)
  const connectionId = ref('')
  const heartbeatRttMs = ref<number | null>(null)

  /** 本地是否保存过连接配置（用于连接门禁判断，不能用 fallback 代替）。 */
  const hasSavedConfig = ref(stored !== null)
  /** 本次会话是否成功连上过（成功后断线保留控制台）。 */
  const hasEverConnected = ref(false)
  /** 用户手动断开后回到门禁页，直到再次连接。 */
  const disconnectedManually = ref(false)
  /** 忘记设备时抑制 watch 回写（否则会立刻重建 localStorage）。 */
  const suppressPersist = ref(false)
  let bootstrapDone = false

  const events = useEventsStore()
  const inventory = useInventoryStore()

  const isOnline = computed(() => phase.value === 'online')
  /** 连接中 / 注册中 / 重连中（用于加载态，区别于离线空态）。 */
  const isConnecting = computed(() => phase.value !== 'online' && phase.value !== 'offline')
  /** 是否显示全屏连接门禁。 */
  const showGate = computed(() => !hasEverConnected.value || disconnectedManually.value)

  watch([url, deviceId, token, autoConnect], () => persistSettings())

  watch(phase, (next) => {
    if (next === 'online') {
      hasEverConnected.value = true
      disconnectedManually.value = false
    }
  })

  function persistSettings(): void {
    if (suppressPersist.value) return
    try {
      window.localStorage.setItem(
        STORAGE_KEY,
        JSON.stringify({
          url: url.value,
          deviceId: deviceId.value,
          token: token.value,
          autoConnect: autoConnect.value,
        }),
      )
    } catch {
      // localStorage 不可用时仅保留会话内生效
    }
  }

  function setError(message: string): void {
    lastError.value = message
  }

  /** 快照应用的统一入口：nodes → inventory，事件/游标/权限 → events。 */
  function applySnapshot(snapshot: Snapshot): void {
    inventory.applyNodes(Array.isArray(snapshot.nodes) ? snapshot.nodes : [])
    events.applySnapshotData(snapshot)
  }

  function logSnapshotApplied(snapshot: Snapshot): void {
    events.addLog(
      'in',
      'sync · snapshot',
      `状态已同步 · ${snapshot.nodes.length} 个 Node`,
      [
        summarize(snapshot.nodes.map((node) => `${node.node_id}:${node.status}`)),
        `events=${snapshot.events?.length ?? 0}`,
        `pending_permissions=${snapshot.pending_permissions?.length ?? 0}`,
      ].join('\n'),
    )
  }

  // ---------- cloudClient 回调装配 ----------

  const handlers: CloudClientHandlers = {
    getConfig: () => ({
      url: url.value.trim(),
      deviceId: deviceId.value.trim(),
      token: token.value.trim(),
    }),
    getLastSequences: () => ({ ...events.lastSequences }),
    isHeartbeatLogEnabled: () => events.showHeartbeats,

    onPhase: (next, attempt) => {
      phase.value = next
      reconnectAttempt.value = attempt
    },
    onHeartbeatRtt: (ms) => {
      heartbeatRttMs.value = ms
    },
    onError: (message) => {
      lastError.value = message
    },
    onLog: (direction, type, title, detail) => {
      events.addLog(direction, type, title, detail)
    },

    onRegisterSucceeded: ({ connectionId: id, snapshot, isResume, resumeFrom }) => {
      connectionId.value = id
      lastError.value = ''
      if (snapshot) {
        applySnapshot(snapshot)
      } else {
        inventory.applyNodes([])
      }
      events.addLog(
        'in',
        'register',
        'Device 注册成功',
        [
          `connection_id=${id || 'unknown'}`,
          `nodes=${inventory.nodes.length}`,
          `events=${events.events.length}`,
          `pending_permissions=${events.pendingPermissions.length}`,
        ].join(' · '),
      )
      if (isResume) {
        events.addLog('info', 'sync · events', '重连成功，补发断线期间事件', summarize(resumeFrom))
        cloudClient.requestEventReplay(resumeFrom)
      }
    },
    onCommandResponse: (envelope) => {
      events.handleCommandResponse(envelope)
    },
    onProtocolError: (envelope, payload: ErrorPayload | undefined) => {
      if (envelope.request_id) {
        events.removePending(envelope.request_id)
      }
      lastError.value = payload?.message ?? 'Cloud 返回错误'
      events.addLog(
        'error',
        'error',
        payload?.code ?? 'protocol_error',
        [payload?.message, payload?.detail].filter(Boolean).join('\n'),
      )
    },
    onSnapshot: (payload) => {
      const snapshot = payload as Snapshot | undefined
      if (!snapshot || !Array.isArray(snapshot.nodes)) {
        events.addLog('error', 'sync', '快照格式无效', summarize(payload))
        return
      }
      applySnapshot(snapshot)
      logSnapshotApplied(snapshot)
    },
    onEventReplay: (result) => {
      events.handleEventReplay(result)
    },
    onEventsTruncated: (nodeIds) => {
      events.dropEventsOfNodes(nodeIds)
    },
    onEvent: (payload) => {
      events.handleLiveEvent(payload)
    },
    onPendingTimeout: (requestId, action, nodeId) => {
      events.handlePendingTimeout(requestId, action, nodeId)
    },
  }

  if (!handlersRegistered) {
    cloudClient.setHandlers(handlers)
    handlersRegistered = true
  }

  // ---------- 连接动作 ----------

  function connect(): void {
    const trimmedUrl = url.value.trim()
    if (!trimmedUrl.startsWith('ws://') && !trimmedUrl.startsWith('wss://')) {
      setError('Cloud 地址必须以 ws:// 或 wss:// 开头')
      events.addLog('error', 'connection', '连接参数错误', lastError.value)
      return
    }
    if (!deviceId.value.trim() || !token.value.trim()) {
      setError('Device ID 和 Token 不能为空')
      events.addLog('error', 'connection', '连接参数错误', lastError.value)
      return
    }

    hasSavedConfig.value = true
    disconnectedManually.value = false
    persistSettings()
    lastError.value = ''
    events.resetForConnect()
    cloudClient.connect()
  }

  /** 首次挂载：有保存配置且开启自动连接时自动握手。 */
  function bootstrap(): void {
    if (bootstrapDone) return
    bootstrapDone = true
    if (!hasSavedConfig.value || !autoConnect.value) return
    connect()
  }

  function disconnect(): void {
    cloudClient.disconnect()
    events.clearForDisconnect('连接已手动断开')
    disconnectedManually.value = true
  }

  /** 立即重连：复用配置重新握手，不清空本地清单（由快照覆盖）。 */
  function retryNow(): void {
    lastError.value = ''
    disconnectedManually.value = false
    cloudClient.connect()
  }

  function setAutoConnect(next: boolean): void {
    autoConnect.value = next
    persistSettings()
  }

  /** 忘记此设备：清除连接配置与选中态，回到门禁并填入默认值。 */
  function forgetDevice(): void {
    suppressPersist.value = true
    try {
      window.localStorage.removeItem(STORAGE_KEY)
    } catch {
      // 忽略
    }
    cloudClient.disconnect()
    events.clearForDisconnect('已清除本地连接配置')
    inventory.clearSelection()
    url.value = FALLBACK_SETTINGS.url
    deviceId.value = FALLBACK_SETTINGS.deviceId
    token.value = FALLBACK_SETTINGS.token
    autoConnect.value = true
    hasSavedConfig.value = false
    hasEverConnected.value = false
    disconnectedManually.value = true
    lastError.value = ''
    connectionId.value = ''
    heartbeatRttMs.value = null
    void nextTick(() => {
      suppressPersist.value = false
      try {
        window.localStorage.removeItem(STORAGE_KEY)
      } catch {
        // 忽略
      }
    })
  }

  return {
    url,
    deviceId,
    token,
    autoConnect,
    phase,
    lastError,
    reconnectAttempt,
    connectionId,
    heartbeatRttMs,
    hasSavedConfig,
    hasEverConnected,
    disconnectedManually,
    isOnline,
    isConnecting,
    showGate,
    connect,
    bootstrap,
    disconnect,
    retryNow,
    setAutoConnect,
    forgetDevice,
  }
})