/**
 * The network-password generator.
 *
 * Read AGENTS.md section 6 before treating this as a security control: the lobby password is a
 * hygiene filter, not authentication. It is hashed to 63 bits and both gates that use it publish
 * their slices on the wire, so a peer who sees one packet defeats it without cracking anything.
 * What this generator owes the user is therefore modest and exact: a value that survives the
 * round trip through t7patch.conf unchanged, and no bias that a stronger threat model would have
 * cared about.
 */

/**
 * Excluded, and why - all three are round-trip hazards, not strength choices:
 *
 *   \r \n  end the line. `patch/Protection.cpp` reads t7patch.conf line by line, so a password
 *          containing either is silently cut in half.
 *   space  and tab, because both the DLL's `trim_config_field` and `SetNetworkPassword` trim
 *          " \t\r\n" from the ends. A generated password that began or ended with one would hash
 *          as a different string than the one on screen - the user would copy a value that does
 *          not work and nothing would look wrong. Dropping them from the alphabet entirely is
 *          simpler than generating and then re-rolling the ends.
 *
 * `=` is NOT excluded for the reason an earlier comment here claimed. It cannot corrupt a
 * key=value line: `Protection.cpp` splits on `line.find("=")`, the FIRST one, so everything after
 * the first `=` is the value verbatim (there is a Rust round-trip test for exactly "pa=ss word").
 * It stays out only because a password is a thing people paste into other tools, some of which
 * are less careful. Keeping the shipped alphabet byte-for-byte is worth more than the character.
 */
export const PASSWORD_ALPHABET =
  'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*()-_+.,'

/** Comfortably under the DLL's 1023-byte cap, and ASCII, so length and byte length agree. */
export const DEFAULT_PASSWORD_LENGTH = 32

/** The DLL's cap on `networkpassword`, in BYTES (Protection.cpp: `val.substr(0, 1023)`). */
export const MAX_PASSWORD_BYTES = 1023

/**
 * Default entropy source. Deliberately has no fallback: if `crypto.getRandomValues` is missing,
 * this throws rather than reaching for `Math.random()`. A silently-weak password looks exactly
 * like a strong one on screen, so the failure has to be loud or it is not a failure at all.
 *
 * @param {number} count
 * @returns {Uint8Array}
 */
function systemRandomBytes(count) {
  const webcrypto = globalThis.crypto
  if (!webcrypto || typeof webcrypto.getRandomValues !== 'function') {
    throw new Error('no cryptographic random source available')
  }
  return webcrypto.getRandomValues(new Uint8Array(count))
}

/**
 * A uniformly random password.
 *
 * Rejection sampling, NOT `bytes[i] % alphabet.length`. The alphabet is 77 characters and 256 is
 * not a multiple of it (256 = 3x77 + 25), so a modulo would give the first 25 characters a 4/256
 * chance each and the other 52 only 3/256. Bytes at or above 231, the largest multiple of 77, are
 * discarded and redrawn instead.
 *
 * @param {number} [length]
 * @param {object} [opts]
 * @param {string} [opts.alphabet]
 * @param {(count: number) => Uint8Array} [opts.randomBytes] test seam / alternative source
 * @returns {string}
 */
export function generatePassword(length = DEFAULT_PASSWORD_LENGTH, opts = {}) {
  const { alphabet = PASSWORD_ALPHABET, randomBytes = systemRandomBytes } = opts

  if (!Number.isInteger(length) || length <= 0) {
    throw new RangeError('password length must be a positive integer')
  }
  assertUsableAlphabet(alphabet)

  const n = alphabet.length
  // The largest multiple of n that fits in a byte. Everything from here up is discarded.
  const limit = 256 - (256 % n)

  // Drawn in batches sized for the known rejection rate rather than one byte at a time: each
  // call to getRandomValues has real cost, and limit/256 is the acceptance probability.
  const batch = Math.max(8, Math.ceil((length * 256) / limit))
  let out = ''
  while (out.length < length) {
    const bytes = randomBytes(batch)
    if (!bytes || bytes.length === 0) {
      throw new Error('random source returned no bytes')
    }
    for (let i = 0; i < bytes.length && out.length < length; i++) {
      const b = bytes[i]
      if (b >= limit) continue
      out += alphabet[b % n]
    }
  }
  return out
}

/**
 * @param {string} alphabet
 * @throws if the alphabet cannot produce an unbiased, conf-safe password
 */
export function assertUsableAlphabet(alphabet) {
  if (typeof alphabet !== 'string' || alphabet.length < 2) {
    throw new RangeError('alphabet needs at least two characters')
  }
  // Above 256 a single byte can no longer address the alphabet, and the rejection window below
  // would be wrong rather than merely inefficient.
  if (alphabet.length > 256) {
    throw new RangeError('alphabet must fit in one byte')
  }
  if (new Set(alphabet).size !== alphabet.length) {
    // A repeated character is picked twice as often - the exact bias rejection sampling exists
    // to remove, reintroduced by hand.
    throw new RangeError('alphabet has duplicate characters')
  }
  for (let i = 0; i < alphabet.length; i++) {
    const code = alphabet.charCodeAt(i)
    // ASCII only, and printable. Indexing by code unit is what `alphabet[b % n]` does, so a
    // character outside the BMP would be picked as half a surrogate pair; and the DLL's cap on
    // the password is a BYTE count against a C buffer, so a multi-byte character would make the
    // length on screen stop meaning the length on disk. This is also what excludes \r and \n.
    if (code < 0x21 || code > 0x7e) {
      throw new RangeError('alphabet must be printable ASCII with no spaces, tabs or line breaks')
    }
  }
}

/**
 * Codes, not prose, for what a hand-typed password will lose on its way through t7patch.conf.
 * Empty array means it survives verbatim. An empty password is not an issue: `networkpassword=`
 * with no value deliberately CLEARS the password, and that is a supported state.
 *
 * @param {string} value
 * @returns {Array<'NEWLINE' | 'TRIMMED_ENDS' | 'TOO_LONG'>}
 */
export function describePasswordIssues(value) {
  /** @type {Array<'NEWLINE' | 'TRIMMED_ENDS' | 'TOO_LONG'>} */
  const issues = []
  if (typeof value !== 'string' || value === '') return issues

  if (value.includes('\r') || value.includes('\n')) issues.push('NEWLINE')

  // The DLL trims " \t\r\n" from both ends twice over - once reading the file, once in
  // SetNetworkPassword - so these characters are not merely cosmetic, they change the hash.
  const trimmed = value.replace(/^[ \t\r\n]+/, '').replace(/[ \t\r\n]+$/, '')
  if (trimmed !== value) issues.push('TRIMMED_ENDS')

  if (new TextEncoder().encode(value).length > MAX_PASSWORD_BYTES) issues.push('TOO_LONG')

  return issues
}
