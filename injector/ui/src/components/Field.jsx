import { cloneElement, isValidElement, useId } from 'react'
import './Field.css'

function join(...parts) {
  const kept = parts.filter(Boolean)
  return kept.length ? kept.join(' ') : undefined
}

/**
 * Label + control + hint + error, wired together by id.
 *
 * The wiring is the entire point, and it is the part that is always wrong when it is hand-rolled
 * per call site: `htmlFor` needs an id the control actually has, `aria-describedby` needs the hint
 * to exist in the DOM, and `aria-errormessage` is IGNORED unless `aria-invalid` is also set.
 *
 * The child gets those props one of two ways:
 *
 *   <Field label="Player name"><TextInput value={x} onValueChange={f} /></Field>
 *   <Field label="Player name">{(p) => <TextInput {...p} value={x} onValueChange={f} />}</Field>
 *
 * The function form is for controls that need the props somewhere other than their root element.
 * The element form clones, MERGING any `aria-describedby` the child already carries rather than
 * replacing it.
 *
 * The control is a direct grid child - there is no wrapper element - because `.field-inline` puts
 * label and control in two grid columns and an extra box between them would break that.
 *
 * The error node is rendered even when there is no error. A live region has to be in the DOM
 * *before* its content changes or the change is not announced. The hint stays in
 * `aria-describedby` and the error stays in `aria-errormessage` deliberately: putting the error in
 * both is how you get it read out twice.
 */
export default function Field({
  label,
  hint,
  error,
  required = false,
  inline = false,
  id: idProp,
  group = false,
  requiredMark = '*',
  className = '',
  children,
  ...rest
}) {
  const base = useId()
  const id = idProp || `${base}-control`
  const labelId = `${base}-label`
  const hintId = `${base}-hint`
  const errorId = `${base}-error`

  const invalid = Boolean(error)
  const controlProps = {
    id,
    'aria-describedby': hint ? hintId : undefined,
    'aria-invalid': invalid || undefined,
    'aria-errormessage': invalid ? errorId : undefined,
    required: required || undefined,
  }

  let control
  if (typeof children === 'function') {
    control = children(controlProps)
  } else if (isValidElement(children)) {
    control = cloneElement(children, {
      ...controlProps,
      // An id the caller put on the child wins - they may already be pointing something at it.
      id: children.props.id ?? controlProps.id,
      'aria-describedby': join(children.props['aria-describedby'], controlProps['aria-describedby']),
    })
  } else {
    control = children
  }

  const labelBody = label ? (
    <>
      {label}
      {required && (
        <span className="field-required" aria-hidden="true">
          {requiredMark}
        </span>
      )}
    </>
  ) : null

  const classes = ['field', inline ? 'field-inline' : '', className].filter(Boolean).join(' ')

  const body = (
    <>
      {labelBody &&
        // `group` is for a field whose control is really several controls (a radio set, a
        // segmented toggle). `htmlFor` can only point at one element, so those get a labelled
        // group and a plain span instead of a <label>.
        (group ? (
          <span className="field-label" id={labelId}>
            {labelBody}
          </span>
        ) : (
          <label className="field-label" htmlFor={id}>
            {labelBody}
          </label>
        ))}
      {control}
      {hint && (
        <p className="field-hint" id={hintId}>
          {hint}
        </p>
      )}
      <p className="field-error" id={errorId} aria-live="polite">
        {error || ''}
      </p>
    </>
  )

  return group ? (
    <div className={classes} role="group" aria-labelledby={label ? labelId : undefined} {...rest}>
      {body}
    </div>
  ) : (
    <div className={classes} {...rest}>
      {body}
    </div>
  )
}
