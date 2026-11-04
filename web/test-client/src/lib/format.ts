/** 展示层与日志共用的格式化工具（纯函数，无副作用）。 */

/** 相对/绝对时间格式化：非法输入原样返回，空值返回占位符。 */
export function formatTime(value?: string): string {
  if (!value) return '—'
  const date = new Date(value)
  if (Number.isNaN(date.getTime())) return value
  return date.toLocaleString('zh-CN', { hour12: false })
}
export function shortId(value: string): string {
  if (!value) return ''
  return value.length > 16 ? `${value.slice(0, 8)}…${value.slice(-4)}` : value
}

/** payload 预览：字符串原样截断；对象 pretty-print 后按 400 字符截断。 */
export function formatPayload(payload: unknown): string {
  if (payload === undefined || payload === null) return ''
  if (typeof payload === 'string') return truncate(payload, 400)
  try {
    return truncate(JSON.stringify(payload, null, 2), 400)
  } catch {
    return truncate(String(payload), 400)
  }
}

/** 日志明细：长 JSON pretty-print，整体按 2000 字符截断。 */
export function summarize(value: unknown): string {
  if (value === undefined || value === null) return ''
  if (typeof value === 'string') return truncate(value, 2000)
  try {
    return truncate(JSON.stringify(value, null, 2), 2000)
  } catch {
    return truncate(String(value), 2000)
  }
}

export function truncate(value: string, max: number): string {
  return value.length > max ? `${value.slice(0, max)}…` : value
}
