// Port of T7Patch.Injector.Core to Rust, sized to be representative of the real thing:
// config read/write, Steam library discovery, hijack install, PE export verification,
// update check, and (Windows) CreateRemoteThread injection + registry + elevation.
use std::fs;
use std::path::{Path, PathBuf};

pub const DSOUND: &[u8] = include_bytes!("../../assets/dsound.dll");
pub const T7PATCH: &[u8] = include_bytes!("../../assets/t7patch.dll");

mod config;
pub use config::PatchConfig;

mod vdf;

mod gamebuild;
pub use gamebuild::{game_build, game_build_ids_from_bytes, GameBuild, SUPPORTED_GAME_BUILDS};

// ---------- Steam game location ----------
#[cfg(windows)]
fn steam_root_from_registry() -> Option<PathBuf> {
    use windows_sys::Win32::System::Registry::*;
    unsafe fn read(hive: HKEY, sub: &str, name: &str) -> Option<String> {
        let w = |s: &str| s.encode_utf16().chain(std::iter::once(0)).collect::<Vec<u16>>();
        let mut buf = [0u16; 1024];
        let mut cb = (buf.len() * 2) as u32;
        let rc = RegGetValueW(hive, w(sub).as_ptr(), w(name).as_ptr(), RRF_RT_REG_SZ,
                              std::ptr::null_mut(), buf.as_mut_ptr() as *mut _, &mut cb);
        if rc != 0 { return None; }
        let n = buf.iter().position(|&c| c == 0).unwrap_or(0);
        // An EMPTY value is not a Steam root. Returning Some("") here made PathBuf::from("") the
        // root, which both suppressed the two HKLM fallbacks below and turned every subsequent
        // lookup into a CWD-relative path - so the tool probed ./steamapps/common/... next to
        // wherever the exe happened to be launched from.
        if n == 0 { return None; }
        Some(String::from_utf16_lossy(&buf[..n]))
    }
    unsafe {
        read(HKEY_CURRENT_USER, "Software\\Valve\\Steam", "SteamPath")
            .or_else(|| read(HKEY_LOCAL_MACHINE, "SOFTWARE\\WOW6432Node\\Valve\\Steam", "InstallPath"))
            .or_else(|| read(HKEY_LOCAL_MACHINE, "SOFTWARE\\Valve\\Steam", "InstallPath"))
            .map(PathBuf::from)
    }
}

#[cfg(not(windows))]
fn steam_root_from_registry() -> Option<PathBuf> {
    let home = std::env::var("HOME").ok()?;
    for c in [".steam/steam", ".local/share/Steam", ".var/app/com.valvesoftware.Steam/data/Steam"] {
        let p = Path::new(&home).join(c);
        if p.is_dir() { return Some(p); }
    }
    None
}

pub fn steam_libraries() -> Vec<PathBuf> {
    let mut out = Vec::new();
    let root = match steam_root_from_registry() { Some(r) => r, None => return out };
    out.push(root.clone());
    let vdf = root.join("steamapps").join("libraryfolders.vdf");
    if let Ok(text) = fs::read_to_string(&vdf) {
        out.extend(vdf::library_paths(&text).into_iter().map(PathBuf::from));
    }
    out
}

pub fn find_game() -> Option<PathBuf> {
    for lib in steam_libraries() {
        let p = lib.join("steamapps").join("common").join("Call of Duty Black Ops III");
        if p.join("BlackOps3.exe").is_file() { return Some(p); }
    }
    None
}

// ---------- PE export directory ----------
// Every read below is bounds-checked and every conversion is fallible. That is not defensive
// habit: the release profile sets `panic = "abort"`, so a single out-of-range slice on a
// truncated or hostile DLL would kill the whole application instantly with no message. This
// parses a file the tool did not write and must not trust.
fn rva_to_off(buf: &[u8], rva: u32, sec_off: usize, nsec: usize) -> Option<usize> {
    for i in 0..nsec {
        let s = sec_off + i * 40;
        if s + 40 > buf.len() { return None; }
        let va = u32::from_le_bytes(buf.get(s+12..s+16)?.try_into().ok()?);
        let vsz = u32::from_le_bytes(buf.get(s+8..s+12)?.try_into().ok()?);
        let raw = u32::from_le_bytes(buf.get(s+20..s+24)?.try_into().ok()?);
        if rva >= va && rva < va + vsz.max(1) {
            return (raw as usize).checked_add((rva - va) as usize);
        }
    }
    None
}

/// Reads a u32 at `at`, or None if it does not lie wholly inside `buf`.
fn u32_at(buf: &[u8], at: usize) -> Option<u32> {
    Some(u32::from_le_bytes(buf.get(at..at.checked_add(4)?)?.try_into().ok()?))
}

/// Reads a u16 at `at`, or None if it does not lie wholly inside `buf`.
fn u16_at(buf: &[u8], at: usize) -> Option<u16> {
    Some(u16::from_le_bytes(buf.get(at..at.checked_add(2)?)?.try_into().ok()?))
}

/// Just the export names - the common case (verifying a DLL is a real t7patch.dll).
pub fn pe_exports(path: &Path) -> std::io::Result<Vec<String>> {
    Ok(pe_export_rvas(path)?.into_iter().map(|(n, _)| n).collect())
}

/// Export names paired with their RVAs. The RVA is what the Windows injector needs: an export is
/// called in the target at (module base + RVA), and the 32-bit thread exit code from a remote
/// LoadLibraryW is a truncated HMODULE that cannot serve as that base.
pub fn pe_export_rvas(path: &Path) -> std::io::Result<Vec<(String, u32)>> {
    pe_export_rvas_from_bytes(&fs::read(path)?)
}

/// Export names of an in-memory image. Exists so the installer can verify the payload it is about
/// to write while the target folder is still untouched, rather than writing first and verifying
/// the file afterwards.
pub fn pe_exports_from_bytes(buf: &[u8]) -> std::io::Result<Vec<String>> {
    Ok(pe_export_rvas_from_bytes(buf)?.into_iter().map(|(n, _)| n).collect())
}

pub fn pe_export_rvas_from_bytes(buf: &[u8]) -> std::io::Result<Vec<(String, u32)>> {
    let buf = buf.to_vec();
    let bad = |m: &str| std::io::Error::new(std::io::ErrorKind::InvalidData, m);
    let truncated = || bad("truncated PE");

    if buf.len() < 0x40 || &buf[0..2] != b"MZ" { return Err(bad("not a PE")); }
    let e_lfanew = u32_at(&buf, 0x3c).ok_or_else(truncated)? as usize;
    if buf.len() < e_lfanew + 24 || buf.get(e_lfanew..e_lfanew+4) != Some(&b"PE\0\0"[..]) {
        return Err(bad("no PE signature"));
    }

    let coff = e_lfanew + 4;
    let nsec = u16_at(&buf, coff+2).ok_or_else(truncated)? as usize;
    let opt_sz = u16_at(&buf, coff+16).ok_or_else(truncated)? as usize;
    let opt = coff + 20;
    let magic = u16_at(&buf, opt).ok_or_else(truncated)?;
    // PE32+ puts the data directories 16 bytes further in than PE32.
    let dd = match magic {
        0x20b => opt + 112,
        0x10b => opt + 96,
        _ => return Err(bad("bad optional header magic")),
    };

    let exp_rva = u32_at(&buf, dd).ok_or_else(truncated)?;
    if exp_rva == 0 { return Ok(Vec::new()); }

    let sec_off = opt.checked_add(opt_sz).ok_or_else(truncated)?;
    let exp = rva_to_off(&buf, exp_rva, sec_off, nsec).ok_or_else(|| bad("bad export rva"))?;
    let n_names = u32_at(&buf, exp+24).ok_or_else(truncated)? as usize;
    // A real t7patch.dll exports a handful. This only has to be low enough to stop a corrupt
    // header from asking for a multi-gigabyte allocation.
    if n_names > 65536 { return Err(bad("implausible export count")); }
    let names_rva = u32_at(&buf, exp+32).ok_or_else(truncated)?;
    let names = rva_to_off(&buf, names_rva, sec_off, nsec).ok_or_else(|| bad("bad name table"))?;
    // AddressOfFunctions (+28) and AddressOfNameOrdinals (+36). A name at index i does NOT index
    // the function table directly - it indexes the ordinal table, which then indexes functions.
    let funcs_rva = u32_at(&buf, exp+28).ok_or_else(truncated)?;
    let ords_rva = u32_at(&buf, exp+36).ok_or_else(truncated)?;
    let funcs = rva_to_off(&buf, funcs_rva, sec_off, nsec);
    let ords = rva_to_off(&buf, ords_rva, sec_off, nsec);

    let mut out = Vec::new();
    for i in 0..n_names {
        let r = match u32_at(&buf, names + i*4) { Some(r) => r, None => break };
        let o = match rva_to_off(&buf, r, sec_off, nsec) { Some(o) => o, None => continue };
        let rest = match buf.get(o..) { Some(r) => r, None => continue };
        // No terminator before EOF means the file is truncated mid-name; take what is there.
        let end = rest.iter().position(|&c| c == 0).unwrap_or(rest.len());
        let name = String::from_utf8_lossy(&rest[..end]).into_owned();

        // A missing or unreadable ordinal/function table is not fatal: the NAME is still valid,
        // and every caller that only verifies exports exist still works. Such an entry gets RVA 0,
        // which the injector treats as unresolvable rather than as address zero.
        let rva = (|| {
            let ords = ords?;
            let funcs = funcs?;
            let ord = u16::from_le_bytes(buf.get(ords + i*2..ords + i*2 + 2)?.try_into().ok()?) as usize;
            u32_at(&buf, funcs + ord*4)
        })().unwrap_or(0);
        out.push((name, rva));
    }
    Ok(out)
}
// ---------- install / uninstall ----------

/// Whether `dir` actually looks like a Black Ops III install.
///
/// The predecessor grew this check after a hand-picked folder was accepted with none, silently
/// installing both DLLs and a t7patch.conf into an unrelated directory. It is a soft signal, not
/// a gate: the caller warns, it does not refuse, because an unusual-but-correct layout should
/// still be installable.
pub fn looks_like_game_dir(dir: &Path) -> bool {
    dir.join(GAME_EXE).is_file()
}

pub const GAME_EXE: &str = "BlackOps3.exe";

/// The marker that identifies our dsound.dll proxy: the name of the payload its DllMain loads.
///
/// `dsound-proxy/dsound_proxy.cpp` passes this literal to LoadLibraryA, so every build of the
/// proxy contains it. No other dsound.dll wrapper has a reason to reference our payload's
/// filename, which is what makes it a safe positive test.
const DSOUND_MARKER: &[u8] = b"t7patch.dll";

/// True when `path` is a dsound.dll this tool wrote, as opposed to somebody else's.
///
/// `dsound.dll` is one of the most common wrapper filenames in PC gaming - DSOAL, dgVoodoo and
/// some ReShade setups all use it. Overwriting one without a backup, and later DELETING it on
/// uninstall, destroys an unrelated piece of the user's setup. So ownership has to be tested, not
/// assumed.
///
/// TOMBSTONE - this compared the file byte-for-byte against the embedded copy, and that was
/// WRONG in a way that broke the tool in the field. Exact bytes mean "this is the dsound.dll from
/// THIS EXACT BUILD", not "this is ours". The moment the proxy is rebuilt - a new release, or
/// merely stripping debug symbols from the embedded payload, which is exactly what happened - a
/// previously-installed copy stops matching. Everything downstream then goes wrong at once:
/// uninstall silently leaves the file behind (it looks like somebody else's), install renames it
/// to dsound.dll.before-t7patch, and install_state reports the patch as not installed, so the
/// first-run prompt reappears for a user who already has it.
///
/// Scanning for the marker instead is version-independent: it holds across every build of the
/// proxy, past and future, and still says no to DSOAL and friends.
pub fn is_our_dsound(path: &Path) -> bool {
    match fs::read(path) {
        Ok(b) => contains(&b, DSOUND_MARKER),
        Err(_) => false,
    }
}

fn contains(haystack: &[u8], needle: &[u8]) -> bool {
    haystack.windows(needle.len()).any(|w| w == needle)
}

/// What an install had to do to the folder, so the caller can tell the user the truth rather than
/// a fixed success string.
///
/// Lives here rather than in the Tauri shell so the shell needs no serde dependency of its own -
/// and so this stays testable without a webview.
#[derive(Debug, Default, serde::Serialize)]
pub struct InstallReport {
    /// A pre-existing dsound.dll that was NOT ours was moved aside to this filename.
    pub displaced_dsound: Option<String>,
    /// The folder did not contain BlackOps3.exe.
    pub not_a_game_dir: bool,
    /// How many exports the payload was verified to have, before anything was written.
    pub exports_verified: usize,
}

pub fn install_hijack(dir: &Path) -> std::io::Result<InstallReport> {
    let mut report = InstallReport { not_a_game_dir: !looks_like_game_dir(dir), ..Default::default() };

    // Back up a foreign dsound.dll rather than destroying it. Named with a fixed suffix, not a
    // timestamp, so a second install does not accumulate copies - and only ever written when the
    // existing file is demonstrably not ours.
    let dsound = dir.join("dsound.dll");
    if dsound.exists() && !is_our_dsound(&dsound) {
        let backup = dir.join("dsound.dll.before-t7patch");
        fs::rename(&dsound, &backup)?;
        report.displaced_dsound = Some("dsound.dll.before-t7patch".into());
    }

    // t7patch.dll FIRST, then dsound.dll: the payload before the loader. A failure on the second
    // write then leaves the patch merely inactive, rather than leaving a dsound.dll whose
    // DllMain does LoadLibraryA("t7patch.dll") against a file that is not there.
    write_dll(&dir.join("t7patch.dll"), T7PATCH)?;
    write_dll(&dsound, DSOUND)?;
    Ok(report)
}

/// Puts `bytes` at `dest`, replacing whatever is there.
///
/// Not Windows: write a sibling temp file, then rename it over `dest`. An in-place `fs::write`
/// truncates the existing inode - and under Wine/Proton a running game has that inode mmap'd as
/// its loaded image, so truncating it pulls pages out from under live code (SIGBUS, or code
/// silently changing mid-run). rename(2) swaps the directory entry instead: the running process
/// keeps the old inode until it exits, and the next launch loads the new file. The temp lives in
/// the SAME directory because rename is only atomic within one filesystem.
///
/// Windows keeps the in-place write. There the loader holds a mapped DLL open without
/// FILE_SHARE_WRITE, so the write fails cleanly with ERROR_SHARING_VIOLATION, which
/// is_file_in_use turns into "close the game". A rename over a mapped image would instead fail
/// with ERROR_ACCESS_DENIED, which is_permission_denied would misreport as a rights problem and
/// offer to elevate for - elevation that cannot help.
fn write_dll(dest: &Path, bytes: &[u8]) -> std::io::Result<()> {
    #[cfg(windows)]
    {
        fs::write(dest, bytes)
    }
    #[cfg(not(windows))]
    {
        let name = dest.file_name().map(|n| n.to_string_lossy().into_owned()).unwrap_or_default();
        let tmp = dest.with_file_name(format!("{name}.{}.tmp", std::process::id()));
        // The error returned is always the original one: a failed cleanup must not replace the
        // write/rename error the caller classifies (is_permission_denied etc.).
        let result = fs::write(&tmp, bytes).and_then(|()| fs::rename(&tmp, dest));
        if result.is_err() {
            let _ = fs::remove_file(&tmp);
        }
        result
    }
}

/// Everything T7Patch leaves in the game folder. The two DLLs, the config, and the DLLs' own
/// diagnostic logs - deleting those is still "remove everything T7Patch left behind".
///
/// dsound.dll is handled separately (ownership), so it is not in this list.
const INSTALLED_FILES: &[&str] = &[
    "t7patch.dll",
    "t7patch.conf",
    "t7patch-dsound.log",
    "t7patch_trace.log",
    "t7patch_trace.log.1",
    "crashes.log",
];

/// Removes everything install put there, and restores a displaced wrapper.
///
/// This used to delete only the two DLLs, while the UI's own description promised it also removed
/// t7patch.conf "including your saved player name, password, and preferences". A tool whose
/// uninstall leaves the user's password on disk after saying it deleted it is worse than one that
/// never claimed to.
pub fn uninstall_hijack(dir: &Path) -> std::io::Result<()> {
    let dsound = dir.join("dsound.dll");
    if !dsound.exists() || is_our_dsound(&dsound) {
        remove_if_present(&dsound)?;
        let backup = dir.join("dsound.dll.before-t7patch");
        if backup.exists() {
            fs::rename(&backup, &dsound)?;
        }
    }
    // else: somebody else's wrapper is installed over ours. Deleting it would be destroying a
    // file we did not write, which is exactly the failure this function used to have.

    for name in INSTALLED_FILES {
        remove_if_present(&dir.join(name))?;
    }

    // PatchConfig::save writes "t7patch.conf.<pid>.tmp" then renames it into place, and (off
    // Windows) write_dll does the same for both DLLs. A process that died between the two leaves
    // a file no fixed name above matches, which would survive an uninstall that promises the
    // folder is as if never installed. Best-effort: a leftover scratch file must not turn an
    // otherwise complete uninstall into a failure.
    if let Ok(entries) = fs::read_dir(dir) {
        for entry in entries.flatten() {
            let name = entry.file_name();
            let name = name.to_string_lossy();
            let ours = ["t7patch.conf.", "t7patch.dll.", "dsound.dll."]
                .iter().any(|p| name.starts_with(p));
            if ours && name.ends_with(".tmp") {
                let _ = fs::remove_file(entry.path());
            }
        }
    }

    Ok(())
}

/// What is actually on disk, as opposed to what the UI last announced.
#[derive(Debug, Default, serde::Serialize)]
pub struct InstallState {
    pub dsound_installed: bool,
    pub patch_installed: bool,
    pub conf_exists: bool,
    pub is_game_dir: bool,
    /// A foreign dsound.dll is present - ours is NOT the one that would load.
    pub foreign_dsound: bool,
    /// BlackOps3.exe's build ids and whether the shipped DLL accepts them. None when the exe is
    /// missing or not a PE32+. See gamebuild.rs for why `supported: false` is only a warning.
    pub game_build: Option<GameBuild>,
}

/// Probes the folder. Cheap enough to call on every relevant change; the only reads of any size
/// are the ~100 KB dsound.dll marker scan, only when one exists, and the first 4 KiB of the exe.
pub fn install_state(dir: &Path) -> InstallState {
    let dsound = dir.join("dsound.dll");
    let dsound_exists = dsound.is_file();
    let ours = dsound_exists && is_our_dsound(&dsound);
    InstallState {
        dsound_installed: ours,
        foreign_dsound: dsound_exists && !ours,
        patch_installed: dir.join("t7patch.dll").is_file(),
        conf_exists: dir.join("t7patch.conf").is_file(),
        is_game_dir: looks_like_game_dir(dir),
        game_build: game_build(&dir.join(GAME_EXE)),
    }
}

fn remove_if_present(p: &Path) -> std::io::Result<()> {
    match fs::remove_file(p) {
        Ok(()) => Ok(()),
        Err(e) if e.kind() == std::io::ErrorKind::NotFound => Ok(()),
        Err(e) => Err(e),
    }
}

/// Whether this failure is "you do not have the rights", which is the only case worth offering
/// to elevate for.
pub fn is_permission_denied(e: &std::io::Error) -> bool {
    e.kind() == std::io::ErrorKind::PermissionDenied
}

/// Whether the game (or anything else) currently has the file open.
///
/// Windows reports ERROR_SHARING_VIOLATION (32) / ERROR_LOCK_VIOLATION (33) here, and neither
/// maps to ErrorKind::PermissionDenied - so installing while the game runs, the single most
/// common failure this tool has, used to be neither elevated for nor explained. Elevating would
/// not have helped anyway; closing the game is the fix, and the user has to be told that.
pub fn is_file_in_use(e: &std::io::Error) -> bool {
    matches!(e.raw_os_error(), Some(32) | Some(33))
}

// ---------- launching the game ----------
/// The Steam app id for Call of Duty: Black Ops III.
pub const STEAM_APP_ID: &str = "311210";

/// The Steam launch option a Linux/Proton install needs for the dsound.dll hijack to be picked up
/// by Wine. Shown for the user to copy; nothing here edits localconfig.vdf.
pub const LINUX_LAUNCH_OPTION: &str = r#"WINEDLLOVERRIDES="dsound=n,b" %command%"#;

/// Launches through Steam's URL protocol rather than running BlackOps3.exe directly. A direct
/// launch skips Steam's launch-option environment variables - including the WINEDLLOVERRIDES the
/// Linux hijack depends on - so this is the only correct way to start the game, not merely the
/// more convenient one.
pub fn launch_game() -> std::io::Result<()> {
    let url = format!("steam://run/{STEAM_APP_ID}");
    #[cfg(windows)]
    let mut cmd = {
        // cmd's `start` takes a window title first; without the empty "" it treats the URL as
        // the title and opens nothing.
        let mut c = std::process::Command::new("cmd");
        c.args(["/C", "start", "", &url]);
        c
    };
    #[cfg(not(windows))]
    let mut cmd = {
        let mut c = std::process::Command::new("xdg-open");
        c.arg(&url);
        c
    };
    cmd.spawn().map(|_| ())
}

// ---------- GitHub update check ----------
//
// TOMBSTONE: this used to live here, using ureq + rustls + ring to make ONE unauthenticated GET
// to api.github.com. Measured by rebuilding without it, that TLS stack was **1,191,360 bytes of
// the shipped binary** - 12.5% of it, for a feature that tells the user a newer tag exists.
//
// It is now done by the frontend with fetch(), through the webview's own TLS, which this binary
// links anyway (webkit2gtk on Linux, WebView2 on Windows). See ui/src/lib/updates.js.
//
// The trade, stated plainly because it is not free: network egress moves into the webview, so the
// CSP has to permit connect-src to api.github.com. That is a wider CSP than 'self' alone. It was
// judged worth 1.2 MB on a tool distributed as a single downloaded file.
//
// Do not reintroduce a Rust HTTP client for this. If a future feature genuinely needs one - a
// signed-manifest check, say - that is a different decision with a different justification, and
// it should be made explicitly rather than by re-adding this.

/// TOMBSTONE: `is_newer_than` lived here, duplicating ui/src/lib/updates.js. When the update
/// check moved into the frontend (to delete ureq + rustls + ring, 1,191,360 bytes) this copy lost
/// its only caller, leaving the same loose-version rule implemented twice in two languages with
/// nothing keeping them in step. The frontend does the comparison because the frontend does the
/// fetch; this side keeps only the version string itself, below.

/// The upstream patch version this build is based on, NOT a fork-specific number. The hardened
/// DLL stays wire-compatible with upstream peers, so "is there a newer upstream release" is the
/// right question for the update check, and upstream tags are what it is compared against. The
/// DLL's own string adds a `-hardened.N` suffix (patch/framework.h) so bug reports can tell the
/// two apart; version parsing stops at the `-`, so the suffix never affects the comparison.
/// `version_tests` keeps this, ui/src/lib/updates.js and framework.h in step.
pub const CURRENT_VERSION: &str = "3.09";

#[cfg(test)]
mod version_tests {
    use super::CURRENT_VERSION;

    fn repo_file(rel: &str) -> String {
        let p = std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join("../..").join(rel);
        std::fs::read_to_string(&p).unwrap_or_else(|e| panic!("reading {}: {e}", p.display()))
    }

    #[test]
    fn the_ui_copy_matches() {
        let js = repo_file("injector/ui/src/lib/updates.js");
        let needle = format!("export const CURRENT_VERSION = '{CURRENT_VERSION}'");
        assert!(js.contains(&needle), "updates.js must contain `{needle}`");
    }

    #[test]
    fn the_dll_reports_the_same_base_version() {
        let h = repo_file("patch/framework.h");
        let line = h
            .lines()
            .find(|l| l.contains("#define ZBR_VERSION_FULL"))
            .expect("framework.h defines ZBR_VERSION_FULL");
        // "Patch 3.09" alone or "Patch 3.09-<suffix>": anything else (3.091, 3.10) is drift.
        let at = line
            .find(&format!("Patch {CURRENT_VERSION}"))
            .unwrap_or_else(|| panic!("ZBR_VERSION_FULL is not based on {CURRENT_VERSION}: {line}"));
        let next = line[at + "Patch ".len() + CURRENT_VERSION.len()..].chars().next();
        assert!(matches!(next, Some(' ' | '-' | '"')), "ZBR_VERSION_FULL: {line}");
    }
}

// ---------- elevation / shell open ----------
#[cfg(windows)]
pub fn relaunch_elevated() -> bool {
    use windows_sys::Win32::UI::Shell::ShellExecuteW;
    let exe = match std::env::current_exe() { Ok(e) => e, Err(_) => return false };
    let w = |s: String| s.encode_utf16().chain(std::iter::once(0)).collect::<Vec<u16>>();
    let verb = w("runas".into());
    let file = w(exe.to_string_lossy().into_owned());
    unsafe {
        let h = ShellExecuteW(std::ptr::null_mut(), verb.as_ptr(), file.as_ptr(),
                              std::ptr::null(), std::ptr::null(), 1);
        (h as isize) > 32
    }
}

#[cfg(not(windows))]
pub fn relaunch_elevated() -> bool { false }

// ---------- Windows-only injection ----------
#[cfg(windows)]
pub mod inject {
    use windows_sys::Win32::Foundation::*;
    use windows_sys::Win32::System::Diagnostics::Debug::WriteProcessMemory;
    use windows_sys::Win32::System::LibraryLoader::{GetModuleHandleW, GetProcAddress};
    use windows_sys::Win32::System::Memory::*;
    use windows_sys::Win32::System::ProcessStatus::*;
    use windows_sys::Win32::System::Threading::*;

    struct Handle(HANDLE);
    impl Drop for Handle {
        fn drop(&mut self) { unsafe { if !self.0.is_null() { CloseHandle(self.0); } } }
    }

    fn wide(s: &str) -> Vec<u16> { s.encode_utf16().chain(std::iter::once(0)).collect() }

    /// The two exports that turn a merely-loaded t7patch.dll into a running one. Named constants
    /// because three places must agree on them: the pre-flight check, the RVA lookup, and the
    /// remote calls. See patch/dllmain.cpp for why loading alone is not enough.
    const ACTIVATE_ENABLE: &str = "EnableInjectorlessInstall";
    const ACTIVATE_RUN: &str = "zbr_run_gamemode_lui";

    /// Generous rather than tight: CreateRemoteThread issued while the target holds its loader
    /// lock blocks until release, and reporting failure for a load that completes a moment later
    /// helps nobody. A timeout is handled by leaking, never by freeing - see inject().
    const LOAD_TIMEOUT_MS: u32 = 30_000;

    /// Appends GetLastError to a message. Every P/Invoke here sets SetLastError, and this is what
    /// stops distinct failures collapsing into one opaque string.
    fn last_error(what: &str) -> String {
        let code = unsafe { windows_sys::Win32::Foundation::GetLastError() };
        format!("{what} (Win32 error {code})")
    }

    /// The load address of `dll_path` inside the target, matched by full path where possible and
    /// by filename otherwise. The thread exit code from a remote LoadLibraryW is a 32-bit
    /// truncation of an HMODULE and must never be used as the base on x64.
    fn module_base(pid: u32, dll_path: &str) -> Option<usize> {
        unsafe {
            let h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, 0, pid);
            if h.is_null() { return None; }
            let h = Handle(h);
            let mut mods = [std::ptr::null_mut(); 1024];
            let mut needed = 0u32;
            if EnumProcessModulesEx(h.0, mods.as_mut_ptr(), std::mem::size_of_val(&mods) as u32,
                                    &mut needed, LIST_MODULES_ALL) == 0 { return None; }
            let n = (needed as usize / std::mem::size_of::<HMODULE>()).min(mods.len());
            let wanted_full = dll_path.to_ascii_lowercase();
            let wanted_leaf = std::path::Path::new(dll_path)
                .file_name().map(|s| s.to_string_lossy().to_ascii_lowercase())
                .unwrap_or_default();
            let mut buf = [0u16; 260];
            for i in 0..n {
                let len = GetModuleFileNameExW(h.0, mods[i], buf.as_mut_ptr(), buf.len() as u32);
                if len == 0 { continue; }
                let s = String::from_utf16_lossy(&buf[..len as usize]).to_ascii_lowercase();
                if s == wanted_full || (!wanted_leaf.is_empty() && s.ends_with(&wanted_leaf)) {
                    return Some(mods[i] as usize);
                }
            }
            None
        }
    }

    /// The RVA of one named export in the local file, for calling it at base+RVA in the target.
    fn export_rva(dll_path: &str, name: &str) -> Option<u32> {
        crate::pe_export_rvas(std::path::Path::new(dll_path)).ok()?.into_iter()
            .find(|(n, _)| n == name).map(|(_, rva)| rva)
    }


    pub fn module_loaded(pid: u32, name: &str) -> Option<bool> {
        unsafe {
            let h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, 0, pid);
            if h.is_null() { return None; }
            let h = Handle(h);
            let mut mods = [std::ptr::null_mut(); 1024];
            let mut needed = 0u32;
            if EnumProcessModulesEx(h.0, mods.as_mut_ptr(), std::mem::size_of_val(&mods) as u32,
                                    &mut needed, LIST_MODULES_ALL) == 0 { return None; }
            let n = (needed as usize / std::mem::size_of::<HMODULE>()).min(mods.len());
            let mut buf = [0u16; 260];
            for i in 0..n {
                let len = GetModuleFileNameExW(h.0, mods[i], buf.as_mut_ptr(), buf.len() as u32);
                if len == 0 { continue; }
                let s = String::from_utf16_lossy(&buf[..len as usize]).to_ascii_lowercase();
                if s.ends_with(&name.to_ascii_lowercase()) { return Some(true); }
            }
            Some(false)
        }
    }

    /// Loads `dll` into the target and then ACTIVATES it. Loading alone accomplishes nothing:
    /// `patch/dllmain.cpp`'s DllMain does only DisableThreadLibraryCalls, and every hook, the
    /// Arxan checksum patch, packet validation and the t7patch.conf watcher live behind
    /// `RunPatching()`, whose only in-process caller is the export `zbr_run_gamemode_lui`, gated
    /// on a hash of a passphrase. A LoadLibraryW that succeeds and stops leaves an inert DLL
    /// mapped into the game - no hooks, no friends-only gate, not even the config applied - while
    /// reporting success. dsound-proxy resolves and calls the same two exports for this reason.
    /// Running processes whose main module is BlackOps3.exe, as (pid, full path) pairs.
    ///
    /// The path is returned, not just the pid, because the caller must verify the target IS the
    /// game before starting a remote thread in it - a pid alone is a number that can be recycled
    /// between the enumeration and the injection.
    pub fn list_game_processes() -> Vec<(u32, String)> {
        let mut out = Vec::new();
        unsafe {
            let mut pids = [0u32; 4096];
            let mut needed = 0u32;
            if EnumProcesses(pids.as_mut_ptr(), std::mem::size_of_val(&pids) as u32, &mut needed) == 0 {
                return out;
            }
            let n = (needed as usize / std::mem::size_of::<u32>()).min(pids.len());
            for &pid in &pids[..n] {
                if pid == 0 { continue; }
                // Most processes will refuse to open; that is expected and not an error.
                let h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, 0, pid);
                if h.is_null() { continue; }
                let h = Handle(h);
                let mut buf = [0u16; 260];
                let len = GetModuleFileNameExW(h.0, std::ptr::null_mut(), buf.as_mut_ptr(), buf.len() as u32);
                if len == 0 { continue; }
                let path = String::from_utf16_lossy(&buf[..len as usize]);
                if std::path::Path::new(&path)
                    .file_name()
                    .is_some_and(|f| f.eq_ignore_ascii_case(crate::GAME_EXE))
                {
                    out.push((pid, path));
                }
            }
        }
        out
    }

    pub fn inject(pid: u32, dll_path: &str) -> Result<(), String> {
        // Resolved from the LOCAL file before anything is mapped, so a file that is not a
        // t7patch.dll is rejected rather than discovered inert afterwards. Both RVAs are relative
        // to the module base the loader reports in the target.
        let exports = crate::pe_exports(std::path::Path::new(dll_path))
            .map_err(|e| format!("could not read the DLL's exports: {e}"))?;
        for needed in [ACTIVATE_ENABLE, ACTIVATE_RUN] {
            if !exports.iter().any(|e| e == needed) {
                return Err(format!(
                    "{dll_path} does not export {needed} - it is not a t7patch.dll, so there \
                     would be no way to activate it after loading"
                ));
            }
        }

        unsafe {
            let h = OpenProcess(
                PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION
                    | PROCESS_VM_WRITE | PROCESS_VM_READ,
                0,
                pid,
            );
            if h.is_null() {
                return Err(last_error("OpenProcess failed - is the game running elevated?"));
            }
            let h = Handle(h);

            // The FULL path, not a bare filename: LoadLibraryW resolves a bare name through the
            // TARGET's DLL search order, which would happily load some other t7patch.dll from the
            // game directory instead of the one the user picked.
            let w = wide(dll_path);
            let bytes = w.len() * 2;
            let rem = VirtualAllocEx(h.0, std::ptr::null(), bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            if rem.is_null() {
                return Err(last_error("VirtualAllocEx failed"));
            }

            // From here the remote buffer may be read by a thread inside the target, so it is
            // freed ONLY on paths where no such thread can still exist. See the wait below.
            if WriteProcessMemory(h.0, rem, w.as_ptr() as *const _, bytes, std::ptr::null_mut()) == 0 {
                let e = last_error("WriteProcessMemory failed");
                VirtualFreeEx(h.0, rem, 0, MEM_RELEASE);
                return Err(e);
            }

            let k32 = GetModuleHandleW(wide("kernel32.dll").as_ptr());
            if k32.is_null() {
                let e = last_error("GetModuleHandleW(kernel32) failed");
                VirtualFreeEx(h.0, rem, 0, MEM_RELEASE);
                return Err(e);
            }
            let load = match GetProcAddress(k32, c"LoadLibraryW".as_ptr() as *const u8) {
                Some(p) => p,
                None => {
                    let e = last_error("GetProcAddress(LoadLibraryW) failed");
                    VirtualFreeEx(h.0, rem, 0, MEM_RELEASE);
                    return Err(e);
                }
            };

            let start: LPTHREAD_START_ROUTINE = Some(std::mem::transmute(load));
            let th = CreateRemoteThread(h.0, std::ptr::null(), 0, start, rem, 0, std::ptr::null_mut());
            if th.is_null() {
                let e = last_error("CreateRemoteThread failed");
                VirtualFreeEx(h.0, rem, 0, MEM_RELEASE);
                return Err(e);
            }
            let th = Handle(th);

            // Bounded, not INFINITE - the remote thread runs under the target's loader lock and
            // this tool has no business hanging forever if that lock is contended.
            //
            // The return value is CHECKED, and that is the whole point. On timeout the remote
            // thread is STILL INSIDE LoadLibraryW dereferencing `rem`; freeing it there decommits
            // the region under a running thread and faults inside ntdll's loader - i.e. it crashes
            // the user's game, having just reported success. A timeout therefore leaks one page
            // deliberately. That leak is the correct trade and must not be "tidied up".
            if WaitForSingleObject(th.0, LOAD_TIMEOUT_MS) != WAIT_OBJECT_0 {
                return Err(format!(
                    "the game did not finish loading the DLL within {}s. The remote thread may \
                     still be running, so a small buffer in the target was deliberately not freed.",
                    LOAD_TIMEOUT_MS / 1000
                ));
            }

            let mut module = 0u32;
            if GetExitCodeThread(th.0, &mut module) == 0 {
                VirtualFreeEx(h.0, rem, 0, MEM_RELEASE);
                return Err(last_error("GetExitCodeThread failed"));
            }
            // The thread has exited, so nothing can be reading the buffer any more.
            VirtualFreeEx(h.0, rem, 0, MEM_RELEASE);

            if module == 0 {
                return Err("the game refused to load the DLL (LoadLibraryW returned NULL)".into());
            }

            // LOADED, NOT YET ACTIVE. Find the module's real base in the target - the 32-bit
            // thread exit code above is a truncated HMODULE and cannot be used as one - then call
            // the two activation exports at base+RVA.
            let base = module_base(pid, dll_path)
                .ok_or("the DLL loaded but its base address in the game could not be found, so \
                        its activation exports cannot be called")?;

            for name in [ACTIVATE_ENABLE, ACTIVATE_RUN] {
                let rva = export_rva(dll_path, name)
                    .ok_or_else(|| format!("could not resolve the RVA of {name}"))?;
                let target: LPTHREAD_START_ROUTINE =
                    Some(std::mem::transmute(base.wrapping_add(rva as usize)));
                // Fire-and-forget, deliberately: RunPatching shows a modal MessageBoxA inside the
                // game for an unsupported build and does not return until it is dismissed, so any
                // bounded wait would report failure for a patch merely mid-dialog.
                let t = CreateRemoteThread(h.0, std::ptr::null(), 0, target, std::ptr::null(), 0, std::ptr::null_mut());
                if t.is_null() {
                    return Err(last_error(&format!("could not start {name} in the game")));
                }
                let _ = Handle(t);
            }

            Ok(())
        }
    }
}

#[cfg(test)]
mod pe_tests {
    use super::*;

    fn write(bytes: &[u8]) -> PathBuf {
        let p = std::env::temp_dir().join(format!("t7pe-{}-{}.bin", std::process::id(), bytes.len()));
        fs::write(&p, bytes).unwrap();
        p
    }

    /// The point of the bounds checking: under `panic = "abort"` any of these would have taken
    /// the whole application down instead of returning an error.
    #[test]
    fn malformed_input_errors_instead_of_panicking() {
        for (name, bytes) in [
            ("empty", vec![]),
            ("short", b"MZ".to_vec()),
            // Valid MZ, e_lfanew points far past the end of the file.
            ("e_lfanew past eof", {
                let mut v = vec![0u8; 0x40];
                v[0] = b'M'; v[1] = b'Z';
                v[0x3c..0x40].copy_from_slice(&0xdead_beefu32.to_le_bytes());
                v
            }),
            // Valid MZ + PE signature, then nothing - every header read runs off the end.
            ("headers truncated", {
                let mut v = vec![0u8; 0x48];
                v[0] = b'M'; v[1] = b'Z';
                v[0x3c..0x40].copy_from_slice(&0x40u32.to_le_bytes());
                v[0x40..0x44].copy_from_slice(b"PE\0\0");
                v
            }),
            ("all zeroes", vec![0u8; 512]),
            ("random-ish", (0..1024u32).map(|i| (i.wrapping_mul(2654435761) >> 24) as u8).collect()),
        ] {
            let p = write(&bytes);
            // Must return, either way - the assertion is that this line is reached at all.
            let _ = pe_exports(&p);
            fs::remove_file(&p).ok();
            eprintln!("  survived: {name}");
        }
    }

    #[test]
    fn a_file_that_is_not_a_pe_is_rejected() {
        let p = write(b"this is not an executable");
        assert!(pe_exports(&p).is_err());
        fs::remove_file(&p).ok();
    }

    #[test]
    fn the_real_embedded_payload_parses_and_exports_what_the_installer_checks() {
        // T7PATCH is the actual DLL this tool ships, so this is a real end-to-end parse rather
        // than a synthetic fixture - and it pins the export the install path looks for.
        let p = write(T7PATCH);
        let exports = pe_exports(&p).expect("the embedded t7patch.dll must parse");
        assert!(
            exports.iter().any(|e| e == "SetPlayerName"),
            "embedded t7patch.dll is missing SetPlayerName; got {exports:?}"
        );
        fs::remove_file(&p).ok();
    }
}

#[cfg(test)]
mod export_rva_tests {
    use super::*;

    /// The RVA path is the risky part of the parser: a name at index i does NOT index the function
    /// table directly, it indexes the ORDINAL table which then indexes functions. Getting that
    /// indirection wrong yields plausible-looking but wrong addresses, and the only symptom would
    /// be a remote thread starting at the wrong place inside the user's game. Pin it against the
    /// real DLL this tool ships.
    #[test]
    fn the_activation_exports_resolve_to_plausible_distinct_rvas() {
        let p = std::env::temp_dir().join(format!("t7rva-{}.dll", std::process::id()));
        fs::write(&p, T7PATCH).unwrap();
        let pairs = pe_export_rvas(&p).expect("embedded t7patch.dll must parse");
        fs::remove_file(&p).ok();

        let find = |n: &str| pairs.iter().find(|(name, _)| name == n).map(|(_, r)| *r);
        for name in ["EnableInjectorlessInstall", "zbr_run_gamemode_lui", "Unload"] {
            let rva = find(name).unwrap_or_else(|| panic!("{name} not exported"));
            assert!(rva != 0, "{name} resolved to RVA 0, i.e. unresolved");
        }
        let a = find("EnableInjectorlessInstall").unwrap();
        let b = find("zbr_run_gamemode_lui").unwrap();
        assert_ne!(a, b, "two different exports resolved to the same RVA - the ordinal \
                          indirection is wrong");

        // Every RVA must land inside the file's own address space, not past it.
        for (name, rva) in &pairs {
            assert!((*rva as usize) < T7PATCH.len() * 4,
                "{name} RVA {rva:#x} is implausibly far outside the image");
        }
        assert_eq!(pe_exports(&p.with_extension("gone")).is_err(), true);
    }
}


#[cfg(test)]
mod install_tests {
    use super::*;

    fn tmp(tag: &str) -> PathBuf {
        let d = std::env::temp_dir().join(format!("t7inst-{tag}-{}", std::process::id()));
        let _ = fs::remove_dir_all(&d);
        fs::create_dir_all(&d).unwrap();
        d
    }

    /// The finding this exists for: dsound.dll is one of the most common wrapper filenames in PC
    /// gaming (DSOAL, dgVoodoo, some ReShade setups). Install used to overwrite it with no backup
    /// and uninstall used to delete it unconditionally, destroying an unrelated part of the
    /// user's setup.
    #[test]
    fn a_foreign_dsound_is_displaced_not_destroyed_and_is_restored_on_uninstall() {
        let d = tmp("foreign");
        let theirs = b"NOT OUR DSOUND - somebody else's audio wrapper".to_vec();
        fs::write(d.join("dsound.dll"), &theirs).unwrap();

        let report = install_hijack(&d).unwrap();
        assert_eq!(report.displaced_dsound.as_deref(), Some("dsound.dll.before-t7patch"));
        assert_eq!(fs::read(d.join("dsound.dll.before-t7patch")).unwrap(), theirs,
                   "their wrapper must survive verbatim");
        assert!(is_our_dsound(&d.join("dsound.dll")), "ours must be installed over it");

        uninstall_hijack(&d).unwrap();
        assert_eq!(fs::read(d.join("dsound.dll")).unwrap(), theirs, "theirs must be put back");
        assert!(!d.join("t7patch.dll").exists());
        fs::remove_dir_all(&d).ok();
    }

    /// The mirror case: if somebody else's wrapper is sitting there at uninstall time, it is not
    /// ours to delete.
    #[test]
    fn uninstall_leaves_a_foreign_dsound_alone() {
        let d = tmp("leave");
        fs::write(d.join("t7patch.dll"), T7PATCH).unwrap();
        let theirs = b"someone else's".to_vec();
        fs::write(d.join("dsound.dll"), &theirs).unwrap();

        uninstall_hijack(&d).unwrap();
        assert_eq!(fs::read(d.join("dsound.dll")).unwrap(), theirs);
        assert!(!d.join("t7patch.dll").exists(), "ours still goes");
        fs::remove_dir_all(&d).ok();
    }

    #[test]
    fn a_clean_install_round_trips_and_reports_a_non_game_folder() {
        let d = tmp("clean");
        let report = install_hijack(&d).unwrap();
        assert!(report.displaced_dsound.is_none());
        assert!(report.not_a_game_dir, "no BlackOps3.exe here, so the UI must be able to warn");
        assert!(is_our_dsound(&d.join("dsound.dll")));

        fs::write(d.join(GAME_EXE), b"stub").unwrap();
        assert!(looks_like_game_dir(&d));
        assert!(!install_hijack(&d).unwrap().not_a_game_dir);

        uninstall_hijack(&d).unwrap();
        assert!(!d.join("dsound.dll").exists() && !d.join("t7patch.dll").exists());
        assert!(!d.join("dsound.dll.before-t7patch").exists(), "no stray backup from a clean run");
        fs::remove_dir_all(&d).ok();
    }

    /// Reinstalling while the game runs under Wine: the old DLL is mapped, so it must be replaced
    /// by a new inode, never truncated in place. A handle opened before the reinstall stands in
    /// for the game's mapping - it has to keep seeing the old bytes, whole.
    #[cfg(unix)]
    #[test]
    fn reinstall_replaces_the_dll_inode_instead_of_truncating_a_mapped_one() {
        use std::io::Read;
        use std::os::unix::fs::MetadataExt;
        let d = tmp("mapped");
        let old = b"an older t7patch.dll that the running game has mapped".to_vec();
        fs::write(d.join("t7patch.dll"), &old).unwrap();
        let mut held = fs::File::open(d.join("t7patch.dll")).unwrap();
        let old_ino = fs::metadata(d.join("t7patch.dll")).unwrap().ino();

        install_hijack(&d).unwrap();

        let mut seen = Vec::new();
        held.read_to_end(&mut seen).unwrap();
        assert_eq!(seen, old, "the in-use file was modified in place");
        assert_ne!(fs::metadata(d.join("t7patch.dll")).unwrap().ino(), old_ino);
        assert_eq!(fs::read(d.join("t7patch.dll")).unwrap(), T7PATCH);
        assert!(is_our_dsound(&d.join("dsound.dll")));
        let stray: Vec<_> = fs::read_dir(&d).unwrap().flatten()
            .map(|e| e.file_name().to_string_lossy().into_owned())
            .filter(|n| n.ends_with(".tmp")).collect();
        assert!(stray.is_empty(), "temp files left behind: {stray:?}");
        fs::remove_dir_all(&d).ok();
    }

    /// A failed rename must not leave the temp copy behind, and must surface the rename's own
    /// error (the caller classifies it). A non-empty directory squatting on the name makes
    /// rename(2) fail portably across Unix filesystems.
    #[cfg(unix)]
    #[test]
    fn a_failed_replace_cleans_up_its_temp_file_and_writes_the_loader_last() {
        let d = tmp("failrename");
        fs::create_dir_all(d.join("t7patch.dll").join("blocker")).unwrap();

        assert!(install_hijack(&d).is_err());
        let left: Vec<_> = fs::read_dir(&d).unwrap().flatten()
            .map(|e| e.file_name().to_string_lossy().into_owned()).collect();
        assert_eq!(left, vec!["t7patch.dll".to_string()],
                   "expected only the blocking directory: no temp file, and no dsound.dll \
                    written after the payload failed");
        fs::remove_dir_all(&d).ok();
    }

    /// Verification now runs against the embedded buffer, before anything is written - so a bad
    /// payload cannot leave an armed install behind while the tool reports failure.
    #[test]
    fn the_embedded_payload_exports_what_the_loader_actually_calls() {
        let names = pe_exports_from_bytes(T7PATCH).unwrap();
        for needed in ["EnableInjectorlessInstall", "zbr_run_gamemode_lui"] {
            assert!(names.iter().any(|n| n == needed), "missing {needed}; got {names:?}");
        }
    }
}

/// One running game process, as an injection target. In core rather than the Tauri shell because
/// the shell has no serde dependency of its own - and keeping it here means the type is testable
/// without a webview.
#[derive(Debug, serde::Serialize)]
pub struct GameProcess {
    pub pid: u32,
    pub path: String,
}

#[cfg(test)]
mod ownership_tests {
    use super::*;

    /// The regression that broke uninstall in the field: an exact byte compare says "not ours"
    /// for every build but the current one, so an older install becomes untouchable.
    #[test]
    fn a_different_build_of_our_own_proxy_is_still_recognised_as_ours() {
        let d = std::env::temp_dir().join(format!("t7own-{}", std::process::id()));
        let _ = fs::remove_dir_all(&d);
        fs::create_dir_all(&d).unwrap();

        // Same proxy, different build: the marker survives, the bytes do not.
        let mut older = DSOUND.to_vec();
        older.extend_from_slice(b"\x00\x00padding from an older link\x00\x00");
        let p = d.join("dsound.dll");
        fs::write(&p, &older).unwrap();
        assert!(is_our_dsound(&p), "a rebuilt proxy must still be recognised as ours");

        // And the current one obviously still is.
        fs::write(&p, DSOUND).unwrap();
        assert!(is_our_dsound(&p));
        fs::remove_dir_all(&d).ok();
    }

    #[test]
    fn a_foreign_wrapper_is_not_ours() {
        let d = std::env::temp_dir().join(format!("t7own2-{}", std::process::id()));
        let _ = fs::remove_dir_all(&d);
        fs::create_dir_all(&d).unwrap();
        let p = d.join("dsound.dll");
        // Shaped like a real wrapper: a PE header and DirectSound exports, but no t7patch payload.
        fs::write(&p, b"MZ\x90\x00...DSOAL dsound wrapper...DirectSoundCreate...").unwrap();
        assert!(!is_our_dsound(&p));
        fs::write(&p, b"").unwrap();
        assert!(!is_our_dsound(&p), "an empty file is not ours");
        fs::remove_dir_all(&d).ok();
    }
}

#[cfg(test)]
mod uninstall_tests {
    use super::*;

    #[test]
    fn uninstall_removes_everything_it_claims_to() {
        let d = std::env::temp_dir().join(format!("t7un-{}", std::process::id()));
        let _ = fs::remove_dir_all(&d);
        fs::create_dir_all(&d).unwrap();
        fs::write(d.join(GAME_EXE), b"stub").unwrap();
        install_hijack(&d).unwrap();
        // The things the UI's description promises are removed.
        for f in ["t7patch.conf", "t7patch-dsound.log", "t7patch_trace.log", "crashes.log"] {
            fs::write(d.join(f), b"x").unwrap();
        }
        // And scratch files from a save or install that died mid-rename.
        for f in ["t7patch.conf.4321.tmp", "t7patch.dll.4321.tmp", "dsound.dll.4321.tmp"] {
            fs::write(d.join(f), b"x").unwrap();
        }

        uninstall_hijack(&d).unwrap();

        let left: Vec<_> = fs::read_dir(&d).unwrap()
            .flatten().map(|e| e.file_name().to_string_lossy().into_owned())
            .filter(|n| n != GAME_EXE)
            .collect();
        assert!(left.is_empty(), "uninstall left files behind: {left:?}");
        fs::remove_dir_all(&d).ok();
    }

    #[test]
    fn uninstall_is_idempotent_and_does_not_fail_on_a_clean_folder() {
        let d = std::env::temp_dir().join(format!("t7un2-{}", std::process::id()));
        let _ = fs::remove_dir_all(&d);
        fs::create_dir_all(&d).unwrap();
        uninstall_hijack(&d).unwrap();
        uninstall_hijack(&d).unwrap();
        fs::remove_dir_all(&d).ok();
    }
}
