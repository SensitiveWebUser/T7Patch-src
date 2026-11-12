// Which BlackOps3.exe build is installed, and whether the shipped t7patch.dll will agree to run on
// it.
//
// The DLL decides that in patch/GameBuild.h bo3::detect_build(): a known TimeDateStamp plus one of
// that build's two accepted SizeOfImage values (the retail exe keeps 0x200 alignment; IDA's
// reconstructed dumps round up to the page). Anything else is Build::Unknown and the patch does
// not apply. That refusal surfaces at game start - long after the installer said "installed".
// Reading the same header fields here lets the installer warn at install time instead.
//
// UNVERIFIED: the DLL reads these fields from the IN-MEMORY image (GetModuleHandleW(nullptr));
// this reads them from the file on disk. For a normal PE the loader maps the headers verbatim, but
// BlackOps3.exe is Arxan-packed and nobody has confirmed the packer leaves the on-disk header
// identical to the mapped one. So a mismatch is a strong warning, never grounds to block install.
use std::fs;
use std::io::Read;
use std::path::Path;

use super::{u16_at, u32_at};

/// (TimeDateStamp, SizeOfImage) pairs the shipped DLL accepts - every build's retail and dump
/// image size. EntryPoint is deliberately absent: GameBuild.h does not compare it.
///
/// Mirrors the `<Build>TimeDateStamp` / `<Build>RetailImageSize` / `<Build>DumpImageSize`
/// constants in patch/GameBuild.h; a test parses that file and fails if the two drift. Adding a
/// build here without adding it there would make the installer promise a load the DLL then
/// refuses, so never edit this alone.
pub const SUPPORTED_GAME_BUILDS: &[(u32, u32)] = &[
    (0x693D731E, 0x1D74AC00), // February 2026, retail exe
    (0x693D731E, 0x1D74B000), // February 2026, page-rounded dump
    (0x6A7B6355, 0x1D75BC00), // September 2026, retail exe
    (0x6A7B6355, 0x1D75C000), // September 2026, page-rounded dump
];

/// The installed game's identifying header fields. Serialised to the UI as `game_build`.
/// entry_point is not part of the support decision; it is carried only so a user's report block
/// matches what the game's own diagnostics print.
#[derive(Debug, Clone, Copy, PartialEq, Eq, serde::Serialize)]
pub struct GameBuild {
    pub time_date_stamp: u32,
    pub size_of_image: u32,
    pub entry_point: u32,
    /// (time_date_stamp, size_of_image) is in SUPPORTED_GAME_BUILDS - the DLL's own check passes.
    pub supported: bool,
}

/// The DOS stub, PE signature, file header and the start of the optional header all live well
/// inside the first page of any real exe. Reading only this much keeps install_state cheap -
/// BlackOps3.exe is hundreds of megabytes and install_state runs on every UI refresh.
const HEADER_READ_LIMIT: u64 = 4096;

const IMAGE_FILE_MACHINE_AMD64: u16 = 0x8664;

/// The fields bo3::detect_build reads, with the same acceptance: MZ, a positive e_lfanew, "PE\0\0"
/// and FileHeader.Machine == AMD64 (it then reads them as IMAGE_NT_HEADERS64). Every read is
/// bounds-checked: this parses a file the tool did not write, and under `panic = "abort"` a bad
/// slice is a silent exit.
pub fn game_build_ids_from_bytes(buf: &[u8]) -> Option<(u32, u32, u32)> {
    if buf.get(0..2)? != b"MZ" { return None; }
    // LONG in the DLL, and `e_lfanew <= 0` is rejected there.
    let e_lfanew = u32_at(buf, 0x3c)? as i32;
    if e_lfanew <= 0 { return None; }
    let e_lfanew = e_lfanew as usize;
    if buf.get(e_lfanew..e_lfanew.checked_add(4)?)? != b"PE\0\0" { return None; }

    let coff = e_lfanew.checked_add(4)?;
    if u16_at(buf, coff)? != IMAGE_FILE_MACHINE_AMD64 { return None; }
    let time_date_stamp = u32_at(buf, coff.checked_add(4)?)?;
    let opt = coff.checked_add(20)?;
    let entry_point = u32_at(buf, opt.checked_add(16)?)?;
    let size_of_image = u32_at(buf, opt.checked_add(56)?)?;
    Some((time_date_stamp, size_of_image, entry_point))
}

/// Reads `exe`'s build ids. None when the file is missing, unreadable or not an x64 PE - the UI
/// then shows nothing rather than a guess.
pub fn game_build(exe: &Path) -> Option<GameBuild> {
    let mut head = Vec::with_capacity(HEADER_READ_LIMIT as usize);
    fs::File::open(exe).ok()?.take(HEADER_READ_LIMIT).read_to_end(&mut head).ok()?;
    let (time_date_stamp, size_of_image, entry_point) = game_build_ids_from_bytes(&head)?;
    Some(GameBuild {
        time_date_stamp,
        size_of_image,
        entry_point,
        supported: SUPPORTED_GAME_BUILDS.contains(&(time_date_stamp, size_of_image)),
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::BTreeMap;
    use std::path::PathBuf;

    /// Every (TimeDateStamp, SizeOfImage) pair GameBuild.h accepts, built from its
    /// `inline constexpr std::uint32_t <Build>{TimeDateStamp,RetailImageSize,DumpImageSize} = 0x..;`
    /// constants. Deliberately simple: if those constants are renamed or reformatted so this finds
    /// nothing, the test fails loudly rather than passing on an empty list.
    fn gamebuild_h_supported_pairs() -> Vec<(u32, u32)> {
        let path = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../patch/GameBuild.h");
        let src = fs::read_to_string(&path)
            .unwrap_or_else(|e| panic!("cannot read {}: {e}", path.display()));

        // build name -> (ts, retail, dump)
        let mut builds: BTreeMap<String, [Option<u32>; 3]> = BTreeMap::new();
        for line in src.lines() {
            let code = line.split("//").next().unwrap_or("").trim();
            let Some(rest) = code.strip_prefix("inline constexpr std::uint32_t ") else { continue };
            let Some((name, value)) = rest.split_once('=') else { continue };
            let name = name.trim();
            let value = value.trim().trim_end_matches(';').trim();
            let (build, slot) = if let Some(b) = name.strip_suffix("TimeDateStamp") {
                (b, 0)
            } else if let Some(b) = name.strip_suffix("RetailImageSize") {
                (b, 1)
            } else if let Some(b) = name.strip_suffix("DumpImageSize") {
                (b, 2)
            } else {
                continue;
            };
            let hex = value.strip_prefix("0x").or_else(|| value.strip_prefix("0X"))
                .unwrap_or_else(|| panic!("non-hex value {value:?} for {name}"));
            let v = u32::from_str_radix(hex, 16).unwrap_or_else(|e| panic!("{name}: {e}"));
            builds.entry(build.to_string()).or_default()[slot] = Some(v);
        }
        assert!(!builds.is_empty(), "parsed no build constants out of GameBuild.h");

        let mut pairs = Vec::new();
        for (build, [ts, retail, dump]) in &builds {
            let ts = ts.unwrap_or_else(|| panic!("{build} has no TimeDateStamp"));
            pairs.push((ts, retail.unwrap_or_else(|| panic!("{build} has no RetailImageSize"))));
            pairs.push((ts, dump.unwrap_or_else(|| panic!("{build} has no DumpImageSize"))));
        }
        pairs.sort();
        pairs
    }

    #[test]
    fn supported_builds_mirror_the_dlls_own_detection() {
        let mut ours = SUPPORTED_GAME_BUILDS.to_vec();
        ours.sort();
        assert_eq!(gamebuild_h_supported_pairs(), ours,
                   "core::SUPPORTED_GAME_BUILDS has drifted from patch/GameBuild.h");
    }

    /// A minimal x64 PE header with the three fields set, e_lfanew at 0x80 like a real linker.
    fn pe64(ts: u32, size: u32, ep: u32) -> Vec<u8> {
        let mut v = vec![0u8; 0x200];
        v[0..2].copy_from_slice(b"MZ");
        v[0x3c..0x40].copy_from_slice(&0x80u32.to_le_bytes());
        v[0x80..0x84].copy_from_slice(b"PE\0\0");
        let coff = 0x84;
        v[coff..coff + 2].copy_from_slice(&IMAGE_FILE_MACHINE_AMD64.to_le_bytes());
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
    fn every_supported_pair_is_recognised_whatever_the_entry_point() {
        for (i, &(ts, size)) in SUPPORTED_GAME_BUILDS.iter().enumerate() {
            for ep in [0u32, 0x02BD40FC, 0x02BD334C, 0xDEADBEEF] {
                let p = write_tmp(&format!("ok{i}-{ep:x}"), &pe64(ts, size, ep));
                let b = game_build(&p).expect("valid x64 PE must parse");
                fs::remove_file(&p).ok();
                assert_eq!(b, GameBuild { time_date_stamp: ts, size_of_image: size, entry_point: ep, supported: true });
            }
        }
    }

    #[test]
    fn the_september_2026_retail_exe_is_supported() {
        // The values a user reported from the old patch's refusal box. 0x1D75BC00 is real: the
        // retail exe keeps 0x200 section alignment, so SizeOfImage need not be page-aligned.
        let p = write_tmp("sep", &pe64(0x6A7B6355, 0x1D75BC00, 0x02BD334C));
        let b = game_build(&p).expect("valid x64 PE must parse");
        fs::remove_file(&p).ok();
        assert!(b.supported);
    }

    #[test]
    fn an_unknown_build_parses_but_is_not_supported() {
        let p = write_tmp("new", &pe64(0x11111111, 0x22222000, 0x00333333));
        let b = game_build(&p).expect("valid x64 PE must parse");
        fs::remove_file(&p).ok();
        assert_eq!((b.time_date_stamp, b.size_of_image, b.entry_point), (0x11111111, 0x22222000, 0x00333333));
        assert!(!b.supported);
    }

    #[test]
    fn a_known_timestamp_with_a_foreign_size_or_a_size_from_another_build_is_unsupported() {
        for (tag, ts, size) in [
            ("ts", 0x693D731E + 1, 0x1D74AC00),
            ("sz", 0x693D731E, 0x1D74C000),
            ("cross", 0x693D731E, 0x1D75BC00), // September's size under February's timestamp
        ] {
            let p = write_tmp(tag, &pe64(ts, size, 0));
            let b = game_build(&p);
            fs::remove_file(&p).ok();
            assert!(!b.expect("valid x64 PE must parse").supported, "{tag} reported supported");
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
    }

    /// detect_build rejects anything whose FileHeader.Machine is not AMD64 (e.g. an x86 exe).
    #[test]
    fn a_non_amd64_image_is_rejected() {
        let mut v = pe64(1, 2, 3);
        v[0x84..0x86].copy_from_slice(&0x014cu16.to_le_bytes()); // IMAGE_FILE_MACHINE_I386
        assert_eq!(game_build_ids_from_bytes(&v), None);
    }

    #[test]
    fn e_lfanew_out_of_range_or_non_positive_is_rejected() {
        for lfanew in [0u32, 0x200, 0x1000, 0x7fff_fff0, 0x8000_0000, 0xffff_fff0, u32::MAX] {
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
        v[0x2004..0x2006].copy_from_slice(&IMAGE_FILE_MACHINE_AMD64.to_le_bytes());
        assert!(game_build_ids_from_bytes(&v).is_some(), "the fixture itself is well-formed");
        let p = write_tmp("far", &v);
        assert_eq!(game_build(&p), None);
        fs::remove_file(&p).ok();

        assert_eq!(game_build(&std::env::temp_dir().join("t7gb-does-not-exist.exe")), None);
    }
}
