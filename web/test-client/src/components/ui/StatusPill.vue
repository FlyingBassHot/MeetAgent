<script setup lang="ts">
withDefaults(
  defineProps<{
    tone?: 'success' | 'busy' | 'idle' | 'danger' | 'info'
  }>(),
  { tone: 'idle' },
)
</script>

<template>
  <span class="status-pill" :class="tone" role="status">
    <span class="status-dot" aria-hidden="true"></span>
    <slot />
  </span>
</template>
