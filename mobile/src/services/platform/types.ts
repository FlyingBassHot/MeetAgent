export type PlatformKind = 'browser' | 'capacitor';

export interface PlatformAdapter {
  readonly kind: PlatformKind;
  readonly notificationSupported: boolean;
  requestNotificationPermission(): Promise<boolean>;
  notify(title: string, body: string): void;
}
