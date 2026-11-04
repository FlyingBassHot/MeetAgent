<script setup lang="ts">
import { ACCENT_OPTIONS, useThemeStore, type AccentTheme } from '../../stores/theme'

const theme = useThemeStore()

const swatchVar = (value: AccentTheme): string => `var(--swatch-${value})`
</script>

<template>
  <div class="theme-switcher" role="group" aria-label="主题设置">
    <div class="theme-swatches" role="group" aria-label="强调色">
      <button
        v-for="option in ACCENT_OPTIONS"
        :key="option.value"
        type="button"
        class="swatch"
        :style="{ '--swatch': swatchVar(option.value) }"
        :aria-pressed="theme.accent === option.value"
        :aria-label="`强调色：${option.label}`"
        :title="option.label"
        @click="theme.setAccent(option.value)"
      ></button>
    </div>

    <button
      type="button"
      class="mode-toggle"
      :aria-label="theme.isDark ? '切换到亮色模式' : '切换到暗色模式'"
      @click="theme.toggleMode()"
    >
      <svg v-if="theme.isDark" viewBox="0 0 24 24" fill="none" stroke-width="2" stroke-linecap="round" aria-hidden="true">
        <circle cx="12" cy="12" r="4.5" />
        <path d="M12 2v2.5M12 19.5V22M2 12h2.5M19.5 12H22M4.9 4.9l1.8 1.8M17.3 17.3l1.8 1.8M19.1 4.9l-1.8 1.8M6.7 17.3l-1.8 1.8" />
      </svg>
      <svg v-else viewBox="0 0 24 24" fill="none" stroke-width="2" stroke-linejoin="round" aria-hidden="true">
        <path d="M21 12.8A9 9 0 1 1 11.2 3a7 7 0 0 0 9.8 9.8Z" />
      </svg>
      <span>{{ theme.isDark ? '暗色' : '亮色' }}</span>
    </button>
  </div>
</template>
