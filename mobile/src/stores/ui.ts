import { defineStore } from 'pinia';
import { ref } from 'vue';
import type { Tone } from '@/domain/labels';

export interface Toast {
  id: number;
  message: string;
  tone: Tone;
}

let toastCounter = 0;

export const useUiStore = defineStore('ui', () => {
  const toasts = ref<Toast[]>([]);

  function dismiss(id: number): void {
    toasts.value = toasts.value.filter((toast) => toast.id !== id);
  }

  function push(message: string, tone: Tone = 'info'): number {
    toastCounter += 1;
    const id = toastCounter;
    toasts.value.push({ id, message, tone });
    window.setTimeout(() => dismiss(id), 4200);
    return id;
  }

  return { toasts, push, dismiss };
});
