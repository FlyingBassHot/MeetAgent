<script setup lang="ts">
import { computed, ref } from 'vue';
import { useRoute, useRouter } from 'vue-router';
import type { PermissionDecision } from '@/domain/protocol';
import type { ActionSheetAction } from '@/domain/action-sheet';
import { sessionStatusLabel, sessionStatusTone } from '@/domain/labels';
import { buildTimeline } from '@/domain/timeline';
import { useCommandsStore } from '@/stores/commands';
import { useConnectionStore } from '@/stores/connection';
import { useNodeStore } from '@/stores/node';
import { usePreferencesStore } from '@/stores/preferences';
import { useUiStore } from '@/stores/ui';
import StatusPill from '@/components/StatusPill.vue';
import PermissionCard from '@/components/PermissionCard.vue';
import SessionTimeline from '@/components/SessionTimeline.vue';
import PromptComposer from '@/components/PromptComposer.vue';
import ActionSheet from '@/components/ActionSheet.vue';
import EmptyState from '@/components/EmptyState.vue';

const route = useRoute();
const router = useRouter();
const nodeStore = useNodeStore();
const commands = useCommandsStore();
const connection = useConnectionStore();
const preferences = usePreferencesStore();
const ui = useUiStore();

const sessionId = computed(() => String(route.params.sessionId ?? ''));
const session = computed(() => nodeStore.sessionById[sessionId.value]);
const agent = computed(() => nodeStore.agentFor(sessionId.value));
const pendingPermissions = computed(() => nodeStore.pendingFor(sessionId.value));
const timeline = computed(() => buildTimeline(nodeStore.sessionEvents(sessionId.value)));
const prompts = computed(() => commands.promptsFor(sessionId.value));
const hasMessages = computed(() => timeline.value.length > 0 || prompts.value.length > 0);
const draft = ref('');
const actionsOpen = ref(false);
const confirmOpen = ref(false);

const title = computed(() => session.value?.title?.trim() || '会话详情');
const canInterrupt = computed(
  () => session.value?.status === 'running' || session.value?.status === 'waiting',
);
const interruptHint = computed(() => {
  if (!connection.isOnline) return '当前离线';
  if (!canInterrupt.value) return '当前会话未运行';
  return undefined;
});
const actions = computed<ActionSheetAction[]>(() => [
  {
    key: 'interrupt',
    label: '中断运行',
    tone: 'danger',
    disabled: interruptHint.value !== undefined,
    description: interruptHint.value,
  },
  { key: 'copy', label: '复制会话 ID' },
  { key: 'raw', label: '打开原始事件' },
]);
const confirmActions: ActionSheetAction[] = [
  { key: 'confirm', label: '确认中断', tone: 'danger' },
];

async function copySessionId(): Promise<void> {
  try {
    await navigator.clipboard.writeText(sessionId.value);
    ui.push('已复制会话 ID', 'success');
  } catch {
    ui.push('复制失败，请重试', 'warning');
  }
}

function openRawEvents(): void {
  preferences.patch({ showRawEvents: true });
  void router.push('/activity');
}

function selectAction(key: string): void {
  actionsOpen.value = false;
  if (key === 'interrupt') {
    confirmOpen.value = true;
    return;
  }
  if (key === 'copy') {
    void copySessionId();
    return;
  }
  if (key === 'raw') openRawEvents();
}

function confirmInterrupt(): void {
  confirmOpen.value = false;
  if (!canInterrupt.value || !connection.isOnline) return;
  void commands.interrupt(sessionId.value);
}

async function sendPrompt(): Promise<void> {
  const ok = await commands.prompt(sessionId.value, draft.value);
  if (ok) draft.value = '';
}

function respond(permissionId: string, decision: PermissionDecision): void {
  void commands.respondPermission(permissionId, decision);
}
</script>

<template>
  <div class="session" :class="{ 'session--empty': !hasMessages }">
    <header class="detail-nav">
      <RouterLink to="/sessions" class="detail-nav__button" aria-label="返回">‹</RouterLink>
      <div class="detail-nav__title">{{ title }}</div>
      <StatusPill
        v-if="session"
        :label="sessionStatusLabel(session.status)"
        :tone="sessionStatusTone(session.status)"
      />
      <button
        type="button"
        class="detail-nav__button"
        aria-label="更多操作"
        @click="actionsOpen = true"
      >
        ⋯
      </button>
    </header>

    <ActionSheet
      :open="actionsOpen"
      title="会话操作"
      :actions="actions"
      @close="actionsOpen = false"
      @select="selectAction"
    />
    <ActionSheet
      :open="confirmOpen"
      title="中断运行？"
      :description="`会话「${title}」的当前回复会停止，已产生的输出会保留。`"
      :actions="confirmActions"
      @close="confirmOpen = false"
      @select="confirmInterrupt"
    />

    <template v-if="session">
      <div v-if="!connection.isOnline" class="banner banner--danger">
        网络离线，暂时无法发送消息。
      </div>

      <div v-if="pendingPermissions.length > 0" class="session__permissions">
        <PermissionCard
          v-for="permission in pendingPermissions"
          :key="permission.permission_id"
          :permission="permission"
          :agent-name="agent?.name"
          :session-title="session.title"
          :busy="commands.isBusy(`permission:${permission.permission_id}`)"
          @respond="respond"
        />
      </div>

      <SessionTimeline
        :entries="timeline"
        :prompts="prompts"
        :agent-name="agent?.name"
        :session-title="session.title"
      />

      <PromptComposer
        v-model="draft"
        :busy="commands.isBusy(`prompt:${session.session_id}`)"
        :disabled="!connection.isOnline"
        @send="sendPrompt"
      />
    </template>

    <EmptyState
      v-else
      title="会话不存在或已被删除"
      description="返回会话列表刷新当前快照。"
    >
      <RouterLink to="/sessions" class="btn btn--sm">返回会话</RouterLink>
    </EmptyState>
  </div>
</template>

<style scoped src="./session-detail.css"></style>
