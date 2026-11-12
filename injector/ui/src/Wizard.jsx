import { useState } from 'react'
import { t } from './i18n/index.js'
import Button from './components/Button.jsx'
import Field from './components/Field.jsx'
import TextInput from './components/TextInput.jsx'
import SecretInput from './components/SecretInput.jsx'
import Checkbox from './components/Checkbox.jsx'
import CopyButton from './components/CopyButton.jsx'

/**
 * First-run setup, as the content of a tab.
 *
 * WHY THIS EXISTS AGAIN. A five-step wizard was removed because it "looks really bad and seems
 * wrong as I already have an install and it still suggests adding a new one". The second half of
 * that was a backend bug (is_our_dsound compared the installed dsound.dll byte-for-byte against
 * the embedded copy, so any rebuild made a real install look foreign) and it is fixed. The first
 * half was the design, and this is the part that must not come back:
 *
 *   - No step counter. Nothing here knows "which step we are on". `done` for every step is read
 *     out of install_state, so a step that is already satisfied renders as satisfied the first
 *     time it is painted, and the step that is expanded is simply the first unsatisfied one.
 *   - No Welcome step and no Done step. Both were pure ceremony around one decision; a step whose
 *     only content is prose is a page-turn, not a step. The intro is one sentence above the list.
 *   - No unconditional literals. The predecessor's Done step printed "T7Patch is installed" no
 *     matter what happened. Every sentence below is chosen by a probed boolean.
 *
 * The four rows are an accordion, not a sequence: any of them can be opened at any time, in any
 * order, and none of them gates another. That is what makes "already satisfied - leave it alone"
 * the default reading rather than "four things you must click through".
 *
 * Nothing here talks to ipc. Every action is a callback App already owns, so the sentinel
 * handling, the elevation-then-exit path and the debounced save exist in exactly one place.
 */
export default function Wizard({
  dir,
  state,
  cfg,
  installed,
  launchOption,
  busy,
  onEdit,
  onBrowse,
  onSaveSettings,
  onInstall,
  onLaunch,
  onCopyPassword,
  onDismiss,
}) {
  // Which row the user has pulled open, or null for "whichever one is outstanding". Cleared by
  // every action below, so finishing a step hands the expansion to the next unsatisfied one
  // instead of leaving the user looking at a row they have just completed.
  const [opened, setOpened] = useState(null)

  const folderDone = state.is_game_dir
  const settingsDone = state.conf_exists
  const patchDone = state.dsound_installed && state.patch_installed && !state.foreign_dsound
  const allDone = folderDone && settingsDone && patchDone

  const act = (fn) => async () => { setOpened(null); await fn() }

  const steps = [
    {
      id: 'folder',
      title: t('wizard.stepFolder'),
      done: folderDone,
      body: (
        <>
          <div className="inline">
            <TextInput
              value={dir ?? ''}
              placeholder={t('advanced.gameFolderPlaceholder')}
              title={dir ?? ''}
              aria-label={t('advanced.gameFolder')}
              readOnly
              mono
              selectOnFocus
            />
            <Button variant={folderDone ? 'default' : 'primary'} onClick={act(onBrowse)} loading={busy === 'browse'}>
              {t('action.browse')}
            </Button>
          </div>
          {/* Three different facts, never one literal: the exe is here, the folder is wrong, or
              there is no folder yet. */}
          <p className="wiz-note">
            {folderDone ? t('wizard.folderFound') : dir ? t('state.notGameDir') : t('protection.actionBrowse')}
          </p>
        </>
      ),
    },
    {
      id: 'settings',
      title: t('wizard.stepSettings'),
      done: settingsDone,
      body: (
        <>
          <Field label={t('basic.playerName')}>
            <TextInput value={cfg.playername} onValueChange={(v) => onEdit({ playername: v })} />
          </Field>
          <Field label={t('basic.password')} hint={t('basic.passwordHint')}>
            <SecretInput
              value={cfg.networkpassword}
              onValueChange={(v) => onEdit({ networkpassword: v })}
              onCopy={onCopyPassword}
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
            checked={cfg.isfriendsonly}
            onValueChange={(v) => onEdit({ isfriendsonly: v })}
          />
          <div className="wiz-act">
            <p className="wiz-note">{settingsDone ? t('wizard.settingsSaved') : t('wizard.settingsNew')}</p>
            {/* Typing already saves - App debounces at 750ms. This button is for the user who
                looks at the defaults and agrees with them, which the debounce never hears about
                and which would otherwise leave the row untickable forever. */}
            <Button
              variant={settingsDone ? 'default' : 'primary'}
              onClick={act(onSaveSettings)}
              loading={busy === 'save'}
              disabled={!dir}
            >
              {t('wizard.saveSettings')}
            </Button>
          </div>
        </>
      ),
    },
    {
      id: 'patch',
      title: t('wizard.stepInstall'),
      done: patchDone,
      body: (
        <>
          <p className="wiz-note">
            {state.foreign_dsound
              ? <>{t('protection.foreignBlocked')} {t('protection.actionMoveAside')}</>
              : patchDone ? t('wizard.installDone') : t('wizard.installTodo')}
          </p>
          <div className="wiz-act wiz-act-end">
            <Button
              variant={patchDone ? 'default' : 'primary'}
              onClick={act(onInstall)}
              loading={busy === 'install'}
              disabled={!dir}
            >
              {installed ? t('advanced.reinstall') : t('advanced.installPatch')}
            </Button>
          </div>
        </>
      ),
    },
    {
      id: 'play',
      title: t('wizard.stepPlay'),
      done: allDone,
      body: (
        <>
          {/* onboard.doneBody is the predecessor's Wizard_DoneSummary, and it is the sentence that
              used to print unconditionally. It renders here only behind allDone, which is three
              probed booleans - the same three the ticks above are drawn from. */}
          <p className="wiz-note">{allDone ? t('onboard.doneBody') : t('wizard.playBlocked')}</p>
          {/* Linux only, and only once there is something to launch: on Proton the hijack does
              nothing until this string is in the launch options, so leaving it to the Advanced tab
              would make "all set" a lie on exactly one platform. */}
          {allDone && launchOption && (
            <Field label={t('advanced.steamLaunchOptions')} hint={t('advanced.steamLaunchOptionsHint')}>
              {(p) => (
                <div className="inline">
                  <TextInput {...p} value={launchOption} readOnly mono selectOnFocus />
                  <CopyButton
                    value={launchOption}
                    labels={{ copy: t('action.copy'), copied: t('action.copied'), failed: t('action.copyFailed') }}
                  />
                </div>
              )}
            </Field>
          )}
          <div className="wiz-act wiz-act-end">
            <Button variant="primary" onClick={onLaunch} loading={busy === 'launch'} disabled={!allDone}>
              {t('basic.launchGame')}
            </Button>
          </div>
        </>
      ),
    },
  ]

  // The expanded row is the first unsatisfied one - a derived index, never a stored cursor. When
  // everything is satisfied it is the last row, which is the only row with anything left to offer.
  const outstanding = steps.findIndex((s) => !s.done)
  const current = outstanding === -1 ? steps.length - 1 : outstanding
  const chosen = opened === null ? -1 : steps.findIndex((s) => s.id === opened)
  const active = chosen === -1 ? current : chosen

  return (
    <div className="wiz">
      <p className="wiz-intro">{allDone ? t('wizard.introDone') : t('wizard.intro')}</p>

      <ol className="wiz-steps">
        {steps.map((step, index) => {
          const isOpen = index === active
          return (
            <li key={step.id} className="wiz-step" data-done={step.done ? '' : undefined}>
              <h2 className="wiz-h">
                <button
                  type="button"
                  className="wiz-head"
                  aria-expanded={isOpen}
                  aria-controls={`wiz-body-${step.id}`}
                  // Clicking the open row closes it, which hands the expansion back to whichever
                  // row is actually outstanding. Clicking the outstanding row is therefore a
                  // no-op by construction, and it should be: it is the one that has to stay open.
                  onClick={() => setOpened(isOpen ? null : step.id)}
                >
                  <span className="wiz-mark" aria-hidden="true">{step.done ? '✓' : '○'}</span>
                  <span className="wiz-name">{step.title}</span>
                  <span className="sr-only">{step.done ? t('wizard.stepDone') : t('wizard.stepTodo')}</span>
                </button>
              </h2>
              <div id={`wiz-body-${step.id}`} className="wiz-body" hidden={!isOpen}>
                {step.body}
              </div>
            </li>
          )
        })}
      </ol>

      {/* One button, and it is the only thing in this pane that is not a step. Dismissible was an
          explicit requirement; "Finish" and "Skip setup" are the same action with the honest word
          for what state it is being taken in. */}
      <div className="wiz-foot">
        <Button variant={allDone ? 'primary' : 'default'} onClick={onDismiss}>
          {allDone ? t('action.finish') : t('action.skipSetup')}
        </Button>
      </div>
    </div>
  )
}
