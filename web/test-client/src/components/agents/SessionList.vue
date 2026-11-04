<script setup lang="ts">
import type { SessionView } from '../../lib/protocol'

defineProps<{
  sessions: SessionView[]
  selectedSessionId: string
}>()

const emit = defineEmits<{
  pick: [sessionId: string]
}>()
</script>

<template>
  <ul class="session-list">
    <li
      v-for="session in sessions"
      :key="session.session_id"
      :class="{ selected: session.session_id === selectedSessionId }"
      role="button"
      tabindex="0"
      :aria-pressed="session.session_id === selectedSessionId"
      @click.stop="emit('pick', session.session_id)"
      @keydown.enter.stop="emit('pick', session.session_id)"
      @keydown.space.prevent.stop="emit('pick', session.session_id)"
    >
      <span>{{ session.title || session.session_id }}</span>
      <code>{{ session.status }}</code>
    </li>
  </ul>
</template>
