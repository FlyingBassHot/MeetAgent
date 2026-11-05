export type NavIconName = 'home' | 'sessions' | 'activity' | 'settings';

export interface NavItem {
  name: string;
  label: string;
  icon: NavIconName;
}

export const NAV_ITEMS: NavItem[] = [
  { name: 'home', label: 'Home', icon: 'home' },
  { name: 'sessions', label: 'Sessions', icon: 'sessions' },
  { name: 'activity', label: 'Activity', icon: 'activity' },
  { name: 'settings', label: 'Settings', icon: 'settings' },
];
