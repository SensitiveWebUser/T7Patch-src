// Which BlackOps3.exe build is installed, and whether the shipped t7patch.dll will agree to run on
// it.
//
// The DLL refuses any build outside patch/Arxan.cpp kSupportedBuilds, because every RVA it patches
// is specific to one link of the exe. That refusal is a MessageBox at game start - long after the
// installer said "installed". Reading the same three header fields here lets the installer warn at
// install time instead.
//
// UNVERIFIED: the DLL reads these fields from the IN-MEMORY image (GetModuleHandleA(NULL)); this
// reads them from the file on disk. For a normal PE the loader maps the headers verbatim, but
// BlackOps3.exe is Arxan-packed and nobody has confirmed the packer leaves the on-disk header
// identical to the mapped one. So a mismatch is a strong warning, never grounds to block install.
use std::fs;
use std::io::Read;
use std::path::Path;

use super::{u16_at, u32_at};

/// (TimeDateStamp, SizeOfImage, AddressOfEntryPoint) of every build the shipped DLL accepts.
///
/// Mirrors patch/Arxan.cpp kSupportedBuilds exactly; a test parses that file and fails if the two
/// drift. Adding a build here without adding it there would make the installer promise a load the
/// DLL then refuses, so never edit this alone.
pub const SUPPORTED_GAME_BUILDS: &[(u32, u32, u32)] = &[
    (0x693D731E, 0x1D74B000, 0x02BD40FC), // retail Steam build, linked 2025-12-13
];

/// The installed game's identifying header fields. Serialised to the UI as `game_build`.
#[derive(Debug, Clone, Copy, PartialEq, Eq, serde::Serialize)]
pub struct GameBuild {
    pub time_date_stamp: u32,
    pub size_of_image: u32,
    pub entry_point: u32,
    /// The triple is in SUPPORTED_GAME_BUILDS - i.e. the DLL's own gate would pass.
    pub supported: bool,
}

/// The DOS stub, PE signature, file header and the start of the optional header all live well
/// inside the first page of any real exe. Reading only this much keeps install_state cheap -
/// BlackOps3.exe is hundreds of megabytes and install_state runs on every UI refresh.
const HEADER_READ_LIMIT: u64 = 4096;

/// The same three fields, read the same way, as patch/Arxan.cpp ReadGameBuildIds - including
/// rejecting anything that is not PE32+, as the DLL does. Every read is bounds-checked: this
/// parses a file the tool did not write, and under `panic = "abort"` a bad slice is a silent exit.
pub fn game_build_ids_from_bytes(buf: &[u8]) -> Option<(u32, u32, u32)> {
    if buf.get(0..2)? != b"MZ" { return None; }
    let e_lfanew = u32_at(buf, 0x3c)? as usize;
    if buf.get(e_lfanew..e_lfanew.checked_add(4)?)? != b"PE\0\0" { return None; }

    let coff = e_lfanew.checked_add(4)?;
    let time_date_stamp = u32_at(buf, coff.checked_add(4)?)?;
    let opt = coff.checked_add(20)?;
    if u16_at(buf, opt)? != 0x20b { return None; }
    let entry_point = u32_at(buf, opt.checked_add(16)?)?;
    let size_of_image = u32_at(buf, opt.checked_add(56)?)?;
    Some((time_date_stamp, size_of_image, entry_point))
}

/// Reads `exe`'s build ids. None when the file is missing, unreadable or not a PE32+ - the UI
/// then shows nothing rather than a guess.
pub fn game_build(exe: &Path) -> Option<GameBuild> {
    let mut head = Vec::with_capacity(HEADER_READ_LIMIT as usize);
    fs::File::open(exe).ok()?.take(HEADER_READ_LIMIT).read_to_end(&mut head).ok()?;
    let (time_date_stamp, size_of_image, entry_point) = game_build_ids_from_bytes(&head)?;
    Some(GameBuild {
        time_date_stamp,
        size_of_image,
        entry_point,
        supported: SUPPORTED_GAME_BUILDS.contains(&(time_date_stamp, size_of_image, entry_point)),
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::path::PathBuf;

    /// Extracts the `{ a, b, c }` rows of kSupportedBuilds from Arxan.cpp. Deliberately simple:
    /// if someone reformats that table so this no longer finds rows, the test fails loudly rather
    /// than passing on an empty list.
    fn arxan_supported_builds() -> Vec<(u32, u32, u32)> {
        let path = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../patch/Arxan.cpp");
        let src = fs::read_to_string(&path)
            .unwrap_or_else(|e| panic!("cannot read {}: {e}", path.display()));
        let start = src.find("kSupportedBuilds[]").expect("kSupportedBuilds not found in Arxan.cpp");
        let body = &src[start..];
        let body = &body[body.find('{').expect("table has no opening brace") + 1..];
        let body = &body[..body.find("};").expect("table has no closing `};`")];

        let mut rows = Vec::new();
        for line in body.lines() {
            let code = line.split("//").next().unwrap_or("");
            let (Some(a), Some(b)) = (code.find('{'), code.find('}')) else { continue };
            let vals: Vec<u32> = code[a + 1..b]
                .split(',')
                .map(|v| {
                    let v = v.trim();
                    let hex = v.strip_prefix("0x").or_else(|| v.strip_prefix("0X"))
                        .unwrap_or_else(|| panic!("non-hex value {v:?} in kSupportedBuilds"));
                    u32::from_str_radix(hex, 16).unwrap()
                })
                .collect();
            assert_eq!(vals.len(), 3, "kSupportedBuilds row is not a triple: {line:?}");
            rows.push((vals[0], vals[1], vals[2]));
        }
        assert!(!rows.is_empty(), "parsed no rows out of kSupportedBuilds");
        rows
    }

    #[test]
    fn supported_builds_mirror_the_dlls_own_gate() {
        assert_eq!(arxan_supported_builds(), SUPPORTED_GAME_BUILDS,
                   "core::SUPPORTED_GAME_BUILDS has drifted from patch/Arxan.cpp kSupportedBuilds");
    }

    /// A minimal PE32+ header with the three fields set, e_lfanew at 0x80 like a real linker.
    fn pe64(ts: u32, size: u32, ep: u32) -> Vec<u8> {
        let mut v = vec![0u8; 0x200];
        v[0..2].copy_from_slice(b"MZ");
        v[0x3c..0x40].copy_from_slice(&0x80u32.to_le_bytes());
        v[0x80..0x84].copy_from_slice(b"PE\0\0");
        let coff = 0x84;
        v[coff + 4..coff + 8].copy_from_slice(&ts.to_le_bytes());
        let opt = coff + 20;
        v[opt..opt + 2].copy_from_slice(&0x20bu16.to_le_bytes());
        v[opt + 16..opt + 20].copy_from_slice(&ep.to_le_bytes());
        v[opt + 56..opt + 60].copy_from_slice(&size.to_le_bytes());
        v
    }

    fn write_tmp(tag: &str, bytes: &[u8]) -> PathBuf {
        let p = std::env::temp_dir().join(format!("t7gb-{tag}-{}.exe", std::process::id()));
        fs::write(&p, bytes).unwrap();
        p
    }

    #[test]
    fn a_supported_build_is_recognised() {
        let (ts, size, ep) = SUPPORTED_GAME_BUILDS[0];
        let p = write_tmp("ok", &pe64(ts, size, ep));
        let b = game_build(&p).expect("valid PE32+ must parse");
        fs::remove_file(&p).ok();
        assert_eq!(b, GameBuild { time_date_stamp: ts, size_of_image: size, entry_point: ep, supported: true });
    }

    #[test]
    fn an_unknown_build_parses_but_is_not_supported() {
        let p = write_tmp("new", &pe64(0x11111111, 0x22222000, 0x00333333));
        let b = game_build(&p).expect("valid PE32+ must parse");
        fs::remove_file(&p).ok();
        assert_eq!((b.time_date_stamp, b.size_of_image, b.entry_point), (0x11111111, 0x22222000, 0x00333333));
        assert!(!b.supported);
    }

    #[test]
    fn only_one_field_differing_is_still_unsupported() {
        let (ts, size, ep) = SUPPORTED_GAME_BUILDS[0];
        for (tag, ids) in [("ts", (ts + 1, size, ep)), ("sz", (ts, size + 0x1000, ep)), ("ep", (ts, size, ep + 1))] {
            let p = write_tmp(tag, &pe64(ids.0, ids.1, ids.2));
            let b = game_build(&p);
            fs::remove_file(&p).ok();
            assert!(!b.expect("valid PE32+ must parse").supported, "{tag} differs yet reported supported");
        }
    }

    #[test]
    fn truncated_headers_are_rejected_at_every_length() {
        let full = pe64(1, 2, 3);
        // Every prefix short of the SizeOfImage field must return None rather than panic.
        let need = 0x84 + 20 + 60;
        for len in 0..need {
            assert_eq!(game_build_ids_from_bytes(&full[..len]), None, "len {len}");
        }
        assert_eq!(game_build_ids_from_bytes(&full[..need]), Some((1, 2, 3)));
    }

    #[test]
    fn bad_signatures_are_rejected() {
        let mut no_mz = pe64(1, 2, 3);
        no_mz[0] = b'X';
        assert_eq!(game_build_ids_from_bytes(&no_mz), None);

        let mut no_pe = pe64(1, 2, 3);
        no_pe[0x81] = b'X';
        assert_eq!(game_build_ids_from_bytes(&no_pe), None);

        let mut bad_magic = pe64(1, 2, 3);
        bad_magic[0x84 + 20..0x84 + 22].copy_from_slice(&0x1234u16.to_le_bytes());
        assert_eq!(game_build_ids_from_bytes(&bad_magic), None);
    }

    /// The DLL's ReadGameBuildIds rejects a non-PE32+ image, and PE32 puts the fields at the same
    /// offsets anyway - so accepting one here would report ids the DLL would never compare.
    #[test]
    fn a_pe32_image_is_rejected() {
        let mut v = pe64(1, 2, 3);
        v[0x84 + 20..0x84 + 22].copy_from_slice(&0x10bu16.to_le_bytes());
        assert_eq!(game_build_ids_from_bytes(&v), None);
    }

    #[test]
    fn e_lfanew_out_of_range_is_rejected() {
        for lfanew in [0x200u32, 0x1000, 0xffff_fff0, u32::MAX] {
            let mut v = pe64(1, 2, 3);
            v[0x3c..0x40].copy_from_slice(&lfanew.to_le_bytes());
            assert_eq!(game_build_ids_from_bytes(&v), None, "e_lfanew {lfanew:#x}");
        }
    }

    /// Headers past the first page are out of reach by design (game_build reads 4 KiB); that has
    /// to degrade to None, not to reading garbage.
    #[test]
    fn headers_beyond_the_read_limit_yield_none_and_a_missing_file_too() {
        let mut v = vec![0u8; 0x3000];
        v[0..2].copy_from_slice(b"MZ");
        v[0x3c..0x40].copy_from_slice(&0x2000u32.to_le_bytes());
        v[0x2000..0x2004].copy_from_slice(b"PE\0\0");
        v[0x2000 + 24..0x2000 + 26].copy_from_slice(&0x20bu16.to_le_bytes());
        assert!(game_build_ids_from_bytes(&v).is_some(), "the fixture itself is well-formed");
        let p = write_tmp("far", &v);
        assert_eq!(game_build(&p), None);
        fs::remove_file(&p).ok();

        assert_eq!(game_build(&std::env::temp_dir().join("t7gb-does-not-exist.exe")), None);
    }
}
