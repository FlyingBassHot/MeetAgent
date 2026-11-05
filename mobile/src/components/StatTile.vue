<script setup lang="ts">
defineProps<{
  label: string;
  value: string | number;
  hint?: string;
}>();
</script>

<template>
  <div class="stat">
    <div class="stat__label">{{ label }}</div>
    <div class="stat__value">{{ value }}</div>
    <div v-if="hint" class="stat__hint">{{ hint }}</div>
  </div>
</template>
