<script setup lang="ts">
import type { CommandActionMeta, TestAction } from '../../stores/commands'

defineProps<{
  modelValue: TestAction
  actions: readonly CommandActionMeta[]
}>()

const emit = defineEmits<{
  'update:modelValue': [value: TestAction]
}>()
</script>

<template>
  <div class="action-picker" role="group" aria-label="命令选择">
    <button
      v-for="item in actions"
      :key="item.value"
      type="button"
      class="action-option"
      :class="{ active: modelValue === item.value }"
      :aria-pressed="modelValue === item.value"
      @click="emit('update:modelValue', item.value)"
    >
      <strong>{{ item.label }}</strong>
      <small>{{ item.hint }}</small>
    </button>
  </div>
</template>
