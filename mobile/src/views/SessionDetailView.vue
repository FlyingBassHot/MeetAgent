<script setup lang="ts">
import { computed, onBeforeUnmount, ref, watch } from 'vue';
import { useRoute, useRouter } from 'vue-router';
import type { PermissionDecision } from '@/domain/protocol';
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
const menuOpen = ref(false);
const confirmingInterrupt = ref(false);
let interruptTimer: number | null = null;

const title = computed(() => session.value?.title?.trim() || '会话详情');
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
  if (!canInterrupt.value || !connection.isOnline) return;
  if (!confirmingInterrupt.value) {
    confirmingInterrupt.value = true;
    interruptTimer = window.setTimeout(() => {
      confirmingInterrupt.value = false;
      interruptTimer = null;
    }, 3000);
    return;
  }
  resetInterrupt();
  menuOpen.value = false;
  void commands.interrupt(sessionId.value);
}

async function copySessionId(): Promise<void> {
  try {
    await navigator.clipboard.writeText(sessionId.value);
    ui.push('已复制会话 ID', 'success');
  } catch {
    ui.push('复制失败，请重试', 'warning');
  }
  menuOpen.value = false;
}

function openRawEvents(): void {
  preferences.patch({ showRawEvents: true });
  menuOpen.value = false;
  void router.push('/activity');
}

async function sendPrompt(): Promise<void> {
  const ok = await commands.prompt(sessionId.value, draft.value);
  if (ok) draft.value = '';
}

function respond(permissionId: string, decision: PermissionDecision): void {
  void commands.respondPermission(permissionId, decision);
}

watch(menuOpen, (open) => {
  if (!open) resetInterrupt();
});

watch(canInterrupt, (active) => {
  if (!active) resetInterrupt();
});

onBeforeUnmount(resetInterrupt);
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
        @click="menuOpen = !menuOpen"
      >
        ⋯
      </button>
    </header>

    <div v-if="menuOpen" class="menu-backdrop" @click="menuOpen = false" />
    <div v-if="menuOpen" class="detail-menu">
      <button
        type="button"
        class="detail-menu__item detail-menu__item--danger"
        :disabled="!canInterrupt || !connection.isOnline"
        @click="onInterrupt"
      >
        {{ confirmingInterrupt ? '确认中断' : '中断运行' }}
      </button>
      <button type="button" class="detail-menu__item" @click="copySessionId">
        复制会话 ID
      </button>
      <button type="button" class="detail-menu__item" @click="openRawEvents">
        打开原始事件
      </button>
    </div>

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
