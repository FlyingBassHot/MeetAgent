/*
 * Shared fixtures for the real-machine E2E scripts.
 *
 * `prepare.mjs` writes a small state file describing the session created for
 * this run. Scripts read it so they can be launched without exporting anything,
 * while environment variables still win when set explicitly.
 */

const { readFileSync } = require('node:fs')
const { tmpdir } = require('node:os')
const { join } = require('node:path')

const STATE_FILE = process.env.E2E_STATE_FILE ?? join(tmpdir(), 'opencode', 'meetagent-e2e', 'state.json')

function readState() {
  try {
    return JSON.parse(readFileSync(STATE_FILE, 'utf8'))
  } catch {
    return {}
  }
}

function resolveSession() {
  const state = readState()
  return {
    id: process.env.E2E_SESSION_ID ?? state.session_id ?? '',
    title: process.env.E2E_SESSION_TITLE ?? state.session_title ?? 'MeetAgent E2E',
    stateFile: STATE_FILE,
  }
}

module.exports = { readState, resolveSession, STATE_FILE }
