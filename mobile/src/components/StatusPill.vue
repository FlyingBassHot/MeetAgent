<script setup lang="ts">
import type { Tone } from '@/domain/labels';

withDefaults(
  defineProps<{
    label: string;
    tone?: Tone;
    dot?: boolean;
  }>(),
  { tone: 'neutral', dot: true },
);
</script>

<template>
  <span class="pill" :class="[`pill--${tone}`, { 'pill--dot': dot }]">{{ label }}</span>
</template>
