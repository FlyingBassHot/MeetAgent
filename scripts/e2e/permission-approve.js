/* Phase 3 E2E #3：真机权限审批（permission.asked → 浏览器批准 → 工具执行）。 */
const { execFile } = require('node:child_process')
const WebSocket = require('./lib/ws.cjs')
const { resolveSession } = require('./lib/fixtures.cjs')
const { mkdirSync, writeFileSync } = require('node:fs')
const { tmpdir } = require('node:os')
const { dirname, join } = require('node:path')

const BASE = process.env.APP_URL ?? 'http://127.0.0.1:4173'
const ADMIN_URL = process.env.ADMIN_URL ?? 'http://127.0.0.1:8080'
const ADMIN_TOKEN = process.env.ADMIN_TOKEN ?? 'dev-admin-token-change-me'
const CHROME_BIN = process.env.CHROME_BIN ?? '/usr/bin/chromium'
const PORT = Number(process.env.CDP_PORT ?? 9233)
const SESSION = resolveSession()
const SESSION_TITLE = SESSION.title
const SESSION_ID = SESSION.id

// A unique *.env path forces a real read on every run, so a previous
// approval in the same session cannot short-circuit the permission prompt.
const E2E_FILE = process.env.E2E_FILE ?? join(tmpdir(), 'opencode', 'meetagent-e2e', `approve-${process.pid}-${Date.now()}.env`)
mkdirSync(dirname(E2E_FILE), { recursive: true })
writeFileSync(E2E_FILE, 'E2E_PERMISSION_PROBE=1\n')
const PROMPT = `请读取文件 ${E2E_FILE}，并只回复文件中的原始内容，不要解释。`

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
  await ev(pickAction('List sessions'))
  await sleep(200)
  await ev(clickButton('发送命令'))
  await sleep(2500)
  const picked = await ev(`(() => { const li = [...document.querySelectorAll('.session-list li')].find(x => x.textContent.includes(${JSON.stringify(SESSION_TITLE)})); if (!li) return 'not-found'; li.click(); return 'clicked' })()`)
  check('session selected', picked === 'clicked', String(picked))

  await ev(pickAction('Send prompt'))
  await sleep(300)
  await ev(`(() => { const ta = document.querySelector('textarea[placeholder="输入发送给所选 Session 的 Prompt"]'); const setter = Object.getOwnPropertyDescriptor(HTMLTextAreaElement.prototype, 'value').set; setter.call(ta, ${JSON.stringify(PROMPT)}); ta.dispatchEvent(new Event('input', { bubbles: true })); return 'ok' })()`)
  await sleep(200)
  await ev(clickButton('发送命令'))

  // 等待权限请求出现在「待审批权限」卡片
  await ev(`location.hash = '#/console'`)
  let permissionText = ''
  let appeared = false
  for (let i = 0; i < 60; i += 1) {
    await sleep(1000)
    const state = await ev(`(() => {
      const list = document.querySelector('.permission-list')
      const count = document.querySelectorAll('.permission-list li').length
      return { count, text: list ? list.innerText : '' }
    })()`)
    if (state?.count > 0) { appeared = true; permissionText = state.text; break }
  }
  check('permission request surfaced in UI', appeared, permissionText.replace(/\n/g, ' | ').slice(0, 200))

  // 批准一次
  const approved = await ev(clickButton('允许一次'))
  check('approve button clicked', approved === 'clicked', String(approved))
  await sleep(2500)
  const remaining = await ev(`document.querySelectorAll('.permission-list li').length`)
  check('permission cleared from panel', remaining === 0, `remaining=${remaining}`)

  // 活动日志记录 permission_response
  await ev(`location.hash = '#/activity'`)
  await sleep(1500)
  const activity = await ev('document.body.innerText')
  check('permission_response logged', /permission_response @/.test(activity),
    JSON.stringify(activity.split('\n').filter((l) => /permission|response/.test(l)).slice(-5)))

  // 工具执行后回传文件内容
  await ev(`location.hash = '#/events'`)
  let seen = false
  let rows = 0
  for (let i = 0; i < 40; i += 1) {
    await sleep(1000)
    const text = await ev(`document.body.innerText`)
    rows = await ev(`document.querySelectorAll('.event-row').length`)
    if (/E2E_PERMISSION_PROBE/.test(text)) { seen = true; break }
  }
  check('approved read actually ran and content returned', seen, `rows=${rows}`)

  const failed = checks.filter((c) => !c.ok).length
  console.log(`\n${checks.length - failed}/${checks.length} checks passed`)
  chrome.kill('SIGTERM')
  process.exit(failed ? 1 : 0)
})().catch((e) => { console.error('e2e-permission failed:', e); chrome.kill('SIGTERM'); process.exit(2) })
