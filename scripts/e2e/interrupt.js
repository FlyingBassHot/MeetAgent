/* Phase 3 E2E #2：真机 Interrupt（长任务中途打断）。 */
const { launch, connect, selectSession, sendPrompt, waitLastResponse, adminSnapshot, sleep } = require('./lib/ui.cjs')
const { resolveSession } = require('./lib/fixtures.cjs')

const APP_URL = process.env.APP_URL ?? 'http://127.0.0.1:4173'
const ADMIN_URL = process.env.ADMIN_URL ?? 'http://127.0.0.1:8080'
const ADMIN_TOKEN = process.env.ADMIN_TOKEN ?? 'dev-admin-token-change-me'
const CHROME_BIN = process.env.CHROME_BIN ?? '/usr/bin/chromium'
const PORT = Number(process.env.CDP_PORT ?? 9232)
const DEVICE_ID = process.env.E2E_DEVICE_ID ?? 'web-002'
const DEVICE_TOKEN = process.env.E2E_DEVICE_TOKEN ?? 'dev-device-002-token-change-me'
const WS_URL = process.env.E2E_WS_URL ?? ''
const LONG_PROMPT = process.env.E2E_LONG_PROMPT ?? '请立即执行 shell 命令 sleep 30，命令结束后只回复 done。'
const SESSION = resolveSession()

const checks = []
const check = (name, ok, detail = '') => {
  checks.push({ name, ok: Boolean(ok) })
  console.log(`${ok ? 'PASS' : 'FAIL'}  ${name}${detail ? ` — ${detail}` : ''}`)
}

function statusOf(snapshot) {
  for (const node of snapshot.nodes ?? []) {
    for (const agent of node.agents ?? []) {
      const session = (agent.sessions ?? []).find((item) => item.session_id === SESSION.id)
      if (session) return session.status
    }
  }
  return ''
}

async function waitStatus(expected, timeoutMs = 20000) {
  const deadline = Date.now() + timeoutMs
  let status = ''
  while (Date.now() < deadline) {
    status = statusOf(await adminSnapshot({ adminUrl: ADMIN_URL, adminToken: ADMIN_TOKEN }))
    if (status === expected) return status
    await sleep(500)
  }
  throw new Error(`session status ${status || 'unknown'}, want ${expected}`)
}

;(async () => {
  if (!SESSION.id) throw new Error('no E2E session; run `node scripts/e2e/prepare.mjs` first')
  const page = await launch({ appUrl: APP_URL, port: PORT, chromeBin: CHROME_BIN })

  try {
    await connect(page, { deviceId: DEVICE_ID, token: DEVICE_TOKEN, url: WS_URL })
    check('connected', true)

    await selectSession(page, SESSION.id)
    check('session selected', true, SESSION.title)

    await sendPrompt(page, LONG_PROMPT)
    await waitStatus('running')
    check('generation running before interrupt', true)

    const clicked = await page.click('[data-testid="session-interrupt"]')
    check('interrupt button clicked', clicked === 'clicked', String(clicked))

    await waitLastResponse(page, 'interrupt', 'completed', 20000)
    const detail = await page.ev(
      `document.querySelector('[data-testid="last-response"] .command-result-detail')?.getAttribute('title') ?? ''`,
    )
    check('interrupt response reports interrupted=true', /"interrupted"\s*:\s*true/.test(detail),
      detail.slice(0, 160))

    await waitStatus('idle')
    check('session returns to idle after interrupt', true)

    const snapshot = await adminSnapshot({ adminUrl: ADMIN_URL, adminToken: ADMIN_TOKEN })
    const mine = snapshot.events.filter((event) => event.session_id === SESSION.id)
    const statusEvents = mine.filter((event) => event.event_type === 'session.status_changed')
    const lastStatus = statusEvents[statusEvents.length - 1]
    const afterIdle = mine.filter((event) => event.sequence > (lastStatus?.sequence ?? 0))
    check('no events for this session after idle',
      lastStatus?.payload?.status === 'idle' && afterIdle.length === 0,
      `lastStatus=${lastStatus?.payload?.status} seq=${lastStatus?.sequence} after=${afterIdle.length}`)

    const failed = checks.filter((item) => !item.ok).length
    console.log(`\n${checks.length - failed}/${checks.length} checks passed`)
    await page.close()
    process.exit(failed ? 1 : 0)
  } catch (error) {
    console.error('e2e-interrupt failed:', error)
    await page.close()
    process.exit(2)
  }
})().catch((error) => {
  console.error('e2e-interrupt failed:', error.message)
  process.exit(2)
})
