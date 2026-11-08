import { defineStore } from 'pinia';
import { ref } from 'vue';
import {
  initNetworkStatus,
  isNetworkOnline,
  subscribeNetwork,
} from '@/services/connectivity/networkStatus';

export const useNetworkStore = defineStore('network', () => {
  const online = ref(isNetworkOnline());
  let initialized = false;

  async function init(): Promise<void> {
    if (initialized) return;
    initialized = true;
    await initNetworkStatus();
    online.value = isNetworkOnline();
    subscribeNetwork((next) => {
      online.value = next;
    });
  }

  return { online, init };
});
