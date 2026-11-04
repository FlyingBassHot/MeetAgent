/* Phase 3 E2E #3：真机权限审批（permission.asked → 浏览器批准 → 工具执行）。 */
const { mkdirSync, writeFileSync } = require('node:fs')
const { tmpdir } = require('node:os')
const { dirname, join } = require('node:path')
const { launch, connect, selectSession, sendPrompt, waitLastResponse, adminSnapshot, sleep } = require('./lib/ui.cjs')
const { resolveSession } = require('./lib/fixtures.cjs')

const APP_URL = process.env.APP_URL ?? 'http://127.0.0.1:4173'
const ADMIN_URL = process.env.ADMIN_URL ?? 'http://127.0.0.1:8080'
const ADMIN_TOKEN = process.env.ADMIN_TOKEN ?? 'dev-admin-token-change-me'
const CHROME_BIN = process.env.CHROME_BIN ?? '/usr/bin/chromium'
const PORT = Number(process.env.CDP_PORT ?? 9233)
const DEVICE_ID = process.env.E2E_DEVICE_ID ?? 'web-002'
const DEVICE_TOKEN = process.env.E2E_DEVICE_TOKEN ?? 'dev-device-002-token-change-me'
const WS_URL = process.env.E2E_WS_URL ?? ''
const SESSION = resolveSession()

const MARKER = `E2E_PERMISSION_PROBE=${Date.now().toString(36)}`
const FILE = process.env.E2E_FILE ?? join(tmpdir(), 'opencode', 'meetagent-e2e', `approve-${process.pid}-${Date.now()}.env`)
mkdirSync(dirname(FILE), { recursive: true })
writeFileSync(FILE, `${MARKER}\n`)
const PROMPT = `使用 read 工具读取文件 ${FILE}，只回复文件中的原始内容，不要解释，也不要跳过工具调用。`

const checks = []
const check = (name, ok, detail = '') => {
  checks.push({ name, ok: Boolean(ok) })
  console.log(`${ok ? 'PASS' : 'FAIL'}  ${name}${detail ? ` — ${detail}` : ''}`)
}

function sessionEvents(snapshot, afterSequence) {
  return (snapshot.events ?? []).filter(
    (event) => event.session_id === SESSION.id && event.sequence > afterSequence,
  )
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
    const baseline = Math.max(
      0,
      ...before.events.filter((event) => event.session_id === SESSION.id).map((event) => event.sequence),
    )

    await sendPrompt(page, PROMPT)
    const permission = await page.waitFor(
      `(() => {
        const button = document.querySelector('[data-testid="permission-approve"]')
        if (!button) return ''
        const block = document.querySelector('[data-testid="session-permissions"]')
        return block ? block.innerText : 'permission'
      })()`,
      60000,
      500,
    )
    check('permission request surfaced in UI', permission.includes(FILE),
      permission.replace(/\n/g, ' | ').slice(0, 220))

    const clicked = await page.click('[data-testid="permission-approve"]')
    check('approve button clicked', clicked === 'clicked', String(clicked))

    await page.waitFor(
      `!document.querySelector('[data-testid="session-permissions"]')`,
      15000,
    )
    check('permission cleared from panel', true)

    await waitLastResponse(page, 'permission_response', 'completed', 20000)
    check('permission_response completed', true)

    let resolved = null
    let reply = null
    for (let attempt = 0; attempt < 30; attempt += 1) {
      const snapshot = await adminSnapshot({ adminUrl: ADMIN_URL, adminToken: ADMIN_TOKEN })
      const events = sessionEvents(snapshot, baseline)
      resolved = events.find(
        (event) => event.event_type === 'permission.resolved' && event.payload?.status === 'approved',
      )
      reply = events.find(
        (event) =>
          event.event_type === 'message.completed' &&
          String(event.payload?.text ?? '').includes(MARKER),
      )
      if (resolved && reply) break
      await sleep(500)
    }
    check('permission resolved as approved', Boolean(resolved), JSON.stringify(resolved?.payload ?? null))
    check('approved read actually ran and content returned', Boolean(reply), MARKER)

    const failed = checks.filter((item) => !item.ok).length
    console.log(`\n${checks.length - failed}/${checks.length} checks passed`)
    await page.close()
    process.exit(failed ? 1 : 0)
  } catch (error) {
    console.error('e2e-permission-approve failed:', error)
    await page.close()
    process.exit(2)
  }
})().catch((error) => {
  console.error('e2e-permission-approve failed:', error.message)
  process.exit(2)
})
