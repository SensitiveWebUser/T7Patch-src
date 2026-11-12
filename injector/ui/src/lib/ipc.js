/**
 * The single door between this frontend and anything native.
 *
 * Nothing outside this module imports `@tauri-apps/*`. The command names, argument shapes and
 * error sentinels are a contract with src-tauri/src/main.rs, and a contract restated in eight
 * components is a contract that drifts.
 *
 * TOMBSTONE: `check_update` used to be a command here. It is a `fetch()` in updates.js now,
 * because carrying ureq + rustls + ring to make one unauthenticated GET cost 1,191,360 bytes of
 * binary. Do not add it back to Rust.
 */
import { invoke } from '@tauri-apps/api/core'
import { confirm, open } from '@tauri-apps/plugin-dialog'
import { exit } from '@tauri-apps/plugin-process'

/**
 * Rust returns these instead of prose, so the frontend owns the wording - which is also what
 * makes them translatable.
 *
 * Compared EXACTLY, never by substring or `.includes()`. This app once tested elevation with a
 * substring match against an English sentence; someone reworded the sentence and the prompt
 * stopped firing, silently, because a failed match looks identical to "no elevation needed".
 */
export const NEEDS_ELEVATION = 'NEEDS_ELEVATION'
export const GAME_IS_RUNNING = 'GAME_IS_RUNNING'
/** Carries the missing export: `EMBEDDED_PAYLOAD_INVALID:zbr_run_gamemode_lui`. */
export const EMBEDDED_PAYLOAD_INVALID = 'EMBEDDED_PAYLOAD_INVALID'

/**
 * A rejection is a sentinel only if it arrived as a bare string. Tauri rejects `Err(String)` with
 * the string itself; an Error object is a JS fault, and `String(err)` on one gives
 * "Error: NEEDS_ELEVATION", which must not match.
 */
const sentinelOf = (err) => (typeof err === 'string' ? err : null)

export const isSentinel = (err, sentinel) => sentinelOf(err) === sentinel

/**
 * The export name from `EMBEDDED_PAYLOAD_INVALID:<export>`, or null for any other failure.
 *
 * "" and null are different answers and the caller branches on it: "" still means the payload is
 * wrong, null means this was some other failure entirely. Hence `!== undefined` rather than a
 * truthiness check, which would collapse the two.
 */
export function embeddedPayloadInvalidExport(err) {
  const s = sentinelOf(err)
  if (s === null) return null
  const [code, ...rest] = s.split(':')
  return code === EMBEDDED_PAYLOAD_INVALID && rest.length ? rest.join(':') : null
}

/**
 * A sentinel is a code, not a message - the caller supplies wording for those it recognises. For
 * everything else the original words are the only information there is.
 *
 * The `.message` duck-type is not redundant with `instanceof Error`: a rejection that crossed a
 * realm boundary, or a plain object thrown by a dependency, has a message and fails the
 * instanceof. Without it those render as "[object Object]".
 */
export function errorText(err) {
  const s = sentinelOf(err)
  if (s !== null) return s
  if (err && typeof err === 'object' && typeof err.message === 'string') return err.message
  return String(err)
}

// Swappable so the contract test can record calls without a webview.
let transport = invoke
export function setInvokeTransport(fn) {
  transport = fn || invoke
}
const call = (cmd, args) => transport(cmd, args)

/**
 * @typedef {object} InstallState
 * @property {boolean} dsound_installed
 * @property {boolean} patch_installed
 * @property {boolean} conf_exists
 * @property {boolean} is_game_dir
 * @property {boolean} foreign_dsound a dsound.dll that is NOT ours is what would load
 */

export const locateGame = () => call('locate_game')
export const currentVersion = () => call('current_version')
export const loadConfig = (dir) => call('load_config', { dir })
export const saveConfig = (dir, cfg) => call('save_config', { dir, cfg })
export const install = (dir) => call('install', { dir })
export const uninstall = (dir) => call('uninstall', { dir })
export const launchGame = () => call('launch_game')
/** null on Windows, which needs no launch option. */
export const linuxLaunchOption = () => call('linux_launch_option')
export const elevate = () => call('elevate')
/** @returns {Promise<InstallState>} */
export const installState = (dir) => call('install_state', { dir })
export const injectInto = (pid, dllPath) => call('inject_into', { pid, dllPath })
export const listGameProcesses = () => call('list_game_processes')

/** The native folder picker. Returns null when the user cancels. */
export async function pickFolder(title) {
  const picked = await open({ directory: true, title })
  return typeof picked === 'string' ? picked : null
}

export const confirmUser = (text, opts) => confirm(text, opts)
export const exitApp = (code = 0) => exit(code)
