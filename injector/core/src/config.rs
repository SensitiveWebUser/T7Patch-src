//! t7patch.conf, in exactly the format the DLL reads and writes.
//!
//! The format is not negotiable and is not this tool's to choose: `patch/Protection.cpp` writes
//! `key=value` lines (`outfile << "playername=" << ...`) and its reader trims exactly " \t\r\n"
//! from both halves. A tool that writes anything else produces a file the game silently ignores
//! while appearing to have saved - which is worse than failing.
use std::io::Write;
use std::path::Path;

/// The DLL declares `char playername[16]` and truncates at 15 bytes (Protection.cpp).
const MAX_PLAYER_NAME_BYTES: usize = 15;
/// The cap the DLL applies to networkpassword.
const MAX_NETWORK_PASSWORD_BYTES: usize = 1023;

/// Field names are the on-disk keys, so serde's JSON to the frontend uses the same vocabulary the
/// config file and the DLL do. `isfriendsonly` is deliberately not renamed to something prettier:
/// three layers already agree on that spelling and a fourth name would be a translation nobody
/// asked for.
#[derive(serde::Serialize, serde::Deserialize, Clone, Debug)]
// Missing fields fall back to Default rather than failing the whole deserialise: the frontend
// sends this struct back on every edit, and a partial object from a call site that has not loaded
// a config yet must not turn into an error the user sees.
#[serde(default)]
pub struct PatchConfig {
    pub playername: String,
    pub networkpassword: String,
    pub isfriendsonly: bool,
    pub debuglog: bool,
    // The tool's own preferences live in this same file rather than a settings file next to the
    // executable, matching the .NET build - the game directory is the one location both halves
    // already agree on. The `tool` prefix keeps them clear of the DLL's own keys; the DLL ignores
    // anything it does not recognise.
    pub toolautolaunchgame: bool,
    pub toolcheckforupdatesonlaunch: bool,
    pub toollastseenupstreamtag: String,
}

impl Default for PatchConfig {
    fn default() -> Self {
        Self {
            // The DLL's own default (Protection.cpp: strcat_s(playername, "Unknown Soldier")).
            playername: "Unknown Soldier".into(),
            networkpassword: String::new(),
            isfriendsonly: true,
            debuglog: false,
            toolautolaunchgame: false,
            toolcheckforupdatesonlaunch: true,
            toollastseenupstreamtag: String::new(),
        }
    }
}

/// Trims exactly what the DLL's `trim_config_field` trims - space, tab, CR, LF - and no more.
/// Not `str::trim`, which trims every Unicode whitespace character: a name padded with a
/// non-breaking space would then read differently here than in the game.
fn trim_field(s: &str) -> &str {
    s.trim_matches([' ', '\t', '\r', '\n'])
}

/// The DLL accepts more than "1"/"0" here. An unrecognised value returns None so the caller keeps
/// whatever it already had, rather than silently flipping a setting to false.
fn parse_bool(value: &str) -> Option<bool> {
    match trim_field(value).to_ascii_lowercase().as_str() {
        "1" | "true" | "yes" | "on" => Some(true),
        "0" | "false" | "no" | "off" => Some(false),
        _ => None,
    }
}

/// Truncates on a UTF-8 character boundary at or below a BYTE cap, because the DLL's caps are
/// byte counts against fixed C buffers, not character counts.
fn truncate_utf8_bytes(s: &str, max: usize) -> &str {
    if s.len() <= max {
        return s;
    }
    let mut end = max;
    while end > 0 && !s.is_char_boundary(end) {
        end -= 1;
    }
    &s[..end]
}

/// Strips newlines (which would split one setting into two lines) and then trims, so a value can
/// never corrupt the line that carries it. '=' needs no handling: the reader splits on the FIRST
/// '=', so one inside a value survives the round trip intact.
fn sanitize(value: &str, max_bytes: Option<usize>) -> String {
    let flattened: String = value.chars().filter(|c| *c != '\r' && *c != '\n').collect();
    let trimmed = trim_field(&flattened);
    match max_bytes {
        Some(max) => trim_field(truncate_utf8_bytes(trimmed, max)).to_string(),
        None => trimmed.to_string(),
    }
}

impl PatchConfig {
    /// Reads the config, keeping the default for anything absent or unparseable. A missing file is
    /// not an error - it is what a fresh install looks like.
    ///
    /// Read as BYTES, then decoded lossily. `read_to_string` was wrong and destructively so: it
    /// returns Err for a single non-UTF-8 byte, this mapped every Err to `default()`, and the
    /// frontend's debounced auto-save then wrote that reset over the user's real file on their
    /// next keystroke - silently replacing their lobby password with an empty one. The DLL can
    /// itself produce a file this rejected: `patch/Protection.cpp` round-trips `playername` as
    /// raw bytes and truncates at byte 15, which can sever a multi-byte character. A hand-edit in
    /// a non-UTF-8 editor does it too.
    ///
    /// Lossy decoding is the right answer rather than a hard error: it preserves every line that
    /// IS valid, which is the behaviour the .NET reader had (its StreamReader used replacement
    /// fallback and never threw). Only the undecodable bytes become U+FFFD.
    pub fn load(dir: &Path) -> PatchConfig {
        let mut c = PatchConfig::default();
        let bytes = match std::fs::read(dir.join("t7patch.conf")) {
            Ok(b) => b,
            Err(_) => return c,
        };
        let text = String::from_utf8_lossy(&bytes);

        for raw in text.lines() {
            // Strip a UTF-8 BOM, same as the DLL's reader: a conf saved by Notepad otherwise has
            // its first key read as "\u{feff}playername" and silently ignored.
            let line = raw.trim_start_matches('\u{feff}');
            let (key, value) = match line.split_once('=') {
                Some(kv) => kv,
                None => continue,
            };
            let value = trim_field(value);
            match trim_field(key).to_ascii_lowercase().as_str() {
                "playername" => c.playername = value.to_string(),
                "networkpassword" => c.networkpassword = value.to_string(),
                "isfriendsonly" => {
                    if let Some(b) = parse_bool(value) {
                        c.isfriendsonly = b;
                    }
                }
                "debuglog" => {
                    if let Some(b) = parse_bool(value) {
                        c.debuglog = b;
                    }
                }
                "toolautolaunchgame" => {
                    if let Some(b) = parse_bool(value) {
                        c.toolautolaunchgame = b;
                    }
                }
                "toolcheckforupdatesonlaunch" => {
                    if let Some(b) = parse_bool(value) {
                        c.toolcheckforupdatesonlaunch = b;
                    }
                }
                "toollastseenupstreamtag" => c.toollastseenupstreamtag = value.to_string(),
                _ => {}
            }
        }
        c
    }

    /// Writes the config atomically - a temp file beside the target, then a rename over it -
    /// because the DLL re-reads this file once a second and must never observe a torn write.
    pub fn save(&self, dir: &Path) -> std::io::Result<()> {
        let target = dir.join("t7patch.conf");
        let tmp = dir.join(format!("t7patch.conf.{}.tmp", std::process::id()));
        {
            let mut f = std::fs::File::create(&tmp)?;
            writeln!(f, "playername={}", sanitize(&self.playername, Some(MAX_PLAYER_NAME_BYTES)))?;
            writeln!(f, "isfriendsonly={}", u8::from(self.isfriendsonly))?;
            writeln!(
                f,
                "networkpassword={}",
                sanitize(&self.networkpassword, Some(MAX_NETWORK_PASSWORD_BYTES))
            )?;
            writeln!(f, "debuglog={}", u8::from(self.debuglog))?;
            writeln!(f, "toolautolaunchgame={}", u8::from(self.toolautolaunchgame))?;
            writeln!(
                f,
                "toolcheckforupdatesonlaunch={}",
                u8::from(self.toolcheckforupdatesonlaunch)
            )?;
            writeln!(
                f,
                "toollastseenupstreamtag={}",
                sanitize(&self.toollastseenupstreamtag, None)
            )?;
            f.sync_all()?;
        }
        std::fs::rename(&tmp, &target)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn round_trips_through_the_dll_format() {
        let dir = std::env::temp_dir().join(format!("t7conf-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        let mut c = PatchConfig::default();
        c.playername = "Tester".into();
        c.networkpassword = "pa=ss word".into();
        c.isfriendsonly = false;
        c.save(&dir).unwrap();

        let text = std::fs::read_to_string(dir.join("t7patch.conf")).unwrap();
        assert!(text.contains("playername=Tester"), "{text}");
        assert!(text.contains("isfriendsonly=0"), "{text}");

        let back = PatchConfig::load(&dir);
        assert_eq!(back.playername, "Tester");
        // An '=' inside a value survives: the reader splits on the FIRST one only.
        assert_eq!(back.networkpassword, "pa=ss word");
        assert!(!back.isfriendsonly);
        std::fs::remove_dir_all(&dir).ok();
    }

    #[test]
    fn caps_are_bytes_and_land_on_char_boundaries() {
        // 'é' is two bytes, so 8 of them are 16 bytes: one over the 15-byte playername cap.
        assert_eq!(sanitize(&"é".repeat(8), Some(MAX_PLAYER_NAME_BYTES)).len(), 14);
    }

    #[test]
    fn bom_and_bool_spellings() {
        assert_eq!(parse_bool("YES"), Some(true));
        assert_eq!(parse_bool("off"), Some(false));
        assert_eq!(parse_bool("maybe"), None);
        assert_eq!("\u{feff}playername=x".trim_start_matches('\u{feff}'), "playername=x");
    }

    #[test]
    fn trims_only_the_dll_character_set() {
        // A non-breaking space is whitespace to str::trim and is NOT trimmed by the DLL.
        assert_eq!(trim_field("  x\u{00a0}  "), "x\u{00a0}");
    }
}

#[cfg(test)]
mod lossy_tests {
    use super::*;

    /// The regression that mattered most: a conf with one bad byte used to reset EVERY setting,
    /// and the frontend's auto-save then persisted that reset over the user's real file. The
    /// password must survive a corrupt player name.
    #[test]
    fn one_bad_byte_does_not_reset_the_other_settings() {
        let dir = std::env::temp_dir().join(format!("t7lossy-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        let mut raw = Vec::new();
        raw.extend_from_slice(b"playername=Caf\xC3");   // truncated 2-byte sequence, as the DLL can emit
        raw.extend_from_slice(b"\nnetworkpassword=hunter2\nisfriendsonly=0\n");
        std::fs::write(dir.join("t7patch.conf"), &raw).unwrap();

        let c = PatchConfig::load(&dir);
        assert_eq!(c.networkpassword, "hunter2", "the password must survive an undecodable name");
        assert!(!c.isfriendsonly, "isfriendsonly must survive too");
        assert!(c.playername.starts_with("Caf"), "got {:?}", c.playername);
        std::fs::remove_dir_all(&dir).ok();
    }

    #[test]
    fn a_wholly_binary_file_still_yields_defaults_without_panicking() {
        let dir = std::env::temp_dir().join(format!("t7bin-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        std::fs::write(dir.join("t7patch.conf"), [0xffu8; 512]).unwrap();
        let c = PatchConfig::load(&dir);
        assert_eq!(c.playername, "Unknown Soldier");
        std::fs::remove_dir_all(&dir).ok();
    }
}
