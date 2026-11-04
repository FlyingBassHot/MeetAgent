import { defineStore } from 'pinia'
import { computed, ref } from 'vue'

export type AccentTheme = 'teal' | 'jade' | 'cyan' | 'violet' | 'amber' | 'rose'
export type ColorMode = 'dark' | 'light'

const STORAGE_KEY = 'meetagent.theme.v1'

const ACCENT_VALUES: readonly AccentTheme[] = ['teal', 'jade', 'cyan', 'violet', 'amber', 'rose']

export const ACCENT_OPTIONS: ReadonlyArray<{ value: AccentTheme; label: string }> = [
  { value: 'teal', label: '青绿' },
  { value: 'jade', label: '翡翠' },
  { value: 'cyan', label: '天青' },
  { value: 'violet', label: '紫罗兰' },
  { value: 'amber', label: '琥珀' },
  { value: 'rose', label: '玫瑰' },
]

interface StoredTheme {
  accent?: unknown
  mode?: unknown
}

function isAccent(value: unknown): value is AccentTheme {
  return typeof value === 'string' && (ACCENT_VALUES as readonly string[]).includes(value)
}

function isMode(value: unknown): value is ColorMode {
  return value === 'dark' || value === 'light'
}

function readStoredTheme(): { accent: AccentTheme; mode: ColorMode } | null {
  try {
    const raw = window.localStorage.getItem(STORAGE_KEY)
    if (!raw) return null
    const parsed = JSON.parse(raw) as StoredTheme
    if (!isAccent(parsed.accent) || !isMode(parsed.mode)) return null
    return { accent: parsed.accent, mode: parsed.mode }
  } catch {
    return null
  }
}

function systemPreferredMode(): ColorMode {
  try {
    return window.matchMedia('(prefers-color-scheme: light)').matches ? 'light' : 'dark'
  } catch {
    return 'dark'
  }
}

function applyToDocument(accent: AccentTheme, mode: ColorMode): void {
  const root = document.documentElement
  root.dataset.theme = accent
  root.dataset.mode = mode
}

export const useThemeStore = defineStore('theme', () => {
  const accent = ref<AccentTheme>('teal')
  const mode = ref<ColorMode>('dark')
  const initialized = ref(false)

  const isDark = computed(() => mode.value === 'dark')

  function persist(): void {
    try {
      window.localStorage.setItem(STORAGE_KEY, JSON.stringify({ accent: accent.value, mode: mode.value }))
    } catch {
      // localStorage 不可用（如 file:// 隐私模式）时静默降级，仅保留本次会话生效
    }
  }

  function init(): void {
    if (initialized.value) return
    const stored = readStoredTheme()
    accent.value = stored?.accent ?? 'teal'
    mode.value = stored?.mode ?? systemPreferredMode()
    applyToDocument(accent.value, mode.value)
    initialized.value = true
  }

  function setAccent(next: AccentTheme): void {
    accent.value = next
    applyToDocument(accent.value, mode.value)
    persist()
  }

  function setMode(next: ColorMode): void {
    mode.value = next
    applyToDocument(accent.value, mode.value)
    persist()
  }

  function toggleMode(): void {
    setMode(mode.value === 'dark' ? 'light' : 'dark')
  }

  return { accent, mode, isDark, init, setAccent, setMode, toggleMode }
})
