/* Phase 3 E2E #4：生成中断线 → 重连补发 / 快照恢复。 */
const { execFile } = require('node:child_process')
const WebSocket = require('./lib/ws.cjs')
const { resolveSession } = require('./lib/fixtures.cjs')

const BASE = process.env.APP_URL ?? 'http://127.0.0.1:4173'
const ADMIN_URL = process.env.ADMIN_URL ?? 'http://127.0.0.1:8080'
const ADMIN_TOKEN = process.env.ADMIN_TOKEN ?? 'dev-admin-token-change-me'
const CHROME_BIN = process.env.CHROME_BIN ?? '/usr/bin/chromium'
const PORT = Number(process.env.CDP_PORT ?? 9234)
const SESSION = resolveSession()
const SESSION_TITLE = SESSION.title
const SESSION_ID = SESSION.id
const PROMPT = process.env.E2E_LONG_PROMPT ?? '请立即执行 shell 命令 sleep 25，命令结束后只回复 done。'

if (!SESSION_ID) {
  console.error(`no E2E session found (${SESSION.stateFile}); run \`node scripts/e2e/prepare.mjs\` first.`)
  process.exit(2)
}

const chrome = execFile(CHROME_BIN, ['--headless=new', '--disable-gpu', '--no-sandbox',
  '--disable-dev-shm-usage', `--remote-debugging-port=${PORT}`, '--window-size=1440,1000', `${BASE}/#/console`], () => {})
const sleep = (ms) => new Promise((r) => setTimeout(r, ms))
const checks = []
const check = (name, ok, detail = '') => {
  checks.push({ name, ok: Boolean(ok) })
  console.log(`${ok ? 'PASS' : 'FAIL'}  ${name}${detail ? ` — ${detail}` : ''}`)
}
const adminSnapshot = async () =>
  (await fetch(`${ADMIN_URL}/api/v1/snapshot`, {
    headers: { Authorization: `Bearer ${ADMIN_TOKEN}` },
  })).json()

;(async () => {
  let page
  for (let i = 0; i < 80 && !page; i += 1) {
    try { page = (await (await fetch(`http://127.0.0.1:${PORT}/json/list`)).json()).find((t) => t.type === 'page') } catch {}
    await sleep(250)
  }
  const ws = await new Promise((res, rej) => {
    const w = new WebSocket(page.webSocketDebuggerUrl, { perMessageDeflate: false })
    let id = 0; const pending = new Map()
    w.on('open', () => res({ send: (m, p = {}) => new Promise((r2, j2) => { id += 1; pending.set(id, { r2, j2 }); w.send(JSON.stringify({ id, method: m, params: p })) }) }))
    w.on('message', (raw) => { const m = JSON.parse(String(raw)); if (m.id && pending.has(m.id)) { const p = pending.get(m.id); pending.delete(m.id); m.error ? p.j2(new Error(m.error.message)) : p.r2(m.result) } })
    w.on('error', rej)
  })
  const { send } = ws
  await send('Runtime.enable')
  await sleep(1200)
  const ev = async (e) => (await send('Runtime.evaluate', { expression: e, returnByValue: true, awaitPromise: true }))?.result?.value
  const clickButton = (text) => `(() => { const b = [...document.querySelectorAll('button')].find(x => x.textContent.trim() === ${JSON.stringify(text)} || x.textContent.trim().includes(${JSON.stringify(text)})); if (!b) return 'not-found'; if (b.disabled) return 'disabled'; b.click(); return 'clicked' })()`
  const pickAction = (label) => `(() => { const b = [...document.querySelectorAll('.action-option')].find(x => x.textContent.includes(${JSON.stringify(label)})); if (!b) return 'not-found'; b.click(); return 'clicked' })()`

  await ev(clickButton('连接 Cloud'))
  await sleep(2500)
  check('connected', (await ev('document.body.innerText')).includes('断开连接'))
  await ev(`location.hash = '#/sessions'`)
  await sleep(1200)
  await ev(`(() => { const el = document.querySelector('.node-card'); el && el.click(); return 'ok' })()`)
  await sleep(400)
  await ev(`(() => { const el = [...document.querySelectorAll('.agent-row')].find(x => x.textContent.includes('OpenCode Main')); el && el.click(); return 'ok' })()`)
  await sleep(400)
  await ev(pickAction('List sessions'))
  await sleep(200)
  await ev(clickButton('发送命令'))
  await sleep(2500)
  const picked = await ev(`(() => { const li = [...document.querySelectorAll('.session-list li')].find(x => x.textContent.includes(${JSON.stringify(SESSION_TITLE)})); if (!li) return 'not-found'; li.click(); return 'clicked' })()`)
  check('session selected', picked === 'clicked', String(picked))

  const before = await adminSnapshot()
  const beforeSeq = before.last_sequences?.['node-001'] ?? 0
  const beforeMine = before.events.filter((e) => e.session_id === SESSION_ID).length

  await ev(pickAction('Send prompt'))
  await sleep(300)
  await ev(`(() => { const ta = document.querySelector('textarea[placeholder="输入发送给所选 Session 的 Prompt"]'); const setter = Object.getOwnPropertyDescriptor(HTMLTextAreaElement.prototype, 'value').set; setter.call(ta, ${JSON.stringify(PROMPT)}); ta.dispatchEvent(new Event('input', { bubbles: true })); return 'ok' })()`)
  await sleep(200)
  await ev(clickButton('发送命令'))
  await sleep(3000)

  // 断线（浏览器主动断开）
  await ev(`location.hash = '#/console'`)
  await sleep(800)
  const disconnected = await ev(clickButton('断开连接'))
  check('device disconnected mid-generation', disconnected === 'clicked', String(disconnected))
  await sleep(2000)
  const offlineText = await ev('document.body.innerText')
  check('UI shows offline while disconnected', /离线/.test(offlineText))

  // 离线期间 Cloud 继续积累事件
  await sleep(6000)
  const during = await adminSnapshot()
  const duringSeq = during.last_sequences?.['node-001'] ?? 0
  const duringMine = during.events.filter((e) => e.session_id === SESSION_ID).length
  check('events accumulated while device offline', duringSeq > beforeSeq && duringMine >= beforeMine,
    `seq ${beforeSeq} → ${duringSeq} · sessionEvents ${beforeMine} → ${duringMine}`)

  // 重连
  const reconnected = await ev(clickButton('连接 Cloud'))
  await sleep(4000)
  const onlineText = await ev('document.body.innerText')
  check('reconnected', reconnected === 'clicked' && onlineText.includes('断开连接'))

  await ev(`location.hash = '#/events'`)
  await sleep(3000)
  const rows = await ev(`document.querySelectorAll('.event-row').length`)
  const eventsText = await ev('document.body.innerText')
  const after = await adminSnapshot()
  const afterSeq = after.last_sequences?.['node-001'] ?? 0

  check('replay filled the gap after reconnect', rows > 0 && afterSeq >= duringSeq,
    `rows=${rows} · seq now ${afterSeq}`)
  const displayed = await ev(`[...document.querySelectorAll('.event-seq')].map(x => Number(String(x.textContent).replace('#','')))`)
  const offlineSeqs = after.events
    .filter((e) => e.sequence > beforeSeq && e.sequence <= duringSeq)
    .map((e) => e.sequence)
  const missing = offlineSeqs.filter((seq) => !displayed.includes(seq))
  check('every offline-window event replayed into UI', offlineSeqs.length > 0 && missing.length === 0,
    `offlineSeqs=${JSON.stringify(offlineSeqs)} missing=${JSON.stringify(missing)} rows=${rows}`)

  // 在途请求不应该无限悬挂
  await ev(`location.hash = '#/console'`)
  await sleep(1500)
  const pendingText = await ev(`document.querySelector('.page-stack')?.innerText ?? ''`)
  const pendingCount = await ev(`document.querySelectorAll('.pending-list li').length`)
  check('no permanently stuck in-flight request after reconnect', pendingCount <= 1,
    `pending=${pendingCount} · ${pendingText.split('\n').filter((l) => /在途|timeout|failed|pending/.test(l)).slice(0, 3).join(' | ')}`)

  const failed = checks.filter((c) => !c.ok).length
  console.log(`\n${checks.length - failed}/${checks.length} checks passed`)
  chrome.kill('SIGTERM')
  process.exit(failed ? 1 : 0)
})().catch((e) => { console.error('e2e-reconnect failed:', e); chrome.kill('SIGTERM'); process.exit(2) })
