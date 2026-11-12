import { test } from 'node:test'
import assert from 'node:assert/strict'

import {
  CURRENT_VERSION,
  GITHUB_LATEST_RELEASE_URL,
  UpdateCheckError,
  checkForUpdate,
  compareVersions,
  fetchLatestRelease,
  isNewerThan,
  isUnseen,
  parseVersion,
} from './updates.js'

// ---------------------------------------------------------------------------
// isNewerThan - tested hardest, because "an update is available" is the one claim this app makes
// that the user cannot check for themselves.
// ---------------------------------------------------------------------------

test('the three tag shapes this project has actually published are one release', () => {
  assert.equal(isNewerThan('3.05.0', '3.05'), false)
  assert.equal(isNewerThan('v3.05', '3.05'), false)
  assert.equal(isNewerThan('3.05', 'v3.05.0'), false)
  assert.equal(isNewerThan('v3.05.0', '3.05'), false)
  assert.equal(isNewerThan('3.5', '3.05'), false, 'leading zeros are not significant')
  assert.equal(isNewerThan('1.0.0.0', '1'), false)
})

test('strictly newer is the only thing that reports an update', () => {
  assert.equal(isNewerThan('3.06', '3.05'), true)
  assert.equal(isNewerThan('v3.5.1', '3.5.0'), true)
  assert.equal(isNewerThan('4.0', '3.99'), true)
  assert.equal(isNewerThan('3.05.1', '3.05'), true)
  assert.equal(isNewerThan('10.0', '9.99'), true, 'components compare numerically, not as text')
})

test('an older tag is never an update - the bug this replaces reported ANY tag', () => {
  assert.equal(isNewerThan('3.04', '3.05'), false)
  assert.equal(isNewerThan('3.5.0', '3.5.1'), false)
  assert.equal(isNewerThan('2.99', '3.00'), false)
  assert.equal(isNewerThan('9.99', '10.0'), false)
})

test('an unparseable version reports nothing rather than crying wolf', () => {
  // Strictly null, not merely falsy: a caller that writes `if (!isNewerThan(a, b))` must be able
  // to be wrong loudly rather than quietly.
  assert.equal(isNewerThan('nightly', '3.05'), null)
  assert.equal(isNewerThan('3.05', ''), null)
  assert.equal(isNewerThan('', '3.05'), null)
  assert.equal(isNewerThan('v', '3.05'), null)
  assert.equal(isNewerThan('latest', 'latest'), null)
  assert.equal(isNewerThan(undefined, '3.05'), null)
  assert.equal(isNewerThan(null, '3.05'), null)
  assert.equal(isNewerThan(3.05, '3.05'), null, 'a number is not a tag')
  assert.equal(isNewerThan({ tag_name: '3.06' }, '3.05'), null)
})

test('scanning stops at the first non-numeric component, exactly as the Rust did', () => {
  // "3.05.0-beta" is 3.5.0 - the suffix is dropped, so a prerelease of a release we already have
  // is not an update.
  assert.deepEqual(parseVersion('3.05.0-beta'), [3n, 5n, 0n])
  assert.equal(isNewerThan('3.05.0-beta', '3.05'), false)
  assert.equal(isNewerThan('3.06-beta', '3.05'), true)

  // "3.05rc1" has no separator before the suffix, so the whole second component is non-numeric
  // and only the leading 3 survives. That makes it OLDER than 3.05, which is a strange-looking
  // answer that is nevertheless the safe one: it does not announce an update.
  assert.deepEqual(parseVersion('3.05rc1'), [3n])
  assert.equal(isNewerThan('3.05rc1', '3.05'), false)
})

test('a component longer than u64 or Number can hold still compares exactly', () => {
  assert.equal(isNewerThan('3.99999999999999999999', '3.5'), true)
  assert.equal(isNewerThan('3.5', '3.99999999999999999999'), false)
  // Two values that are equal as doubles but differ as integers.
  assert.equal(isNewerThan('1.9007199254740993', '1.9007199254740992'), true)
})

test('nothing is newer than itself, including the version this build ships', () => {
  assert.equal(isNewerThan(CURRENT_VERSION, CURRENT_VERSION), false)
  for (const tag of ['3.05', 'v3.05', '3.05.0', '0.0.1', '12.34.56']) {
    assert.equal(isNewerThan(tag, tag), false, tag)
  }
})

test('comparison is antisymmetric and transitive across the tag shapes in play', () => {
  const ordered = ['1.0', 'v1.0.1', '1.1', '2', '3.04', '3.05', 'v3.05.1', '3.06', '4.0.0']
  for (let i = 0; i < ordered.length; i++) {
    for (let j = 0; j < ordered.length; j++) {
      const a = ordered[i]
      const b = ordered[j]
      const expected = i === j ? 0 : i > j ? 1 : -1
      assert.equal(compareVersions(a, b), expected, `${a} vs ${b}`)
      assert.equal(isNewerThan(a, b), expected > 0, `${a} newer than ${b}`)
    }
  }
})

test('compareVersions is null-propagating rather than guessing an order', () => {
  assert.equal(compareVersions('nightly', 'nightly'), null)
  assert.equal(compareVersions('3.05', 'nightly'), null)
})

// ---------------------------------------------------------------------------
// isUnseen - the "do not nag twice" gate
// ---------------------------------------------------------------------------

test('a tag with no last-seen value has not been seen', () => {
  assert.equal(isUnseen('3.06', ''), true)
  assert.equal(isUnseen('3.06', '   '), true)
  assert.equal(isUnseen('3.06', undefined), true)
})

test('the same release re-tagged upstream does not nag a second time', () => {
  assert.equal(isUnseen('v3.06', '3.06'), false)
  assert.equal(isUnseen('3.06.0', '3.06'), false)
  assert.equal(isUnseen('3.07', '3.06'), true)
})

test('an exotic tag falls back to an exact string match, so it still announces once', () => {
  assert.equal(isUnseen('nightly', 'nightly'), false)
  assert.equal(isUnseen('nightly-2', 'nightly'), true)
})

// ---------------------------------------------------------------------------
// fetchLatestRelease / checkForUpdate
// ---------------------------------------------------------------------------

function jsonResponse(body, { ok = true, status = 200 } = {}) {
  return {
    ok,
    status,
    json: async () => body,
  }
}

/** A fetch that never settles until its signal aborts. */
function hangingFetch(_url, init) {
  return new Promise((_resolve, reject) => {
    const fail = () => {
      const err = new Error('aborted')
      err.name = 'AbortError'
      reject(err)
    }
    if (init.signal.aborted) fail()
    else init.signal.addEventListener('abort', fail, { once: true })
  })
}

test('a newer release comes back with its tag and url', async () => {
  const calls = []
  const fetchImpl = async (url, init) => {
    calls.push({ url, init })
    return jsonResponse({ tag_name: 'v3.06', html_url: 'https://example.invalid/r/3.06', name: '3.06' })
  }
  const result = await checkForUpdate({ currentVersion: '3.05', fetchImpl })
  assert.deepEqual(result, {
    tag: 'v3.06',
    url: 'https://example.invalid/r/3.06',
    name: '3.06',
    publishedAt: null,
  })
  assert.equal(calls.length, 1)
  assert.equal(calls[0].url, GITHUB_LATEST_RELEASE_URL)
  assert.equal(calls[0].init.headers['User-Agent'], 'T7Patch-Injector')
  assert.equal(calls[0].init.headers.Accept, 'application/vnd.github+json')
})

test('the current release and an older one both report no update', async () => {
  const same = async () => jsonResponse({ tag_name: '3.05', html_url: 'x' })
  assert.equal(await checkForUpdate({ currentVersion: '3.05', fetchImpl: same }), null)

  const older = async () => jsonResponse({ tag_name: 'v3.04', html_url: 'x' })
  assert.equal(await checkForUpdate({ currentVersion: '3.05', fetchImpl: older }), null)
})

test('an unparseable upstream tag reports no update rather than failing', async () => {
  const weird = async () => jsonResponse({ tag_name: 'nightly', html_url: 'x' })
  assert.equal(await checkForUpdate({ currentVersion: '3.05', fetchImpl: weird }), null)
})

test('a hung request gives up after the timeout, and says it was a timeout', async () => {
  const started = Date.now()
  await assert.rejects(
    () => checkForUpdate({ fetchImpl: hangingFetch, timeoutMs: 20 }),
    (err) => {
      assert.ok(err instanceof UpdateCheckError)
      assert.equal(err.code, 'UPDATE_TIMEOUT')
      return true
    },
  )
  assert.ok(Date.now() - started < 5000, 'must not have waited for a real timeout')
})

test("the caller's own abort is not reported as a timeout", async () => {
  const controller = new AbortController()
  const pending = checkForUpdate({
    fetchImpl: hangingFetch,
    timeoutMs: 60_000,
    signal: controller.signal,
  })
  controller.abort()
  await assert.rejects(pending, (err) => {
    assert.equal(err.code, 'UPDATE_NETWORK')
    return true
  })
})

test('an already-aborted caller signal fails immediately', async () => {
  const controller = new AbortController()
  controller.abort()
  await assert.rejects(
    () => checkForUpdate({ fetchImpl: hangingFetch, timeoutMs: 60_000, signal: controller.signal }),
    (err) => err.code === 'UPDATE_NETWORK',
  )
})

test('rate limiting gets its own code, because it is the failure people actually hit', async () => {
  for (const status of [403, 429]) {
    await assert.rejects(
      () => fetchLatestRelease({ fetchImpl: async () => jsonResponse({}, { ok: false, status }) }),
      (err) => {
        assert.equal(err.code, 'UPDATE_RATE_LIMITED')
        assert.equal(err.status, status)
        return true
      },
    )
  }
})

test('any other HTTP failure carries its status', async () => {
  await assert.rejects(
    () => fetchLatestRelease({ fetchImpl: async () => jsonResponse({}, { ok: false, status: 500 }) }),
    (err) => {
      assert.equal(err.code, 'UPDATE_HTTP')
      assert.equal(err.status, 500)
      return true
    },
  )
})

test('a transport failure - which is what a CSP block looks like - is UPDATE_NETWORK', async () => {
  await assert.rejects(
    () => fetchLatestRelease({ fetchImpl: async () => { throw new TypeError('Load failed') } }),
    (err) => err.code === 'UPDATE_NETWORK',
  )
})

test('a body without a usable tag_name is malformed, not an update', async () => {
  for (const body of [{}, { tag_name: '' }, { tag_name: '   ' }, { tag_name: 42 }, null]) {
    await assert.rejects(
      () => fetchLatestRelease({ fetchImpl: async () => jsonResponse(body) }),
      (err) => err.code === 'UPDATE_MALFORMED',
      JSON.stringify(body),
    )
  }
})

test('unreadable JSON is malformed too', async () => {
  await assert.rejects(
    () => fetchLatestRelease({
      fetchImpl: async () => ({ ok: true, status: 200, json: async () => { throw new SyntaxError('bad') } }),
    }),
    (err) => err.code === 'UPDATE_MALFORMED',
  )
})

test('an environment with no fetch fails as UPDATE_NETWORK, not as a bare TypeError', async () => {
  // Every other test here injects fetchImpl, deliberately: this suite must never touch the real
  // api.github.com, or it becomes a rate-limited flake that fails on the sixty-first CI run.
  const realFetch = globalThis.fetch
  globalThis.fetch = undefined
  try {
    await assert.rejects(
      () => fetchLatestRelease({ timeoutMs: 10 }),
      (err) => err instanceof UpdateCheckError && err.code === 'UPDATE_NETWORK',
    )
  } finally {
    globalThis.fetch = realFetch
  }
})

test('the tag is trimmed, and a missing html_url does not become "undefined"', async () => {
  const r = await fetchLatestRelease({
    fetchImpl: async () => jsonResponse({ tag_name: '  v3.09  ' }),
  })
  assert.equal(r.tag, 'v3.09')
  assert.equal(r.url, null, 'a missing html_url must not become the string "undefined"')
  assert.equal(r.name, null)
  assert.equal(r.publishedAt, null)
})
