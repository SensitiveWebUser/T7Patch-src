import { forwardRef, useEffect, useRef, useState } from 'react'
import './StatusLog.css'

// `lib/format.js` builds log lines as "[12:00:01] Installed." - one string, because that is what
// gets copied and what the .NET tool's log looked like. StatusLog.css wants the timestamp in its
// own column so the messages line up, so the string is split back apart HERE rather than changing
// the stored shape: the array stays plain strings that can be joined and pasted, and the split is
// purely presentational. A line that is not in that shape is rendered whole, with no timestamp.
const STAMPED = /^\[([^\]]{1,16})\]\s(.*)$/s

function splitLine(line) {
  if (typeof line !== 'string') {
    return { time: line?.time ?? null, text: line?.text ?? '' }
  }
  const m = STAMPED.exec(line)
  return m ? { time: m[1], text: m[2] } : { time: null, text: line }
}

/**
 * A collapsible diagnostic log built on `<details>`.
 *
 * The announcer is the subtle part. Content inside a CLOSED `<details>` is hidden from the
 * accessibility tree, so a live region in there announces nothing at all - which is precisely the
 * case that matters, because the log is collapsed by default and the whole point is to hear
 * "Installed" without opening it. So the newest line is mirrored into a visually hidden live
 * region OUTSIDE the `<details>`, which is always in the tree.
 *
 * For the same reason the body is NOT `role="log"`. That role implies `aria-live="polite"`, which
 * would announce nothing while collapsed and duplicate the announcer while open.
 *
 * Only the message is announced, never the timestamp - "twelve oh oh oh one" before every line is
 * noise, and the time is on screen for anyone who wants it.
 *
 * Returns a fragment - the announcer, then the `<details>` - so it adds no layout box of its own.
 * `.sr-only` is out of flow, so the `<details>` is the only thing the parent's layout sees.
 */
const StatusLog = forwardRef(function StatusLog(
  {
    lines = [],
    label = 'Status',
    emptyText = 'Nothing logged yet.',
    open: openProp,
    defaultOpen = false,
    onOpenChange,
    announce = true,
    onClear,
    clearLabel = 'Clear',
    formatCount = (count) => String(count),
    className = '',
    ...rest
  },
  ref,
) {
  const [internalOpen, setInternalOpen] = useState(defaultOpen)
  const open = openProp === undefined ? internalOpen : openProp
  const body = useRef(null)
  const pinned = useRef(true)

  const rows = lines.map(splitLine)
  const latest = rows.length ? rows[rows.length - 1].text : ''

  useEffect(() => {
    const node = body.current
    if (!node || !pinned.current) return
    node.scrollTop = node.scrollHeight
  }, [latest, rows.length, open])

  return (
    <>
      {announce && (
        <p className="sr-only" aria-live="polite" aria-atomic="true">
          {latest}
        </p>
      )}

      <details
        ref={ref}
        className={['log', className].filter(Boolean).join(' ')}
        open={open}
        onToggle={(event) => {
          const next = event.currentTarget.open
          if (openProp === undefined) setInternalOpen(next)
          onOpenChange?.(next)
        }}
        {...rest}
      >
        <summary className="log-summary">
          <span className="log-label">{label}</span>
          <span className="log-count">{formatCount(rows.length)}</span>
          {onClear && rows.length > 0 && (
            // Inside the <summary>, so it costs no row of its own in a small window. stopPropagation
            // because a click anywhere in a summary toggles the <details>, and clearing the log
            // should not also collapse the pane you are reading.
            <button
              type="button"
              className="log-clear"
              onClick={(event) => {
                event.preventDefault()
                event.stopPropagation()
                onClear()
              }}
            >
              {clearLabel}
            </button>
          )}
        </summary>

        <div
          ref={body}
          className="log-body"
          // A scrollable region needs a tab stop or a keyboard user cannot scroll it: there is
          // nothing focusable inside for them to arrow from.
          tabIndex={0}
          onScroll={(event) => {
            // Only auto-scroll while the user is already at the bottom. Yanking the view back down
            // while someone is reading an earlier line makes a log unreadable exactly when it is
            // being read.
            const el = event.currentTarget
            pinned.current = el.scrollHeight - el.scrollTop - el.clientHeight < 8
          }}
        >
          {rows.length === 0 ? (
            <p className="log-empty">{emptyText}</p>
          ) : (
            rows.map((row, index) => (
              // Index keys: the array is append-only and capped, so an index never comes to mean a
              // different line than it did on the previous render.
              <div className="log-line" key={index}>
                {row.time && <span className="log-time">{row.time}</span>}
                <span className="log-text">{row.text}</span>
              </div>
            ))
          )}
        </div>
      </details>
    </>
  )
})

export default StatusLog
