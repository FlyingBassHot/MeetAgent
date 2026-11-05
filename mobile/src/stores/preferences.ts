import { defineStore } from 'pinia';
import { computed, reactive } from 'vue';
import {
  loadPreferences,
  savePreferences,
  type StoredPreferences,
} from '@/services/storage/preferences';

export const usePreferencesStore = defineStore('preferences', () => {
  const settings = reactive<StoredPreferences>(loadPreferences());

  function patch(next: Partial<StoredPreferences>): void {
    Object.assign(settings, next);
    savePreferences({ ...settings });
  }

  function setToken(token: string | null): void {
    patch({ token });
  }

  const tokenPreview = computed(() => {
    const token = settings.token;
    if (!token) return '未保存';
    return `${token.slice(0, 8)}…${token.slice(-4)}`;
  });

  return { settings, patch, setToken, tokenPreview };
});
