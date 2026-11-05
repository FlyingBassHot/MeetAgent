<script setup lang="ts">
import { computed, ref, watch } from 'vue';
import { useRoute, useRouter } from 'vue-router';
import type { Info } from '@/domain/protocol';
import { formatClock } from '@/domain/format';
import { fetchInfo } from '@/services/api/http';
import type { ClientMode } from '@/services/storage/preferences';
import { useConnectionStore } from '@/stores/connection';
import { usePreferencesStore } from '@/stores/preferences';
import { useUiStore } from '@/stores/ui';

const route = useRoute();
const router = useRouter();
const connection = useConnectionStore();
const preferences = usePreferencesStore();
const ui = useUiStore();

const mode = ref<ClientMode>(preferences.settings.mode);
const baseUrl = ref(preferences.settings.baseUrl);
const pairingCode = ref('');
const token = ref(preferences.settings.token ?? '');
const deviceName = ref(preferences.settings.deviceName);
const probe = ref<Info | null>(null);
const probeError = ref('');
const probedAt = ref('');
const probing = ref(false);

const canSubmit = computed(() => {
  if (connection.isBusy) return false;
  if (deviceName.value.trim().length === 0) return false;
  if (mode.value === 'mock') return true;
  if (baseUrl.value.trim().length === 0) return false;
  return token.value.trim().length > 0 || pairingCode.value.trim().length > 0;
});

const statusLine = computed(() => {
  if (connection.error) return connection.error;
  if (connection.isBusy) return connection.detail;
  return connection.isOnline ? connection.detail : '选择 Mock 演示，或填写真实 Node 地址与配对码';
});

async function submit(): Promise<void> {
  if (!canSubmit.value) {
    ui.push(
      mode.value === 'live' ? '真实 Node 需要配对码或 Token' : '设备名称不能为空',
      'warning',
    );
    return;
  }
  await connection.connect({
    mode: mode.value,
    baseUrl: baseUrl.value,
    pairingCode: pairingCode.value,
    token: token.value.trim() ? token.value.trim() : null,
  });
}

async function runProbe(): Promise<void> {
  if (baseUrl.value.trim().length === 0) {
    probeError.value = '请先填写 Node 地址';
    return;
  }
  probing.value = true;
  probeError.value = '';
  probe.value = null;
  try {
    probe.value = await fetchInfo(baseUrl.value);
    probedAt.value = new Date().toISOString();
  } catch (err) {
    probeError.value = err instanceof Error ? err.message : '检测失败';
  } finally {
    probing.value = false;
  }
}

function clearToken(): void {
  token.value = '';
  preferences.setToken(null);
  ui.push('已清除本地 Token', 'success');
}

watch(
  () => connection.isOnline,
  (online) => {
    if (!online) return;
    const next = typeof route.query.next === 'string' ? route.query.next : '/home';
    void router.replace(next.startsWith('/') ? next : '/home');
  },
  { immediate: true },
);
</script>

<template>
  <div class="connect">
    <section class="connect__intro">
      <div class="brand connect__brand">MeetAgent <span>V2.1</span></div>
      <h1>连接你的 Node</h1>
      <p class="muted">
        Mobile 是完整的远程交互入口。先在局域网配对 Node，再查看 Agent、Session 与审批。
      </p>
      <div class="connect__modes">
        <button
          type="button"
          class="connect__mode"
          :class="{ 'connect__mode--active': mode === 'mock' }"
          @click="mode = 'mock'"
        >
          <strong>Mock 演示</strong>
          <span>本地脚本数据，无需 Node</span>
        </button>
        <button
          type="button"
          class="connect__mode"
          :class="{ 'connect__mode--active': mode === 'live' }"
          @click="mode = 'live'"
        >
          <strong>真实 Node</strong>
          <span>HTTP / WSS，局域网或 VPN</span>
        </button>
      </div>
    </section>

    <form class="card card--pad connect__form stack" @submit.prevent="submit">
      <div v-if="mode === 'mock'" class="banner banner--info">
        Mock 模式由本地脚本生成 Node、Agent、Session、事件与审批；页面会持续标注 MOCK，避免误认为真实
        Agent 进度。
      </div>

      <template v-else>
        <div class="field">
          <label for="node-url">Node 地址</label>
          <input id="node-url" v-model="baseUrl" class="input mono" placeholder="http://192.168.1.10:8787" />
        </div>
        <div class="connect__pair">
          <div class="field">
            <label for="pair-code">配对码</label>
            <input
              id="pair-code"
              v-model="pairingCode"
              class="input mono"
              inputmode="numeric"
              autocomplete="one-time-code"
              maxlength="6"
              placeholder="6 位配对码"
            />
          </div>
          <div class="field">
            <label for="token">已有 Token（可选）</label>
            <input
              id="token"
              v-model="token"
              class="input mono"
              type="password"
              autocomplete="off"
              placeholder="已配对 Token"
            />
          </div>
        </div>
        <div class="row row--wrap">
          <button type="button" class="btn btn--sm" :disabled="probing" @click="runProbe">
            {{ probing ? '检测中…' : '检测 Node' }}
          </button>
          <button
            v-if="preferences.settings.token"
            type="button"
            class="btn btn--sm btn--ghost"
            @click="clearToken"
          >
            清除已保存 Token
          </button>
        </div>
      </template>

      <div class="field">
        <label for="device-name">设备名称</label>
        <input id="device-name" v-model="deviceName" class="input" maxlength="40" />
      </div>

      <div class="connect__status small" :class="{ 'connect__status--error': connection.error }">{{ statusLine }}</div>

      <div class="stack stack--sm">
        <button type="submit" class="btn btn--primary btn--block" :disabled="!canSubmit">
          {{ connection.isBusy ? '连接中…' : mode === 'mock' ? '使用 Mock 数据进入' : '连接并配对' }}
        </button>
        <button
          v-if="connection.isBusy"
          type="button"
          class="btn btn--ghost btn--block"
          @click="connection.disconnect()"
        >
          取消连接
        </button>
      </div>

      <div v-if="probeError" class="alert">{{ probeError }}</div>
      <div v-if="probe" class="connect__probe">
        <div class="section__head">
          <h2>Node 信息</h2>
          <span class="pill pill--success">在线</span>
        </div>
        <dl class="kv small">
          <dt>名称</dt><dd>{{ probe.node.name }}</dd>
          <dt>Node ID</dt><dd class="mono">{{ probe.node.node_id }}</dd>
          <dt>平台</dt><dd class="mono">{{ probe.node.platform || '—' }}</dd>
          <dt>版本</dt><dd>{{ probe.node.version || '—' }} · 协议 v{{ probe.protocol_version }}</dd>
          <dt>WSS 路径</dt><dd class="mono">{{ probe.ws_path }}</dd>
          <dt>配对</dt>
          <dd>{{ probe.pairing_required ? '需要配对码' : '已关闭配对' }} · {{ probe.pairing_code_ttl_ms / 60000 }} 分钟有效</dd>
          <dt>Adapter</dt><dd>{{ probe.adapters?.map((adapter) => adapter.name).join('、') || '—' }}</dd>
          <dt>检测时间</dt><dd>{{ formatClock(probedAt) }}</dd>
        </dl>
      </div>
      <p v-if="mode === 'live'" class="small muted connect__note">
        Token 由 Node 配对返回，Web 原型暂存 localStorage；Capacitor 版本将迁移到系统安全存储。
      </p>
    </form>
  </div>
</template>

<style scoped src="./connect.css"></style>
