<script setup lang="ts">
defineProps<{
  open: boolean;
  title: string;
}>();

const emit = defineEmits<{ close: [] }>();
</script>

<template>
  <div v-if="open" class="sheet" @click.self="emit('close')">
    <section class="sheet__panel">
      <header class="sheet__head">
        <h2>{{ title }}</h2>
        <button type="button" class="sheet__close" aria-label="关闭" @click="emit('close')">×</button>
      </header>
      <slot />
    </section>
  </div>
</template>

<style scoped>
.sheet {
  position: fixed;
  inset: 0;
  z-index: 60;
  display: flex;
  align-items: flex-end;
  background: rgba(5, 7, 10, 0.62);
}

.sheet__panel {
  width: 100%;
  max-height: 72dvh;
  overflow: auto;
  padding: 16px 16px calc(20px + env(safe-area-inset-bottom));
  border-top: 1px solid var(--border);
  border-radius: 18px 18px 0 0;
  background: var(--surface);
}

.sheet__head {
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin-bottom: 12px;
}

.sheet__head h2 {
  font-size: 16px;
  font-weight: 650;
}

.sheet__close {
  width: 32px;
  height: 32px;
  border: 0;
  border-radius: 50%;
  background: var(--surface-3);
  color: var(--muted);
  font-size: 18px;
  cursor: pointer;
}

@media (min-width: 900px) {
  .sheet {
    align-items: center;
    justify-content: center;
  }

  .sheet__panel {
    max-width: 420px;
    border: 1px solid var(--border);
    border-radius: var(--radius);
  }
}
</style>
