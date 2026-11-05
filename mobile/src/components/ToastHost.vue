<script setup lang="ts">
import { useUiStore } from '@/stores/ui';

const ui = useUiStore();
</script>

<template>
  <div class="toast-host" aria-live="polite">
    <button
      v-for="toast in ui.toasts"
      :key="toast.id"
      type="button"
      class="toast"
      :class="`toast--${toast.tone}`"
      @click="ui.dismiss(toast.id)"
    >
      {{ toast.message }}
    </button>
  </div>
</template>

<style scoped>
.toast {
  text-align: left;
  cursor: pointer;
}
</style>
