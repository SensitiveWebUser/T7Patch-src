import { useSyncExternalStore } from 'react'
import en from './en.js'

// Catalogues are imported statically, not dynamically. A dynamic import would make `t()` async -
// putting every label in the app behind a promise - to defer 8 KB in a 9 MB binary.
const CATALOGUES = { en }
const FALLBACK = 'en'
const listeners = new Set()

// navigator.language is a BCP 47 tag, so 'en-GB' must try 'en-GB' before 'en' before the fallback.
// Subtags are dropped one at a time rather than only at the first '-', which is what makes
// 'zh-Hant-TW' -> 'zh-Hant' -> 'zh' work if a regional catalogue is ever added.
function resolve(tag) {
  const parts = String(tag || '').split('-')
  while (parts.length > 0) {
    if (CATALOGUES[parts.join('-')]) return parts.join('-')
    parts.pop()
  }
  return FALLBACK
}

let locale = resolve(typeof navigator === 'undefined' ? FALLBACK : navigator.language)

export const getLocale = () => locale

export function setLocale(tag) {
  const next = resolve(tag)
  if (next === locale) return
  locale = next
  listeners.forEach((notify) => notify())
}

export function t(key, ...args) {
  // Fall through to English per KEY, not per catalogue: a half-translated locale should show its
  // own strings where it has them and English where it does not, never the raw key.
  const template = CATALOGUES[locale][key] ?? CATALOGUES[FALLBACK][key]
  if (template === undefined) {
    // Loud in dev, graceful in prod. A missing label that renders as its key is a bug report; one
    // that renders as '' is a button nobody can find and nobody can describe.
    if (import.meta.env?.DEV) console.error(`[i18n] missing key: ${key}`)
    return key
  }
  // An argument that was not passed leaves its hole visible for the same reason - '{1}' in the UI
  // is traceable, the string 'undefined' is not.
  return template.replace(/\{(\d+)\}/g, (hole, i) => (args[i] === undefined ? hole : String(args[i])))
}

function subscribe(notify) {
  listeners.add(notify)
  return () => listeners.delete(notify)
}

// Returns `t` itself rather than a wrapper, so it is stable across renders and safe in a
// dependency array. The subscription exists only to re-render on setLocale().
export function useT() {
  useSyncExternalStore(subscribe, getLocale, getLocale)
  return t
}
