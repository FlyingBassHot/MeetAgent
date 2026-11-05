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
  const detail = ref('尚未连接');
  const rttMs = ref<number | null>(null);
  const serverTime = ref<string | null>(null);
  const clientId = ref<string | null>(null);
  const grantedPermissions = ref<string[]>([]);
  const lastConnectedAt = ref<string | null>(null);

  let transport: ClientTransport | null = null;
  let unsubscribe: (() => void) | null = null;
  let bootstrapped = false;

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

  function handleTransportEvent(event: TransportEvent): void {
    switch (event.kind) {
      case 'phase':
        phase.value = event.phase;
        if (event.detail) detail.value = event.detail;
        if (event.phase === 'online') {
          error.value = null;
          lastConnectedAt.value = new Date().toISOString();
        }
        if (event.phase === 'error' && event.detail) error.value = event.detail;
        break;
      case 'welcome':
        clientId.value = event.welcome.client_id;
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
        if (event.fatal) phase.value = 'error';
        ui.push(event.message, 'danger');
        break;
      default:
        break;
    }
  }

  async function connect(options: ConnectOptions = {}): Promise<void> {
    const targetMode = options.mode ?? preferences.settings.mode;
    const baseUrl = normalizeBaseUrl(options.baseUrl ?? preferences.settings.baseUrl);
    const pairingCode = (options.pairingCode ?? '').trim();
    let token = options.token !== undefined ? options.token : preferences.settings.token;

    if (targetMode === 'live' && !token && pairingCode.length === 0) {
      phase.value = 'error';
      error.value = '需要配对码或 Token 才能连接真实 Node';
      ui.push(error.value, 'danger');
      return;
    }

    if (unsubscribe) unsubscribe();
    unsubscribe = null;
    if (transport) {
      transport.disconnect();
      transport = null;
    }

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
          phase.value = 'error';
          error.value = message;
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
    if (unsubscribe) {
      unsubscribe();
      unsubscribe = null;
    }
    if (transport) {
      transport.disconnect();
      transport = null;
    }
    phase.value = 'offline';
    detail.value = '已断开连接';
    preferences.patch({ autoConnect: false });
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
    detail,
    rttMs,
    serverTime,
    clientId,
    grantedPermissions,
    lastConnectedAt,
    isOnline,
    isBusy,
    connect,
    disconnect,
    bootstrap,
    sendCommand,
    respondPermission,
  };
});
