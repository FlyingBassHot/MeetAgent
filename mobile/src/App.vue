<script setup lang="ts">
import { onMounted } from 'vue';
import ToastHost from '@/components/ToastHost.vue';
import TabGuardOverlay from '@/components/TabGuardOverlay.vue';
import { useConnectionStore } from '@/stores/connection';

const connection = useConnectionStore();

onMounted(() => {
  connection.bootstrap();
});
</script>

<template>
  <RouterView />
  <TabGuardOverlay />
  <ToastHost />
</template>
