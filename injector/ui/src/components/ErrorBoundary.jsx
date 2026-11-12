import { Component } from 'react'
import Button from './Button.jsx'
import CopyButton from './CopyButton.jsx'

/**
 * Everything a bug report needs, as one block of plain text.
 *
 * Defensive to the point of paranoia because it runs AFTER something has already gone wrong: a
 * `stack` getter that throws, or a thrown value that is not an Error at all, would replace the
 * recovery pane with the blank window it exists to prevent.
 */
function describe(error, info) {
  const parts = []
  try {
    parts.push(String(error?.stack || error?.message || error))
  } catch {
    parts.push('the error could not be converted to text')
  }
  try {
    const stack = info?.componentStack
    if (stack) parts.push(String(stack).trim())
  } catch {
    // The component stack is a bonus, never a reason to fail a second time.
  }
  return parts.join('\n\n')
}

/**
 * Catches a render-time throw anywhere below it and shows a recovery pane instead of nothing.
 *
 * This is not decoration. The Rust side installs a panic hook that writes `T7Patch-error.log` next
 * to the executable, precisely because the release profile's `panic = "abort"` + `strip = true`
 * turn a panic into a silent instant exit - and the same reasoning applies with equal force on
 * this side of the bridge. An uncaught throw in render unmounts the entire tree: the window stays
 * open, completely blank, with no message, no log and nothing to click. This is the frontend's
 * half of the same commitment.
 *
 * The error text gets a Copy button rather than a "send report" button because there is nowhere to
 * send it. Pasting it into an issue is the whole workflow.
 *
 * `onError(error, info)` is the hook for persisting it; the natural implementation is a Tauri
 * command appending to that same `T7Patch-error.log`, so a frontend crash and a backend panic end
 * up in one file.
 *
 * The pane is built out of `.card`, `.log-body` and `.btn` - classes that already exist for other
 * reasons - rather than styles of its own. A crash screen that depends on CSS which might itself
 * be what is broken is not a crash screen.
 *
 * An error boundary CANNOT catch: throws from event handlers, throws inside `setTimeout` or a
 * promise, or a throw from its own render. Those still need `try`/`catch` at the call site and a
 * `window.onunhandledrejection` handler at the app level.
 */
export default class ErrorBoundary extends Component {
  constructor(props) {
    super(props)
    this.state = { error: null, info: null }
    this.reset = this.reset.bind(this)
  }

  static getDerivedStateFromError(error) {
    // A thrown falsy value (`throw null`, `throw undefined`) would leave `error` falsy and render
    // children again, straight back into the same throw.
    return { error: error ?? new Error('a falsy value was thrown') }
  }

  componentDidCatch(error, info) {
    this.setState({ info })
    try {
      // Console first, unconditionally: the webview inspector is the only place this surfaces if
      // the app's own reporter is what is broken.
      console.error('T7Patch UI: uncaught render error', error, info)
      this.props.onError?.(error, info)
    } catch {
      // A reporter that throws must not replace the error it was reporting.
    }
  }

  reset() {
    this.setState({ error: null, info: null })
    try {
      this.props.onReset?.()
    } catch {
      // The caller's cleanup failing must not leave the pane stuck on screen.
    }
  }

  render() {
    const { error, info } = this.state
    if (!error) return this.props.children

    const { fallback, labels = {} } = this.props
    if (typeof fallback === 'function') return fallback({ error, info, reset: this.reset })
    if (fallback !== undefined && fallback !== null) return fallback

    const {
      title = 'T7Patch hit an unexpected error',
      message = 'Nothing on disk was changed by this. Copy the details below into a bug report, then try again.',
      detailsLabel = 'Error details',
      retry = 'Try again',
      copy = 'Copy details',
      copied = 'Copied',
      copyFailed = 'Copy failed',
    } = labels

    const detail = describe(error, info)

    return (
      <div className="error-pane" role="alert">
        <section className="card">
          <div className="card-head">
            <h1 className="card-title">{title}</h1>
          </div>
          <p className="card-desc">{message}</p>
          <div className="card-body">
            <pre className="log-body" tabIndex={0} aria-label={detailsLabel}>
              {detail}
            </pre>
          </div>
          <div className="card-foot">
            <CopyButton value={detail} labels={{ copy, copied, failed: copyFailed }} />
            <Button variant="primary" onClick={this.reset}>
              {retry}
            </Button>
          </div>
        </section>
      </div>
    )
  }
}
