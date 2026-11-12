/**
 * Log-line formatting. One function, because that is all the UI needs.
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
