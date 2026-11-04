<script setup lang="ts">
withDefaults(
  defineProps<{
    label: string
    value?: string | number
    hint?: string
  }>(),
  { value: '—', hint: undefined },
)
</script>

<template>
  <div class="stat-item">
    <div class="stat-label">{{ label }}</div>
    <div class="stat-value">{{ value }}</div>
    <div v-if="hint" class="stat-hint">{{ hint }}</div>
  </div>
</template>
