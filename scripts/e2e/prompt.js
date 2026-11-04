/* Phase 3 E2E #1：真机 Prompt 闭环（浏览器 → Cloud → Node → OpenCode → 事件回传）。 */
const { launch, connect, selectSession, sendPrompt, waitLastResponse, adminSnapshot, sleep } = require('./lib/ui.cjs')
const { resolveSession } = require('./lib/fixtures.cjs')

const APP_URL = process.env.APP_URL ?? 'http://127.0.0.1:4173'
const ADMIN_URL = process.env.ADMIN_URL ?? 'http://127.0.0.1:8080'
const ADMIN_TOKEN = process.env.ADMIN_TOKEN ?? 'dev-admin-token-change-me'
const CHROME_BIN = process.env.CHROME_BIN ?? '/usr/bin/chromium'
const PORT = Number(process.env.CDP_PORT ?? 9230)
const DEVICE_ID = process.env.E2E_DEVICE_ID ?? 'web-002'
const DEVICE_TOKEN = process.env.E2E_DEVICE_TOKEN ?? 'dev-device-002-token-change-me'
const WS_URL = process.env.E2E_WS_URL ?? ''
const MARKER = process.env.E2E_MARKER ?? `PONG-${Date.now().toString(36).toUpperCase()}`
const PROMPT = process.env.E2E_PROMPT ?? `Reply with exactly one line: ${MARKER}`
const SESSION = resolveSession()

const checks = []
const check = (name, ok, detail = '') => {
  checks.push({ name, ok: Boolean(ok) })
  console.log(`${ok ? 'PASS' : 'FAIL'}  ${name}${detail ? ` — ${detail}` : ''}`)
}

;(async () => {
  if (!SESSION.id) throw new Error('no E2E session; run `node scripts/e2e/prepare.mjs` first')
  const page = await launch({ appUrl: APP_URL, port: PORT, chromeBin: CHROME_BIN })

  try {
    await connect(page, { deviceId: DEVICE_ID, token: DEVICE_TOKEN, url: WS_URL })
    check('connected', true, `${DEVICE_ID} @ ${APP_URL}`)

    await page.ev(`location.hash = '#/sessions'`)
    await page.waitFor(`document.querySelectorAll('[data-testid="session-item"]').length > 0`, 15000)
    check('session list loaded', true)

    await selectSession(page, SESSION.id)
    const detail = await page.text('[data-testid="session-detail"]')
    check('target session selected', detail.includes(SESSION.title), SESSION.title)

    await sendPrompt(page, PROMPT)
    check('prompt entered and sent', true, PROMPT)

    const completed = await waitLastResponse(page, 'send_prompt', 'completed', 20000)
    check('send_prompt completed', completed.includes('completed'), completed.replace(/\n/g, ' | '))

    // 详情默认显示当前 Session 的实时事件；message.delta 聚合文本应包含回复。
    const detailEvents = await page.waitFor(
      `(() => {
        const el = document.querySelector('[data-testid="session-detail"]')
        if (!el) return ''
        const value = el.innerText
        return value.includes(${JSON.stringify(MARKER)}) ? value : ''
      })()`,
      20000,
    )
    check('model reply visible in session events', detailEvents.includes(MARKER), MARKER)
    check('message lifecycle visible in session events',
      ['message.started', 'message.delta', 'message.completed'].every((type) => detailEvents.includes(type)),
      'started / delta / completed')

    await page.ev(`location.hash = '#/events?tab=events&session=${encodeURIComponent(SESSION.id)}'`)
    await page.waitFor(`document.querySelectorAll('[data-testid="event-item"]').length > 0`, 10000)
    const eventsText = await page.text('[data-testid="event-list"], .event-list')
    check('events page shows message events', /message\.(delta|completed)/.test(eventsText),
      `${await page.ev(`document.querySelectorAll('[data-testid="event-item"]').length`)} rows`)

    const snapshot = await adminSnapshot({ adminUrl: ADMIN_URL, adminToken: ADMIN_TOKEN })
    const replies = snapshot.events.filter(
      (event) =>
        event.session_id === SESSION.id &&
        event.event_type === 'message.completed' &&
        String(event.payload?.text ?? '').includes(MARKER),
    )
    check('message.completed carries the reply text', replies.length > 0,
      `matches=${replies.length}`)

    const failed = checks.filter((item) => !item.ok).length
    console.log(`\n${checks.length - failed}/${checks.length} checks passed`)
    await page.close()
    process.exit(failed ? 1 : 0)
  } catch (error) {
    console.error('e2e-prompt failed:', error)
    await page.close()
    process.exit(2)
  }
})().catch((error) => {
  console.error('e2e-prompt failed:', error.message)
  process.exit(2)
})
