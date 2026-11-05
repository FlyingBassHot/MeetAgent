<script setup lang="ts">
import { useConnectionStore } from '@/stores/connection';

const connection = useConnectionStore();

async function takeover(): Promise<void> {
  await connection.takeover();
}
</script>

<template>
  <div v-if="connection.tabBlocked" class="guard" role="dialog" aria-modal="true">
    <div class="card guard__panel">
      <span class="pill pill--warning">单连接保护</span>
      <h1>已在另一个标签页连接</h1>
      <p class="muted">
        同一浏览器只允许一个标签页连接 Node，避免两个 WebSocket 互相替换造成重连风暴。
      </p>
      <p class="small muted guard__reason">{{ connection.blockedReason }}</p>
      <button
        type="button"
        class="btn btn--primary btn--block"
        :disabled="connection.takeoverPending || connection.isBusy"
        @click="takeover"
      >
        {{ connection.takeoverPending ? '正在接管…' : '在此标签页接管' }}
      </button>
      <p class="small muted guard__hint">
        接管会让另一个标签页断开并释放连接，然后本页使用已保存的 Token 重新连接。
      </p>
    </div>
  </div>
</template>

<style scoped>
.guard {
  position: fixed;
  inset: 0;
  z-index: 100;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 20px;
  background: rgba(5, 7, 10, 0.78);
  backdrop-filter: blur(8px);
}

.guard__panel {
  width: 100%;
  max-width: 380px;
  padding: 20px;
  display: flex;
  flex-direction: column;
  gap: 10px;
}

.guard__panel h1 {
  font-size: 21px;
  font-weight: 680;
  letter-spacing: -0.02em;
}

.guard__panel p {
  line-height: 1.55;
}

.guard__reason {
  padding: 8px 10px;
  border-radius: 8px;
  background: var(--surface-2);
  color: var(--muted);
}

.guard__hint {
  line-height: 1.45;
}
</style>
