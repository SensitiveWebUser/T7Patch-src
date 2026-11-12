/**
 * What the Setup tab, CopyButton and the build notice SAY, as tests.
 *
 * Every sentence in this UI is supposed to be chosen by a probed fact. These pin the ones that
 * are easy to get wrong silently: a copy that failed reported as "Copied", launch-option wording
 * on a platform with no launch option, and an unsupported game build hidden behind a green tick.
 */
import { test, expect, afterEach } from 'bun:test'
import { act } from 'react'
import CopyButton from './components/CopyButton.jsx'
import BuildNotice from './components/BuildNotice.jsx'
import Wizard from './Wizard.jsx'
import { setClipboardWriter } from './lib/clipboard.js'
import { t } from './i18n/index.js'
import { useDom } from './testing/dom.js'

const { win, mount } = useDom()

afterEach(() => setClipboardWriter(null))

async function click(el) {
  await act(async () => {
    el.dispatchEvent(new win.MouseEvent('click', { bubbles: true }))
  })
}

const LABELS = { copy: 'Copy', copied: 'Copied', failed: 'Copy failed' }

// --- CopyButton ------------------------------------------------------------------------------

test('CopyButton shows the failure state when onCopy resolves false', async () => {
  const host = mount(<CopyButton value="x" labels={LABELS} onCopy={async () => false} />)
  await click(host.querySelector('button'))
  expect(host.querySelector('button').textContent).toBe('Copy failed')
})

test('CopyButton shows Copied when onCopy resolves true, and hands it the value', async () => {
  let seen
  const host = mount(<CopyButton value="abc" labels={LABELS} onCopy={async (v) => { seen = v; return true }} />)
  await click(host.querySelector('button'))
  expect(seen).toBe('abc')
  expect(host.querySelector('button').textContent).toBe('Copied')
})

test('CopyButton treats an onCopy that returns nothing as a failure, not a success', async () => {
  const host = mount(<CopyButton value="x" labels={LABELS} onCopy={async () => {}} />)
  await click(host.querySelector('button'))
  expect(host.querySelector('button').textContent).toBe('Copy failed')
})

test('CopyButton shows failure when onCopy throws, instead of an unhandled rejection', async () => {
  const host = mount(<CopyButton value="x" labels={LABELS} onCopy={async () => { throw new Error('no') }} />)
  await click(host.querySelector('button'))
  expect(host.querySelector('button').textContent).toBe('Copy failed')
})

test('CopyButton without onCopy copies itself and reports the result', async () => {
  const written = []
  setClipboardWriter({ writeText: async (v) => { written.push(v) } })
  const host = mount(<CopyButton value="launch-opt" labels={LABELS} />)
  await click(host.querySelector('button'))
  expect(written).toEqual(['launch-opt'])
  expect(host.querySelector('button').textContent).toBe('Copied')
})

// --- BuildNotice -----------------------------------------------------------------------------

// Deliberately synthetic, so it can never collide with a real build GameBuild.h later learns.
const UNSUPPORTED = { time_date_stamp: 0x11111111, size_of_image: 0x22222000, entry_point: 0x00333333, supported: false }
const SUPPORTED = { time_date_stamp: 0x693d731e, size_of_image: 0x1d74b000, entry_point: 0x02bd40fc, supported: true }

test('BuildNotice renders nothing when the exe could not be read', () => {
  const host = mount(<BuildNotice build={null} />)
  expect(host.textContent).toBe('')
})

test('BuildNotice is silent for a supported build unless asked to be quiet-but-present', () => {
  expect(mount(<BuildNotice build={SUPPORTED} />).textContent).toBe('')
  expect(mount(<BuildNotice build={SUPPORTED} quiet />).textContent).toBe(t('build.supported'))
})

test('BuildNotice warns about an unsupported build and shows the exact report block', () => {
  const host = mount(<BuildNotice build={UNSUPPORTED} />)
  expect(host.textContent).toContain(t('build.unsupportedTitle'))
  expect(host.querySelector('pre').textContent)
    .toBe('TimeDateStamp: 0x11111111\nSizeOfImage:   0x22222000\nEntryPoint:    0x00333333')
})

test('BuildNotice copies the report block through the caller', async () => {
  let copied
  const host = mount(<BuildNotice build={UNSUPPORTED} onCopy={async (v) => { copied = v; return true }} />)
  await click(host.querySelector('button'))
  expect(copied).toBe(host.querySelector('pre').textContent)
})

// --- Wizard ----------------------------------------------------------------------------------

const ALL_DONE = {
  is_game_dir: true, conf_exists: true, dsound_installed: true, patch_installed: true,
  foreign_dsound: false, game_build: null,
}
const CFG = { playername: 'p', networkpassword: '', isfriendsonly: true, autoinstallmods: false }
const noop = async () => true

function wizard(state, launchOption) {
  return mount(
    <Wizard
      dir="/game" state={state} cfg={CFG} installed launchOption={launchOption} busy={null}
      onEdit={noop} onBrowse={noop} onSaveSettings={noop} onInstall={noop} onLaunch={noop}
      onCopyPassword={noop} onCopyBuildReport={noop} onDismiss={noop}
    />,
  )
}

const intro = (host) => host.querySelector('.wiz-intro').textContent
const playStep = (host) => host.querySelectorAll('.wiz-step')[3]

test('Wizard on Windows (no launch option) never mentions one', () => {
  const done = wizard(ALL_DONE, null)
  expect(intro(done)).toBe(t('wizard.introDone'))
  expect(done.textContent).not.toContain(t('wizard.playNeedsLaunchOption'))
  expect(done.textContent).toContain(t('onboard.doneBody'))
  expect(playStep(done).hasAttribute('data-done')).toBe(true)

  const todo = wizard({ ...ALL_DONE, conf_exists: false }, null)
  expect(intro(todo)).toBe(t('wizard.intro'))
})

test('Wizard on Linux does not claim everything is done while the launch option is unprobed', () => {
  const opt = 'WINEDLLOVERRIDES="dsound=n,b" %command%'
  const done = wizard(ALL_DONE, opt)
  expect(intro(done)).toBe(t('wizard.introDoneLaunchOption'))
  expect(done.textContent).not.toContain(t('onboard.doneBody'))
  expect(done.textContent).toContain(t('wizard.playNeedsLaunchOption'))
  expect(playStep(done).hasAttribute('data-done')).toBe(false)

  const todo = wizard({ ...ALL_DONE, conf_exists: false }, opt)
  expect(intro(todo)).toBe(t('wizard.introLaunchOption'))
})

test('Wizard shows an unsupported build above the steps, and does not tick Ready to play', () => {
  const host = wizard({ ...ALL_DONE, game_build: UNSUPPORTED }, null)
  const warn = host.querySelector('.build-warn')
  expect(warn).not.toBeNull()
  // Outside every step body, so a collapsed (ticked) folder row cannot hide it.
  expect(warn.closest('.wiz-body')).toBeNull()
  expect(playStep(host).hasAttribute('data-done')).toBe(false)
  expect(host.textContent).not.toContain(t('onboard.doneBody'))
  // The intro must not say "everything is in place" directly above a warning that it is not.
  expect(intro(host)).toBe(t('wizard.introDoneUnsupported'))
  expect(intro(wizard({ ...ALL_DONE, game_build: UNSUPPORTED }, 'opt'))).toBe(t('wizard.introDoneUnsupported'))
  // A warning, never a gate: install and launch stay available.
  const buttons = [...host.querySelectorAll('button')]
  const launch = buttons.find((b) => b.textContent === t('basic.launchGame'))
  const install = buttons.find((b) => b.textContent === t('advanced.reinstall'))
  expect(launch.disabled).toBe(false)
  expect(install.disabled).toBe(false)
})

test('Wizard notes a supported build quietly in the folder step and nowhere else', () => {
  const host = wizard({ ...ALL_DONE, game_build: SUPPORTED }, null)
  expect(host.querySelector('.build-warn')).toBeNull()
  expect(host.querySelector('#wiz-body-folder').textContent).toContain(t('build.supported'))
})

// --- autoinstallmods -------------------------------------------------------------------------

test('Wizard settings step offers auto-install workshop mods, bound to autoinstallmods', async () => {
  const edits = []
  const host = mount(
    <Wizard
      dir="/game" state={{ ...ALL_DONE, conf_exists: false }} cfg={CFG} installed launchOption={null}
      busy={null} onEdit={(patch) => { edits.push(patch) }} onBrowse={noop} onSaveSettings={noop}
      onInstall={noop} onLaunch={noop} onCopyPassword={noop} onCopyBuildReport={noop} onDismiss={noop}
    />,
  )
  const label = [...host.querySelectorAll('label')].find((l) => l.textContent === t('basic.autoInstallMods'))
  expect(label).toBeTruthy()
  const box = host.ownerDocument.getElementById(label.getAttribute('for'))
  expect(box.checked).toBe(false)
  await click(box)
  expect(edits).toContainEqual({ autoinstallmods: true })
})
