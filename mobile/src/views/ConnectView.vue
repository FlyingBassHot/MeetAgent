<script setup lang="ts">
import { computed, ref, watch } from 'vue';
import { useRoute, useRouter } from 'vue-router';
import type { Info } from '@/domain/protocol';
import { formatClock } from '@/domain/format';
import { fetchInfo } from '@/services/api/http';
import { useConnectionStore } from '@/stores/connection';
import { usePreferencesStore } from '@/stores/preferences';
import { useUiStore } from '@/stores/ui';

const route = useRoute();
const router = useRouter();
const connection = useConnectionStore();
const preferences = usePreferencesStore();
const ui = useUiStore();

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
  if (baseUrl.value.trim().length === 0) return false;
  return token.value.trim().length > 0 || pairingCode.value.trim().length > 0;
});

const statusLine = computed(() => {
  if (connection.error) return connection.error;
  if (connection.isBusy) return connection.detail;
  return '输入 Node 地址与配对码，或使用已保存的 Token 连接。';
});

async function submit(): Promise<void> {
  if (!canSubmit.value) {
    ui.push('需要 Node 地址、配对码或 Token', 'warning');
    return;
  }
  await connection.connect({
    mode: 'live',
    baseUrl: baseUrl.value,
    pairingCode: pairingCode.value,
    token: token.value.trim() ? token.value.trim() : null,
  });
}

async function connectMock(): Promise<void> {
  await connection.connect({ mode: 'mock' });
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
      <div class="connect__hero">
        <span class="connect__mark" aria-hidden="true">&gt;_</span>
        <div class="brand connect__brand">MeetAgent <span>V2.1</span></div>
      </div>
      <h1>连接</h1>
      <p class="muted">连接你的 Node，随后即可查看会话、动态并处理审批。</p>
    </section>

    <form class="connect__form" @submit.prevent="submit">
      <label class="connect__field">
        <span>Node 地址</span>
        <input v-model="baseUrl" class="connect__input" placeholder="http://192.168.1.10:8787" />
      </label>
      <label class="connect__field">
        <span>配对码</span>
        <input
          v-model="pairingCode"
          class="connect__input connect__input--code"
          inputmode="numeric"
          autocomplete="one-time-code"
          maxlength="6"
          placeholder="6 位配对码"
        />
      </label>
      <label class="connect__field">
        <span>设备名称</span>
        <input v-model="deviceName" class="connect__input" maxlength="40" />
      </label>

      <p class="connect__status" :class="{ 'connect__status--error': connection.error }">
        {{ statusLine }}
      </p>

      <button type="submit" class="connect__primary" :disabled="!canSubmit">
        {{ connection.isBusy ? '连接中…' : '连接' }}
      </button>
      <button
        v-if="connection.isBusy"
        type="button"
        class="connect__secondary"
        @click="connection.disconnect()"
      >
        取消连接
      </button>

      <div v-if="probe" class="connect__probe">
        <div>
          <strong>{{ probe.node.name }}</strong>
          <span class="pill pill--success">在线</span>
        </div>
        <span class="small muted">{{ probe.node.hostname }}</span>
      </div>
      <div v-if="probeError" class="alert">{{ probeError }}</div>

      <details class="connect__advanced">
        <summary>高级信息</summary>
        <div class="connect__advanced-body">
          <label class="connect__field">
            <span>已有 Token（可选）</span>
            <input
              v-model="token"
              class="connect__input"
              type="password"
              autocomplete="off"
              placeholder="已配对 Token"
            />
          </label>
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
          <dl v-if="probe" class="kv small">
            <dt>Node ID</dt><dd class="mono">{{ probe.node.node_id }}</dd>
            <dt>平台</dt><dd class="mono">{{ probe.node.platform || '—' }}</dd>
            <dt>版本</dt><dd>{{ probe.node.version || '—' }} · 协议 v{{ probe.protocol_version }}</dd>
            <dt>Adapter</dt>
            <dd>{{ probe.adapters?.map((adapter) => adapter.name).join('、') || '—' }}</dd>
            <dt>WSS 路径</dt><dd class="mono">{{ probe.ws_path }}</dd>
            <dt>配对</dt>
            <dd>{{ probe.pairing_required ? '需要配对码' : '已关闭配对' }} · {{ probe.pairing_code_ttl_ms / 60000 }} 分钟</dd>
            <dt>检测时间</dt><dd>{{ formatClock(probedAt) }}</dd>
          </dl>
          <p class="small muted">
            WSS 使用 TLS；自签证书需先在系统/浏览器中信任（浏览器不会暴露证书细节）。Token 暂存 localStorage。
          </p>
        </div>
      </details>
    </form>

    <button type="button" class="connect__mock" :disabled="connection.isBusy" @click="connectMock">
      演示模式（使用 Mock 数据）
    </button>
  </div>
</template>

<style scoped src="./connect.css"></style>
