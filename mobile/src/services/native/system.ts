import { App as CapacitorApp } from '@capacitor/app';
import { Keyboard, KeyboardResize } from '@capacitor/keyboard';
import { StatusBar, Style } from '@capacitor/status-bar';
import type { Router } from 'vue-router';
import { dispatchBack } from './back';
import { isNativeRuntime } from './runtime';

let initialized = false;

function pathName(router: Router): string {
  const name = router.currentRoute.value.name;
  return typeof name === 'string' ? name : '';
}

async function configureStatusBar(): Promise<void> {
  await Promise.allSettled([
    StatusBar.setOverlaysWebView({ overlay: false }),
    StatusBar.setStyle({ style: Style.Light }),
    StatusBar.setBackgroundColor({ color: '#0b0d10' }),
  ]);
}

async function configureKeyboard(): Promise<void> {
  await Keyboard.setResizeMode({ mode: KeyboardResize.Native }).catch(() => undefined);
  let ignoreScrollUntil = 0;
  let hiding = false;

  const onShow = (): void => {
    document.body.classList.add('keyboard-open');
    ignoreScrollUntil = Date.now() + 600;
    window.setTimeout(() => {
      const active = document.activeElement;
      if (active instanceof HTMLTextAreaElement || active instanceof HTMLInputElement) {
        active.scrollIntoView({ block: 'nearest', behavior: 'smooth' });
      }
    }, 80);
  };

  const onHide = (): void => {
    document.body.classList.remove('keyboard-open');
    hiding = false;
  };

  await Keyboard.addListener('keyboardWillShow', onShow);
  await Keyboard.addListener('keyboardDidShow', onShow);
  await Keyboard.addListener('keyboardWillHide', onHide);
  await Keyboard.addListener('keyboardDidHide', onHide);

  window.addEventListener(
    'scroll',
    () => {
      if (!document.body.classList.contains('keyboard-open')) return;
      if (hiding || Date.now() < ignoreScrollUntil) return;
      hiding = true;
      void Keyboard.hide().catch(() => {
        hiding = false;
      });
    },
    { passive: true },
  );
}

async function configureBackButton(router: Router): Promise<void> {
  await CapacitorApp.addListener('backButton', () => {
    if (dispatchBack()) return;
    const name = pathName(router);
    if (name === 'session-detail' || name === 'settings-developer') {
      router.back();
      return;
    }
    void CapacitorApp.exitApp();
  });
}

export async function initNativeSystem(router: Router): Promise<void> {
  if (initialized || !isNativeRuntime()) return;
  initialized = true;
  await Promise.allSettled([configureStatusBar(), configureKeyboard(), configureBackButton(router)]);
}
