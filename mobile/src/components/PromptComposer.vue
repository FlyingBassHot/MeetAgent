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
      rows="2"
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
  gap: 8px;
  padding: 8px;
  border: 1px solid var(--border);
  border-radius: 18px;
  background: var(--surface);
}

.composer__input {
  flex: 1;
  min-height: 40px;
  max-height: 140px;
  padding: 9px 10px;
  border: 0;
  background: transparent;
  color: var(--fg);
  font: inherit;
  resize: none;
  outline: none;
}

.composer__input::placeholder {
  color: var(--faint);
}

.composer__send {
  min-height: 38px;
  padding: 0 15px;
  border: 0;
  border-radius: 13px;
  background: var(--accent);
  color: var(--on-accent);
  font-size: 13px;
  font-weight: 650;
  cursor: pointer;
}

.composer__send:disabled {
  opacity: 0.4;
  cursor: not-allowed;
}
</style>
