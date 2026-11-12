import { useCallback, useEffect, useRef, useState } from 'react'
import { writeText as pluginWriteText } from '@tauri-apps/plugin-clipboard-manager'
import * as ipc from './lib/ipc.js'
import { checkForUpdate, CURRENT_VERSION } from './lib/updates.js'
import { generatePassword } from './lib/password.js'
import { copyText, setClipboardWriter } from './lib/clipboard.js'
import { formatLogLine } from './lib/format.js'
import { t } from './i18n/index.js'
import Tabs, { Tab } from './components/Tabs.jsx'
import Button from './components/Button.jsx'
import Field from './components/Field.jsx'
import TextInput from './components/TextInput.jsx'
import SecretInput from './components/SecretInput.jsx'
import Checkbox from './components/Checkbox.jsx'
import CopyButton from './components/CopyButton.jsx'
import BuildNotice from './components/BuildNotice.jsx'
import StatusLog from './components/StatusLog.jsx'
import Wizard from './Wizard.jsx'
import './app.css'

setClipboardWriter({ writeText: pluginWriteText })

const NO_STATE = { dsound_installed: false, patch_installed: false, conf_exists: false, is_game_dir: false, foreign_dsound: false, game_build: null }
const DEFAULT_CONFIG = {
  playername: 'Unknown Soldier', networkpassword: '', isfriendsonly: true, autoinstallmods: false,
  toolautolaunchgame: false, toolcheckforupdatesonlaunch: true, toollastseenupstreamtag: '',
}

/**
 * Written out rather than built as `protection.${level}` so every key in this app is still a
 * literal that greps back to en.js by pasting it - which is the reason the catalogue is flat.
 */
const PROTECTION_LABEL = {
  none: 'protection.none',
  partial: 'protection.partial',
  full: 'protection.full',
}

/**
 * The one thing this window is for: is the game protected, and if not, what fixes it.
 *
 * Seven rows, tested top to bottom. They are exhaustive and mutually exclusive, so the last `return`
 * is unreachable - it is there because an unreachable branch that guesses GREEN would be a lie
 * about protection, and one that guesses "not installed" is merely wrong about the folder.
 *
 * `conf_exists` is deliberately absent from every row. Protection.cpp's load_settings_initial()
 * writes t7patch.conf itself when it is missing, from patch_config's constructor defaults
 * (friends-only already on), and RunPatching() installs every hook either way - so both DLLs with
 * no conf is FULLY protected. conf_exists only means "the user has saved settings", which is what
 * still drives the Setup tab below. The badge going green while the Setup tab is still offering
 * step 2 is correct, not a bug to fix by dragging conf_exists in here.
 *
 * `foreign_dsound && dsound_installed` cannot happen: install_state() sets `dsound_installed` from
 * `exists && is_our_dsound(..)` and `foreign_dsound` from `exists && !ours`. Row 4 needs no guard.
 *
 * `is_game_dir` false demotes to PARTIAL, never red: looks_like_game_dir() is a soft signal and
 * install_hijack reports not_a_game_dir and installs anyway, on purpose. Red would claim we know
 * it cannot work; all we know is that we cannot see the exe.
 *
 * `game_build.supported === false` demotes a complete install to PARTIAL, never red, and only
 * once every file row has passed - a missing file is the more actionable fact. Arxan.cpp's
 * VerifyGameBuild() returns before a single hook is installed on a build outside kSupportedBuilds,
 * so green there would be the one claim this badge exists not to make. Not red, because the triple
 * comes from the header ON DISK and nobody has verified that it equals what the DLL reads in
 * memory from this Arxan-packed exe; partial is "probably will not work", not "cannot work".
 * Strictly `=== false`: null means the header could not be read, and a badge that guessed either
 * way from that would be inventing a fact.
 *
 * What green does NOT claim: that the hooks are live. The WINEDLLOVERRIDES launch option on Proton
 * still gates that and is not probeable from the folder, which is why it is on Advanced. Green
 * means the folder is right.
 *
 * `action` is which step of the Setup tab resolves the row, not a fourth piece of prose.
 */
function protectionOf(dir, s) {
  if (!s.dsound_installed && !s.patch_installed && !s.is_game_dir) {
    return { level: 'none', text: dir ? t('state.notGameDir') : t('state.noFolder'), hint: t('protection.actionBrowse'), action: 'browse' }
  }
  if (!s.dsound_installed && !s.patch_installed && s.is_game_dir) {
    return { level: 'none', text: t('state.notInstalled'), hint: t('protection.actionInstall'), action: 'install' }
  }
  if (!s.is_game_dir) {
    // A complete pair in the wrong folder and a lone stray file there are one row: the actionable
    // fact is the same, and naming the missing file would be noise next to "wrong folder".
    return { level: 'partial', text: t('protection.wrongFolder'), hint: t('protection.actionBrowse'), action: 'browse' }
  }
  if (s.foreign_dsound) {
    // Orange here means "half installed", never "half working" - the badge claims no protection at
    // all. The DSOAL/dgVoodoo explanation is two sentences and lives in the log.
    return { level: 'partial', text: t('protection.foreignBlocked'), hint: t('protection.actionMoveAside'), action: 'install' }
  }
  if (!s.dsound_installed || !s.patch_installed) {
    // Exactly one is missing by here: rows 1-2 took the neither case, and both-present falls to 6-7.
    return { level: 'partial', text: t('protection.halfInstalled', s.dsound_installed ? 't7patch.dll' : 'dsound.dll'), hint: t('protection.actionReinstall'), action: 'install' }
  }
  if (s.game_build?.supported === false) {
    // action 'none': no step of the Setup tab fixes this. Only a T7Patch release does.
    return { level: 'partial', text: t('build.unsupportedBadge'), hint: '', action: 'none' }
  }
  if (s.is_game_dir && s.dsound_installed && s.patch_installed) {
    return { level: 'full', text: t('protection.fullDetail'), hint: '', action: 'launch' }
  }
  return { level: 'none', text: t('state.notInstalled'), hint: t('protection.actionInstall'), action: 'install' }
}

/** The same sentence, for the log rather than the badge - these say what is on disk, not what it means. */
function folderSummary(s) {
  if (!s.is_game_dir) return t('state.notGameDir')
  if (s.dsound_installed && s.patch_installed) return t('state.installed')
  if (s.dsound_installed || s.patch_installed) {
    return t('state.partial',
      s.dsound_installed ? 'dsound.dll' : 't7patch.dll',
      s.dsound_installed ? 't7patch.dll' : 'dsound.dll')
  }
  return t('state.notInstalled')
}

/**
 * "Nothing is outstanding for this folder" - the single predicate that decides whether the Setup
 * tab exists at all. Probed, never remembered: it is recomputed from install_state after every
 * action, so uninstalling brings the tab back and installing takes it away without anything
 * having to track what the user did.
 */
const isReady = (s) => s.is_game_dir && s.dsound_installed && s.patch_installed && s.conf_exists

/**
 * One line of a settings list: text on the left, control on the right.
 *
 * A row, not a card. `label` is optional - a row whose control already carries its own verb (the
 * install/uninstall pair) gets the explanation and nothing else, because a label that repeats the
 * button underneath it is furniture.
 */
function Setting({ label, hint, children }) {
  return (
    <div className="setting">
      <div className="setting-text">
        {label && <span className="setting-label">{label}</span>}
        {hint && <span className="setting-hint">{hint}</span>}
      </div>
      <div className="setting-action">{children}</div>
    </div>
  )
}

export default function App() {
  const [dir, setDir] = useState(null)
  const [state, setState] = useState(NO_STATE)
  const [cfg, setCfg] = useState(DEFAULT_CONFIG)
  const [launchOption, setLaunchOption] = useState(null)
  const [busy, setBusy] = useState(null)
  const [lines, setLines] = useState([])
  const [version, setVersion] = useState('')
  const [tab, setTab] = useState('settings')
  // Three values, not a boolean, because "should the Setup tab exist" has three different answers
  // and only one of them is the user's:
  //   'auto'   - nobody has said; the probed state decides, which is the first-run case.
  //   'open'   - the user is IN it (or asked for it from Advanced). It stays until they finish,
  //              so completing the last step does not yank the tab out from under them.
  //   'closed' - the user dismissed it. Nothing but an explicit request brings it back.
  const [setup, setSetup] = useState('auto')
  // Until the first probe lands, `state` is NO_STATE - which reads as "nothing is installed" and
  // would flash a Setup tab at a fully set-up user for one frame. Nothing is decided before this.
  const [probed, setProbed] = useState(false)
  const saveTimer = useRef(null)
  // Keyed by FOLDER, not a boolean: a boolean stays true after the first foreign folder, so
  // browsing to a second one that also has a foreign dsound.dll would log nothing.
  const foreignWarned = useRef(null)

  const log = useCallback((text) => {
    setLines((prev) => [...prev, formatLogLine(text)].slice(-500))
  }, [])

  // For CopyButton's onCopy: logs the outcome AND returns it. The return is the contract - the
  // button branches on it - and dropping it made every copy that went through here show "Copied",
  // including the ones the status log, one line below, was reporting as failed.
  const copyAndLog = (okKey) => async (value) => {
    const ok = await copyText(value)
    log(ok ? t(okKey) : t('action.copyFailed'))
    return ok
  }
  const copyPassword = copyAndLog('log.passwordCopied')
  const copyBuildReport = copyAndLog('log.buildReportCopied')

  // Everything the UI says about installation comes from probing the folder, never from what an
  // action believed it did. The predecessor's wizard printed "T7Patch is installed" as a literal
  // because it was a step counter with no idea what happened.
  const refresh = useCallback(async (folder) => {
    if (!folder) return NO_STATE
    try {
      const s = await ipc.installState(folder)
      setState(s)
      // The badge gets one clause for this; the paragraph that explains DSOAL, dgVoodoo and the
      // .before-t7patch rename belongs in the log. Once per occurrence, not once per probe - this
      // runs after every save, and a log that repeats itself every keystroke is not a log.
      if (s.foreign_dsound && foreignWarned.current !== folder) log(t('state.foreignDsound'))
      foreignWarned.current = s.foreign_dsound ? folder : null
      return s
    } catch (e) {
      log(t('log.stateFailed', ipc.errorText(e)))
      return NO_STATE
    }
  }, [log])

  const adopt = useCallback(async (folder) => {
    setDir(folder)
    try { setCfg(await ipc.loadConfig(folder)) } catch (e) { log(t('config.loadFailed', ipc.errorText(e))) }
    return refresh(folder)
  }, [refresh, log])

  useEffect(() => {
    let gone = false
    ;(async () => {
      try {
        const found = await ipc.locateGame()
        if (gone) return
        if (!found) { log(t('log.gameFolderNotFound')); setTab('setup'); return }
        const s = await adopt(found)
        if (gone) return
        log(t('log.foundGameFolder', found))
        log(folderSummary(s))
        // Selected by default ONLY when something is genuinely outstanding. A user who is already
        // installed and configured never sees the tab exist, let alone open - offering setup to
        // someone who is already set up is the exact complaint that killed the last wizard.
        if (!isReady(s)) setTab('setup')
      } catch (e) { log(t('log.locateFailed', ipc.errorText(e))) }
      finally { if (!gone) setProbed(true) }
    })()
    ipc.linuxLaunchOption().then((v) => !gone && setLaunchOption(v)).catch(() => {})
    ipc.currentVersion().then((v) => !gone && setVersion(v)).catch(() => {})
    return () => { gone = true }
  }, [adopt, log])

  // Debounced like the predecessor's 750ms auto-save, and for the same reason: this fires on
  // every keystroke and the DLL re-reads t7patch.conf once a second.
  const edit = (patch) => {
    const next = { ...cfg, ...patch }
    setCfg(next)
    if (!dir) return
    clearTimeout(saveTimer.current)
    saveTimer.current = setTimeout(async () => {
      try { await ipc.saveConfig(dir, next); await refresh(dir) }
      catch (e) { log(t('config.saveFailed', ipc.errorText(e))) }
    }, 750)
  }

  const run = (name, fn) => async () => {
    setBusy(name)
    try { await fn() } finally { setBusy(null) }
  }

  const browse = run('browse', async () => {
    const picked = await ipc.pickFolder(t('file.selectGameFolder'))
    if (!picked) return
    const s = await adopt(picked)
    log(t('log.gameFolderSet', picked))
    log(folderSummary(s))
  })

  const install = run('install', async () => {
    if (!dir) return log(t('install.noFolder'))
    try {
      const r = await ipc.install(dir)
      if (r.not_a_game_dir) log(t('warn.installedIntoNonGameDir'))
      if (r.displaced_dsound) log(t('install.displacedDsound', r.displaced_dsound))
      log(t('install.exportsVerified', r.exports_verified))
    } catch (e) {
      if (ipc.isSentinel(e, ipc.GAME_IS_RUNNING)) return log(t('error.gameIsRunning'))
      if (ipc.isSentinel(e, ipc.NEEDS_ELEVATION)) {
        log(t('error.needsElevation'))
        if (await ipc.elevate()) return ipc.exitApp(0)
        return log(t('error.elevateFailed'))
      }
      const bad = ipc.embeddedPayloadInvalidExport(e)
      log(bad ? t('error.embeddedPayloadInvalid', bad) : t('install.failed', ipc.errorText(e)))
    } finally { await refresh(dir) }
  })

  const uninstall = run('uninstall', async () => {
    if (!dir) return log(t('install.noFolder'))
    // The folder is the whole point of the warning ("this deletes your settings in THAT folder"),
    // and the {0} hole was going out unfilled - the dialog literally read "from {0}".
    if (!(await ipc.confirmUser(t('uninstall.confirmBody', dir), { title: t('uninstall.confirmTitle') }))) return
    try { await ipc.uninstall(dir); log(t('uninstall.succeeded', dir)) }
    catch (e) { log(t('uninstall.failed', ipc.errorText(e))) }
    finally { await refresh(dir) }
  })

  const launch = run('launch', async () => {
    try { await ipc.launchGame(); log(t('log.launching')) }
    catch (e) { log(t('log.launchFailed', ipc.errorText(e))) }
  })

  const update = run('update', async () => {
    try {
      // checkForUpdate resolves to the release ONLY when it is genuinely newer, and to null both
      // for "up to date" and "these tags cannot be compared"; it throws when the check could not
      // be carried out. The old `!r.isNewer` test read a field no release object has, so every
      // available update reported itself as "already the latest".
      const r = await checkForUpdate()
      if (!r) return log(t('update.upToDate', CURRENT_VERSION))
      log(t('update.availableTitle', r.tag, r.url))
      edit({ toollastseenupstreamtag: r.tag })
    } catch (e) { log(t('update.failed', ipc.errorText(e))) }
  })

  // The wizard's step 2 needs a save it can point at, because the debounced auto-save above only
  // fires when a value actually changed - and "these defaults are fine" is a legitimate answer to
  // that step. Writes the same object the debounce would have written, then reprobes, which is
  // what flips conf_exists and ticks the step.
  const saveSettings = run('saveSettings', async () => {
    if (!dir) return log(t('install.noFolder'))
    clearTimeout(saveTimer.current)
    try {
      await ipc.saveConfig(dir, cfg)
      await refresh(dir)
      // Say so. The only other feedback is a tick changing colour, and "these defaults are fine"
      // is a deliberate action that deserves an acknowledgement.
      log(t('log.settingsSaved'))
    } catch (e) {
      log(t('config.saveFailed', ipc.errorText(e)))
    }
  })

  const installed = state.dsound_installed && state.patch_installed
  const prot = protectionOf(dir, state)
  const ready = isReady(state)

  // The Setup tab exists when the probe says something is outstanding, or while the user is in it.
  // No step counter anywhere: `showSetup` is a function of disk state and one user intent.
  const showSetup = probed && setup !== 'closed' && (setup === 'open' || !ready)
  const openSetup = () => { setSetup('open'); setTab('setup') }
  const dismissSetup = () => { setSetup('closed'); setTab('settings') }

  // The controlled tab has to give up a selection that no longer has a tab. Tabs falls back to
  // index 0 on its own, so nothing breaks visually without this - but `tab` would stay 'setup'
  // and the strip's aria-selected would disagree with this component about what is showing.
  useEffect(() => {
    if (!showSetup && tab === 'setup') setTab('settings')
  }, [showSetup, tab])

  // The one button that resolves a non-green state, and it exists only when the tab that would
  // otherwise resolve it is gone: dismissed setup plus something still wrong. Everything it does
  // is bring the Setup tab back, so there is exactly one place in this window where installing,
  // browsing and configuring happen as a guided flow.
  // Not for the unsupported-build row: reopening Setup would offer three ticks and nothing to do.
  const canReopen = probed && !showSetup && prot.level !== 'full' && prot.action !== 'none'

  return (
    <div className="app">
      {/* Tab strip and badge are one row. The badge is NOT inside the tablist - a non-tab child of
          role="tablist" is invalid ARIA - so `.chrome` is a grid, the tablist is column 1, the
          badge is column 2, and Tabs' own root is `display: contents` so its two boxes land in
          that grid directly. That is what makes overlap impossible: `1fr auto` cannot let the
          badge sit on top of a tab the way an absolutely positioned one would. */}
      <div className="chrome">
        <Tabs value={tab} onChange={setTab} label={t('tab.list')}>
          {/* First, so it is the leftmost tab while it exists. It is a real tab: the frame never
              changes shape, nothing overlays anything, and Escape has nothing to dismiss. */}
          {showSetup && (
            <Tab id="setup" label={t('tab.setup')} panelClassName="pane">
              <Wizard
                dir={dir}
                state={state}
                cfg={cfg}
                installed={installed}
                launchOption={launchOption}
                busy={busy}
                onEdit={edit}
                onBrowse={browse}
                onSaveSettings={saveSettings}
                onInstall={async () => { setSetup('open'); await install() }}
                onLaunch={launch}
                onCopyPassword={copyPassword}
                onCopyBuildReport={copyBuildReport}
                onDismiss={dismissSetup}
              />
            </Tab>
          )}

          <Tab id="settings" label={t('tab.basic')} panelClassName="pane">
            {/* Top of the pane the user lands on, not tucked into Advanced: it decides whether
                anything below it does anything. Renders nothing unless the build is unsupported. */}
            <BuildNotice build={state.game_build} onCopy={copyBuildReport} />
            <Field label={t('basic.playerName')}>
              <TextInput value={cfg.playername} onValueChange={(v) => edit({ playername: v })} />
            </Field>
            <Field label={t('basic.password')} hint={t('basic.passwordHint')}>
              <SecretInput
                value={cfg.networkpassword}
                onValueChange={(v) => edit({ networkpassword: v })}
                onGenerate={generatePassword}
                onCopy={copyPassword}
                // Passed rather than left to the component's English defaults: a default string
                // inside a component is a string the catalogue cannot reach.
                labels={{
                  show: t('action.show'), hide: t('action.hide'),
                  copy: t('action.copy'), copied: t('action.copied'),
                  copyFailed: t('action.copyFailed'),
                  generate: t('action.generate'),
                }}
              />
            </Field>
            <Checkbox
              label={t('basic.friendsOnly')}
              hint={t('basic.friendsOnlyHint')}
              checked={cfg.isfriendsonly}
              onValueChange={(v) => edit({ isfriendsonly: v })}
            />
            <Checkbox
              label={t('basic.autoInstallMods')}
              hint={t('basic.autoInstallModsHint')}
              checked={cfg.autoinstallmods}
              onValueChange={(v) => edit({ autoinstallmods: v })}
            />
            {/* Moved off Advanced. It is a preference about the button directly below it, which is
                the definition of a setting a normal user touches - Advanced is for the folder,
                the launch option and the two destructive buttons. */}
            <Checkbox
              label={t('basic.autoLaunch')}
              hint={t('basic.autoLaunchHint')}
              checked={cfg.toolautolaunchgame}
              onValueChange={(v) => edit({ toolautolaunchgame: v })}
            />

            {/* Launch sits with the settings, below a hairline, not in a header or an action bar.
                It is the last thing you do on this pane, so it is the last thing on it. */}
            <hr className="rule" />
            <Button variant="primary" onClick={launch} loading={busy === 'launch'}>
              {t('basic.launchGame')}
            </Button>
          </Tab>

          <Tab id="install" label={t('tab.advanced')} panelClassName="pane">
            {/* The path lives here and only here. It is the answer to one question - "which folder
                is this tool pointed at" - and someone setting a player name is not asking it. */}
            <Field label={t('advanced.gameFolder')}>
              {(p) => (
                <div className="inline">
                  <TextInput
                    {...p}
                    value={dir ?? ''}
                    placeholder={t('advanced.gameFolderPlaceholder')}
                    title={dir ?? ''}
                    readOnly
                    mono
                    selectOnFocus
                  />
                  <Button onClick={browse} loading={busy === 'browse'}>{t('action.browse')}</Button>
                </div>
              )}
            </Field>

            {/* Null on Windows, which needs no launch option - so this whole field is Linux-only
                and the window is a row shorter there. */}
            {launchOption && (
              <Field label={t('advanced.steamLaunchOptions')} hint={t('advanced.steamLaunchOptionsHint')}>
                {(p) => (
                  <div className="inline">
                    <TextInput {...p} value={launchOption} readOnly mono selectOnFocus />
                    <CopyButton value={launchOption} labels={{ copy: t('action.copy'), copied: t('action.copied'), failed: t('action.copyFailed') }} />
                  </div>
                )}
              </Field>
            )}

            <div className="list">
              <Setting hint={t('advanced.reinstallHint')}>
                <Button variant="primary" onClick={install} loading={busy === 'install'}>
                  {installed ? t('advanced.reinstall') : t('advanced.installPatch')}
                </Button>
                <Button variant="danger" onClick={uninstall} loading={busy === 'uninstall'}>{t('advanced.uninstallButton')}</Button>
              </Setting>

              {/* The preference and the one-off are the same subject, so they are one row: the
                  checkbox is the setting, the button is "do it now". Written out rather than run
                  through <Setting> because the left half here is a control, not text. */}
              <div className="setting">
                <Checkbox
                  className="setting-grow"
                  label={t('advanced.checkOnLaunch')}
                  checked={cfg.toolcheckforupdatesonlaunch}
                  onValueChange={(v) => edit({ toolcheckforupdatesonlaunch: v })}
                />
                <div className="setting-action">
                  <Button onClick={update} loading={busy === 'update'}>{t('action.checkNow')}</Button>
                </div>
              </div>

              {/* Re-runnable, per the ask. It only ever adds the tab back and selects it - the
                  wizard itself re-probes, so re-running it over a complete install shows three
                  ticks and a Launch button rather than offering an install nobody needs. */}
              <Setting label={t('advanced.runSetup')} hint={t('advanced.runSetupHint')}>
                <Button onClick={openSetup}>{t('advanced.runSetupButton')}</Button>
              </Setting>
            </div>

            {/* The Debug log checkbox was removed with the fork-only DLL that read `debuglog`; the
                current t7patch.dll has no such key, so the box would have done nothing. */}
          </Tab>
        </Tabs>

        {/* Right end of the tab strip row. Dead space before; the whole height of the old spine
            after. The badge is status only - three words, one dot, no button glued to it - and it
            is announced whole because role="status" watches this subtree. */}
        <div className="chrome-status">
          <span
            className="badge"
            role="status"
            data-level={prot.level}
            title={[prot.text, prot.hint].filter(Boolean).join(' ')}
          >
            <span className="badge-dot" aria-hidden="true" />
            {/* The visible word and the announced sentence say the same thing once each: the
                sighted reader gets "Fully protected" plus the detail on hover and in the Setup
                tab, the screen reader gets the whole sentence and never the word twice. */}
            <span className="sr-only">
              {t('protection.aria', t(PROTECTION_LABEL[prot.level]))} {prot.text} {prot.hint}
            </span>
            <span aria-hidden="true">{t(PROTECTION_LABEL[prot.level])}</span>
          </span>
          {canReopen && (
            <Button size="sm" onClick={openSetup}>{t('wizard.reopen')}</Button>
          )}
        </div>
      </div>

      {/* ONE row: the log's own summary on the left, the version pushed to the far right. The
          version is a grid item placed over the summary row rather than a flex sibling of the
          log, which is the whole fix - as a sibling it stole width from the log body for six
          characters, and as a strip of its own it cost a second full-width row. */}
      <div className="logbar">
        <StatusLog
          lines={lines}
          label={t('status.title')}
          emptyText={t('status.empty')}
          clearLabel={t('status.clear')}
          onClear={() => setLines([])}
        />
        <span className="logbar-version">{version ? t('app.version', version) : ''}</span>
      </div>
    </div>
  )
}
