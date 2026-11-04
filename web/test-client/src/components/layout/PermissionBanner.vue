<script setup lang="ts">
import { computed } from 'vue'
import { useRouter } from 'vue-router'
import { useEventsStore } from '../../stores/events'

const events = useEventsStore()
const router = useRouter()

const count = computed(() => events.pendingPermissions.length)

function goHandle(): void {
  const first = events.pendingPermissions[0]
  if (first?.session_id) {
    router.push({
      path: '/sessions',
      query: { node: first.node_id, agent: first.agent_id ?? '', session: first.session_id },
    })
    return
  }
  router.push('/console')
}
</script>

<template>
  <div v-if="count > 0" class="permission-banner" role="status" data-testid="permission-banner">
    <span class="permission-banner-icon" aria-hidden="true">!</span>
    <span class="permission-banner-text">{{ count }} 个权限请求待处理</span>
    <button type="button" class="banner-action" @click="goHandle">去处理</button>
  </div>
</template>
