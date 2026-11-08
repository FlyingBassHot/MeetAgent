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
    <div class="skeleton__surface">
      <div v-for="row in rows" :key="row" class="skeleton__row">
        <span class="skeleton__line skeleton__line--title shimmer" />
        <span class="skeleton__line skeleton__line--meta shimmer" />
        <span class="skeleton__line skeleton__line--wide shimmer" />
      </div>
    </div>
  </div>
</template>

<style scoped>
.skeleton {
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.skeleton__status {
  width: 58%;
  height: 16px;
  margin-bottom: 2px;
  border-radius: 8px;
}

.skeleton__surface {
  overflow: hidden;
  border: 1px solid var(--border);
  border-radius: var(--radius);
  background: var(--surface);
}

.skeleton__row {
  display: flex;
  flex-direction: column;
  gap: 8px;
  min-height: 66px;
  padding: 13px 15px;
}

.skeleton__row + .skeleton__row {
  border-top: 1px solid var(--border);
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
  width: 27%;
}

.skeleton__line--wide {
  width: 74%;
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
</style>
