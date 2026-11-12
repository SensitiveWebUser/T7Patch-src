import { test } from 'node:test'
import assert from 'node:assert/strict'

import {
  DEFAULT_PASSWORD_LENGTH,
  MAX_PASSWORD_BYTES,
  PASSWORD_ALPHABET,
  assertUsableAlphabet,
  describePasswordIssues,
  generatePassword,
} from './password.js'

const N = PASSWORD_ALPHABET.length
/** The largest multiple of the alphabet length that fits in a byte. */
const LIMIT = 256 - (256 % N)

/** A byte source that hands out a fixed sequence, cycling. */
function sequence(bytes) {
  let cursor = 0
  return (count) => Uint8Array.from({ length: count }, () => bytes[cursor++ % bytes.length])
}

// ---------------------------------------------------------------------------
// The alphabet
// ---------------------------------------------------------------------------

test('the shipped alphabet cannot corrupt or be silently rewritten by t7patch.conf', () => {
  assert.doesNotThrow(() => assertUsableAlphabet(PASSWORD_ALPHABET))
  assert.ok(!PASSWORD_ALPHABET.includes('\r'), 'CR would cut the conf line in half')
  assert.ok(!PASSWORD_ALPHABET.includes('\n'), 'LF would cut the conf line in half')
  // Both the DLL's trim_config_field and SetNetworkPassword trim " \t\r\n" from the ends, so a
  // password that began or ended with one would hash differently from what is on screen.
  assert.ok(!PASSWORD_ALPHABET.includes(' '))
  assert.ok(!PASSWORD_ALPHABET.includes('\t'))
  assert.equal(new Set(PASSWORD_ALPHABET).size, N, 'a repeated character is picked twice as often')
  for (const ch of PASSWORD_ALPHABET) {
    const code = ch.charCodeAt(0)
    assert.ok(code > 0x20 && code < 0x7f, `${JSON.stringify(ch)} is not printable ASCII`)
  }
})

test('an unusable alphabet is refused rather than quietly producing a biased password', () => {
  assert.throws(() => assertUsableAlphabet(''), RangeError)
  assert.throws(() => assertUsableAlphabet('a'), RangeError)
  assert.throws(() => assertUsableAlphabet('aab'), RangeError)
  assert.throws(() => assertUsableAlphabet('ab\n'), RangeError)
  assert.throws(() => assertUsableAlphabet('ab\r'), RangeError)
  assert.throws(() => assertUsableAlphabet('ab '), RangeError)
  assert.throws(() => assertUsableAlphabet('ab\t'), RangeError)
  assert.throws(() => assertUsableAlphabet('abé'), RangeError, 'multi-byte breaks the byte cap')
  assert.throws(() => assertUsableAlphabet('ab🙂'), RangeError, 'astral pairs index as halves')
  assert.throws(() => assertUsableAlphabet(null), RangeError)
})

// ---------------------------------------------------------------------------
// Rejection sampling - the whole reason this file exists
// ---------------------------------------------------------------------------

test('the alphabet does not divide 256, so a modulo really would be biased', () => {
  // If this ever became false the bias test below would pass for the wrong reason.
  assert.notEqual(256 % N, 0)
  assert.ok(LIMIT > 0 && LIMIT < 256)
})

test('every byte value appears once and every character comes out equally often', () => {
  const everyByte = Array.from({ length: 256 }, (_, i) => i)
  const password = generatePassword(LIMIT, { randomBytes: sequence(everyByte) })

  const counts = new Map()
  for (const ch of password) counts.set(ch, (counts.get(ch) ?? 0) + 1)

  assert.equal(counts.size, N, 'every character of the alphabet must be reachable')
  const expected = LIMIT / N
  for (const [ch, seen] of counts) {
    // With `bytes[i] % N` instead, the first 256-LIMIT characters would each show up one extra
    // time here - roughly a third more likely than the rest. That is the bias this replaces.
    assert.equal(seen, expected, `${JSON.stringify(ch)} appeared ${seen} times, not ${expected}`)
  }
})

test('bytes at or above the rejection limit produce no character at all', () => {
  const rejected = Array.from({ length: 256 - LIMIT }, (_, i) => LIMIT + i)
  // Every rejected value, then a single acceptable one.
  const source = sequence([...rejected, 5])
  assert.equal(generatePassword(1, { randomBytes: source }), PASSWORD_ALPHABET[5])
})

test('an accepted byte maps to its modulo position', () => {
  const password = generatePassword(4, { randomBytes: sequence([0, 1, N - 1, N]) })
  assert.equal(
    password,
    PASSWORD_ALPHABET[0] + PASSWORD_ALPHABET[1] + PASSWORD_ALPHABET[N - 1] + PASSWORD_ALPHABET[0],
  )
})

test('a two-character alphabet uses the whole byte range, because 256 divides evenly', () => {
  const bits = generatePassword(4, { alphabet: 'ab', randomBytes: sequence([0, 1, 254, 255]) })
  assert.equal(bits, 'abab')
})

// ---------------------------------------------------------------------------
// Generation
// ---------------------------------------------------------------------------

test('a generated password is the requested length and drawn only from the alphabet', () => {
  for (const length of [1, 8, DEFAULT_PASSWORD_LENGTH, 200]) {
    const password = generatePassword(length)
    assert.equal(password.length, length)
    for (const ch of password) {
      assert.ok(PASSWORD_ALPHABET.includes(ch), `${JSON.stringify(ch)} is not in the alphabet`)
    }
  }
})

test('the default length is what the shipped tool generates', () => {
  assert.equal(generatePassword().length, DEFAULT_PASSWORD_LENGTH)
  assert.ok(DEFAULT_PASSWORD_LENGTH <= MAX_PASSWORD_BYTES)
})

test('two generated passwords are not the same', () => {
  // Not a randomness test - a randomness test in a unit suite is theatre. This catches the
  // specific failure of a source that is seeded once and replayed.
  const seen = new Set(Array.from({ length: 32 }, () => generatePassword()))
  assert.equal(seen.size, 32)
})

test('a nonsense length is refused', () => {
  for (const bad of [0, -1, 1.5, NaN, '8', null, undefined]) {
    if (bad === undefined) continue // undefined means "use the default"
    assert.throws(() => generatePassword(bad), RangeError, String(bad))
  }
})

test('a random source that returns nothing fails loudly instead of looping forever', () => {
  assert.throws(() => generatePassword(4, { randomBytes: () => new Uint8Array(0) }), /no bytes/)
})

test('no cryptographic random source is a hard failure, never a Math.random fallback', () => {
  const realCrypto = globalThis.crypto
  const restore = Object.getOwnPropertyDescriptor(globalThis, 'crypto')
  Object.defineProperty(globalThis, 'crypto', { value: undefined, configurable: true })
  try {
    // A silently-weak password looks exactly like a strong one on screen, which is why this
    // must not degrade gracefully.
    assert.throws(() => generatePassword(8), /random source/)
  } finally {
    if (restore) Object.defineProperty(globalThis, 'crypto', restore)
    else globalThis.crypto = realCrypto
  }
})

test('a generated password always survives the round trip through t7patch.conf', () => {
  for (let i = 0; i < 100; i++) {
    assert.deepEqual(describePasswordIssues(generatePassword()), [])
  }
})

// ---------------------------------------------------------------------------
// describePasswordIssues
// ---------------------------------------------------------------------------

test('an ordinary password, and an empty one, have no issues', () => {
  assert.deepEqual(describePasswordIssues('hunter2'), [])
  // networkpassword= with no value deliberately CLEARS the password; that is a supported state.
  assert.deepEqual(describePasswordIssues(''), [])
  assert.deepEqual(describePasswordIssues('pa=ss word'), [], 'the DLL splits on the FIRST =')
})

test('a line break is reported, because the conf reader would cut the value in half', () => {
  assert.deepEqual(describePasswordIssues('a\nb'), ['NEWLINE'])
  assert.deepEqual(describePasswordIssues('a\rb'), ['NEWLINE'])
  assert.deepEqual(describePasswordIssues('ab\n'), ['NEWLINE', 'TRIMMED_ENDS'])
})

test('whitespace on the ends is reported, because the DLL trims it before hashing', () => {
  assert.deepEqual(describePasswordIssues(' hunter2'), ['TRIMMED_ENDS'])
  assert.deepEqual(describePasswordIssues('hunter2 '), ['TRIMMED_ENDS'])
  assert.deepEqual(describePasswordIssues('\thunter2\t'), ['TRIMMED_ENDS'])
  assert.deepEqual(describePasswordIssues('hun ter2'), [], 'a space in the middle survives')
})

test('the length cap is counted in BYTES, the way the DLL counts it', () => {
  assert.deepEqual(describePasswordIssues('a'.repeat(MAX_PASSWORD_BYTES)), [])
  assert.deepEqual(describePasswordIssues('a'.repeat(MAX_PASSWORD_BYTES + 1)), ['TOO_LONG'])
  // 342 euro signs are 342 characters and 1026 bytes.
  const euros = '€'.repeat(342)
  assert.equal(euros.length, 342)
  assert.deepEqual(describePasswordIssues(euros), ['TOO_LONG'])
})
