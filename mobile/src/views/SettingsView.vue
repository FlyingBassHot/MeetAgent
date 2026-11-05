<script setup lang="ts">
import { ref } from 'vue';
import { useRouter } from 'vue-router';
import { DEFAULT_CAPABILITIES, CLIENT_VERSION } from '@/domain/protocol';
import { shortId } from '@/domain/format';
import { platform } from '@/services/platform';
import type { ClientMode } from '@/services/storage/preferences';
import { useConnectionStore } from '@/stores/connection';
import { usePreferencesStore } from '@/stores/preferences';
import { useUiStore } from '@/stores/ui';
import PageHeader from '@/components/PageHeader.vue';
import ConnectionBadge from '@/components/ConnectionBadge.vue';
import NodeInfoCard from '@/components/NodeInfoCard.vue';
import ConnectionDiagnostics from '@/components/ConnectionDiagnostics.vue';

const router = useRouter();
const preferences = usePreferencesStore();
const connection = useConnectionStore();
const ui = useUiStore();

const mode = ref<ClientMode>(preferences.settings.mode);
const baseUrl = ref(preferences.settings.baseUrl);
const deviceName = ref(preferences.settings.deviceName);

function setMode(next: ClientMode): void {
  mode.value = next;
  preferences.patch({ mode: next });
}

async function reconnect(): Promise<void> {
  preferences.patch({
    mode: mode.value,
    baseUrl: baseUrl.value,
    deviceName: deviceName.value,
  });
  if (mode.value === 'live' && !preferences.settings.token) {
    ui.push('请返回连接页完成配对，或先填入 Token', 'warning');
    return;
  }
  await connection.connect({
    mode: mode.value,
    baseUrl: baseUrl.value,
    token: preferences.settings.token,
  });
}

function saveIdentity(): void {
  preferences.patch({
    deviceName: deviceName.value.trim() || 'Mobile Web',
    baseUrl: baseUrl.value,
  });
  ui.push('设备设置已保存', 'success');
}

function leaveConnection(): void {
  connection.disconnect();
  void router.push('/connect');
}

async function toggleNotifications(): Promise<void> {
  if (preferences.settings.notifyEnabled) {
    preferences.patch({ notifyEnabled: false });
    return;
  }
  const granted = await platform.requestNotificationPermission();
  if (!granted) {
    ui.push(
      platform.notificationSupported ? '浏览器未授权通知' : '当前环境不支持通知',
      'warning',
    );
    return;
  }
  preferences.patch({ notifyEnabled: true });
  ui.push('已启用本地通知', 'success');
}

function toggleAutoConnect(): void {
  preferences.patch({ autoConnect: !preferences.settings.autoConnect });
}

function clearToken(): void {
  preferences.setToken(null);
  ui.push('已清除本地 Token', 'success');
}
</script>

<template>
  <div class="stack">
    <PageHeader title="Settings" subtitle="连接、设备与安全设置" />

    <section class="card card--pad settings-card">
      <div class="section__head">
        <h2>连接</h2>
        <ConnectionBadge />
      </div>
      <div class="settings-modes">
        <button
          type="button"
          class="chip"
          :class="{ 'chip--active': mode === 'mock' }"
          @click="setMode('mock')"
        >
          Mock 演示
        </button>
        <button
          type="button"
          class="chip"
          :class="{ 'chip--active': mode === 'live' }"
          @click="setMode('live')"
        >
          真实 Node
        </button>
      </div>
      <div v-if="mode === 'live'" class="field">
        <label for="settings-url">Node 地址</label>
        <input id="settings-url" v-model="baseUrl" class="input mono" />
      </div>
      <div class="field">
        <label for="settings-device">设备名称</label>
        <input id="settings-device" v-model="deviceName" class="input" maxlength="40" />
      </div>
      <p class="small muted">{{ connection.detail }}</p>
      <div class="row row--wrap">
        <button type="button" class="btn btn--sm" @click="saveIdentity">保存设备</button>
        <button type="button" class="btn btn--sm btn--primary" :disabled="connection.isBusy" @click="reconnect">
          保存并重连
        </button>
        <button type="button" class="btn btn--sm btn--ghost" @click="leaveConnection">
          断开并返回连接页
        </button>
      </div>
    </section>

    <NodeInfoCard />
    <ConnectionDiagnostics />

    <section class="card card--pad settings-card">
      <div class="section__head">
        <h2>设备与权限</h2>
        <span class="pill">{{ platform.kind }}</span>
      </div>
      <dl class="kv small">
        <dt>Client ID</dt>
        <dd class="mono">{{ connection.clientId || '连接后由 Node 分配' }}</dd>
        <dt>设备 Token</dt>
        <dd class="mono">{{ preferences.tokenPreview }}</dd>
        <dt>已授权权限</dt>
        <dd>{{ connection.grantedPermissions.join('、') || '—' }}</dd>
      </dl>
      <div class="settings-chips">
        <span v-for="capability in DEFAULT_CAPABILITIES" :key="capability" class="pill small mono">
          {{ capability }}
        </span>
      </div>
      <div class="row row--wrap">
        <button type="button" class="btn btn--sm btn--danger" :disabled="!preferences.settings.token" @click="clearToken">
          清除 Token
        </button>
        <button type="button" class="btn btn--sm btn--ghost" @click="toggleAutoConnect">
          自动连接：{{ preferences.settings.autoConnect ? '开' : '关' }}
        </button>
      </div>
      <p class="small muted">
        Web 原型将 Token 存在 localStorage；Capacitor 版本必须迁移到 Keychain / Keystore。
      </p>
    </section>

    <section class="card card--pad settings-card">
      <div class="section__head">
        <h2>通知</h2>
        <span class="pill" :class="preferences.settings.notifyEnabled ? 'pill--success' : ''">
          {{ preferences.settings.notifyEnabled ? '已启用' : '未启用' }}
        </span>
      </div>
      <p class="small muted">
        通知用于提醒新的审批请求；页面处于前台时仍以 Activity / Home 内联提示为主。
      </p>
      <button type="button" class="btn btn--sm" @click="toggleNotifications">
        {{ preferences.settings.notifyEnabled ? '关闭本地通知' : '启用本地通知' }}
      </button>
    </section>

    <section class="card card--pad settings-card">
      <div class="section__head">
        <h2>关于</h2>
        <span class="small muted mono">v{{ CLIENT_VERSION }}</span>
      </div>
      <p class="small muted">
        MeetAgent Mobile Web 原型（V2.1 / M5a）。页面与通信层分离，Mock、局域网 Node 与自签 WSS Node
        共用同一协议客户端；当前不包含语音、推送与原生打包。
      </p>
      <div class="settings-foot small muted">
        <span>协议 schema v1</span>
        <span>地址支持 ws / wss</span>
        <span class="mono">{{ shortId(connection.clientId ?? undefined, 8) }}</span>
      </div>
    </section>
  </div>
</template>

<style scoped src="./settings.css"></style>
