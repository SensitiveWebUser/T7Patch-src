import { test } from 'node:test'
import assert from 'node:assert/strict'

import * as ipc from './ipc.js'

/** Swaps in a recording transport and hands back the calls it saw. */
function record(result = undefined) {
  const calls = []
  const previous = ipc.setInvokeTransport(async (cmd, args) => {
    calls.push({ cmd, args })
    if (result instanceof Error || typeof result === 'string') throw result
    return result
  })
  return { calls, restore: () => ipc.setInvokeTransport(previous) }
}

// ---------------------------------------------------------------------------
// The command contract
// ---------------------------------------------------------------------------

test('every command sends the name and argument shape Rust declares', async () => {
  const { calls, restore } = record()
  try {
    await ipc.locateGame()
    await ipc.currentVersion()
    await ipc.loadConfig('/games/bo3')
    await ipc.saveConfig('/games/bo3', { playername: 'Tester' })
    await ipc.install('/games/bo3')
    await ipc.uninstall('/games/bo3')
    await ipc.launchGame()
    await ipc.linuxLaunchOption()
    await ipc.elevate()
    await ipc.installState('/games/bo3')
    await ipc.injectInto(4321, 'C:\\games\\bo3\\t7patch.dll')
    await ipc.listGameProcesses()
  } finally {
    restore()
  }

  assert.deepEqual(calls, [
    { cmd: 'locate_game', args: undefined },
    { cmd: 'current_version', args: undefined },
    { cmd: 'load_config', args: { dir: '/games/bo3' } },
    { cmd: 'save_config', args: { dir: '/games/bo3', cfg: { playername: 'Tester' } } },
    { cmd: 'install', args: { dir: '/games/bo3' } },
    { cmd: 'uninstall', args: { dir: '/games/bo3' } },
    { cmd: 'launch_game', args: undefined },
    { cmd: 'linux_launch_option', args: undefined },
    { cmd: 'elevate', args: undefined },
    { cmd: 'install_state', args: { dir: '/games/bo3' } },
    // camelCase here, snake_case in Rust: Tauri converts the key. Passing dll_path would arrive
    // as a missing argument, which is a runtime error and not a build one.
    { cmd: 'inject_into', args: { pid: 4321, dllPath: 'C:\\games\\bo3\\t7patch.dll' } },
    { cmd: 'list_game_processes', args: undefined },
  ])
})

test('a command hands back whatever Rust returned, untouched', async () => {
  const report = { displaced_dsound: 'dsound.dll.before-t7patch', not_a_game_dir: false, exports_verified: 6 }
  const { restore } = record(report)
  try {
    assert.deepEqual(await ipc.install('/games/bo3'), report)
  } finally {
    restore()
  }
})

test('a rejection reaches the caller as the bare string Rust sent', async () => {
  const { restore } = record(ipc.NEEDS_ELEVATION)
  try {
    await assert.rejects(() => ipc.install('/games/bo3'), (err) => {
      assert.equal(err, ipc.NEEDS_ELEVATION)
      assert.equal(typeof err, 'string', 'wrapping it in an Error would break every sentinel test')
      return true
    })
  } finally {
    restore()
  }
})

// ---------------------------------------------------------------------------
// Sentinels
// ---------------------------------------------------------------------------

test('the sentinel constants are the exact strings Rust returns', () => {
  assert.equal(ipc.NEEDS_ELEVATION, 'NEEDS_ELEVATION')
  assert.equal(ipc.GAME_IS_RUNNING, 'GAME_IS_RUNNING')
  assert.equal(ipc.EMBEDDED_PAYLOAD_INVALID, 'EMBEDDED_PAYLOAD_INVALID')
})

test('a sentinel matches only itself, exactly', () => {
  assert.equal(ipc.isSentinel('NEEDS_ELEVATION', ipc.NEEDS_ELEVATION), true)
  assert.equal(ipc.isSentinel('GAME_IS_RUNNING', ipc.GAME_IS_RUNNING), true)
  assert.equal(ipc.isSentinel('GAME_IS_RUNNING', ipc.NEEDS_ELEVATION), false)
})

test('a sentinel is never matched by substring - this is the regression', () => {
  // The bug: elevation was once detected with a substring test against an English sentence.
  // Someone reworded the sentence and the elevation prompt silently stopped firing, because a
  // failed match is indistinguishable from "no elevation needed".
  for (const nearly of [
    'Install failed: NEEDS_ELEVATION',
    'NEEDS_ELEVATION ',
    ' NEEDS_ELEVATION',
    'needs_elevation',
    'NEEDS_ELEVATION_NOW',
    'That folder needs administrator rights.',
  ]) {
    assert.equal(ipc.isSentinel(nearly, ipc.NEEDS_ELEVATION), false, JSON.stringify(nearly))
  }
})

test('only a bare string can be a sentinel', () => {
  // String(new Error('NEEDS_ELEVATION')) is 'Error: NEEDS_ELEVATION', which must not match; and
  // an Error reaching here at all means something failed in JS, not in Rust.
  assert.equal(ipc.isSentinel(new Error('NEEDS_ELEVATION'), ipc.NEEDS_ELEVATION), false)
  assert.equal(ipc.isSentinel({ message: 'NEEDS_ELEVATION' }, ipc.NEEDS_ELEVATION), false)
  assert.equal(ipc.isSentinel(null, ipc.NEEDS_ELEVATION), false)
  assert.equal(ipc.isSentinel(undefined, ipc.NEEDS_ELEVATION), false)
})

test('the payload sentinel gives up the export name that was missing', () => {
  assert.equal(
    ipc.embeddedPayloadInvalidExport('EMBEDDED_PAYLOAD_INVALID:zbr_run_gamemode_lui'),
    'zbr_run_gamemode_lui',
  )
  assert.equal(
    ipc.embeddedPayloadInvalidExport('EMBEDDED_PAYLOAD_INVALID:EnableInjectorlessInstall'),
    'EnableInjectorlessInstall',
  )
})

test('the payload sentinel is still matched on the code, not by looking for it inside prose', () => {
  for (const nearly of [
    'EMBEDDED_PAYLOAD_INVALID',
    'install failed: EMBEDDED_PAYLOAD_INVALID:x',
    'XEMBEDDED_PAYLOAD_INVALID:x',
    'NEEDS_ELEVATION',
    '',
  ]) {
    assert.equal(ipc.embeddedPayloadInvalidExport(nearly), null, JSON.stringify(nearly))
  }
  assert.equal(ipc.embeddedPayloadInvalidExport(new Error('EMBEDDED_PAYLOAD_INVALID:x')), null)
  assert.equal(ipc.embeddedPayloadInvalidExport(undefined), null)
})

test('an empty export name is reported as empty, not as no match', () => {
  // Rust cannot produce this, but the difference between "" and null is the difference between
  // "the payload is wrong" and "this was some other failure", and the caller branches on it.
  assert.equal(ipc.embeddedPayloadInvalidExport('EMBEDDED_PAYLOAD_INVALID:'), '')
})

// ---------------------------------------------------------------------------
// errorText
// ---------------------------------------------------------------------------

test('an unplanned failure keeps its own words, because they are the only information there is', () => {
  assert.equal(ipc.errorText('The system cannot find the path specified. (os error 3)'),
    'The system cannot find the path specified. (os error 3)')
  assert.equal(ipc.errorText(new Error('boom')), 'boom')
  assert.equal(ipc.errorText({ message: 'boom' }), 'boom')
  assert.equal(ipc.errorText(42), '42')
  assert.equal(ipc.errorText(null), 'null')
  assert.equal(ipc.errorText(undefined), 'undefined')
})
