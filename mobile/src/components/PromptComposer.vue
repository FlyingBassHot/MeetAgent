<script setup lang="ts">
import { computed } from 'vue';

const props = withDefaults(
  defineProps<{
    modelValue: string;
    disabled?: boolean;
    busy?: boolean;
  }>(),
  { disabled: false, busy: false },
);

const emit = defineEmits<{
  'update:modelValue': [value: string];
  send: [];
}>();

const draft = computed({
  get: () => props.modelValue,
  set: (value: string) => emit('update:modelValue', value),
});

const canSend = computed(
  () => !props.disabled && !props.busy && props.modelValue.trim().length > 0,
);

function submit(): void {
  if (!canSend.value) return;
  emit('send');
}
</script>

<template>
  <div class="composer">
    <textarea
      v-model="draft"
      class="composer__input"
      :disabled="disabled || busy"
      placeholder="输入消息…"
      rows="1"
      aria-label="Prompt"
      @keydown.ctrl.enter.prevent="submit"
      @keydown.meta.enter.prevent="submit"
    />
    <button
      type="button"
      class="composer__send"
      :disabled="!canSend"
      aria-label="发送"
      @click="submit"
    >
      {{ busy ? '发送中' : '发送' }}
    </button>
  </div>
</template>

<style scoped>
.composer {
  display: flex;
  align-items: flex-end;
  gap: 6px;
  padding: 6px;
  border: 1px solid var(--border);
  border-radius: 16px;
  background: var(--surface);
  transition: border-color 0.15s ease, box-shadow 0.15s ease;
}

.composer:focus-within {
  border-color: rgba(95, 157, 255, 0.55);
  box-shadow: 0 0 0 3px var(--accent-soft);
}

.composer__input {
  flex: 1;
  min-height: 44px;
  max-height: 140px;
  padding: 10px 10px;
  border: 0;
  background: transparent;
  color: var(--fg);
  font: inherit;
  font-size: 14.5px;
  resize: none;
  outline: none;
}

.composer__input::placeholder {
  color: var(--faint);
}

.composer__send {
  min-height: 44px;
  padding: 0 16px;
  border: 0;
  border-radius: 12px;
  background: linear-gradient(180deg, var(--accent-1), var(--accent-2));
  color: var(--on-accent);
  font-size: 13px;
  font-weight: 650;
  cursor: pointer;
  transition: opacity 0.14s ease, filter 0.14s ease;
}

.composer__send:active:not(:disabled) {
  filter: brightness(0.94);
}

.composer__send:disabled {
  background: var(--surface-3);
  color: var(--faint);
  opacity: 1;
  cursor: not-allowed;
}
</style>
