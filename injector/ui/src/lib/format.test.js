import { test } from 'node:test'
import assert from 'node:assert/strict'
import { formatBuildReport, formatLogLine } from './format.js'

test('formatLogLine produces the shape StatusLog parses back out', () => {
  const line = formatLogLine('Installed.')
  assert.match(line, /^\[\d{2}:\d{2}:\d{2}\] Installed\.$/)
})

test('formatLogLine leaves the message untouched, including brackets in it', () => {
  assert.ok(formatLogLine('a [b] c').endsWith('] a [b] c'))
})

test('formatBuildReport matches the DLL dialog: field names, alignment, 0x%08X uppercase', () => {
  const report = formatBuildReport({ time_date_stamp: 0x693d731e, size_of_image: 0x1d74b000, entry_point: 0x02bd40fc, supported: true })
  assert.equal(report, 'TimeDateStamp: 0x693D731E\nSizeOfImage:   0x1D74B000\nEntryPoint:    0x02BD40FC')
})

test('formatBuildReport zero-pads short values and keeps the top bit unsigned', () => {
  const report = formatBuildReport({ time_date_stamp: 0, size_of_image: 0x1000, entry_point: 0xffffffff, supported: false })
  assert.equal(report, 'TimeDateStamp: 0x00000000\nSizeOfImage:   0x00001000\nEntryPoint:    0xFFFFFFFF')
})

test('formatBuildReport renders nothing for a missing build, never a block of zeros', () => {
  assert.equal(formatBuildReport(null), null)
  assert.equal(formatBuildReport(undefined), null)
})
