<script setup lang="ts">
import { onMounted } from 'vue';
import { useRouter } from 'vue-router';
import ToastHost from '@/components/ToastHost.vue';
import TabGuardOverlay from '@/components/TabGuardOverlay.vue';
import { initNativeSystem } from '@/services/native/system';
import { useConnectionStore } from '@/stores/connection';
import { useNetworkStore } from '@/stores/network';

const router = useRouter();
const connection = useConnectionStore();
const network = useNetworkStore();

onMounted(async () => {
  void initNativeSystem(router);
  await network.init();
  connection.bootstrap();
});
</script>

<template>
  <RouterView />
  <TabGuardOverlay />
  <ToastHost />
</template>
