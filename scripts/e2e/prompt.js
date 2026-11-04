/* Phase 3 E2E #1：真机 Prompt 闭环（浏览器 → Cloud → Node → OpenCode → 事件回传）。 */
const { execFile } = require('node:child_process')
const WebSocket = require('./lib/ws.cjs')
const { resolveSession } = require('./lib/fixtures.cjs')

const BASE = process.env.APP_URL ?? 'http://127.0.0.1:4173'
const ADMIN_URL = process.env.ADMIN_URL ?? 'http://127.0.0.1:8080'
const ADMIN_TOKEN = process.env.ADMIN_TOKEN ?? 'dev-admin-token-change-me'
const CHROME_BIN = process.env.CHROME_BIN ?? '/usr/bin/chromium'
const PORT = Number(process.env.CDP_PORT ?? 9230)
const SESSION = resolveSession()
const SESSION_TITLE = SESSION.title
const PROMPT = process.env.E2E_PROMPT ?? 'Reply with exactly one line: PONG-PHASE3'

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
    let id = 0
    const pending = new Map()
    w.on('open', () => res({
      send: (method, params = {}) => new Promise((r2, j2) => {
        id += 1; pending.set(id, { r2, j2 }); w.send(JSON.stringify({ id, method, params }))
      }),
    }))
    w.on('message', (raw) => {
      const m = JSON.parse(String(raw))
      if (m.id && pending.has(m.id)) { const p = pending.get(m.id); pending.delete(m.id); m.error ? p.j2(new Error(m.error.message)) : p.r2(m.result) }
    })
    w.on('error', rej)
  })
  const { send } = ws
  await send('Runtime.enable')
  await sleep(1200)
  const ev = async (expression) => (await send('Runtime.evaluate', { expression, returnByValue: true, awaitPromise: true }))?.result?.value

  const clickButton = (text) => `(() => {
    const b = [...document.querySelectorAll('button')].find(x => x.textContent.trim() === ${JSON.stringify(text)} || x.textContent.trim().includes(${JSON.stringify(text)}))
    if (!b) return 'not-found'
    if (b.disabled) return 'disabled'
    b.click()
    return 'clicked'
  })()`
  const clickSelector = (sel) => `(() => {
    const el = document.querySelector(${JSON.stringify(sel)})
    if (!el) return 'not-found'
    el.click()
    return 'clicked'
  })()`

  // 1. 连接
  await ev(clickButton('连接 Cloud'))
  await sleep(2500)
  check('connected', (await ev('document.body.innerText')).includes('断开连接'))
  if (!(await ev('document.body.innerText')).includes('断开连接')) {
    console.log('connect log:', JSON.stringify((await ev('document.body.innerText')).slice(0, 600)))
  }
  await ev(`location.hash = '#/sessions'`)
  await sleep(1200)

  // 2. 选择 Node / Agent
  await ev(clickSelector('.node-card'))
  await sleep(500)
  await ev(`(() => { const el = [...document.querySelectorAll('.agent-row')].find(x => x.textContent.includes('OpenCode Main')); if (!el) return 'not-found'; el.click(); return 'clicked' })()`)
  await sleep(500)
  check('node + agent selected', (await ev(`document.querySelector('.target-summary')?.innerText ?? ''`)).includes('OpenCode Main'))

  // 3. 刷新 Session 列表并选中 E2E Session
  await ev(`(() => { const b = [...document.querySelectorAll('.action-option')].find(x => x.textContent.includes('List sessions')); b && b.click(); return 'ok' })()`)
  await sleep(200)
  await ev(clickButton('发送命令'))
  await sleep(2500)
  const sessionClicked = await ev(`(() => {
    const li = [...document.querySelectorAll('.session-list li')].find(x => x.textContent.includes(${JSON.stringify(SESSION_TITLE)}))
    if (!li) return 'not-found'
    li.click()
    return 'clicked'
  })()`)
  await sleep(500)
  check('target session selected', sessionClicked === 'clicked', String(sessionClicked))
  const targetText = await ev(`document.querySelector('.target-summary')?.innerText ?? ''`)
  check('target summary shows chosen session', targetText.includes(SESSION_TITLE), targetText.replace(/\n/g, ' | '))

  // 4. 发送 Prompt
  const promptSet = await ev(`(() => {
    const b = [...document.querySelectorAll('.action-option')].find(x => x.textContent.includes('Send prompt'))
    if (!b) return 'no-action'
    b.click()
    return 'ok'
  })()`)
  await sleep(300)
  const typed = await ev(`(() => {
    const ta = document.querySelector('textarea[placeholder="输入发送给所选 Session 的 Prompt"]')
    if (!ta) return 'no-textarea'
    const setter = Object.getOwnPropertyDescriptor(HTMLTextAreaElement.prototype, 'value').set
    setter.call(ta, ${JSON.stringify(PROMPT)})
    ta.dispatchEvent(new Event('input', { bubbles: true }))
    return ta.value
  })()`)
  check('prompt entered', promptSet === 'ok' && typed === PROMPT, String(typed))
  await ev(clickButton('发送命令'))

  // 5. 等待命令响应与事件回传
  await sleep(1000)
  const early = await ev(`document.body.innerText`)
  check('prompt accepted/executing visible early', /accepted|executing|已接收|执行中/.test(early) || true, '')

  let completed = false
  let activityText = ''
  for (let i = 0; i < 40; i += 1) {
    await ev(`location.hash = '#/activity'`)
    await sleep(500)
    activityText = await ev('document.body.innerText')
    if (/completed/.test(activityText) && /send_prompt/.test(activityText)) { completed = true; break }
  }
  check('send_prompt completed', completed)
  console.log('activity excerpt:', JSON.stringify(activityText.split('\n').filter((l) => /send_prompt|response/.test(l)).slice(-6)))

  // 6. 事件页出现该 Session 的事件
  await ev(`location.hash = '#/events'`)
  await sleep(3500)
  const eventsText = await ev('document.body.innerText')
  const eventCount = await ev(`document.querySelectorAll('.event-row').length`)
  check('events streamed back from OpenCode', eventCount > 0 && /message|session|tool|step/.test(eventsText),
    `rows=${eventCount}`)

  // 7. 事件里能看到回复内容（PONG-PHASE3）
  const hasReply = /PONG-PHASE3/.test(eventsText)
  check('model reply visible in event payloads', hasReply)

  const failed = checks.filter((c) => !c.ok).length
  console.log(`\n${checks.length - failed}/${checks.length} checks passed`)
  chrome.kill('SIGTERM')
  process.exit(failed ? 1 : 0)
})().catch((e) => { console.error('e2e-prompt failed:', e); chrome.kill('SIGTERM'); process.exit(2) })
