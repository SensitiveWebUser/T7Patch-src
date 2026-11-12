import { forwardRef, useEffect, useId, useRef } from 'react'
import './Checkbox.css'

function join(...parts) {
  const kept = parts.filter(Boolean)
  return kept.length ? kept.join(' ') : undefined
}

/**
 * A checkbox with its own label, so it is not wrapped in a `<Field>`: `<Field>` associates one
 * label with one control by id, which is exactly what this already is, and wrapping it produces
 * two labels for one input that screen readers read one after the other.
 *
 * The row is a `<div>` with an explicit `<label htmlFor>`, not a wrapping `<label>`. A wrapping
 * label would swallow the hint into the control's accessible NAME - so the name becomes
 * "Friends only Only people on your Steam friends list can join" - instead of leaving it as a
 * description the user can skip.
 *
 * `indeterminate` has no HTML attribute (it is an IDL property only), so it has to be written to
 * the node after render. That is the only reason this keeps an internal ref beside the forwarded
 * one.
 */
const Checkbox = forwardRef(function Checkbox(
  {
    checked,
    defaultChecked,
    onChange,
    onValueChange,
    indeterminate = false,
    label,
    hint,
    disabled = false,
    id: idProp,
    className = '',
    ...rest
  },
  ref,
) {
  const auto = useId()
  const id = idProp || `${auto}-checkbox`
  const hintId = `${auto}-hint`
  const node = useRef(null)

  useEffect(() => {
    if (node.current) node.current.indeterminate = indeterminate
  }, [indeterminate, checked])

  const setRef = (el) => {
    node.current = el
    if (typeof ref === 'function') ref(el)
    else if (ref) ref.current = el
  }

  return (
    <div className={['check', disabled ? 'is-disabled' : '', className].filter(Boolean).join(' ')}>
      <input
        ref={setRef}
        type="checkbox"
        id={id}
        checked={checked}
        defaultChecked={defaultChecked}
        disabled={disabled || undefined}
        onChange={(event) => {
          onValueChange?.(event.target.checked)
          onChange?.(event)
        }}
        {...rest}
        aria-describedby={join(rest['aria-describedby'], hint ? hintId : undefined)}
      />
      <div className="check-body">
        {label && (
          <label className="check-label" htmlFor={id}>
            {label}
          </label>
        )}
        {hint && (
          <p className="check-hint" id={hintId}>
            {hint}
          </p>
        )}
      </div>
    </div>
  )
})

export default Checkbox
