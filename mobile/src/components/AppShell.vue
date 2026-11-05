<script setup lang="ts">
import { computed } from 'vue';
import { NAV_ITEMS } from '@/app/nav';
import { shortId } from '@/domain/format';
import { useConnectionStore } from '@/stores/connection';
import { useNodeStore } from '@/stores/node';
import ConnectionBadge from './ConnectionBadge.vue';
import MockBadge from './MockBadge.vue';
import NavIcon from './NavIcon.vue';

const connection = useConnectionStore();
const nodeStore = useNodeStore();

const nodeName = computed(() => nodeStore.node?.name ?? 'MeetAgent Node');
const nodeHost = computed(
  () => nodeStore.node?.hostname ?? (connection.mode === 'mock' ? 'local mock' : '未连接'),
);
const nodeId = computed(() => shortId(nodeStore.node?.node_id, 8));
const isMock = computed(() => connection.mode === 'mock');
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
      <div class="nav-foot">
        <MockBadge v-if="isMock" />
        <span class="mono">{{ nodeId }}</span>
        <span>{{ isMock ? '本地 Mock Node' : 'Node 连接' }}</span>
      </div>
    </aside>

    <div class="shell__main">
      <header class="topbar">
        <div class="topbar__node">
          <span class="topbar__name">{{ nodeName }}</span>
          <span class="topbar__host">{{ nodeHost }}</span>
        </div>
        <div class="topbar__status">
          <MockBadge v-if="isMock" />
          <ConnectionBadge />
        </div>
      </header>

      <main class="page">
        <RouterView />
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
