import { Children, forwardRef, isValidElement, useId, useRef, useState } from 'react'
import './Tabs.css'

/**
 * Declares one tab. Configuration only - `Tabs` reads these props and renders the markup itself,
 * so this never renders on its own and returning null is correct rather than a stub.
 *
 * Props: `id` (required, stable), `label` (the tab's visible text), `disabled`, `children` (the
 * panel content), `className` (extra classes on the tab button), `panelClassName`.
 */
export function Tab() {
  return null
}

function nextEnabled(items, from, delta) {
  const n = items.length
  let index = from
  for (let i = 0; i < n; i += 1) {
    index = (index + delta + n) % n
    if (!items[index].disabled) return index
  }
  return from
}

function firstEnabled(items, from, delta) {
  for (let i = from; i >= 0 && i < items.length; i += delta) {
    if (!items[i].disabled) return i
  }
  return from
}

/**
 * The WAI-ARIA tabs pattern.
 *
 * TOMBSTONE - what this replaces and why it was wrong: the previous implementation marked the
 * SELECTED tab `disabled` to make it look pressed (App.jsx:228-229). That is three defects in one
 * attribute. A disabled button is out of the tab order, so the selected tab could not be reached
 * by keyboard at all; it is announced as unavailable, which is the opposite of selected; and there
 * was no `role="tab"`, `aria-selected` or `aria-controls` anywhere, so nothing connected the two
 * buttons to the content below them. Selection is `aria-selected` plus a class. It is never
 * `disabled` - Tabs.css deliberately defines no `:disabled` style for `.tab` to fall back on.
 *
 * Panels stay MOUNTED and are toggled with the `hidden` attribute rather than conditionally
 * rendered, so per-panel state - scroll position, a half-typed field, an open `<details>` -
 * survives a tab switch. `hidden` also removes the panel from the accessibility tree and the tab
 * order, which conditional rendering gets for free but a CSS `display` toggle does not.
 *
 * Activation is automatic by default (selection follows focus), which is what the APG recommends
 * when panels are already mounted and switching is instant - as it is here. Pass
 * `activation="manual"` to require Enter or Space.
 *
 * Layout: `.tabs` is the tab strip itself and `.tabpanel` is the content, per Tabs.css. They are
 * wrapped in `.tabs-root`, which the frame is expected to lay out as a column
 * (`display:flex; flex-direction:column; min-height:0`) so the strip stays put and only the panel
 * scrolls.
 */
const Tabs = forwardRef(function Tabs(
  {
    value,
    defaultValue,
    onChange,
    label,
    labelledBy,
    orientation = 'horizontal',
    activation = 'automatic',
    className = '',
    tablistClassName = '',
    children,
    ...rest
  },
  ref,
) {
  const base = useId()
  const tabNodes = useRef(new Map())

  const items = Children.toArray(children)
    .filter((child) => isValidElement(child) && child.type === Tab)
    .map((child, index) => ({
      key: String(child.props.id ?? index),
      label: child.props.label,
      disabled: Boolean(child.props.disabled),
      className: child.props.className,
      panelClassName: child.props.panelClassName,
      content: child.props.children,
    }))

  const controlled = value !== undefined
  const [internal, setInternal] = useState(() => defaultValue ?? items[0]?.key)
  const selected = controlled ? value : internal

  // Falling back to index 0 rather than rendering nothing: a `value` matching no tab is a caller
  // bug, and a window with a tab strip and no panel under it is far harder to diagnose than a
  // window showing the wrong tab.
  const found = items.findIndex((item) => item.key === selected)
  const activeIndex = found >= 0 ? found : 0

  const select = (key) => {
    if (!controlled) setInternal(key)
    if (key !== selected) onChange?.(key)
  }

  const handleKeyDown = (index) => (event) => {
    const vertical = orientation === 'vertical'
    let target = null

    if (event.key === (vertical ? 'ArrowDown' : 'ArrowRight')) {
      target = nextEnabled(items, index, 1)
    } else if (event.key === (vertical ? 'ArrowUp' : 'ArrowLeft')) {
      target = nextEnabled(items, index, -1)
    } else if (event.key === 'Home') {
      target = firstEnabled(items, 0, 1)
    } else if (event.key === 'End') {
      target = firstEnabled(items, items.length - 1, -1)
    } else {
      return
    }

    event.preventDefault()
    // Focus always moves; selection follows only under automatic activation. That split is what
    // makes manual activation work at all - the user arrows along the strip and commits with
    // Enter, and a tab holding focus without being selected is the intended intermediate state.
    tabNodes.current.get(items[target]?.key)?.focus()
    if (activation === 'automatic') select(items[target].key)
  }

  return (
    <div ref={ref} className={['tabs-root', className].filter(Boolean).join(' ')} {...rest}>
      <div
        role="tablist"
        aria-label={labelledBy ? undefined : label}
        aria-labelledby={labelledBy}
        aria-orientation={orientation}
        className={['tabs', tablistClassName].filter(Boolean).join(' ')}
      >
        {items.map((item, index) => {
          const isSelected = index === activeIndex
          return (
            <button
              key={item.key}
              ref={(node) => {
                if (node) tabNodes.current.set(item.key, node)
                else tabNodes.current.delete(item.key)
              }}
              type="button"
              role="tab"
              id={`${base}-tab-${item.key}`}
              aria-controls={`${base}-panel-${item.key}`}
              aria-selected={isSelected}
              // aria-disabled, never the native attribute: a disabled tab cannot be focused, so it
              // cannot be discovered, so the user never learns the tab exists or why it is off.
              aria-disabled={item.disabled || undefined}
              // Roving tabindex: exactly one tab is in the page's tab order, and the arrow keys
              // move within the strip. Without it, Tab steps through every tab before reaching the
              // panel - which is the whole difference between a tablist and a row of buttons.
              tabIndex={isSelected ? 0 : -1}
              className={[
                'tab',
                item.disabled ? 'is-disabled' : '',
                item.className,
              ]
                .filter(Boolean)
                .join(' ')}
              onClick={() => {
                if (item.disabled) return
                select(item.key)
              }}
              onKeyDown={handleKeyDown(index)}
            >
              {item.label}
            </button>
          )
        })}
      </div>

      {items.map((item, index) => (
        <div
          key={item.key}
          role="tabpanel"
          id={`${base}-panel-${item.key}`}
          aria-labelledby={`${base}-tab-${item.key}`}
          hidden={index !== activeIndex}
          className={['tabpanel', item.panelClassName].filter(Boolean).join(' ')}
        >
          {item.content}
        </div>
      ))}
    </div>
  )
})

// No tabIndex on the panels. The APG only asks for one when a panel holds nothing focusable, and
// adding it unconditionally puts a stop in the tab order between the strip and the first control
// of every panel - exactly the extra keystroke the roving tabindex above exists to remove.

Tabs.Tab = Tab

export default Tabs
