<script setup lang="ts">
withDefaults(
  defineProps<{
    rows?: number;
    status?: boolean;
  }>(),
  { rows: 4, status: true },
);
</script>

<template>
  <div class="skeleton" aria-busy="true" aria-label="正在加载">
    <div v-if="status" class="skeleton__status shimmer" />
    <div v-for="row in rows" :key="row" class="skeleton__row">
      <span class="skeleton__line skeleton__line--title shimmer" />
      <span class="skeleton__line skeleton__line--meta shimmer" />
      <span class="skeleton__line skeleton__line--wide shimmer" />
    </div>
  </div>
</template>

<style scoped>
.skeleton {
  display: flex;
  flex-direction: column;
  gap: 9px;
}

.skeleton__status {
  width: 68%;
  height: 14px;
  margin-bottom: 3px;
  border-radius: 7px;
}

.skeleton__row {
  display: flex;
  flex-direction: column;
  gap: 8px;
  padding: 14px;
  border: 1px solid var(--border);
  border-radius: 13px;
  background: var(--surface);
}

.skeleton__line {
  height: 10px;
  border-radius: 6px;
}

.skeleton__line--title {
  width: 48%;
  height: 13px;
}

.skeleton__line--meta {
  width: 28%;
}

.skeleton__line--wide {
  width: 76%;
}

.shimmer {
  background: linear-gradient(
    90deg,
    var(--surface-2) 0%,
    var(--surface-3) 45%,
    var(--surface-2) 100%
  );
  background-size: 220% 100%;
  animation: skeleton-shimmer 1.4s ease-in-out infinite;
}

@keyframes skeleton-shimmer {
  to {
    background-position: -220% 0;
  }
}

@media (prefers-reduced-motion: reduce) {
  .shimmer {
    animation: none;
  }
}
</style>
