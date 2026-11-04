<script setup lang="ts">
withDefaults(
  defineProps<{
    icon?: string
    title: string
    hint?: string
    compact?: boolean
  }>(),
  { icon: '◇', hint: undefined, compact: false },
)
</script>

<template>
  <div class="empty-state" :class="{ compact }">
    <span class="empty-icon" aria-hidden="true">{{ icon }}</span>
    <p>{{ title }}</p>
    <small v-if="hint">{{ hint }}</small>
  </div>
</template>
