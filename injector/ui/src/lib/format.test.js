import { test } from 'node:test'
import assert from 'node:assert/strict'
import { formatLogLine } from './format.js'

test('formatLogLine produces the shape StatusLog parses back out', () => {
  const line = formatLogLine('Installed.')
  assert.match(line, /^\[\d{2}:\d{2}:\d{2}\] Installed\.$/)
})

test('formatLogLine leaves the message untouched, including brackets in it', () => {
  assert.ok(formatLogLine('a [b] c').endsWith('] a [b] c'))
})
