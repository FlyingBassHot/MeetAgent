/* Phase 3 E2E #2：真机 Interrupt（长任务中途打断）。 */
const { execFile } = require('node:child_process')
const WebSocket = require('./lib/ws.cjs')
const { resolveSession } = require('./lib/fixtures.cjs')

const BASE = process.env.APP_URL ?? 'http://127.0.0.1:4173'
const ADMIN_URL = process.env.ADMIN_URL ?? 'http://127.0.0.1:8080'
const ADMIN_TOKEN = process.env.ADMIN_TOKEN ?? 'dev-admin-token-change-me'
const CHROME_BIN = process.env.CHROME_BIN ?? '/usr/bin/chromium'
const PORT = Number(process.env.CDP_PORT ?? 9232)
const SESSION = resolveSession()
const SESSION_TITLE = SESSION.title
const SESSION_ID = SESSION.id
const LONG_PROMPT = process.env.E2E_LONG_PROMPT ?? '请立即执行 shell 命令 sleep 30，命令结束后只回复 done。'

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
  // Refresh sessions through the Node so a session created after the Node connected shows up.
  await ev(pickAction('List sessions'))
  await sleep(200)
  await ev(clickButton('发送命令'))
  await sleep(2500)
  const picked = await ev(`(() => { const li = [...document.querySelectorAll('.session-list li')].find(x => x.textContent.includes(${JSON.stringify(SESSION_TITLE)})); if (!li) return 'not-found'; li.click(); return 'clicked' })()`)
  check('session selected', picked === 'clicked', String(picked))

  // 发送长任务
  await ev(pickAction('Send prompt'))
  await sleep(300)
  await ev(`(() => { const ta = document.querySelector('textarea[placeholder="输入发送给所选 Session 的 Prompt"]'); const setter = Object.getOwnPropertyDescriptor(HTMLTextAreaElement.prototype, 'value').set; setter.call(ta, ${JSON.stringify(LONG_PROMPT)}); ta.dispatchEvent(new Event('input', { bubbles: true })); return 'ok' })()`)
  await sleep(200)
  await ev(clickButton('发送命令'))
  await sleep(5000)

  // 确认正在生成
  await ev(`location.hash = '#/events'`)
  await sleep(1500)
  const before = await ev(`document.querySelectorAll('.event-row').length`)
  const runningText = await ev(`document.body.innerText`)
  const busy = /running|busy|executing/.test(runningText)
  check('generation running before interrupt', before > 0, `rows=${before} busySignal=${busy}`)

  // 发送 interrupt
  await ev(`location.hash = '#/sessions'`)
  await sleep(800)
  await ev(pickAction('Interrupt'))
  await sleep(300)
  await ev(clickButton('发送命令'))
  await sleep(4000)

  await ev(`location.hash = '#/activity'`)
  await sleep(1200)
  const activity = await ev('document.body.innerText')
  const interruptLogged = /interrupt/.test(activity)
  const interrupted = /interrupted/.test(activity)
  check('interrupt command logged', interruptLogged)
  check('interrupt response reports interrupted=true', /"interrupted": true/.test(activity),
    JSON.stringify(activity.split('\n').filter((l) => /interrupt|interrupted/.test(l)).slice(-6)))

  // 会话回到 idle
  await ev(`location.hash = '#/events'`)
  await sleep(2500)
  const eventsText = await ev('document.body.innerText')
  const idleAfter = /session\.status_changed/.test(eventsText) && /idle/.test(eventsText)
  check('session returns to idle after interrupt', idleAfter)
  const snap = await (await fetch(`${ADMIN_URL}/api/v1/snapshot`, {
    headers: { Authorization: `Bearer ${ADMIN_TOKEN}` },
  })).json()
  const mine = snap.events.filter((e) => e.session_id === SESSION_ID)
  const statusEvents = mine.filter((e) => e.event_type === 'session.status_changed')
  const lastStatus = statusEvents[statusEvents.length - 1]
  const after = mine.filter((e) => e.sequence > (lastStatus?.sequence ?? 0))
  check('no events for this session after idle', lastStatus?.payload?.status === 'idle' && after.length === 0,
    `lastStatus=${lastStatus?.payload?.status} seq=${lastStatus?.sequence} after=${after.length}`)

  const failed = checks.filter((c) => !c.ok).length
  console.log(`\n${checks.length - failed}/${checks.length} checks passed`)
  chrome.kill('SIGTERM')
  process.exit(failed ? 1 : 0)
})().catch((e) => { console.error('e2e-interrupt failed:', e); chrome.kill('SIGTERM'); process.exit(2) })
