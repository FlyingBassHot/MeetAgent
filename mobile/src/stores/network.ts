import { defineStore } from 'pinia';
import { ref } from 'vue';

export const useNetworkStore = defineStore('network', () => {
  const online = ref(typeof navigator === 'undefined' ? true : navigator.onLine);
  let initialized = false;

  function sync(): void {
    online.value = navigator.onLine;
  }

  function init(): void {
    if (initialized) return;
    initialized = true;
    window.addEventListener('online', sync);
    window.addEventListener('offline', sync);
    sync();
  }

  return { online, init };
});
