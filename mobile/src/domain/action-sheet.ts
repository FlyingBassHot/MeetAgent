export interface ActionSheetAction {
  key: string;
  label: string;
  tone?: 'default' | 'danger';
  disabled?: boolean;
  description?: string;
}
