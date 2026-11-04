/*
 * Shared Chrome DevTools Protocol helpers for the real-machine E2E scripts.
 *
 * Requires Node >= 22 (global WebSocket) and a Chromium binary. Each script
 * gets an isolated browser profile, so the connection gate is always shown and
 * the developer's own browser session (web-001) is not replaced.
 */

const { execFile } = require('node:child_process')
const { mkdtempSync, rmSync } = require('node:fs')
const { tmpdir } = require('node:os')
const { join } = require('node:path')
const WebSocket = require('./ws.cjs')

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms))

async function waitForPage(port, timeoutMs = 20000) {
  const deadline = Date.now() + timeoutMs
  while (Date.now() < deadline) {
    try {
      const list = await (await fetch(`http://127.0.0.1:${port}/json/list`)).json()
      const page = list.find((target) => target.type === 'page')
      if (page) return page
    } catch {
      // Chrome is still starting.
    }
    await sleep(200)
  }
  throw new Error(`Chrome DevTools did not start on port ${port}`)
}

function connectCdp(wsUrl) {
  return new Promise((resolve, reject) => {
    const ws = new WebSocket(wsUrl)
    let id = 0
    const pending = new Map()
    ws.on('open', () => {
      resolve({
        send: (method, params = {}) =>
          new Promise((res, rej) => {
            id += 1
            pending.set(id, { res, rej })
            ws.send(JSON.stringify({ id, method, params }))
          }),
      })
    })
    ws.on('message', (raw) => {
      const message = JSON.parse(String(raw))
      if (message.id && pending.has(message.id)) {
        const entry = pending.get(message.id)
        pending.delete(message.id)
        if (message.error) entry.rej(new Error(message.error.message))
        else entry.res(message.result)
      }
    })
    ws.on('error', reject)
  })
}

async function launch({ appUrl, port, hash = '#/console', chromeBin = process.env.CHROME_BIN ?? '/usr/bin/chromium', windowSize = '1440,1000' }) {
  const profile = mkdtempSync(join(tmpdir(), 'meetagent-e2e-chrome-'))
  const chrome = execFile(
    chromeBin,
    [
      '--headless=new',
      '--disable-gpu',
      '--no-sandbox',
      '--disable-dev-shm-usage',
      '--no-first-run',
      '--no-default-browser-check',
      `--user-data-dir=${profile}`,
      `--remote-debugging-port=${port}`,
      `--window-size=${windowSize}`,
      `${appUrl.replace(/\/$/, '')}/${hash}`,
    ],
    () => {},
  )

  const page = await waitForPage(port)
  const { send } = await connectCdp(page.webSocketDebuggerUrl)
  await send('Runtime.enable')
  await send('Page.enable')
  await sleep(800)

  const ev = async (expression) => {
    const result = await send('Runtime.evaluate', {
      expression,
      returnByValue: true,
      awaitPromise: true,
    })
    return result?.result?.value
  }

  const waitFor = async (expression, timeoutMs = 15000, intervalMs = 250) => {
    const deadline = Date.now() + timeoutMs
    let last
    while (Date.now() < deadline) {
      last = await ev(expression)
      if (last) return last
      await sleep(intervalMs)
    }
    throw new Error(`timed out waiting for ${expression} (last=${JSON.stringify(last)})`)
  }

  const text = (selector) =>
    ev(`document.querySelector(${JSON.stringify(selector)})?.innerText ?? ''`)

  const click = (selector) =>
    ev(`(() => {
      const el = document.querySelector(${JSON.stringify(selector)})
      if (!el) return 'not-found'
      if (el.disabled) return 'disabled'
      el.click()
      return 'clicked'
    })()`)

  const setValue = (selector, value, kind = 'input') =>
    ev(`(() => {
      const el = document.querySelector(${JSON.stringify(selector)})
      if (!el) return 'not-found'
      const proto = ${kind === 'textarea' ? 'HTMLTextAreaElement' : 'HTMLInputElement'}.prototype
      const setter = Object.getOwnPropertyDescriptor(proto, 'value').set
      setter.call(el, ${JSON.stringify(value)})
      el.dispatchEvent(new Event('input', { bubbles: true }))
      el.dispatchEvent(new Event('change', { bubbles: true }))
      return el.value
    })()`)

  const close = async () => {
    try {
      chrome.kill('SIGTERM')
    } catch {
      // ignore
    }
    await sleep(400)
    try {
      rmSync(profile, { recursive: true, force: true })
    } catch {
      // Chrome may still hold the profile briefly; it lives under /tmp.
    }
  }

  return { send, ev, waitFor, text, click, setValue, close, chrome }
}

/** Fill the connection gate and wait until the console shell is visible. */
async function connect(page, { deviceId, token, url }) {
  await page.waitFor(
    `!!document.querySelector('[data-testid="connection-gate"]')`,
    10000,
  )
  if (url) await page.setValue('[data-testid="connection-url"]', url)
  await page.setValue('[data-testid="connection-device"]', deviceId)
  await page.setValue('[data-testid="connection-token"]', token)
  const clicked = await page.click('[data-testid="connection-submit"]')
  if (clicked !== 'clicked') throw new Error(`connection submit: ${clicked}`)

  const result = await page.waitFor(
    `(() => {
      const gate = document.querySelector('[data-testid="connection-gate"]')
      if (!gate) return 'online'
      const error = document.querySelector('[data-testid="connection-error"]')
      return error ? 'error:' + error.innerText : ''
    })()`,
    20000,
  )
  if (result !== 'online') throw new Error(`connection failed: ${result}`)
  await page.waitFor(`!!document.querySelector('.app-shell')`, 5000)
}

/** Select a Session by id in the workbench. */
async function selectSession(page, sessionId) {
  await page.ev(`location.hash = '#/sessions'`)
  await page.waitFor(`!!document.querySelector('[data-testid="session-list-pane"]')`, 10000)
  const clicked = await page.waitFor(
    `(() => {
      const el = document.querySelector('[data-testid="session-item"][data-session-id=${JSON.stringify(sessionId)}]')
      if (!el) return ''
      el.click()
      return 'clicked'
    })()`,
    15000,
  )
  if (clicked !== 'clicked') throw new Error(`session not found: ${sessionId}`)
  await page.waitFor(`!!document.querySelector('[data-testid="session-detail"]')`, 5000)
}

async function sendPrompt(page, text) {
  await page.setValue('[data-testid="prompt-input"]', text, 'textarea')
  const clicked = await page.click('[data-testid="prompt-send"]')
  if (clicked !== 'clicked') throw new Error(`prompt send: ${clicked}`)
}

/** Wait for the inline last-response strip to show an action + status. */
function waitLastResponse(page, action, status, timeoutMs = 20000) {
  return page.waitFor(
    `(() => {
      const el = document.querySelector('[data-testid="last-response"]')
      if (!el) return ''
      const value = el.innerText
      return value.includes(${JSON.stringify(action)}) && value.includes(${JSON.stringify(status)})
        ? value
        : ''
    })()`,
    timeoutMs,
  )
}

function adminSnapshot({ adminUrl, adminToken }) {
  return fetch(`${adminUrl}/api/v1/snapshot`, {
    headers: { Authorization: `Bearer ${adminToken}` },
  }).then((response) => response.json())
}

module.exports = {
  sleep,
  launch,
  connect,
  selectSession,
  sendPrompt,
  waitLastResponse,
  adminSnapshot,
}
