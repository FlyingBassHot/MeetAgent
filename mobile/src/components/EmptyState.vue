<script setup lang="ts">
withDefaults(
  defineProps<{
    title: string;
    description?: string;
    mark?: string;
  }>(),
  { description: '', mark: '' },
);
</script>

<template>
  <div class="empty">
    <span v-if="mark" class="empty__mark" aria-hidden="true">{{ mark }}</span>
    <strong>{{ title }}</strong>
    <span v-if="description" class="small">{{ description }}</span>
    <slot />
  </div>
</template>
