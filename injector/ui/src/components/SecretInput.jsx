import { forwardRef, useId, useState } from 'react'
import Button from './Button.jsx'
import CopyButton from './CopyButton.jsx'
import { generatePassword } from '../lib/password.js'
import './SecretInput.css'

/**
 * A masked text input with reveal, copy and generate.
 *
 * TOMBSTONE: an earlier draft implemented the generator here - rejection sampling and an alphabet
 * with a comment about newlines. `lib/password.js` already had it, with the reasoning that
 * actually justifies the alphabet (the DLL trims " \t\r\n" from both ends, so a generated password
 * that started or ended with a space would hash as a different string than the one on screen) and
 * with tests. Two generators is how one of them quietly drifts. This one calls that one.
 *
 * `generate` stays a prop, defaulting to `lib/password.js`, so the component itself still knows
 * nothing about t7patch.conf - it knows it has a button that asks something else for a string.
 *
 * The reveal control is a plain button whose LABEL changes (Show / Hide) rather than a toggle with
 * `aria-pressed`. Both patterns are valid; mixing them is not, and a button labelled "Show" that
 * also reports itself as pressed reads as a contradiction. The label alone says what pressing it
 * will do, which is what a user needs to know.
 *
 * `labels` exists so this carries no English of its own that the app cannot replace.
 */
const SecretInput = forwardRef(function SecretInput(
  {
    value = '',
    onChange,
    onValueChange,
    onGenerate,
    onCopy,
    note,
    labels = {},
    generate = () => generatePassword(),
    defaultRevealed = false,
    revealed: revealedProp,
    onRevealedChange,
    allowReveal = true,
    allowCopy = true,
    allowGenerate = true,
    disabled = false,
    readOnly = false,
    id: idProp,
    className = '',
    ...rest
  },
  ref,
) {
  const {
    show = 'Show',
    hide = 'Hide',
    copy = 'Copy',
    copied = 'Copied',
    copyFailed = 'Copy failed',
    generate: generateLabel = 'Generate',
  } = labels

  const auto = useId()
  const id = idProp || `${auto}-secret`
  const [internalRevealed, setInternalRevealed] = useState(defaultRevealed)
  const revealed = revealedProp === undefined ? internalRevealed : revealedProp

  const setRevealed = (next) => {
    if (revealedProp === undefined) setInternalRevealed(next)
    onRevealedChange?.(next)
  }

  return (
    <div className={['secret', className].filter(Boolean).join(' ')}>
      <div className="secret-row">
        <input
          ref={ref}
          id={id}
          type={revealed ? 'text' : 'password'}
          className="input"
          value={value}
          disabled={disabled || undefined}
          readOnly={readOnly || undefined}
          // A lobby password is not an account credential. Offering to remember it, autocorrect it
          // or capitalise its first letter is wrong in all three ways at once.
          autoComplete="off"
          autoCorrect="off"
          autoCapitalize="none"
          spellCheck={false}
          onChange={(event) => {
            onValueChange?.(event.target.value)
            onChange?.(event)
          }}
          {...rest}
        />
        <div className="secret-actions">
          {allowReveal && (
            <Button
              variant="ghost"
              size="sm"
              aria-controls={id}
              disabled={disabled}
              onClick={() => setRevealed(!revealed)}
            >
              {revealed ? hide : show}
            </Button>
          )}
          {allowCopy && (
            <CopyButton
              variant="ghost"
              size="sm"
              value={value}
              labels={{ copy, copied, failed: copyFailed }}
              onCopy={onCopy}
              disabled={disabled || value.length === 0}
            />
          )}
          {allowGenerate && (
            <Button
              variant="ghost"
              size="sm"
              disabled={disabled || readOnly}
              onClick={() => {
                const next = generate()
                onValueChange?.(next)
                // Callers that only wired `onChange` still hear about a generated value; without a
                // synthetic event here, Generate would silently do nothing for them.
                onChange?.({ target: { value: next, id } })
                // A freshly generated secret you cannot see is not usable: you cannot check it
                // against what you paste elsewhere, and Copy is not the route everyone takes.
                setRevealed(true)
                onGenerate?.(next)
              }}
            >
              {generateLabel}
            </Button>
          )}
        </div>
      </div>
      {note && <p className="secret-note">{note}</p>}
    </div>
  )
})

export default SecretInput
