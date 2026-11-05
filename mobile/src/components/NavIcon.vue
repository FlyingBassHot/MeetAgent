<script setup lang="ts">
import type { NavIconName } from '@/app/nav';

defineProps<{ name: NavIconName }>();

const PATHS: Record<NavIconName, string[]> = {
  home: ['M3.5 10.5 12 3.5l8.5 7V20a1 1 0 0 1-1 1h-5v-6h-5v6h-5a1 1 0 0 1-1-1z'],
  sessions: [
    'M20 14.5a2.5 2.5 0 0 1-2.5 2.5H9l-4 3.5v-14A2.5 2.5 0 0 1 7.5 4h10A2.5 2.5 0 0 1 20 6.5z',
    'M8 8.5h8M8 12h5',
  ],
  activity: ['M3 12h3l2.6-6.5 4 13 2.4-6.5H21'],
  settings: [
    'M12 15.2a3.2 3.2 0 1 0 0-6.4 3.2 3.2 0 0 0 0 6.4z',
    'M19.4 15a1.7 1.7 0 0 0 .34 1.87l.06.06a2 2 0 1 1-2.83 2.83l-.06-.06A1.7 1.7 0 0 0 15 19.4a1.7 1.7 0 0 0-1 1.55V21a2 2 0 1 1-4 0v-.09A1.7 1.7 0 0 0 9 19.4a1.7 1.7 0 0 0-1.87.34l-.06.06a2 2 0 1 1-2.83-2.83l.06-.06A1.7 1.7 0 0 0 4.6 15a1.7 1.7 0 0 0-1.55-1H3a2 2 0 1 1 0-4h.09A1.7 1.7 0 0 0 4.6 9a1.7 1.7 0 0 0-.34-1.87l-.06-.06a2 2 0 1 1 2.83-2.83l.06.06A1.7 1.7 0 0 0 9 4.6a1.7 1.7 0 0 0 1-1.55V3a2 2 0 1 1 4 0v.09A1.7 1.7 0 0 0 15 4.6a1.7 1.7 0 0 0 1.87-.34l.06-.06a2 2 0 1 1 2.83 2.83l-.06.06A1.7 1.7 0 0 0 19.4 9c.1.38.38.7.75.87.3.13.63.2.96.2H21a2 2 0 1 1 0 4h-.09a1.7 1.7 0 0 0-1.51 1z',
  ],
};
</script>

<template>
  <svg
    viewBox="0 0 24 24"
    fill="none"
    stroke="currentColor"
    stroke-width="1.7"
    stroke-linecap="round"
    stroke-linejoin="round"
    aria-hidden="true"
  >
    <path v-for="(path, index) in PATHS[name]" :key="index" :d="path" />
  </svg>
</template>
