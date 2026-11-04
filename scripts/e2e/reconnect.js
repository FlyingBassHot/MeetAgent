/* Phase 3 E2E #5：生成中断线 → 重连后快照/补发恢复。 */
const { launch, connect, selectSession, sendPrompt, adminSnapshot, sleep } = require('./lib/ui.cjs')
const { resolveSession } = require('./lib/fixtures.cjs')

const APP_URL = process.env.APP_URL ?? 'http://127.0.0.1:4173'
const ADMIN_URL = process.env.ADMIN_URL ?? 'http://127.0.0.1:8080'
const ADMIN_TOKEN = process.env.ADMIN_TOKEN ?? 'dev-admin-token-change-me'
const CHROME_BIN = process.env.CHROME_BIN ?? '/usr/bin/chromium'
const PORT = Number(process.env.CDP_PORT ?? 9234)
const DEVICE_ID = process.env.E2E_DEVICE_ID ?? 'web-002'
const DEVICE_TOKEN = process.env.E2E_DEVICE_TOKEN ?? 'dev-device-002-token-change-me'
const WS_URL = process.env.E2E_WS_URL ?? ''
const PROMPT = process.env.E2E_LONG_PROMPT ?? '请立即执行 shell 命令 sleep 25，命令结束后只回复 done。'
const SESSION = resolveSession()

const checks = []
const check = (name, ok, detail = '') => {
  checks.push({ name, ok: Boolean(ok) })
  console.log(`${ok ? 'PASS' : 'FAIL'}  ${name}${detail ? ` — ${detail}` : ''}`)
}

function lastSequence(snapshot) {
  return snapshot.last_sequences?.['node-001'] ??
    Math.max(0, ...(snapshot.events ?? []).map((event) => event.sequence ?? 0))
}

function sessionEvents(snapshot) {
  return (snapshot.events ?? []).filter((event) => event.session_id === SESSION.id)
}

;(async () => {
  if (!SESSION.id) throw new Error('no E2E session; run `node scripts/e2e/prepare.mjs` first')
  const page = await launch({ appUrl: APP_URL, port: PORT, chromeBin: CHROME_BIN })

  try {
    await connect(page, { deviceId: DEVICE_ID, token: DEVICE_TOKEN, url: WS_URL })
    check('connected', true)

    await selectSession(page, SESSION.id)
    check('session selected', true, SESSION.title)

    const before = await adminSnapshot({ adminUrl: ADMIN_URL, adminToken: ADMIN_TOKEN })
    const beforeSeq = lastSequence(before)

    await sendPrompt(page, PROMPT)
    await sleep(3000)

    await page.ev(`location.hash = '#/settings'`)
    await page.waitFor(`!!document.querySelector('[data-testid="connection-disconnect"]')`, 5000)
    const disconnected = await page.click('[data-testid="connection-disconnect"]')
    check('device disconnected mid-generation', disconnected === 'clicked', String(disconnected))

    await page.waitFor(`!!document.querySelector('[data-testid="connection-gate"]')`, 10000)
    check('gate shown while disconnected', true)

    await sleep(8000)
    const during = await adminSnapshot({ adminUrl: ADMIN_URL, adminToken: ADMIN_TOKEN })
    const duringSeq = lastSequence(during)
    check('events accumulated while device offline', duringSeq > beforeSeq,
      `seq ${beforeSeq} → ${duringSeq}`)

    const reconnected = await page.click('[data-testid="connection-submit"]')
    if (reconnected !== 'clicked') throw new Error(`reconnect click: ${reconnected}`)
    await page.waitFor(`!document.querySelector('[data-testid="connection-gate"]')`, 20000)
    check('reconnected', true)

    await page.ev(`location.hash = '#/events?tab=events&session=${encodeURIComponent(SESSION.id)}'`)
    await page.waitFor(`document.querySelectorAll('[data-testid="event-item"]').length > 0`, 15000)
    const rows = await page.ev(`document.querySelectorAll('[data-testid="event-item"]').length`)
    const after = await adminSnapshot({ adminUrl: ADMIN_URL, adminToken: ADMIN_TOKEN })
    const afterSeq = lastSequence(after)
    check('snapshot/replay filled the gap after reconnect', afterSeq >= duringSeq,
      `rows=${rows} · seq now ${afterSeq}`)

    const displayed = await page.ev(
      `[...document.querySelectorAll('[data-testid="event-item"]')].map((el) => el.getAttribute('data-event-id'))`,
    )
    const offlineEvents = sessionEvents(after)
      .filter((event) => event.sequence > beforeSeq && event.sequence <= duringSeq)
    const missing = offlineEvents.filter((event) => !displayed.includes(event.event_id))
    check('every offline-window session event is present in UI',
      offlineEvents.length > 0 && missing.length === 0,
      `offline=${offlineEvents.length} missing=${missing.length} rows=${rows}`)

    await page.ev(`location.hash = '#/console'`)
    await page.waitFor(`!!document.querySelector('[data-testid="connection-status-bar"]')`, 5000)
    await sleep(500)
    const pending = await page.ev(`document.querySelectorAll('.pending-list li').length`)
    check('no permanently stuck in-flight request after reconnect', pending === 0,
      `pending=${pending}`)

    const failed = checks.filter((item) => !item.ok).length
    console.log(`\n${checks.length - failed}/${checks.length} checks passed`)
    await page.close()
    process.exit(failed ? 1 : 0)
  } catch (error) {
    console.error('e2e-reconnect failed:', error)
    await page.close()
    process.exit(2)
  }
})().catch((error) => {
  console.error('e2e-reconnect failed:', error.message)
  process.exit(2)
})
