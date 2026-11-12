//! t7patch.conf, in exactly the format the DLL reads and writes.
//!
//! The format is not negotiable and is not this tool's to choose: `patch/Protection.cpp`
//! `patch_config::saveto` writes `key=value` lines (`outfile << "playername=" << ...`) and
//! `loadfrom` reads them back. A tool that writes anything else produces a file the game silently
//! ignores while appearing to have saved - which is worse than failing.
//!
//! The reader here mirrors `loadfrom` exactly, quirks included, so the UI shows what the game will
//! actually use: keys are matched case-sensitively and untrimmed (FNV-1a of the raw text before
//! '='), values are not trimmed, only a trailing CR is stripped, and there is no BOM handling.
use std::io::Write;
use std::path::Path;

/// The DLL declares `char playername[16]` and truncates at 15 bytes (Protection.cpp loadfrom).
const MAX_PLAYER_NAME_BYTES: usize = 15;
/// loadfrom resizes networkpassword to 1023 bytes.
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
    /// Upstream's `autoinstallmods` (SetAutoInstallMods): auto-install workshop mods a lobby needs.
    pub autoinstallmods: bool,
    // `debuglog` was removed here: it belonged to an earlier fork-only DLL (its trace log). The
    // current DLL has no such key, so writing it would only advertise a switch that does nothing.
    //
    // The tool's own preferences live in this same file rather than a settings file next to the
    // executable, matching the .NET build - the game directory is the one location both halves
    // already agree on. The `tool` prefix keeps them clear of the DLL's own keys; the DLL ignores
    // anything it does not recognise, and only ever rewrites the file when it does not exist.
    pub toolautolaunchgame: bool,
    pub toolcheckforupdatesonlaunch: bool,
    pub toollastseenupstreamtag: String,
}

impl Default for PatchConfig {
    fn default() -> Self {
        // The DLL's own patch_config() constructor.
        Self {
            playername: "Unknown Soldier".into(),
            networkpassword: String::new(),
            isfriendsonly: true,
            autoinstallmods: false,
            toolautolaunchgame: false,
            toolcheckforupdatesonlaunch: true,
            toollastseenupstreamtag: String::new(),
        }
    }
}

/// What `std::istringstream >> int` yields for a value: skip leading whitespace, optional sign,
/// then at least one digit; anything after the digits is ignored ("1abc" reads as 1). None is the
/// failbit case - no digits, or out of `int` range.
fn istream_int(value: &str) -> Option<i32> {
    let s = value.trim_start_matches([' ', '\t', '\n', '\x0b', '\x0c', '\r']);
    let (neg, digits) = match s.as_bytes().first() {
        Some(b'-') => (true, &s[1..]),
        Some(b'+') => (false, &s[1..]),
        _ => (false, s),
    };
    let n = digits.bytes().take_while(u8::is_ascii_digit).count();
    if n == 0 {
        return None;
    }
    let mag: i64 = digits[..n].parse().ok()?;
    i32::try_from(if neg { -mag } else { mag }).ok()
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

/// Strips newlines (which would split one setting into two lines) and surrounding space/tab/CR/LF,
/// so a value can never corrupt the line that carries it. '=' needs no handling: the reader splits
/// on the FIRST '=', so one inside a value survives the round trip intact.
fn sanitize(value: &str, max_bytes: Option<usize>) -> String {
    let flattened: String = value.chars().filter(|c| *c != '\r' && *c != '\n').collect();
    let trim = |s: &str| s.trim_matches([' ', '\t']).to_string();
    let trimmed = trim(&flattened);
    match max_bytes {
        Some(max) => trim(truncate_utf8_bytes(&trimmed, max)),
        None => trimmed,
    }
}

impl PatchConfig {
    /// Reads the config the way the DLL's `loadfrom` does, starting from the DLL's defaults. A
    /// missing file is not an error - it is what a fresh install looks like.
    ///
    /// Read as BYTES, then decoded lossily. `read_to_string` was wrong and destructively so: it
    /// returns Err for a single non-UTF-8 byte, this mapped every Err to `default()`, and the
    /// frontend's debounced auto-save then wrote that reset over the user's real file on their
    /// next keystroke - silently replacing their lobby password with an empty one. The DLL can
    /// itself produce a file this rejected: it truncates `playername` at byte 15, which can sever
    /// a multi-byte character. A hand-edit in a non-UTF-8 editor does it too.
    pub fn load(dir: &Path) -> PatchConfig {
        let mut c = PatchConfig::default();
        let bytes = match std::fs::read(dir.join("t7patch.conf")) {
            Ok(b) => b,
            Err(_) => return c,
        };
        let text = String::from_utf8_lossy(&bytes);

        // str::lines splits on '\n' and drops one trailing '\r', which is exactly getline plus
        // loadfrom's `if (line.back() == '\r') line.pop_back()`.
        for line in text.lines() {
            let Some((key, value)) = line.split_once('=') else { continue };
            match key {
                "playername" => {
                    // Empty keeps the current name; longer than 15 bytes is cut at byte 15.
                    if !value.is_empty() {
                        c.playername = truncate_utf8_bytes(value, MAX_PLAYER_NAME_BYTES).to_string();
                    }
                }
                // Empty -> false, unparseable -> false, otherwise any non-zero int is true.
                "isfriendsonly" => c.isfriendsonly = istream_int(value).is_some_and(|v| v != 0),
                // Same, but only exactly 1 is true (`autoinstallmods = autoinstallmods == 1`).
                "autoinstallmods" => c.autoinstallmods = istream_int(value) == Some(1),
                // Any value, including empty (which clears the password), cut at 1023 bytes.
                "networkpassword" => {
                    c.networkpassword =
                        truncate_utf8_bytes(value, MAX_NETWORK_PASSWORD_BYTES).to_string()
                }
                "toolautolaunchgame" => c.toolautolaunchgame = istream_int(value) == Some(1),
                "toolcheckforupdatesonlaunch" => {
                    c.toolcheckforupdatesonlaunch = istream_int(value) == Some(1)
                }
                "toollastseenupstreamtag" => c.toollastseenupstreamtag = value.to_string(),
                _ => {}
            }
        }
        c
    }

    /// Writes the config atomically - a temp file beside the target, then a rename over it -
    /// because the DLL re-reads this file once a second and must never observe a torn write.
    /// The DLL's own keys come first, in `saveto`'s order.
    pub fn save(&self, dir: &Path) -> std::io::Result<()> {
        let target = dir.join("t7patch.conf");
        let tmp = dir.join(format!("t7patch.conf.{}.tmp", std::process::id()));
        {
            let mut f = std::fs::File::create(&tmp)?;
            writeln!(f, "playername={}", sanitize(&self.playername, Some(MAX_PLAYER_NAME_BYTES)))?;
            writeln!(f, "isfriendsonly={}", u8::from(self.isfriendsonly))?;
            writeln!(f, "autoinstallmods={}", u8::from(self.autoinstallmods))?;
            writeln!(
                f,
                "networkpassword={}",
                sanitize(&self.networkpassword, Some(MAX_NETWORK_PASSWORD_BYTES))
            )?;
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

    fn tmpdir(tag: &str) -> std::path::PathBuf {
        let dir = std::env::temp_dir().join(format!("t7conf-{tag}-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        dir
    }

    fn load_text(tag: &str, text: &[u8]) -> PatchConfig {
        let dir = tmpdir(tag);
        std::fs::write(dir.join("t7patch.conf"), text).unwrap();
        let c = PatchConfig::load(&dir);
        std::fs::remove_dir_all(&dir).ok();
        c
    }

    #[test]
    fn round_trips_through_the_dll_format() {
        let dir = tmpdir("rt");
        let mut c = PatchConfig::default();
        c.playername = "Tester".into();
        c.networkpassword = "pa=ss word".into();
        c.isfriendsonly = false;
        c.autoinstallmods = true;
        c.save(&dir).unwrap();

        let text = std::fs::read_to_string(dir.join("t7patch.conf")).unwrap();
        // The DLL's four keys first, in saveto's order, and no fork-only debuglog.
        assert!(text.starts_with(
            "playername=Tester\nisfriendsonly=0\nautoinstallmods=1\nnetworkpassword=pa=ss word\n"
        ), "{text}");
        assert!(!text.contains("debuglog"), "{text}");

        let back = PatchConfig::load(&dir);
        assert_eq!(back.playername, "Tester");
        // An '=' inside a value survives: the reader splits on the FIRST one only.
        assert_eq!(back.networkpassword, "pa=ss word");
        assert!(!back.isfriendsonly);
        assert!(back.autoinstallmods);
        std::fs::remove_dir_all(&dir).ok();
    }

    #[test]
    fn caps_are_bytes_and_land_on_char_boundaries() {
        // 'é' is two bytes, so 8 of them are 16 bytes: one over the 15-byte playername cap.
        assert_eq!(sanitize(&"é".repeat(8), Some(MAX_PLAYER_NAME_BYTES)).len(), 14);
        let c = load_text("cap", "playername=ABCDEFGHIJKLMNOPQRS\n".as_bytes());
        assert_eq!(c.playername, "ABCDEFGHIJKLMNO");
        let long = format!("networkpassword={}\n", "x".repeat(2000));
        assert_eq!(load_text("pwcap", long.as_bytes()).networkpassword.len(), 1023);
    }

    #[test]
    fn istream_int_matches_operator_extraction() {
        assert_eq!(istream_int("1"), Some(1));
        assert_eq!(istream_int("  0"), Some(0));
        assert_eq!(istream_int("2"), Some(2));
        assert_eq!(istream_int("-1"), Some(-1));
        assert_eq!(istream_int("1abc"), Some(1)); // trailing junk is not a failure
        assert_eq!(istream_int("true"), None);
        assert_eq!(istream_int(""), None);
        assert_eq!(istream_int("-"), None);
        assert_eq!(istream_int("99999999999"), None); // out of int range sets failbit
    }

    #[test]
    fn bool_semantics_match_the_dll() {
        // isfriendsonly: empty / unparseable -> false; any non-zero int -> true.
        assert!(!load_text("f0", b"isfriendsonly=\n").isfriendsonly);
        assert!(!load_text("f1", b"isfriendsonly=yes\n").isfriendsonly);
        assert!(load_text("f2", b"isfriendsonly=2\n").isfriendsonly);
        assert!(!load_text("f3", b"isfriendsonly=0\n").isfriendsonly);
        // autoinstallmods: only exactly 1 is true.
        assert!(load_text("a0", b"autoinstallmods=1\n").autoinstallmods);
        assert!(!load_text("a1", b"autoinstallmods=2\n").autoinstallmods);
        assert!(!load_text("a2", b"autoinstallmods=\n").autoinstallmods);
    }

    #[test]
    fn keys_are_exact_and_values_untrimmed_like_loadfrom() {
        // FNV of the raw key: a different case or a padded key is simply not that setting.
        let c = load_text("keys", b"PlayerName=Nope\nisfriendsonly =0\nplayername= Spaced \n");
        assert_eq!(c.playername, " Spaced ");
        assert!(c.isfriendsonly, "a padded key must be ignored, leaving the default");
        // CRLF: only the CR before the newline goes.
        assert_eq!(load_text("crlf", b"playername=Win\r\n").playername, "Win");
        // A BOM makes the first key unrecognised, in the DLL and therefore here.
        assert_eq!(load_text("bom", "\u{feff}playername=X\n".as_bytes()).playername, "Unknown Soldier");
    }

    #[test]
    fn empty_values_follow_the_dll() {
        let c = load_text("empty", b"networkpassword=old\nplayername=\nnetworkpassword=\n");
        assert_eq!(c.playername, "Unknown Soldier", "an empty name keeps the current one");
        assert_eq!(c.networkpassword, "", "an empty password clears it");
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
