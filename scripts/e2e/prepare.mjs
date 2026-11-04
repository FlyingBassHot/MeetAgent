#!/usr/bin/env node
/*
 * Prepare a real-machine E2E run:
 *   1. create a fresh OpenCode session (fresh context keeps the permission
 *      tests deterministic; reuse only when E2E_SESSION_TITLE is set)
 *   2. write the state file the other scripts read
 *   3. report Cloud health
 *
 * Required:
 *   OPENCODE_PASSWORD   password printed by `opencode service get password`
 *
 * Optional:
 *   OPENCODE_URL        default http://127.0.0.1:49374
 *   E2E_SESSION_TITLE   reuse an existing session with this exact title
 *   E2E_STATE_FILE      default <tmp>/opencode/meetagent-e2e/state.json
 *   ADMIN_URL / ADMIN_TOKEN  used only to report the Cloud version
 */

import { mkdirSync, writeFileSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { dirname, join } from 'node:path'

const OPENCODE_URL = process.env.OPENCODE_URL ?? 'http://127.0.0.1:49374'
const PASSWORD = process.env.OPENCODE_PASSWORD ?? ''
const REQUESTED_TITLE = process.env.E2E_SESSION_TITLE ?? ''
const STATE_FILE = process.env.E2E_STATE_FILE ?? join(tmpdir(), 'opencode', 'meetagent-e2e', 'state.json')
const ADMIN_URL = process.env.ADMIN_URL ?? 'http://127.0.0.1:8080'
const ADMIN_TOKEN = process.env.ADMIN_TOKEN ?? 'dev-admin-token-change-me'

if (!PASSWORD) {
  console.error('OPENCODE_PASSWORD is required: export OPENCODE_PASSWORD="$(opencode service get password)"')
  process.exit(2)
}

const auth = `Basic ${Buffer.from(`opencode:${PASSWORD}`).toString('base64')}`

async function api(method, path, body) {
  const response = await fetch(`${OPENCODE_URL}${path}`, {
    method,
    headers: { Authorization: auth, 'Content-Type': 'application/json' },
    body: body === undefined ? undefined : JSON.stringify(body),
  })
  if (!response.ok) {
    throw new Error(`${method} ${path} -> ${response.status} ${await response.text()}`)
  }
  return response.json()
}

async function main() {
  const title = REQUESTED_TITLE || `MeetAgent E2E ${new Date().toISOString().slice(0, 19)}`
  let session = null

  if (REQUESTED_TITLE) {
    const listed = await api('GET', '/api/session?limit=100&order=desc')
    session = (listed.data ?? listed).find((item) => item.title === REQUESTED_TITLE) ?? null
  }

  if (session) {
    console.log(`reusing session ${session.id} (${title})`)
  } else {
    const created = await api('POST', '/api/session', { title, agent: 'build' })
    session = created.data ?? created
    console.log(`created session ${session.id} (${title})`)
  }

  mkdirSync(dirname(STATE_FILE), { recursive: true })
  writeFileSync(STATE_FILE, `${JSON.stringify({ session_id: session.id, session_title: title }, null, 2)}\n`)
  console.log(`wrote ${STATE_FILE}`)

  try {
    const status = await (await fetch(`${ADMIN_URL}/api/v1/status`, {
      headers: { Authorization: `Bearer ${ADMIN_TOKEN}` },
    })).json()
    console.log(`cloud up: version=${status.version} nodes_online=${status.nodes_online} devices_online=${status.devices_online}`)
  } catch (error) {
    console.warn(`cloud status unavailable (${error.message}); start Cloud/Node before running the tests`)
  }

  console.log('\nscripts pick this session up automatically; optional overrides:\n')
  console.log(`export E2E_SESSION_ID=${session.id}`)
  console.log(`export E2E_SESSION_TITLE='${title}'`)
}

main().catch((error) => {
  console.error('prepare failed:', error.message)
  process.exit(2)
})
