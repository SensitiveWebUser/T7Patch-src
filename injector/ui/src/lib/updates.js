/**
 * The GitHub update check, done from the frontend.
 *
 * This is the frontend half of the trade recorded in REFACTOR-PLAN.md: the Rust check used
 * ureq + rustls + ring - a whole TLS stack, 1,191,360 measured bytes of the shipped binary - to
 * make one unauthenticated GET. The webview already links a TLS stack (WebKitGTK on Linux,
 * WebView2 on Windows), so doing it here costs nothing that was not already paid for.
 *
 * REQUIRES from tauri.conf.json: the CSP's `connect-src` must permit https://api.github.com.
 * Without it the webview blocks the request before it leaves, and the failure arrives as an
 * ordinary TypeError from fetch() with no mention of CSP anywhere in it. That is the real cost of
 * this trade - network egress moved into the webview - and it is not free.
 */

/** `/latest` already excludes drafts and prereleases, so nothing here has to filter them out. */
export const GITHUB_LATEST_RELEASE_URL =
  'https://api.github.com/repos/Scroptss/T7Patch/releases/latest'

/**
 * The version of the PATCH this build ships, which is not `tauri.conf.json`'s package version.
 * It is what an upstream tag is compared against. `core::CURRENT_VERSION` is the other copy of
 * this number; when the patch is bumped, both move.
 */
export const CURRENT_VERSION = '3.05'

/**
 * fetch() has no timeout of its own; its default is "until the OS gives up", which behind a
 * Check-now button is indistinguishable from a hang. Ten seconds is already long enough to feel
 * broken.
 */
export const UPDATE_TIMEOUT_MS = 10_000

/**
 * Failure of the check itself, as a code plus an optional HTTP status - never prose. The caller
 * owns the wording, the same way it owns the wording of Rust's error sentinels, which is what
 * makes these messages translatable at all.
 *
 * Codes: UPDATE_TIMEOUT, UPDATE_NETWORK, UPDATE_HTTP, UPDATE_RATE_LIMITED, UPDATE_MALFORMED.
 */
export class UpdateCheckError extends Error {
  /**
   * @param {'UPDATE_TIMEOUT'|'UPDATE_NETWORK'|'UPDATE_HTTP'|'UPDATE_RATE_LIMITED'|'UPDATE_MALFORMED'} code
   * @param {{ status?: number, cause?: unknown }} [detail]
   */
  constructor(code, detail = {}) {
    super(code, detail.cause === undefined ? undefined : { cause: detail.cause })
    this.name = 'UpdateCheckError'
    this.code = code
    /** @type {number | undefined} */
    this.status = detail.status
  }
}

// ---------------------------------------------------------------------------
// Version comparison
// ---------------------------------------------------------------------------

/**
 * The numeric components of a release tag, or null if there are none.
 *
 * Deliberately loose, because this project's tags are loose, and deliberately identical to the
 * `core::is_newer_than` parser it replaces: "3.05", "v3.05" and "3.05.0" have all been published
 * and are all the same release. Leading v/V is stripped, components split on `.`, `-` and `+`,
 * and reading STOPS at the first component that is not purely digits - so "3.05.0-beta" reads as
 * 3.5.0 and "nightly" reads as nothing at all.
 *
 * BigInt rather than Number: a component is an arbitrarily long run of digits, and both the u64
 * this used to parse into and a JS double have a ceiling above which a silly-but-valid tag gets a
 * wrong answer rather than a refusal. Refusing is this module's whole safety story, so nothing
 * here is allowed to quietly lose precision instead.
 *
 * @param {unknown} tag
 * @returns {bigint[] | null}
 */
export function parseVersion(tag) {
  if (typeof tag !== 'string') return null
  const trimmed = tag.trim().replace(/^[vV]+/, '')
  /** @type {bigint[]} */
  const parts = []
  for (const piece of trimmed.split(/[.\-+]/)) {
    if (piece.length === 0 || !/^\d+$/.test(piece)) break
    parts.push(BigInt(piece))
  }
  return parts.length === 0 ? null : parts
}

/**
 * -1 / 0 / 1, or null when either side is unparseable. A missing component counts as 0, which is
 * what makes "3.05.0" equal to "3.05".
 *
 * @param {unknown} a
 * @param {unknown} b
 * @returns {-1 | 0 | 1 | null}
 */
export function compareVersions(a, b) {
  const left = parseVersion(a)
  const right = parseVersion(b)
  if (left === null || right === null) return null
  const width = Math.max(left.length, right.length)
  for (let i = 0; i < width; i++) {
    const x = left[i] ?? 0n
    const y = right[i] ?? 0n
    if (x !== y) return x > y ? 1 : -1
  }
  return 0
}

/**
 * true when `candidate` is STRICTLY newer than `current`, false when it is the same or older, and
 * null when either side cannot be parsed.
 *
 * The three-valued result is the point, and it mirrors the Rust `Option<bool>` exactly. An
 * unparseable tag is not "no update" and not "an update" - it is "we do not know", and the right
 * response to that is silence. The bug this replaced reported ANY upstream tag as an available
 * update, including the version already running and including an older one.
 *
 * Callers MUST test `=== true`. `if (isNewerThan(a, b))` is right by accident and
 * `if (!isNewerThan(a, b))` is wrong on purpose, because null is falsy.
 *
 * @param {unknown} candidate
 * @param {unknown} current
 * @returns {boolean | null}
 */
export function isNewerThan(candidate, current) {
  const order = compareVersions(candidate, current)
  return order === null ? null : order > 0
}

/**
 * Whether this tag is one the user has not already been shown, for `toollastseenupstreamtag`.
 * Compared as versions rather than as strings, so re-tagging "3.06" as "v3.06" upstream does not
 * make one release nag twice.
 *
 * @param {unknown} tag
 * @param {unknown} lastSeenTag
 * @returns {boolean}
 */
export function isUnseen(tag, lastSeenTag) {
  if (typeof lastSeenTag !== 'string' || lastSeenTag.trim() === '') return true
  const order = compareVersions(tag, lastSeenTag)
  // Unparseable on either side: fall back to an exact string match, so even an exotic tag is
  // still announced only once.
  if (order === null) return String(tag).trim() !== lastSeenTag.trim()
  return order !== 0
}

// ---------------------------------------------------------------------------
// The fetch
// ---------------------------------------------------------------------------

/**
 * @typedef {object} Release
 * @property {string} tag
 * @property {string | null} url
 * @property {string | null} name
 * @property {string | null} publishedAt
 */

/**
 * The newest release, with no opinion about whether it is newer than us.
 *
 * Throws an `UpdateCheckError` for anything that is not a well-formed release: no network, a
 * non-2xx status, a body that is not JSON, JSON without a `tag_name`.
 *
 * @param {object} [opts]
 * @param {string} [opts.url]
 * @param {typeof fetch} [opts.fetchImpl] test seam / alternative transport
 * @param {number} [opts.timeoutMs]
 * @param {AbortSignal} [opts.signal] the caller's own cancellation, e.g. a component unmounting
 * @returns {Promise<Release>}
 */
export async function fetchLatestRelease(opts = {}) {
  const {
    url = GITHUB_LATEST_RELEASE_URL,
    fetchImpl = globalThis.fetch,
    timeoutMs = UPDATE_TIMEOUT_MS,
    signal,
  } = opts

  if (typeof fetchImpl !== 'function') {
    throw new UpdateCheckError('UPDATE_NETWORK')
  }

  // AbortController + setTimeout rather than AbortSignal.timeout(): WebKitGTK is the oldest
  // engine this ships against and its version travels with the user's distro, so the constructor
  // that has existed since 2017 is the one to build on. `timedOut` is how a timeout is told apart
  // from the caller cancelling - both arrive as the same AbortError.
  const controller = new AbortController()
  let timedOut = false
  const timer = setTimeout(() => {
    timedOut = true
    controller.abort()
  }, timeoutMs)
  const relay = () => controller.abort()
  if (signal) {
    if (signal.aborted) controller.abort()
    else signal.addEventListener('abort', relay, { once: true })
  }

  let response
  try {
    response = await fetchImpl(url, {
      method: 'GET',
      signal: controller.signal,
      headers: {
        // GitHub rejects an API request that carries no User-Agent at all. The webview always
        // sends its own, and `User-Agent` may well be dropped here as a forbidden header name
        // depending on the engine - so this identifies the tool in GitHub's logs where it
        // survives and is never what makes the request legal. Do not delete it on the theory
        // that it is doing nothing; do not rely on it either.
        'User-Agent': 'T7Patch-Injector',
        // GitHub's documented media type, plus the version pin that keeps the shape of the body
        // stable. Without them the API is free to serve a different one.
        Accept: 'application/vnd.github+json',
        'X-GitHub-Api-Version': '2022-11-28',
      },
    })
  } catch (cause) {
    throw new UpdateCheckError(timedOut ? 'UPDATE_TIMEOUT' : 'UPDATE_NETWORK', { cause })
  } finally {
    clearTimeout(timer)
    if (signal) signal.removeEventListener('abort', relay)
  }

  if (!response.ok) {
    // 403 and 429 are the failure people actually hit: unauthenticated api.github.com allows 60
    // requests an hour per IP, and a shared or NATed address burns that without the user having
    // done anything. It gets its own code because "try again later" is real advice and
    // "something went wrong" is not.
    const rateLimited = response.status === 403 || response.status === 429
    throw new UpdateCheckError(rateLimited ? 'UPDATE_RATE_LIMITED' : 'UPDATE_HTTP', {
      status: response.status,
    })
  }

  let body
  try {
    body = await response.json()
  } catch (cause) {
    throw new UpdateCheckError('UPDATE_MALFORMED', { cause })
  }

  const tag = typeof body?.tag_name === 'string' ? body.tag_name.trim() : ''
  if (tag === '') throw new UpdateCheckError('UPDATE_MALFORMED')

  return {
    tag,
    url: typeof body?.html_url === 'string' ? body.html_url : null,
    name: typeof body?.name === 'string' ? body.name : null,
    publishedAt: typeof body?.published_at === 'string' ? body.published_at : null,
  }
}

/**
 * The whole check: fetch, compare, and report a release ONLY when it is genuinely newer.
 *
 * Resolves to null both for "you are up to date" and for "these tags cannot be compared", and
 * throws for "the check could not be carried out". That distinction is the contract: reporting a
 * network failure as "you are up to date" is the one outcome this must never produce, and so is
 * reporting the running version as an update.
 *
 * @param {object} [opts]
 * @param {string} [opts.currentVersion]
 * @param {string} [opts.url]
 * @param {typeof fetch} [opts.fetchImpl]
 * @param {number} [opts.timeoutMs]
 * @param {AbortSignal} [opts.signal]
 * @returns {Promise<Release | null>}
 */
export async function checkForUpdate(opts = {}) {
  const { currentVersion = CURRENT_VERSION, ...rest } = opts
  const release = await fetchLatestRelease(rest)
  return isNewerThan(release.tag, currentVersion) === true ? release : null
}
