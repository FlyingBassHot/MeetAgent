<script setup lang="ts">
import { computed } from 'vue';
import { formatRtt } from '@/domain/format';
import { toWebSocketUrl } from '@/services/storage/preferences';
import { useConnectionStore } from '@/stores/connection';
import { useNetworkStore } from '@/stores/network';
import { usePreferencesStore } from '@/stores/preferences';

const connection = useConnectionStore();
const network = useNetworkStore();
const preferences = usePreferencesStore();

const scheme = computed(() => {
  try {
    return toWebSocketUrl(preferences.settings.baseUrl).startsWith('wss:') ? 'wss' : 'ws';
  } catch {
    return '—';
  }
});

const onlineLabel = computed(() => (network.online ? 'true（在线）' : 'false（离线）'));
const lastError = computed(() => connection.lastError ?? '无');
const permissionsLabel = computed(() =>
  connection.grantedPermissions.length > 0 ? connection.grantedPermissions.join('、') : '—',
);
</script>

<template>
  <section class="card card--pad diagnostics">
    <div class="section__head">
      <h2>连接诊断</h2>
      <span class="pill" :class="network.online ? 'pill--success' : 'pill--danger'">
        {{ network.online ? '网络在线' : '网络离线' }}
      </span>
    </div>
    <dl class="kv small">
      <dt>navigator.onLine</dt>
      <dd class="mono">{{ onlineLabel }}</dd>
      <dt>WebSocket scheme</dt>
      <dd class="mono">{{ scheme }}</dd>
      <dt>连接阶段</dt>
      <dd>{{ connection.phase }} · {{ connection.detail }}</dd>
      <dt>最后错误</dt>
      <dd class="diagnostics__error">{{ lastError }}</dd>
      <dt>RTT</dt>
      <dd>{{ formatRtt(connection.rttMs) }}</dd>
      <dt>授权权限</dt>
      <dd class="mono">{{ permissionsLabel }}</dd>
      <dt>配置地址</dt>
      <dd class="mono">{{ preferences.settings.baseUrl }}</dd>
    </dl>
    <p v-if="scheme === 'wss'" class="small muted diagnostics__hint">
      WSS 需要有效证书；自签证书请先在系统/浏览器中信任。浏览器通常不会暴露证书细节。
    </p>
  </section>
</template>

<style scoped>
.diagnostics {
  display: flex;
  flex-direction: column;
  gap: 11px;
}

.diagnostics__error {
  color: var(--danger-fg);
  overflow-wrap: anywhere;
}

.diagnostics__hint {
  line-height: 1.5;
}
</style>
