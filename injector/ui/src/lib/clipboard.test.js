import { test } from 'node:test'
import assert from 'node:assert/strict'

import { copyText } from './clipboard.js'

/** The bare minimum of a DOM that `legacyCopy` touches. */
function fakeDocument({ execResult = true, throwOnSelect = false } = {}) {
  const appended = []
  const doc = {
    copied: null,
    lastElement: null,
    createElement() {
      const el = {
        value: '',
        style: {},
        attributes: {},
        setAttribute(k, v) { this.attributes[k] = v },
        select() { if (throwOnSelect) throw new Error('no selection') },
        setSelectionRange() {},
        remove() { appended.splice(appended.indexOf(el), 1) },
      }
      doc.lastElement = el
      return el
    },
    body: { appendChild(el) { appended.push(el) } },
    execCommand(command) {
      if (command === 'copy') doc.copied = doc.lastElement.value
      return execResult
    },
    get appended() { return appended },
  }
  return doc
}

test('the async Clipboard API is used when it works', async () => {
  const written = []
  const clipboard = { writeText: async (t) => { written.push(t) } }
  const doc = fakeDocument()
  assert.equal(await copyText('hunter2', { clipboard, doc }), true)
  assert.deepEqual(written, ['hunter2'])
  assert.equal(doc.copied, null, 'the textarea fallback must not also run')
})

test('a rejected Clipboard API falls back to the textarea, not to failure', async () => {
  // This is the Linux case: the custom protocol this app is served from is not treated as a
  // secure context by every WebKitGTK build, so navigator.clipboard is missing or rejects on
  // exactly the platform where the hijack is fiddliest.
  const clipboard = { writeText: async () => { throw new Error('NotAllowedError') } }
  const doc = fakeDocument()
  assert.equal(await copyText('hunter2', { clipboard, doc }), true)
  assert.equal(doc.copied, 'hunter2')
})

test('a missing Clipboard API goes straight to the textarea', async () => {
  const doc = fakeDocument()
  assert.equal(await copyText('WINEDLLOVERRIDES="dsound=n,b" %command%', { clipboard: undefined, doc }), true)
  assert.equal(doc.copied, 'WINEDLLOVERRIDES="dsound=n,b" %command%')
})

test('the scratch textarea is always removed, even when the copy fails', async () => {
  const doc = fakeDocument({ execResult: false })
  assert.equal(await copyText('x', { clipboard: undefined, doc }), false)
  assert.equal(doc.appended.length, 0)

  const throwing = fakeDocument({ throwOnSelect: true })
  assert.equal(await copyText('x', { clipboard: undefined, doc: throwing }), false)
  assert.equal(throwing.appended.length, 0)
})

test('the scratch textarea is rendered, not hidden', async () => {
  // display:none and [hidden] are both ignored by the selection APIs, and execCommand('copy')
  // then copies whatever the user had selected instead - or nothing at all.
  const doc = fakeDocument()
  await copyText('x', { clipboard: undefined, doc })
  const el = doc.lastElement
  assert.notEqual(el.style.display, 'none')
  assert.equal(el.style.position, 'fixed')
  assert.equal(el.style.opacity, '0')
  assert.equal(el.attributes.readonly, '')
})

test('no clipboard and no document reports failure instead of throwing', async () => {
  assert.equal(await copyText('x', { clipboard: undefined, doc: undefined }), false)
})

test('a non-string is coerced rather than pasted as [object Object]', async () => {
  const written = []
  const clipboard = { writeText: async (t) => { written.push(t) } }
  await copyText(3.05, { clipboard, doc: undefined })
  await copyText(null, { clipboard, doc: undefined })
  assert.deepEqual(written, ['3.05', ''])
})
