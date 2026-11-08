<script setup lang="ts">
import { onMounted } from 'vue';
import ToastHost from '@/components/ToastHost.vue';
import TabGuardOverlay from '@/components/TabGuardOverlay.vue';
import { useConnectionStore } from '@/stores/connection';
import { useNetworkStore } from '@/stores/network';

const connection = useConnectionStore();
const network = useNetworkStore();

onMounted(async () => {
  await network.init();
  connection.bootstrap();
});
</script>

<template>
  <RouterView />
  <TabGuardOverlay />
  <ToastHost />
</template>
