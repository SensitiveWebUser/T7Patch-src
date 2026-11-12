import { forwardRef } from 'react'
import './Button.css'

const VARIANTS = new Set(['default', 'primary', 'danger', 'ghost'])
const SIZES = new Set(['sm', 'md', 'lg'])

/**
 * A button. Generic: it knows nothing about installing, launching or config.
 *
 * `disabled` and `loading` are deliberately NOT the same thing:
 *
 * - `disabled` sets the native attribute, which removes the button from the tab order. That is
 *   correct for an action that is genuinely unavailable, because there is nothing to wait for.
 * - `loading` sets `aria-disabled` + `aria-busy` and swallows the click instead. A natively
 *   disabled element loses focus the moment it is disabled and the browser dumps focus on
 *   `<body>` - so a keyboard user who presses Install is thrown to the top of the window for the
 *   duration of the install and has to tab all the way back. Keeping the button focusable while
 *   busy is the whole reason for the split.
 *
 * `loadingLabel` is announced, not shown: `aria-busy` alone is advisory and most screen readers
 * say nothing about it, so a word like "Installing" is the only thing a non-sighted user gets.
 */
const Button = forwardRef(function Button(
  {
    variant = 'default',
    size = 'md',
    block = false,
    loading = false,
    loadingLabel,
    disabled = false,
    type = 'button',
    className = '',
    onClick,
    children,
    ...rest
  },
  ref,
) {
  const classes = [
    'btn',
    VARIANTS.has(variant) && variant !== 'default' ? `btn-${variant}` : '',
    SIZES.has(size) && size !== 'md' ? `btn-${size}` : '',
    block ? 'btn-block' : '',
    loading ? 'btn-loading' : '',
    className,
  ]
    .filter(Boolean)
    .join(' ')

  return (
    <button
      ref={ref}
      type={type}
      className={classes}
      disabled={disabled || undefined}
      aria-disabled={loading || undefined}
      aria-busy={loading || undefined}
      onClick={(event) => {
        // aria-disabled does not stop the event the way the native attribute does, so the guard
        // has to be here. preventDefault as well as returning early, because a `type="submit"`
        // button would otherwise still submit its form while busy.
        if (loading) {
          event.preventDefault()
          event.stopPropagation()
          return
        }
        onClick?.(event)
      }}
      {...rest}
    >
      {loading && <span className="btn-spinner" aria-hidden="true" />}
      {children}
      {loading && loadingLabel && <span className="sr-only">{loadingLabel}</span>}
    </button>
  )
})

export default Button
