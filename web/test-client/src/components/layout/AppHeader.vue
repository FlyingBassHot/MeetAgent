<script setup lang="ts">
import { computed } from 'vue'
import { useConnectionStore } from '../../stores/connection'
import { useEventsStore } from '../../stores/events'
import { PHASE_LABELS, phaseTone } from '../../lib/status'
import StatusPill from '../ui/StatusPill.vue'
import ThemeSwitcher from '../ui/ThemeSwitcher.vue'

const connection = useConnectionStore()
const events = useEventsStore()

const pendingCount = computed(() => events.pendingPermissions.length)
</script>

<template>
  <header class="topbar">
    <RouterLink to="/console" class="brand" aria-label="MeetAgent 控制台首页">
      <span class="brand-mark" aria-hidden="true">M</span>
      <span class="brand-text">
        <h1>MeetAgent</h1>
        <p>Debug Console</p>
      </span>
    </RouterLink>

    <div class="topbar-end">
      <RouterLink
        to="/settings"
        class="status-link"
        data-testid="connection-status"
        aria-label="连接状态与设置"
      >
        <StatusPill :tone="phaseTone(connection.phase)">
          {{ PHASE_LABELS[connection.phase] }}
        </StatusPill>
      </RouterLink>

      <RouterLink
        v-if="pendingCount > 0"
        to="/console"
        class="pending-badge"
        data-testid="permission-badge"
        :aria-label="`${pendingCount} 个权限请求待处理`"
      >
        <span aria-hidden="true">!</span>
        {{ pendingCount }} 待审批
      </RouterLink>

      <ThemeSwitcher />
    </div>
  </header>
</template>
