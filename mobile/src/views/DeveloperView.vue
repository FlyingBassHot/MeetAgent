<script setup lang="ts">
import { ref } from 'vue';
import { DEFAULT_CAPABILITIES } from '@/domain/protocol';
import { platform } from '@/services/platform';
import type { ClientMode } from '@/services/storage/preferences';
import { useConnectionStore } from '@/stores/connection';
import { usePreferencesStore } from '@/stores/preferences';
import { useUiStore } from '@/stores/ui';
import PageHeader from '@/components/PageHeader.vue';
import NodeInfoCard from '@/components/NodeInfoCard.vue';
import ConnectionDiagnostics from '@/components/ConnectionDiagnostics.vue';

const connection = useConnectionStore();
const preferences = usePreferencesStore();
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
    deviceName: deviceName.value.trim() || 'Mobile Web',
  });
  await connection.connect({
    mode: mode.value,
    baseUrl: baseUrl.value,
    token: preferences.settings.token,
  });
}

function clearToken(): void {
  preferences.setToken(null);
  ui.push('已清除本地 Token', 'success');
}

function toggleAutoConnect(): void {
  preferences.patch({ autoConnect: !preferences.settings.autoConnect });
}

function toggleRawEvents(): void {
  preferences.patch({ showRawEvents: !preferences.settings.showRawEvents });
}
</script>

<template>
  <div class="developer">
    <PageHeader title="开发者信息" subtitle="Node、连接与原始数据">
      <template #actions>
        <RouterLink to="/settings" class="developer__back">返回</RouterLink>
      </template>
    </PageHeader>

    <NodeInfoCard />
    <ConnectionDiagnostics />

    <section class="developer__group">
      <h2>连接配置</h2>
      <div class="developer__modes">
        <button
          type="button"
          class="chip"
          :class="{ 'chip--active': mode === 'mock' }"
          @click="setMode('mock')"
        >
          演示模式
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
      <label class="developer__field">
        <span>Node 地址</span>
        <input v-model="baseUrl" class="developer__input mono" />
      </label>
      <label class="developer__field">
        <span>设备名称</span>
        <input v-model="deviceName" class="developer__input" maxlength="40" />
      </label>
      <button
        type="button"
        class="btn btn--sm btn--primary"
        :disabled="connection.isBusy"
        @click="reconnect"
      >
        保存并重连
      </button>
    </section>

    <section class="developer__group">
      <h2>设备与权限</h2>
      <dl class="kv small">
        <dt>Client ID</dt>
        <dd class="mono">{{ connection.clientId || '—' }}</dd>
        <dt>Token</dt>
        <dd class="mono">{{ preferences.tokenPreview }}</dd>
        <dt>权限</dt>
        <dd>{{ connection.grantedPermissions.join('、') || '—' }}</dd>
        <dt>平台</dt>
        <dd>{{ platform.kind }}</dd>
      </dl>
      <div class="developer__chips">
        <span v-for="capability in DEFAULT_CAPABILITIES" :key="capability" class="pill mono">
          {{ capability }}
        </span>
      </div>
      <div class="row row--wrap">
        <button type="button" class="btn btn--sm" @click="toggleAutoConnect">
          自动连接：{{ preferences.settings.autoConnect ? '开' : '关' }}
        </button>
        <button
          type="button"
          class="btn btn--sm btn--danger"
          :disabled="!preferences.settings.token"
          @click="clearToken"
        >
          清除 Token
        </button>
      </div>
    </section>

    <section class="developer__group">
      <h2>原始数据</h2>
      <button
        type="button"
        class="developer__toggle"
        role="switch"
        :aria-checked="preferences.settings.showRawEvents"
        @click="toggleRawEvents"
      >
        <span>显示原始事件字段</span>
        <span class="switch" :class="{ 'switch--on': preferences.settings.showRawEvents }" />
      </button>
      <p class="small muted">
        开启后「动态」会展示序号、event_id、adapter 与原始 payload；默认关闭。
      </p>
    </section>
  </div>
</template>

<style scoped src="./developer.css"></style>
