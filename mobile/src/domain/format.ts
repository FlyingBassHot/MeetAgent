export function formatRelative(iso: string | undefined, now: number = Date.now()): string {
  if (!iso) return '未知时间';
  const timestamp = Date.parse(iso);
  if (Number.isNaN(timestamp)) return '未知时间';
  const diff = Math.max(0, now - timestamp);
  const minute = 60_000;
  const hour = 60 * minute;
  const day = 24 * hour;
  if (diff < 30_000) return '刚刚';
  if (diff < hour) return `${Math.floor(diff / minute)} 分钟前`;
  if (diff < day) return `${Math.floor(diff / hour)} 小时前`;
  if (diff < 7 * day) return `${Math.floor(diff / day)} 天前`;
  return new Date(timestamp).toLocaleDateString('zh-CN', {
    month: '2-digit',
    day: '2-digit',
  });
}

export function formatClock(iso: string | undefined): string {
  if (!iso) return '--:--';
  const date = new Date(iso);
  if (Number.isNaN(date.getTime())) return '--:--';
  return date.toLocaleTimeString('zh-CN', {
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
    hour12: false,
  });
}

export function formatDate(iso: string | undefined): string {
  if (!iso) return '—';
  const date = new Date(iso);
  if (Number.isNaN(date.getTime())) return '—';
  return date.toLocaleString('zh-CN', {
    year: 'numeric',
    month: '2-digit',
    day: '2-digit',
    hour: '2-digit',
    minute: '2-digit',
    hour12: false,
  });
}

export function formatRtt(rttMs: number | null): string {
  return rttMs === null ? '—' : `${Math.round(rttMs)} ms`;
}

export function formatCountdown(iso: string | undefined, now: number = Date.now()): string {
  if (!iso) return '未设置过期时间';
  const timestamp = Date.parse(iso);
  if (Number.isNaN(timestamp)) return '未知时间';
  const diff = timestamp - now;
  if (diff <= 0) return '已过期';
  const minute = 60_000;
  const hour = 60 * minute;
  const day = 24 * hour;
  if (diff < minute) return '即将过期';
  if (diff < hour) return `${Math.ceil(diff / minute)} 分钟后`;
  if (diff < day) return `${Math.ceil(diff / hour)} 小时后`;
  return formatDate(iso);
}

export function truncate(text: string, max = 80): string {
  const compact = text.replace(/\s+/g, ' ').trim();
  return compact.length > max ? `${compact.slice(0, max)}…` : compact;
}

export function shortId(id: string | undefined, prefix = 6): string {
  if (!id) return '—';
  return id.length > prefix + 1 ? `…${id.slice(-prefix)}` : id;
}
