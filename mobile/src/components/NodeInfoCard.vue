<script setup lang="ts">
import { formatDate, formatRelative, formatRtt } from '@/domain/format';
import { useConnectionStore } from '@/stores/connection';
import { useNodeStore } from '@/stores/node';

const connection = useConnectionStore();
const nodeStore = useNodeStore();
</script>

<template>
  <section class="card card--pad settings-card">
    <div class="section__head">
      <h2>Node</h2>
      <span v-if="connection.mode === 'mock'" class="pill pill--warning">MOCK</span>
    </div>
    <dl class="kv small">
      <dt>名称</dt>
      <dd>{{ nodeStore.node?.name || '未连接' }}</dd>
      <dt>Node ID</dt>
      <dd class="mono">{{ nodeStore.node?.node_id || '—' }}</dd>
      <dt>主机</dt>
      <dd>{{ nodeStore.node?.hostname || '—' }}</dd>
      <dt>平台</dt>
      <dd class="mono">{{ nodeStore.node?.platform || '—' }}</dd>
      <dt>版本</dt>
      <dd>{{ nodeStore.node?.version || '—' }} · 协议 v{{ nodeStore.node?.protocol_version ?? '—' }}</dd>
      <dt>事件序号</dt>
      <dd>{{ nodeStore.lastSequence }}</dd>
      <dt>心跳 RTT</dt>
      <dd>{{ formatRtt(connection.rttMs) }}</dd>
      <dt>服务器时间</dt>
      <dd>{{ formatDate(connection.serverTime ?? undefined) }}</dd>
      <dt>连接时间</dt>
      <dd>{{ formatRelative(nodeStore.connectedAt ?? undefined) }}</dd>
      <dt>Adapter</dt>
      <dd>{{ nodeStore.adapters.map((adapter) => adapter.name).join('、') || '—' }}</dd>
    </dl>
    <div v-if="nodeStore.sequenceGap" class="banner banner--danger">
      事件序号存在缺口，请断开后重新连接以获取完整 Snapshot。
    </div>
  </section>
</template>

<style scoped>
.settings-card {
  display: flex;
  flex-direction: column;
  gap: 11px;
}
</style>
