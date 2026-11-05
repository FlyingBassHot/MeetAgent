<script setup lang="ts">
withDefaults(
  defineProps<{
    title: string;
    description?: string;
  }>(),
  { description: '' },
);
</script>

<template>
  <div class="empty">
    <strong>{{ title }}</strong>
    <span v-if="description" class="small">{{ description }}</span>
    <slot />
  </div>
</template>
