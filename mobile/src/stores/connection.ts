import { defineStore } from 'pinia';
import { computed, ref } from 'vue';
import type {
  CommandResult,
  ConnectionPhase,
  PermissionDecision,
} from '@/domain/protocol';
import {
  CLIENT_TYPE,
  CLIENT_VERSION,
  DEFAULT_CAPABILITIES,
} from '@/domain/protocol';
import { createClient } from '@/services/client';
import type { ClientTransport, CommandRequest, TransportEvent } from '@/services/client/types';
import { ApiHttpError, pairClient } from '@/services/api/http';
import { tabGuard } from '@/services/connectivity/tabGuard';
import { platform } from '@/services/platform';
import {
  normalizeBaseUrl,
  type ClientMode,
} from '@/services/storage/preferences';
import { useNodeStore } from './node';
import { usePreferencesStore } from './preferences';
import { useUiStore } from './ui';

export interface ConnectOptions {
  mode?: ClientMode;
  baseUrl?: string;
  pairingCode?: string;
  token?: string | null;
}

export const useConnectionStore = defineStore('connection', () => {
  const preferences = usePreferencesStore();
  const nodeStore = useNodeStore();
  const ui = useUiStore();

  const phase = ref<ConnectionPhase>('idle');
  const mode = ref<ClientMode>(preferences.settings.mode);
  const error = ref<string | null>(null);
  const lastError = ref<string | null>(null);
  const detail = ref('尚未连接');
  const rttMs = ref<number | null>(null);
  const serverTime = ref<string | null>(null);
  const clientId = ref<string | null>(null);
  const grantedPermissions = ref<string[]>([]);
  const lastConnectedAt = ref<string | null>(null);
  const tabBlocked = ref(false);
  const blockedReason = ref('');
  const takeoverPending = ref(false);

  let transport: ClientTransport | null = null;
  let unsubscribe: (() => void) | null = null;
  let bootstrapped = false;
  const connectionToastIds = new Set<number>();

  tabGuard.init();
  tabGuard.onTakeover(handleTakeoverRequest);

  function clearConnectionErrors(): void {
    for (const id of connectionToastIds) ui.dismiss(id);
    connectionToastIds.clear();
  }

  const isOnline = computed(() => phase.value === 'online');
  const isBusy = computed(
    () =>
      phase.value === 'connecting' ||
      phase.value === 'authenticating' ||
      phase.value === 'reconnecting',
  );

  function attach(next: ClientTransport): void {
    if (unsubscribe) unsubscribe();
    if (transport) transport.disconnect();
    transport = next;
    unsubscribe = next.subscribe(handleTransportEvent);
  }

  function stopTransport(): void {
    if (unsubscribe) {
      unsubscribe();
      unsubscribe = null;
    }
    if (transport) {
      transport.disconnect();
      transport = null;
    }
  }

  function markTabBlocked(reason: string): void {
    tabBlocked.value = true;
    blockedReason.value = reason;
    phase.value = 'offline';
    detail.value = reason;
    error.value = null;
  }

  function handleTakeoverRequest(): void {
    if (!tabGuard.isHolding()) return;
    stopTransport();
    clearConnectionErrors();
    tabGuard.release();
    markTabBlocked('连接已由另一个标签页接管');
  }

  function handleTransportEvent(event: TransportEvent): void {
    switch (event.kind) {
      case 'phase':
        phase.value = event.phase;
        if (event.detail) detail.value = event.detail;
        if (event.phase === 'online') {
          error.value = null;
          lastConnectedAt.value = new Date().toISOString();
          clearConnectionErrors();
        }
        if (event.phase === 'error' && event.detail) {
          error.value = event.detail;
          lastError.value = event.detail;
        }
        if (event.phase === 'reconnecting' && event.detail) {
          lastError.value = event.detail;
        }
        break;
      case 'welcome':
        clientId.value = event.welcome.client_id;
        grantedPermissions.value = event.welcome.permissions ?? [];
        nodeStore.applyWelcome(event.welcome);
        transport?.setLastSequence(nodeStore.lastSequence);
        if (event.welcome.token) preferences.setToken(event.welcome.token);
        preferences.patch({ autoConnect: true });
        serverTime.value = event.welcome.server_time;
        break;
      case 'snapshot':
        nodeStore.applySnapshot(event.snapshot);
        transport?.setLastSequence(nodeStore.lastSequence);
        break;
      case 'event': {
        const before = nodeStore.pendingPermissions.length;
        nodeStore.applyEvent(event.event);
        transport?.setLastSequence(nodeStore.lastSequence);
        if (
          event.event.type === 'permission.requested' &&
          nodeStore.pendingPermissions.length > before &&
          preferences.settings.notifyEnabled &&
          document.visibilityState !== 'visible'
        ) {
          platform.notify('MeetAgent 审批请求', '有新的审批需要处理');
        }
        break;
      }
      case 'command-result':
        serverTime.value = event.result.completed_at;
        break;
      case 'heartbeat':
        rttMs.value = event.rttMs;
        if (event.serverTime) serverTime.value = event.serverTime;
        break;
      case 'error':
        error.value = event.message;
        lastError.value = event.message;
        if (event.fatal) {
          phase.value = 'error';
          tabGuard.release();
        }
        clearConnectionErrors();
        connectionToastIds.add(ui.push(event.message, 'danger'));
        break;
      default:
        break;
    }
  }

  async function connect(options: ConnectOptions = {}, guardHeld = false): Promise<void> {
    const targetMode = options.mode ?? preferences.settings.mode;
    const baseUrl = normalizeBaseUrl(options.baseUrl ?? preferences.settings.baseUrl);
    const pairingCode = (options.pairingCode ?? '').trim();
    let token = options.token !== undefined ? options.token : preferences.settings.token;

    if (targetMode === 'live' && !token && pairingCode.length === 0) {
      phase.value = 'error';
      error.value = '需要配对码或 Token 才能连接真实 Node';
      lastError.value = error.value;
      ui.push(error.value, 'danger');
      return;
    }

    if (targetMode === 'live') {
      if (!guardHeld) {
        const acquired = await tabGuard.acquire();
        if (!acquired) {
          markTabBlocked('已在另一个标签页连接 Node');
          return;
        }
      }
      tabBlocked.value = false;
      blockedReason.value = '';
    } else {
      tabGuard.release();
      tabBlocked.value = false;
      blockedReason.value = '';
    }

    stopTransport();

    mode.value = targetMode;
    phase.value = 'connecting';
    error.value = null;
    detail.value = targetMode === 'mock' ? '正在启动本地 Mock Node' : '正在连接 Node';
    rttMs.value = null;
    clientId.value = null;
    grantedPermissions.value = [];
    preferences.patch({ mode: targetMode, baseUrl, token });
    nodeStore.reset();

    if (targetMode === 'live' && !token && pairingCode) {
      try {
        const paired = await pairClient(baseUrl, {
          client_type: CLIENT_TYPE,
          client_version: CLIENT_VERSION,
          name: preferences.settings.deviceName,
          capabilities: DEFAULT_CAPABILITIES,
          pairing_code: pairingCode,
        });
        token = paired.token;
        grantedPermissions.value = paired.permissions;
        preferences.patch({ token, autoConnect: true });
      } catch (err) {
        const status = err instanceof ApiHttpError ? err.status : -1;
        if (status === 404 || status === 405 || status === 501) {
          detail.value = 'HTTP 配对不可用，改用 WebSocket 配对';
          ui.push('HTTP 配对不可用，改用 WebSocket 配对', 'warning');
        } else {
          const message = err instanceof Error ? err.message : '配对失败';
          tabGuard.release();
          tabBlocked.value = false;
          phase.value = 'error';
          error.value = message;
          lastError.value = message;
          ui.push(message, 'danger');
          return;
        }
      }
    }

    const next = createClient(targetMode);
    attach(next);
    next.setLastSequence(0);
    next.connect({
      mode: targetMode,
      baseUrl,
      token: token ?? null,
      pairingCode,
      deviceName: preferences.settings.deviceName,
      capabilities: DEFAULT_CAPABILITIES,
      lastSequence: 0,
    });
  }

  function disconnect(): void {
    stopTransport();
    tabGuard.release();
    tabBlocked.value = false;
    blockedReason.value = '';
    clearConnectionErrors();
    phase.value = 'offline';
    detail.value = '已断开连接';
    preferences.patch({ autoConnect: false });
  }

  async function takeover(): Promise<void> {
    if (takeoverPending.value) return;
    takeoverPending.value = true;
    try {
      tabGuard.requestTakeover();
      const acquired = await tabGuard.acquireWithRetry(6000);
      if (!acquired) {
        blockedReason.value = '接管失败：另一个标签页未释放连接';
        ui.push(blockedReason.value, 'danger');
        return;
      }
      tabBlocked.value = false;
      blockedReason.value = '';
      await connect(
        {
          mode: preferences.settings.mode,
          baseUrl: preferences.settings.baseUrl,
          token: preferences.settings.token,
        },
        true,
      );
    } finally {
      takeoverPending.value = false;
    }
  }

  function bootstrap(): void {
    if (bootstrapped) return;
    bootstrapped = true;
    if (!preferences.settings.autoConnect) return;
    if (preferences.settings.mode === 'mock' || preferences.settings.token) {
      void connect();
    }
  }

  async function sendCommand(request: CommandRequest): Promise<CommandResult> {
    if (!transport || phase.value !== 'online') throw new Error('尚未连接 Node');
    const result = await transport.send(request);
    if (result.completed_at) serverTime.value = result.completed_at;
    return result;
  }

  async function refreshSnapshot(): Promise<boolean> {
    if (!transport || phase.value !== 'online') {
      ui.push('当前未连接，无法刷新', 'warning');
      return false;
    }
    try {
      await transport.refreshSnapshot();
      return true;
    } catch (err) {
      const message = err instanceof Error ? err.message : '刷新快照失败';
      ui.push(message, 'danger');
      return false;
    }
  }

  function respondPermission(permissionId: string, decision: PermissionDecision): Promise<CommandResult> {
    return sendCommand({
      type: 'permission.respond',
      payload: { permission_id: permissionId, decision },
    });
  }

  return {
    phase,
    mode,
    error,
    lastError,
    detail,
    rttMs,
    serverTime,
    clientId,
    grantedPermissions,
    lastConnectedAt,
    tabBlocked,
    blockedReason,
    takeoverPending,
    isOnline,
    isBusy,
    connect,
    disconnect,
    takeover,
    bootstrap,
    sendCommand,
    refreshSnapshot,
    respondPermission,
  };
});
