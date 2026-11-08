<script setup lang="ts">
import type { ActionSheetAction } from '@/domain/action-sheet';
import AppSheet from './AppSheet.vue';

defineProps<{
  open: boolean;
  title: string;
  description?: string;
  actions: ActionSheetAction[];
}>();

const emit = defineEmits<{
  close: [];
  select: [key: string];
}>();

function select(action: ActionSheetAction): void {
  if (action.disabled) return;
  emit('select', action.key);
}
</script>

<template>
  <AppSheet :open="open" :title="title" :show-close="false" @close="emit('close')">
    <p v-if="description" class="action-sheet__description">{{ description }}</p>
    <div class="action-sheet__group">
      <button
        v-for="action in actions"
        :key="action.key"
        type="button"
        class="action-sheet__item"
        :class="{
          'action-sheet__item--danger': action.tone === 'danger',
          'action-sheet__item--disabled': action.disabled,
        }"
        :disabled="action.disabled"
        @click="select(action)"
      >
        <span>{{ action.label }}</span>
        <span v-if="action.description" class="action-sheet__hint">{{ action.description }}</span>
      </button>
    </div>
    <button type="button" class="action-sheet__cancel" @click="emit('close')">取消</button>
  </AppSheet>
</template>

<style scoped>
.action-sheet__description {
  margin: -2px 0 12px;
  color: var(--muted);
  font-size: 12px;
  line-height: 1.55;
}

.action-sheet__group {
  overflow: hidden;
  border: 1px solid var(--border);
  border-radius: 14px;
  background: var(--surface-2);
}

.action-sheet__item {
  display: flex;
  flex-direction: column;
  align-items: flex-start;
  gap: 2px;
  width: 100%;
  min-height: 52px;
  padding: 12px 15px;
  border: 0;
  border-bottom: 1px solid var(--border);
  background: transparent;
  color: var(--fg);
  font: inherit;
  font-size: 15px;
  text-align: left;
  cursor: pointer;
}

.action-sheet__item:last-child {
  border-bottom: 0;
}

.action-sheet__item:active:not(:disabled) {
  background: var(--surface-3);
}

.action-sheet__item--danger {
  color: var(--danger);
}

.action-sheet__item--disabled {
  color: var(--faint);
  cursor: not-allowed;
}

.action-sheet__hint {
  color: var(--muted);
  font-size: 11px;
}

.action-sheet__cancel {
  width: 100%;
  min-height: 50px;
  margin-top: 8px;
  border: 1px solid var(--border);
  border-radius: 14px;
  background: var(--surface-2);
  color: var(--fg);
  font: inherit;
  font-size: 15px;
  font-weight: 600;
  cursor: pointer;
}

.action-sheet__cancel:active {
  background: var(--surface-3);
}
</style>
