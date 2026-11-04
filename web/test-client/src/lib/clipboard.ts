/** 剪贴板（失败静默返回 false，由调用方展示提示）。 */

export async function copyText(text: string): Promise<boolean> {
  try {
    await navigator.clipboard.writeText(text)
    return true
  } catch {
    return false
  }
}
