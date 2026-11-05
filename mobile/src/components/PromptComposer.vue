<script setup lang="ts">
import { computed } from 'vue';

const props = withDefaults(
  defineProps<{
    modelValue: string;
    disabled?: boolean;
    busy?: boolean;
    hint?: string;
  }>(),
  { disabled: false, busy: false, hint: '' },
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
  <div class="composer card card--pad">
    <textarea
      v-model="draft"
      class="textarea composer__input"
      :disabled="disabled || busy"
      placeholder="输入 Prompt，交给当前 Session 的 Agent…"
      rows="3"
      @keydown.ctrl.enter.prevent="submit"
      @keydown.meta.enter.prevent="submit"
    />
    <div class="composer__foot">
      <span class="small muted">{{ hint || 'Ctrl / ⌘ + Enter 发送' }}</span>
      <div class="row">
        <span class="small mono muted">{{ modelValue.length }}</span>
        <button
          type="button"
          class="btn btn--primary btn--sm"
          :disabled="!canSend"
          @click="submit"
        >
          {{ busy ? '发送中…' : '发送 Prompt' }}
        </button>
      </div>
    </div>
  </div>
</template>

<style scoped>
.composer {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.composer__input {
  min-height: 84px;
}

.composer__foot {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 10px;
}
</style>
