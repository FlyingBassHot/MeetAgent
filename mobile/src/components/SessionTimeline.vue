<script setup lang="ts">
import type { TimelineEntry } from '@/domain/timeline';
import { formatClock } from '@/domain/format';
import EventRow from './EventRow.vue';

defineProps<{
  entries: TimelineEntry[];
  agentName?: string;
  sessionTitle?: string;
}>();
</script>

<template>
  <div v-if="entries.length === 0" class="empty">
    <strong>还没有 Session 事件</strong>
    <span class="small">发送 Prompt 后，事件会按序出现在这里。</span>
  </div>
  <div v-else class="timeline">
    <template v-for="entry in entries" :key="entry.key">
      <article
        v-if="entry.kind === 'message'"
        class="message"
        :class="{ 'message--streaming': entry.streaming }"
      >
        <div class="message__meta">
          <span class="message__who">{{ agentName || 'Agent' }}</span>
          <span class="small muted">{{ formatClock(entry.at) }}</span>
          <span v-if="entry.streaming" class="pill pill--info">生成中</span>
          <span v-else-if="entry.interrupted" class="pill pill--warning">已中断</span>
        </div>
        <p class="message__text">
          <span v-if="entry.text">{{ entry.text }}</span>
          <span v-else-if="entry.streaming" class="muted">正在生成…</span>
          <span v-else class="muted">（无文本内容）</span>
        </p>
      </article>
      <EventRow
        v-else
        :event="entry.event"
        compact
        :agent-name="agentName"
        :session-title="sessionTitle"
      />
    </template>
  </div>
</template>

<style scoped>
.timeline {
  display: flex;
  flex-direction: column;
  gap: 9px;
}

.message {
  padding: 11px 13px;
  border: 1px solid var(--border);
  border-radius: var(--radius-sm);
  background: linear-gradient(180deg, rgba(28, 36, 48, 0.75), rgba(17, 21, 27, 0.9));
}

.message__meta {
  display: flex;
  align-items: center;
  gap: 8px;
  margin-bottom: 5px;
}

.message__who {
  font-size: 12px;
  font-weight: 620;
  color: var(--accent-strong);
}

.message__text {
  font-size: 14px;
  line-height: 1.62;
  white-space: pre-wrap;
  overflow-wrap: anywhere;
}
</style>
