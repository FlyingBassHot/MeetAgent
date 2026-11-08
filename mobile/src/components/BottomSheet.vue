<script setup lang="ts">
import AppSheet from './AppSheet.vue';

defineProps<{
  open: boolean;
  title: string;
}>();

const emit = defineEmits<{ close: [] }>();
</script>

<template>
  <AppSheet :open="open" :title="title" @close="emit('close')">
    <slot />
  </AppSheet>
</template>
