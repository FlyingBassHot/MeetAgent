<script setup lang="ts">
import { computed } from 'vue';
import type { TimelineEntry } from '@/domain/timeline';
import { formatClock } from '@/domain/format';
import EventRow from './EventRow.vue';

interface LocalPrompt {
  text: string;
  at: string;
}

type ChatItem =
  | { kind: 'entry'; key: string; at: string; entry: TimelineEntry }
  | { kind: 'prompt'; key: string; at: string; text: string };

const props = withDefaults(
  defineProps<{
    entries: TimelineEntry[];
    prompts?: LocalPrompt[];
    agentName?: string;
    sessionTitle?: string;
  }>(),
  { prompts: () => [], agentName: '', sessionTitle: '' },
);

const items = computed<ChatItem[]>(() => {
  const merged: ChatItem[] = props.entries.map((entry) => ({
    kind: 'entry',
    key: entry.key,
    at: entry.kind === 'message' ? entry.at : entry.event.occurred_at,
    entry,
  }));
  props.prompts.forEach((prompt, index) => {
    merged.push({
      kind: 'prompt',
      key: `prompt:${prompt.at}:${index}`,
      at: prompt.at,
      text: prompt.text,
    });
  });
  return merged.sort((left, right) => Date.parse(left.at) - Date.parse(right.at));
});
</script>

<template>
  <div v-if="items.length === 0" class="empty">
    <strong>还没有会话内容</strong>
    <span class="small">发送消息后，消息和工具调用会出现在这里。</span>
  </div>
  <div v-else class="timeline">
    <template v-for="item in items" :key="item.key">
      <article v-if="item.kind === 'prompt'" class="prompt-bubble">
        <p>{{ item.text }}</p>
        <span class="small muted">我 · {{ formatClock(item.at) }}</span>
      </article>
      <article
        v-else-if="item.entry.kind === 'message'"
        class="message"
        :class="{ 'message--streaming': item.entry.streaming }"
      >
        <div class="message__meta">
          <span class="message__who">{{ agentName || 'Agent' }}</span>
          <span class="small muted">{{ formatClock(item.entry.at) }}</span>
          <span v-if="item.entry.streaming" class="pill pill--info">生成中</span>
          <span v-else-if="item.entry.interrupted" class="pill pill--warning">已中断</span>
        </div>
        <p class="message__text">
          <span v-if="item.entry.text">{{ item.entry.text }}</span>
          <span v-else-if="item.entry.streaming" class="muted">正在生成…</span>
          <span v-else class="muted">（无文本内容）</span>
        </p>
      </article>
      <EventRow
        v-else
        :event="item.entry.event"
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

.prompt-bubble {
  align-self: flex-end;
  max-width: 86%;
  padding: 10px 13px;
  border-radius: 16px 16px 4px 16px;
  background: var(--accent-soft);
  color: var(--fg);
}

.prompt-bubble p {
  font-size: 14px;
  line-height: 1.55;
  white-space: pre-wrap;
  overflow-wrap: anywhere;
}

.prompt-bubble span {
  display: block;
  margin-top: 3px;
  text-align: right;
  font-size: 10px;
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
