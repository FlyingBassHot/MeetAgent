<script setup lang="ts">
import { computed } from 'vue';
import { useRoute } from 'vue-router';
import { restoreScroll } from '@/app/scroll';
import { NAV_ITEMS } from '@/app/nav';
import { useConnectionStore } from '@/stores/connection';
import { useNetworkStore } from '@/stores/network';
import { useNodeStore } from '@/stores/node';
import ConnectionBadge from './ConnectionBadge.vue';
import MockBadge from './MockBadge.vue';
import NavIcon from './NavIcon.vue';
import PullToRefresh from './PullToRefresh.vue';
import StatusPill from './StatusPill.vue';

const route = useRoute();
const connection = useConnectionStore();
const network = useNetworkStore();
const nodeStore = useNodeStore();

const nodeName = computed(() => nodeStore.node?.name ?? 'MeetAgent');
const isMock = computed(() => connection.mode === 'mock');
const pullEnabled = computed(
  () =>
    connection.isOnline &&
    ['home', 'sessions', 'activity'].includes(String(route.name)),
);

function onPageEntered(): void {
  restoreScroll(route.fullPath);
}
</script>

<template>
  <div class="shell">
    <aside class="shell__nav">
      <div class="brand">MeetAgent <span>V2.1</span></div>
      <nav class="nav-links">
        <RouterLink
          v-for="item in NAV_ITEMS"
          :key="item.name"
          :to="{ name: item.name }"
          class="nav-link"
        >
          <NavIcon :name="item.icon" />
          <span>{{ item.label }}</span>
        </RouterLink>
      </nav>
    </aside>

    <div class="shell__main">
      <header class="topbar">
        <div class="topbar__node">
          <span class="topbar__name">{{ nodeName }}</span>
        </div>
        <div class="topbar__status">
          <MockBadge v-if="isMock" />
          <StatusPill v-if="!network.online" label="网络离线" tone="danger" />
          <ConnectionBadge v-else />
        </div>
      </header>

      <div v-if="!network.online" class="network-banner">
        网络离线：已暂停 Node 自动重连；恢复网络后将立即重连。
      </div>

      <main class="page">
        <PullToRefresh :enabled="pullEnabled" :reset-key="route.fullPath">
          <RouterView v-slot="{ Component, route: viewRoute }">
            <Transition
              :name="viewRoute.meta.transition === 'slide' ? 'page-slide' : 'page-none'"
              mode="out-in"
              @after-enter="onPageEntered"
            >
              <component :is="Component" :key="viewRoute.fullPath" />
            </Transition>
          </RouterView>
        </PullToRefresh>
      </main>
    </div>

    <nav class="tabbar">
      <RouterLink
        v-for="item in NAV_ITEMS"
        :key="item.name"
        :to="{ name: item.name }"
        class="tab"
      >
        <NavIcon :name="item.icon" />
        <span>{{ item.label }}</span>
      </RouterLink>
    </nav>
  </div>
</template>

<style scoped>
.network-banner {
  margin: 12px 20px 0;
  padding: 10px 12px;
  border: 1px solid rgba(244, 119, 119, 0.3);
  border-radius: var(--radius-sm);
  background: var(--danger-soft);
  color: var(--danger-fg);
  font-size: 12px;
}

@media (min-width: 900px) {
  .network-banner {
    margin: 16px 32px 0;
  }
}

.page-slide-enter-active {
  transition: opacity 0.16s ease, transform 0.16s ease;
}

.page-slide-leave-active {
  transition: opacity 0.1s ease, transform 0.1s ease;
}

.page-slide-enter-from {
  opacity: 0;
  transform: translateX(14px);
}

.page-slide-leave-to {
  opacity: 0;
  transform: translateX(-8px);
}

@media (prefers-reduced-motion: reduce) {
  .page-slide-enter-active,
  .page-slide-leave-active {
    transition: none;
  }
}
</style>
