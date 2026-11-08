<script setup lang="ts">
import { computed } from 'vue';
import { useRouter } from 'vue-router';
import { CLIENT_VERSION } from '@/domain/protocol';
import { platform } from '@/services/platform';
import { useConnectionStore } from '@/stores/connection';
import { usePreferencesStore } from '@/stores/preferences';
import { useUiStore } from '@/stores/ui';
import PageHeader from '@/components/PageHeader.vue';
import StatusPill from '@/components/StatusPill.vue';

const router = useRouter();
const preferences = usePreferencesStore();
const connection = useConnectionStore();
const ui = useUiStore();

const connectionLabel = computed(() => {
  if (connection.isOnline) return '已连接';
  if (connection.isBusy) return '连接中';
  if (connection.tabBlocked) return '其他标签页已连接';
  return '未连接';
});

const connectionSummary = computed(() => {
  if (connection.isOnline) return '连接正常';
  if (connection.isBusy) return '正在连接 Node';
  if (connection.tabBlocked) return '请在另一个标签页处理连接';
  return '尚未连接';
});

const connectionTone = computed<'success' | 'warning' | 'neutral'>(() => {
  if (connection.isOnline) return 'success';
  if (connection.isBusy) return 'warning';
  return 'neutral';
});

async function reconnect(): Promise<void> {
  await connection.connect({
    mode: preferences.settings.mode,
    baseUrl: preferences.settings.baseUrl,
    token: preferences.settings.token,
  });
}

function disconnect(): void {
  connection.disconnect();
  void router.push('/connect');
}

async function toggleNotifications(): Promise<void> {
  if (preferences.settings.notifyEnabled) {
    preferences.patch({ notifyEnabled: false });
    return;
  }
  const granted = await platform.requestNotificationPermission();
  if (!granted) {
    ui.push(platform.notificationSupported ? '浏览器未授权通知' : '当前环境不支持通知', 'warning');
    return;
  }
  preferences.patch({ notifyEnabled: true });
  ui.push('已启用本地通知', 'success');
}
</script>

<template>
  <div class="settings">
    <PageHeader title="设置" />

    <section class="settings-group">
      <h2>连接</h2>
      <div class="settings-row">
        <div class="settings-row__text">
          <strong>Node</strong>
          <span>{{ connectionSummary }}</span>
        </div>
        <StatusPill :label="connectionLabel" :tone="connectionTone" />
      </div>
      <button type="button" class="settings-row settings-row--action" @click="reconnect">
        <span>重新连接</span><span class="settings-row__chevron">›</span>
      </button>
      <button type="button" class="settings-row settings-row--action" @click="disconnect">
        <span>断开连接</span><span class="settings-row__chevron">›</span>
      </button>
    </section>

    <section class="settings-group">
      <h2>通知</h2>
      <button
        type="button"
        class="settings-row settings-row--action"
        role="switch"
        :aria-checked="preferences.settings.notifyEnabled"
        @click="toggleNotifications"
      >
        <div class="settings-row__text">
          <strong>审批提醒</strong>
          <span>页面不在前台时提醒新的审批请求</span>
        </div>
        <span class="switch" :class="{ 'switch--on': preferences.settings.notifyEnabled }" />
      </button>
    </section>

    <section class="settings-group">
      <h2>外观</h2>
      <div class="settings-row">
        <div class="settings-row__text">
          <strong>主题</strong>
          <span>深色</span>
        </div>
      </div>
    </section>

    <section class="settings-group">
      <h2>关于</h2>
      <div class="settings-row">
        <div class="settings-row__text">
          <strong>MeetAgent Mobile</strong>
          <span>V2.1 · {{ CLIENT_VERSION }}</span>
        </div>
      </div>
      <div class="settings-row">
        <div class="settings-row__text">
          <strong>运行平台</strong>
          <span>{{ platform.kind === 'capacitor' ? '移动应用' : '浏览器' }}</span>
        </div>
      </div>
    </section>

    <RouterLink to="/settings/developer" class="settings-group settings-dev">
      <span>开发者信息</span>
      <span class="settings-row__chevron">›</span>
    </RouterLink>
  </div>
</template>

<style scoped src="./settings.css"></style>
