<script setup lang="ts">
import { computed, nextTick, onBeforeUnmount, onMounted, ref, watch } from 'vue';
import type { CSSProperties } from 'vue';
import { useConnectionStore } from '@/stores/connection';
import { impactLight, notifyError } from '@/services/native/haptics';

const props = defineProps<{
  enabled: boolean;
  resetKey: string;
}>();

const THRESHOLD = 70;
const connection = useConnectionStore();
const startY = ref<number | null>(null);
const startX = ref<number | null>(null);
const distance = ref(0);
const refreshing = ref(false);

const progress = computed(() => Math.min(1, distance.value / THRESHOLD));
const label = computed(() => {
  if (refreshing.value) return '正在刷新…';
  return distance.value >= THRESHOLD ? '松开刷新' : '下拉刷新';
});
const indicatorStyle = computed<CSSProperties>(() => {
  const offset = -34 + Math.min(42, distance.value * 0.62);
  return {
    opacity: refreshing.value || distance.value > 4 ? Math.min(1, distance.value / 22) : 0,
    transform: `translate(-50%, ${offset}px)`,
  };
});
const spinnerStyle = computed<CSSProperties>(() => ({
  transform: `rotate(${Math.round(progress.value * 180)}deg)`,
}));

function reset(): void {
  startY.value = null;
  startX.value = null;
  distance.value = 0;
}

function canPull(): boolean {
  if (!props.enabled || refreshing.value) return false;
  if (document.body.classList.contains('sheet-open')) return false;
  return window.scrollY <= 1;
}

function onTouchStart(event: TouchEvent): void {
  if (event.touches.length !== 1 || !canPull()) return;
  const touch = event.touches[0];
  startX.value = touch.clientX;
  startY.value = touch.clientY;
  distance.value = 0;
}

function onTouchMove(event: TouchEvent): void {
  if (startY.value === null || startX.value === null || event.touches.length !== 1) return;
  if (window.scrollY > 1) {
    reset();
    return;
  }
  const touch = event.touches[0];
  const dy = touch.clientY - startY.value;
  const dx = touch.clientX - startX.value;
  if (dy <= 0 || Math.abs(dx) > dy * 1.2) return;
  const resisted = dy <= THRESHOLD ? dy : THRESHOLD + (dy - THRESHOLD) * 0.45;
  distance.value = Math.min(112, resisted);
  if (event.cancelable) event.preventDefault();
}

async function finishPull(): Promise<void> {
  startY.value = null;
  startX.value = null;
  if (distance.value < THRESHOLD || refreshing.value) {
    distance.value = 0;
    return;
  }
  const scrollY = window.scrollY;
  refreshing.value = true;
  distance.value = THRESHOLD;
  const startedAt = Date.now();
  const ok = await connection.refreshSnapshot();
  const elapsed = Date.now() - startedAt;
  if (elapsed < 420) await new Promise((resolve) => window.setTimeout(resolve, 420 - elapsed));
  await nextTick();
  window.scrollTo({ top: scrollY });
  if (ok) impactLight();
  else notifyError();
  refreshing.value = false;
  distance.value = 0;
}

function onTouchEnd(): void {
  if (startY.value === null) return;
  if (refreshing.value) return;
  void finishPull();
}

function onTouchCancel(): void {
  if (!refreshing.value) reset();
}

watch(
  () => props.enabled,
  (enabled) => {
    if (!enabled) reset();
  },
);

watch(
  () => props.resetKey,
  () => {
    if (!refreshing.value) reset();
  },
);

onMounted(() => {
  window.addEventListener('touchstart', onTouchStart, { passive: true });
  window.addEventListener('touchmove', onTouchMove, { passive: false });
  window.addEventListener('touchend', onTouchEnd, { passive: true });
  window.addEventListener('touchcancel', onTouchCancel, { passive: true });
});

onBeforeUnmount(() => {
  window.removeEventListener('touchstart', onTouchStart);
  window.removeEventListener('touchmove', onTouchMove);
  window.removeEventListener('touchend', onTouchEnd);
  window.removeEventListener('touchcancel', onTouchCancel);
});
</script>

<template>
  <slot />
  <Teleport to="body">
    <div class="ptr" :style="indicatorStyle" aria-live="polite">
      <span class="ptr__icon" :class="{ 'ptr__icon--spin': refreshing }" :style="spinnerStyle">
        ↓
      </span>
      <span>{{ label }}</span>
    </div>
  </Teleport>
</template>

<style scoped>
.ptr {
  position: fixed;
  top: calc(56px + env(safe-area-inset-top));
  left: 50%;
  z-index: 35;
  display: flex;
  align-items: center;
  gap: 7px;
  padding: 7px 12px;
  border: 1px solid var(--border);
  border-radius: 999px;
  background: var(--surface-3);
  box-shadow: var(--shadow);
  color: var(--muted);
  font-size: 12px;
  pointer-events: none;
  transition: opacity 0.16s ease;
}

.ptr__icon {
  display: inline-flex;
  color: var(--accent-strong);
  font-size: 15px;
  line-height: 1;
}

.ptr__icon--spin {
  animation: ptr-spin 0.7s linear infinite;
}

@keyframes ptr-spin {
  to {
    transform: rotate(360deg);
  }
}

@media (prefers-reduced-motion: reduce) {
  .ptr__icon--spin {
    animation: none;
  }
}
</style>
