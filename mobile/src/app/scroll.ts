const positions = new Map<string, number>();

export function rememberScroll(path: string, top: number): void {
  positions.set(path, top);
}

export function scrollPosition(path: string): number | undefined {
  return positions.get(path);
}

export function restoreScroll(path: string): void {
  window.scrollTo({ top: positions.get(path) ?? 0 });
}
