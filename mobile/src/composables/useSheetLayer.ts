import { onBeforeUnmount, watch } from 'vue';
import { pushBackHandler } from '@/services/native/back';

let lockCount = 0;
let originalOverflow = '';

function lockBody(): void {
  if (lockCount === 0) {
    originalOverflow = document.body.style.overflow;
    document.body.style.overflow = 'hidden';
    document.body.classList.add('sheet-open');
  }
  lockCount += 1;
}

function unlockBody(): void {
  if (lockCount === 0) return;
  lockCount -= 1;
  if (lockCount > 0) return;
  document.body.style.overflow = originalOverflow;
  document.body.classList.remove('sheet-open');
}

export function useSheetLayer(isOpen: () => boolean, close: () => void): void {
  let releaseBack: (() => void) | null = null;
  let bodyLocked = false;

  function activate(): void {
    if (bodyLocked) return;
    bodyLocked = true;
    lockBody();
    releaseBack = pushBackHandler(close);
  }

  function deactivate(): void {
    if (!bodyLocked) return;
    bodyLocked = false;
    unlockBody();
    releaseBack?.();
    releaseBack = null;
  }

  watch(
    isOpen,
    (open) => {
      if (open) activate();
      else deactivate();
    },
    { immediate: true },
  );

  onBeforeUnmount(deactivate);
}
