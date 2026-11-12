/**
 * Copy-to-clipboard: the Tauri plugin first, the DOM as a fallback.
 *
 * TOMBSTONE - a draft dropped the plugin for the DOM API alone, to save the 694,872 B it costs.
 * Reversed, because those bytes are not the clipboard: they are the `image` crate, pulled in by
 * arboard's default `image-data` feature for clipboard IMAGES this app never touches, and cargo
 * features are additive so a dependent cannot subtract it. Whole plugin or none.
 *
 * The plugin leads because `navigator.clipboard.writeText` needs a secure context, and on Linux
 * whether Tauri's custom scheme qualifies depends on the WebKitGTK build the distro ships. When it
 * does not, the button silently does nothing. The DOM path stays as the fallback for tests, a
 * browser, or a future build without the plugin.
 */

/**
 * Copies `text`, returning whether it worked rather than throwing. The caller's only reasonable
 * response either way is one line in the status log, and a rejected clipboard write is a normal
 * event on Linux, not an exception.
 *
 * MUST be called from within a user gesture (the click handler itself, not a promise chain after
 * an `await`). Both paths below are gated on one by the engine.
 *
 * @param {string} text
 * @param {object} [deps] test seam
 * @param {{ writeText(t: string): Promise<void> } | undefined} [deps.clipboard]
 * @param {Document | undefined} [deps.doc]
 * @returns {Promise<boolean>}
 */
export async function copyText(text, deps = {}) {
  const {
    // The plugin, injected by the app at startup (see setClipboardWriter). Left undefined in
    // tests and in a plain browser, where the DOM paths below take over.
    clipboard = pluginWriter ?? globalThis.navigator?.clipboard,
    doc = globalThis.document,
  } = deps

  const value = typeof text === 'string' ? text : String(text ?? '')

  if (clipboard && typeof clipboard.writeText === 'function') {
    try {
      await clipboard.writeText(value)
      return true
    } catch {
      // Fall through. The async Clipboard API needs a secure context, and the custom protocol
      // this app is served from is not treated as one by every WebKitGTK build - so on exactly
      // the platform where the hijack is fiddliest, `navigator.clipboard` is either missing or
      // rejects. The old textarea trick still works there.
    }
  }

  return legacyCopy(value, doc)
}

/**
 * @param {string} value
 * @param {Document | undefined} doc
 * @returns {boolean}
 */
function legacyCopy(value, doc) {
  if (!doc || typeof doc.execCommand !== 'function' || !doc.body) return false

  const field = doc.createElement('textarea')
  field.value = value
  // Not `display: none` and not `hidden`: the selection APIs ignore an unrendered element, and
  // execCommand('copy') then copies whatever the user had selected instead - or nothing.
  field.setAttribute('readonly', '')
  field.style.position = 'fixed'
  field.style.top = '0'
  field.style.left = '0'
  field.style.width = '1px'
  field.style.height = '1px'
  field.style.padding = '0'
  field.style.border = 'none'
  field.style.opacity = '0'
  field.style.pointerEvents = 'none'

  doc.body.appendChild(field)
  try {
    field.select()
    field.setSelectionRange(0, value.length)
    return doc.execCommand('copy') === true
  } catch {
    return false
  } finally {
    field.remove()
  }
}

/**
 * The clipboard writer the app prefers, injected once at startup.
 *
 * Injected rather than imported so this module has no `@tauri-apps` import of its own - which is
 * what lets its tests run under plain node with no webview, and what keeps the fallback honest.
 */
let pluginWriter = null

/** @param {{ writeText(t: string): Promise<void> } | null} writer */
export function setClipboardWriter(writer) {
  pluginWriter = writer
}
