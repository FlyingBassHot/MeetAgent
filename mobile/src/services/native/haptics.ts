import { Haptics, ImpactStyle, NotificationType } from '@capacitor/haptics';
import { isNativeRuntime } from './runtime';

async function impact(style: ImpactStyle): Promise<void> {
  if (!isNativeRuntime()) return;
  try {
    await Haptics.impact({ style });
  } catch {
    // 触感不可用时静默降级，不阻塞交互。
  }
}

async function notification(type: NotificationType): Promise<void> {
  if (!isNativeRuntime()) return;
  try {
    await Haptics.notification({ type });
  } catch {
    // 同上。
  }
}

export function impactLight(): void {
  void impact(ImpactStyle.Light);
}

export function notifySuccess(): void {
  void notification(NotificationType.Success);
}

export function notifyError(): void {
  void notification(NotificationType.Error);
}
