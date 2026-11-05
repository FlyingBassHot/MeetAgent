<script setup lang="ts">
import { computed, onBeforeUnmount, ref, watch } from 'vue';
import { useRoute } from 'vue-router';
import type { PermissionDecision } from '@/domain/protocol';
import { formatClock, formatRelative } from '@/domain/format';
import { sessionStatusLabel, sessionStatusTone } from '@/domain/labels';
import { buildTimeline } from '@/domain/timeline';
import { useCommandsStore } from '@/stores/commands';
import { useConnectionStore } from '@/stores/connection';
import { useNodeStore } from '@/stores/node';
import PageHeader from '@/components/PageHeader.vue';
import StatusPill from '@/components/StatusPill.vue';
import PermissionCard from '@/components/PermissionCard.vue';
import SessionTimeline from '@/components/SessionTimeline.vue';
import PromptComposer from '@/components/PromptComposer.vue';
import EmptyState from '@/components/EmptyState.vue';

const route = useRoute();
const nodeStore = useNodeStore();
const commands = useCommandsStore();
const connection = useConnectionStore();

const sessionId = computed(() => String(route.params.sessionId ?? ''));
const session = computed(() => nodeStore.sessionById[sessionId.value]);
const agent = computed(() => nodeStore.agentFor(sessionId.value));
const pendingPermissions = computed(() => nodeStore.pendingFor(sessionId.value));
const timeline = computed(() => buildTimeline(nodeStore.sessionEvents(sessionId.value)));
const prompts = computed(() => commands.promptsFor(sessionId.value).slice(0, 5));
const draft = ref('');
const confirmingInterrupt = ref(false);
let interruptTimer: number | null = null;

const subtitle = computed(() => {
  if (!session.value) return '';
  const agentName = agent.value?.name ?? session.value.agent_id;
  return `${agentName} · ${session.value.working_directory || '未提供工作目录'}`;
});

const composerHint = computed(() =>
  connection.mode === 'mock'
    ? 'Mock 模式：回复由本地脚本流式生成，不会调用真实 Agent'
    : 'Ctrl / ⌘ + Enter 发送',
);

const canInterrupt = computed(
  () => session.value?.status === 'running' || session.value?.status === 'waiting',
);

function resetInterrupt(): void {
  if (interruptTimer !== null) {
    window.clearTimeout(interruptTimer);
    interruptTimer = null;
  }
  confirmingInterrupt.value = false;
}

function onInterrupt(): void {
  if (!confirmingInterrupt.value) {
    confirmingInterrupt.value = true;
    interruptTimer = window.setTimeout(() => {
      confirmingInterrupt.value = false;
      interruptTimer = null;
    }, 3000);
    return;
  }
  resetInterrupt();
  void commands.interrupt(sessionId.value);
}

async function sendPrompt(): Promise<void> {
  const ok = await commands.prompt(sessionId.value, draft.value);
  if (ok) draft.value = '';
}

function respond(permissionId: string, decision: PermissionDecision): void {
  void commands.respondPermission(permissionId, decision);
}

watch(canInterrupt, (active) => {
  if (!active) resetInterrupt();
});

onBeforeUnmount(resetInterrupt);
</script>

<template>
  <div class="stack">
    <RouterLink to="/sessions" class="back-link small">← Sessions</RouterLink>

    <template v-if="session">
      <PageHeader :title="session.title || session.session_id" :subtitle="subtitle">
        <template #actions>
          <StatusPill
            :label="sessionStatusLabel(session.status)"
            :tone="sessionStatusTone(session.status)"
          />
        </template>
      </PageHeader>

      <div v-if="!connection.isOnline" class="banner banner--danger">
        连接已断开，Prompt 与 Interrupt 暂不可用。
      </div>

      <section class="card card--pad session-bar">
        <div class="session-bar__meta small muted">
          <span class="mono">{{ session.session_id }}</span>
          <span>更新于 {{ formatRelative(session.updated_at) }}</span>
          <span>事件 {{ nodeStore.sessionEvents(session.session_id).length }}</span>
        </div>
        <button
          type="button"
          class="btn btn--sm"
          :class="{ 'btn--danger': confirmingInterrupt }"
          :disabled="commands.sessionBusy(session.session_id) || !connection.isOnline || !canInterrupt"
          @click="onInterrupt"
        >
          {{
            commands.isBusy(`interrupt:${session.session_id}`)
              ? '发送中…'
              : confirmingInterrupt
                ? '再次点击确认中断'
                : 'Interrupt'
          }}
        </button>
      </section>

      <section v-if="pendingPermissions.length > 0" class="section">
        <div class="section__head">
          <h2>待审批</h2>
          <span class="small muted">{{ pendingPermissions.length }} 项</span>
        </div>
        <PermissionCard
          v-for="permission in pendingPermissions"
          :key="permission.permission_id"
          :permission="permission"
          :agent-name="agent?.name"
          :session-title="session.title"
          :busy="commands.isBusy(`permission:${permission.permission_id}`)"
          @respond="respond"
        />
      </section>

      <section v-if="prompts.length > 0" class="section">
        <div class="section__head">
          <h2>本机发送记录</h2>
          <span class="small muted">仅保存在当前浏览器</span>
        </div>
        <div class="local-prompts">
          <div v-for="promptRecord in prompts" :key="promptRecord.at" class="local-prompt">
            <div class="local-prompt__meta small muted">
              <span>我</span><span>{{ formatClock(promptRecord.at) }}</span>
            </div>
            <p>{{ promptRecord.text }}</p>
          </div>
        </div>
      </section>

      <section class="section">
        <div class="section__head">
          <h2>事件时间线</h2>
          <span class="small muted">序号至 {{ nodeStore.lastSequence }}</span>
        </div>
        <SessionTimeline
          :entries="timeline"
          :agent-name="agent?.name"
          :session-title="session.title"
        />
      </section>

      <PromptComposer
        v-model="draft"
        :busy="commands.isBusy(`prompt:${session.session_id}`)"
        :disabled="!connection.isOnline"
        :hint="composerHint"
        @send="sendPrompt"
      />
    </template>

    <EmptyState
      v-else
      title="Session 不存在或已被删除"
      description="返回 Sessions 列表刷新当前 Snapshot。"
    >
      <RouterLink to="/sessions" class="btn btn--sm">返回 Sessions</RouterLink>
    </EmptyState>
  </div>
</template>

<style scoped src="./session-detail.css"></style>
