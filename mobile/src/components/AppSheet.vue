<script setup lang="ts">
import { useSheetLayer } from '@/composables/useSheetLayer';

const props = withDefaults(
  defineProps<{
    open: boolean;
    title?: string;
    showClose?: boolean;
  }>(),
  { title: '', showClose: true },
);

const emit = defineEmits<{ close: [] }>();

useSheetLayer(
  () => props.open,
  () => emit('close'),
);
</script>

<template>
  <Teleport to="body">
    <Transition name="sheet">
      <div v-if="open" class="sheet" @click.self="emit('close')">
        <section class="sheet__panel" role="dialog" aria-modal="true" :aria-label="title || '操作面板'">
          <span class="sheet__grabber" aria-hidden="true" />
          <header v-if="title" class="sheet__head">
            <h2>{{ title }}</h2>
            <button
              v-if="showClose"
              type="button"
              class="sheet__close"
              aria-label="关闭"
              @click="emit('close')"
            >
              ×
            </button>
          </header>
          <div class="sheet__body">
            <slot />
          </div>
        </section>
      </div>
    </Transition>
  </Teleport>
</template>

<style scoped>
.sheet {
  position: fixed;
  inset: 0;
  z-index: 60;
  display: flex;
  align-items: flex-end;
  background: rgba(3, 5, 8, 0.66);
  overscroll-behavior: contain;
}

.sheet__panel {
  display: flex;
  flex-direction: column;
  width: 100%;
  max-height: calc(100dvh - 42px - env(safe-area-inset-top));
  padding: 8px 16px calc(16px + env(safe-area-inset-bottom));
  border-top: 1px solid var(--border);
  border-radius: 20px 20px 0 0;
  background: var(--surface);
  box-shadow: 0 -18px 50px rgba(0, 0, 0, 0.36);
}

.sheet__grabber {
  align-self: center;
  width: 38px;
  height: 4px;
  margin: 0 0 8px;
  border-radius: 999px;
  background: var(--border-strong);
}

.sheet__head {
  display: flex;
  align-items: center;
  justify-content: space-between;
  flex-shrink: 0;
  gap: 12px;
  margin-bottom: 10px;
}

.sheet__head h2 {
  font-size: 16px;
  font-weight: 650;
}

.sheet__close {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 32px;
  height: 32px;
  border: 0;
  border-radius: 50%;
  background: var(--surface-3);
  color: var(--muted);
  font-size: 18px;
  cursor: pointer;
}

.sheet__body {
  min-height: 0;
  overflow-y: auto;
  overscroll-behavior: contain;
  -webkit-overflow-scrolling: touch;
}

.sheet-enter-active,
.sheet-leave-active {
  transition: opacity 0.22s ease;
}

.sheet-enter-from,
.sheet-leave-to {
  opacity: 0;
}

.sheet-enter-active .sheet__panel,
.sheet-leave-active .sheet__panel {
  transition: transform 0.28s cubic-bezier(0.22, 0.8, 0.25, 1);
}

.sheet-enter-from .sheet__panel,
.sheet-leave-to .sheet__panel {
  transform: translateY(100%);
}

@media (min-width: 900px) {
  .sheet {
    align-items: center;
    justify-content: center;
  }

  .sheet__panel {
    width: min(440px, calc(100vw - 48px));
    max-height: min(620px, calc(100dvh - 120px));
    border: 1px solid var(--border);
    border-radius: var(--radius);
    padding-bottom: 16px;
  }

  .sheet__grabber {
    display: none;
  }

  .sheet-enter-from .sheet__panel,
  .sheet-leave-to .sheet__panel {
    transform: translateY(14px) scale(0.98);
  }
}

@media (prefers-reduced-motion: reduce) {
  .sheet-enter-active,
  .sheet-leave-active,
  .sheet-enter-active .sheet__panel,
  .sheet-leave-active .sheet__panel {
    transition: none;
  }
}
</style>
