<script setup lang="ts">
import { computed, onMounted } from 'vue'
import { useRoute } from 'vue-router'
import AppHeader from './components/layout/AppHeader.vue'
import AppSidebar from './components/layout/AppSidebar.vue'
import AppFooter from './components/layout/AppFooter.vue'
import ReconnectBanner from './components/layout/ReconnectBanner.vue'
import PermissionBanner from './components/layout/PermissionBanner.vue'
import BottomNav from './components/layout/BottomNav.vue'
import ConnectionGate from './components/connection/ConnectionGate.vue'
import { useConnectionStore } from './stores/connection'

const connection = useConnectionStore()
const route = useRoute()

/** 手机端会话详情：隐藏底部导航，把空间留给输入条。 */
const sessionDetailOpen = computed(
  () => route.name === 'sessions' && typeof route.query.session === 'string' && route.query.session !== '',
)

onMounted(() => connection.bootstrap())
</script>

<template>
  <ConnectionGate v-if="connection.showGate" />

  <div v-else class="app-shell" :class="{ 'is-session-detail': sessionDetailOpen }">
    <AppHeader />
    <PermissionBanner />
    <ReconnectBanner />
    <main class="layout">
      <AppSidebar />
      <div class="workspace">
        <RouterView v-slot="{ Component }">
          <transition name="view-fade" mode="out-in">
            <component :is="Component" />
          </transition>
        </RouterView>
      </div>
    </main>
    <AppFooter />
    <BottomNav />
  </div>
</template>
