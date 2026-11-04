<script setup lang="ts">
import { computed } from 'vue'
import { useConnectionStore } from '../../stores/connection'
import { useEventsStore } from '../../stores/events'
import { useInventoryStore } from '../../stores/inventory'
import { PHASE_LABELS, phaseTone } from '../../lib/status'
import StatusPill from '../ui/StatusPill.vue'
import ThemeSwitcher from '../ui/ThemeSwitcher.vue'

const connection = useConnectionStore()
const inventory = useInventoryStore()
const events = useEventsStore()

const totalNodes = computed(() => inventory.nodes.length)
</script>

<template>
  <header class="topbar">
    <RouterLink to="/console" class="brand" aria-label="MeetAgent 控制台首页">
      <span class="brand-mark" aria-hidden="true">M</span>
      <span class="brand-text">
        <h1>MeetAgent</h1>
        <p>Phase 2 · Agent Console</p>
      </span>
    </RouterLink>

    <div class="topbar-end">
      <StatusPill :tone="phaseTone(connection.phase)">{{ PHASE_LABELS[connection.phase] }}</StatusPill>
      <span v-if="connection.heartbeatRttMs !== null" class="metric hide-sm">
        RTT <strong>{{ connection.heartbeatRttMs }}ms</strong>
      </span>
      <span v-if="connection.phase === 'reconnecting'" class="metric hide-sm">
        重连 <strong>#{{ connection.reconnectAttempt }}</strong>
      </span>
      <span class="metric">
        Nodes <strong>{{ inventory.onlineNodeCount }}/{{ totalNodes }}</strong>
      </span>
      <span class="metric hide-md">
        Agents <strong>{{ inventory.onlineAgentCount }}</strong>
      </span>
      <span class="metric hide-md">
        Events <strong>{{ events.events.length }}</strong>
      </span>
      <ThemeSwitcher />
    </div>
  </header>
</template>
