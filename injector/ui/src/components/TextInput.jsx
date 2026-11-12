import { forwardRef } from 'react'
import './TextInput.css'

/**
 * A single-line text input.
 *
 * Change is reported twice, on purpose: `onValueChange(value)` for the common case, `onChange(event)`
 * for the cases that need the event. Neither is more correct than the other and picking one forces
 * every call site into a wrapper lambda, so both are supported and both fire.
 *
 * `invalid` is only for standalone use. Inside a `<Field>` the `aria-invalid` prop arrives through
 * the spread and wins, because `{...rest}` is applied last.
 *
 * `readOnly` is NOT `disabled`. The read-only fields here (game path, Steam launch option) exist to
 * be read and copied, so they have to stay focusable and selectable; disabling them would take them
 * out of the tab order and put their text out of reach of a keyboard user entirely.
 */
const TextInput = forwardRef(function TextInput(
  {
    value,
    onChange,
    onValueChange,
    onFocus,
    type = 'text',
    invalid = false,
    mono = false,
    selectOnFocus = false,
    className = '',
    ...rest
  },
  ref,
) {
  const classes = ['input', mono ? 'input-mono' : '', invalid ? 'is-invalid' : '', className]
    .filter(Boolean)
    .join(' ')

  return (
    <input
      ref={ref}
      type={type}
      className={classes}
      value={value}
      aria-invalid={invalid || undefined}
      onChange={(event) => {
        onValueChange?.(event.target.value)
        onChange?.(event)
      }}
      onFocus={(event) => {
        // For read-only paths and launch options: the whole point of showing them is that they get
        // copied, and selecting on focus makes the keyboard route as short as the Copy button.
        if (selectOnFocus) event.target.select()
        onFocus?.(event)
      }}
      {...rest}
    />
  )
})

export default TextInput
