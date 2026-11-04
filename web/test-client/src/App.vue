<script setup lang="ts">
import AppHeader from './components/layout/AppHeader.vue'
import AppSidebar from './components/layout/AppSidebar.vue'
import AppFooter from './components/layout/AppFooter.vue'
import ReconnectBanner from './components/layout/ReconnectBanner.vue'
import BottomNav from './components/layout/BottomNav.vue'
</script>

<template>
  <div class="app-shell">
    <AppHeader />
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
