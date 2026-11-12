/**
 * Text whose exact shape something else depends on: the log-line wire format StatusLog parses,
 * and the build report a maintainer compares against the DLL's own dialog.
 *
 * TOMBSTONE - this was 174 lines exporting eight helpers: formatBytes, truncatePath,
 * utf8ByteLength, truncateUtf8Bytes, MAX_PLAYER_NAME_BYTES, appendLogLine, LOG_LINE_LIMIT. None
 * had a caller outside its own tests. Two were worse than merely unused:
 *
 *  - truncatePath elided the middle of a path, competing with the CSS ellipsis the header already
 *    uses. Two mechanisms for one job means the loser is dead code and the winner is arbitrary.
 *    The header keeps the CSS one, because it can carry title= for the full value on hover.
 *  - MAX_PLAYER_NAME_BYTES / truncateUtf8Bytes reimplemented, in JavaScript, a cap that
 *    core/src/config.rs already applies before writing (15 bytes, on a char boundary, matching
 *    patch/Protection.cpp's fixed buffer). Two copies of a rule the DLL owns is a divergence
 *    waiting to happen; the Rust side is the one that touches the file, so it is the one that
 *    decides.
 */

/** Local wall-clock HH:MM:SS. Not UTC: this is for a human reading their own machine's log. */
function stamp() {
  return new Date().toTimeString().slice(0, 8)
}

/**
 * `[HH:MM:SS] text`.
 *
 * This bracketed shape is a wire format between here and StatusLog, which parses it back out to
 * render the timestamp in its own column. It is deliberately NOT in the i18n catalogue: a
 * translation that moved the bracket would silently break that parser and every line would lose
 * its timestamp.
 */
export function formatLogLine(text) {
  return `[${stamp()}] ${text}`
}

/** `0x` plus eight uppercase hex digits - printf's `0x%08X`, which is what the DLL writes. */
function hex32(n) {
  return `0x${(Number(n) >>> 0).toString(16).toUpperCase().padStart(8, '0')}`
}

/**
 * The block a user pastes into a report when their BlackOps3.exe is not a build the patch knows.
 *
 * Byte-for-byte the three lines of patch/Arxan.cpp's unsupported-build MessageBox, minus the
 * two-space indent the dialog uses to set them off from its prose. Same field names, same column
 * alignment, same `%08X`: a maintainer should be able to match a report from this tool against one
 * copied out of the game's dialog (or grep a ZLOG line) without having to normalise either.
 *
 * Deliberately NOT in the i18n catalogue, for the same reason as formatLogLine's brackets: it is
 * read by a person comparing it to C source, and a translated "TimeDateStamp" helps nobody.
 *
 * Null for a missing build so a caller can render nothing rather than a block of zeros, which
 * would read as three real values.
 *
 * @param {{ time_date_stamp: number, size_of_image: number, entry_point: number } | null | undefined} build
 * @returns {string | null}
 */
export function formatBuildReport(build) {
  if (!build) return null
  return [
    `TimeDateStamp: ${hex32(build.time_date_stamp)}`,
    `SizeOfImage:   ${hex32(build.size_of_image)}`,
    `EntryPoint:    ${hex32(build.entry_point)}`,
  ].join('\n')
}
