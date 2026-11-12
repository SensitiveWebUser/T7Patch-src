/**
 * The happy-dom harness every DOM test file shares.
 *
 * One copy, because the part that matters is the teardown and a second hand-rolled copy is where
 * it would be forgotten: react-dom reads `document` and `window` off globalThis, so a DOM test has
 * no choice but to put them there - and it has to put them back. `bun test` shares one global
 * object across every file, and `lib/clipboard.test.js` asserts what `copyText` does when there is
 * NO `navigator.clipboard`. Leaving happy-dom's behind made that test pass a clipboard it was
 * written to prove the absence of - six failures in a file no DOM test imports.
 *
 * Not named *.test.js, so `bun test` does not collect it on its own.
 */
import { beforeAll, afterAll } from 'bun:test'
import { Window } from 'happy-dom'
import { act } from 'react'
import { createRoot } from 'react-dom/client'

const GLOBALS = [
  'window', 'document', 'navigator', 'HTMLElement', 'Node',
  'Event', 'KeyboardEvent', 'MouseEvent', 'getComputedStyle',
]

/** Call once at the top level of a test file. Returns the Window and a `mount` bound to it. */
export function useDom() {
  const win = new Window({ url: 'http://localhost' })
  const saved = new Map()

  beforeAll(() => {
    for (const name of GLOBALS) {
      saved.set(name, Object.getOwnPropertyDescriptor(globalThis, name))
      globalThis[name] = win[name]
    }
    globalThis.IS_REACT_ACT_ENVIRONMENT = true
  })

  afterAll(() => {
    for (const name of GLOBALS) {
      const descriptor = saved.get(name)
      if (descriptor) Object.defineProperty(globalThis, name, descriptor)
      else delete globalThis[name]
    }
    delete globalThis.IS_REACT_ACT_ENVIRONMENT
  })

  function mount(element) {
    const host = win.document.createElement('div')
    win.document.body.appendChild(host)
    act(() => createRoot(host).render(element))
    return host
  }

  return { win, mount }
}
