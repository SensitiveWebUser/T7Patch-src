import { useEffect, useRef, useState } from 'react'
import Button from './Button.jsx'
import { copyText } from '../lib/clipboard.js'

/**
 * Copy, with a transient confirmation on the button itself rather than a toast.
 *
 * The label swap IS the feedback - a toast for "copied" is a notification the user has to dismiss
 * for something they just did on purpose and can see the result of.
 *
 * `onCopy(value)` - optional. Replaces the built-in copy and must resolve to whether it worked.
 */
export default function CopyButton({ value, labels = {}, variant = 'default', size, onCopy, disabled }) {
  const { copy = 'Copy', copied = 'Copied', failed = 'Copy failed' } = labels
  const [state, setState] = useState('idle')
  const timer = useRef(null)

  // Cleared on unmount: this fires 1.2s later and would otherwise set state on a gone component.
  useEffect(() => () => clearTimeout(timer.current), [])

  const onClick = async () => {
    // onCopy was accepted by every call site and silently dropped here, so the caller's own
    // feedback - the status-log line for a copied password, in SecretInput's case - never fired.
    // When a caller supplies one it owns the copy AND the reporting; otherwise this does both.
    //
    // The caller's boolean is the contract, compared strictly. An onCopy that forgets to return
    // copyText's result shows "Copy failed" on a copy that worked - loud, and found the first time
    // anyone clicks. Treating undefined as success would instead show "Copied" on the Linux
    // WebKitGTK builds where the clipboard genuinely refuses, which nobody would ever notice.
    let ok
    try {
      ok = onCopy ? (await onCopy(value)) === true : await copyText(value)
    } catch {
      ok = false
    }
    setState(ok ? 'copied' : 'failed')
    clearTimeout(timer.current)
    timer.current = setTimeout(() => setState('idle'), 1200)
  }

  return (
    <Button variant={variant} size={size} disabled={disabled} onClick={onClick}>
      {state === 'copied' ? copied : state === 'failed' ? failed : copy}
    </Button>
  )
}
