//! The one piece of Steam's VDF format this tool needs: the `"path"` entries in
//! `steamapps/libraryfolders.vdf`.
//!
//! Hand-written rather than `regex`. That crate was pulled in for this single pattern and cost
//! **1,201,712 bytes of the shipped binary** - measured by rebuilding without it. For a tool whose
//! distribution promise is one small file, 1.2 MB to find quoted strings is not a trade worth
//! making. This is ~40 lines and is tested against the same cases the predecessor's parser was.
//!
//! Deliberately NOT a general VDF parser. It does not build a tree, handle nested objects, or
//! validate structure - it scans for `"path"` followed by a quoted value, which is exactly what
//! the old regex did and exactly what the caller needs. A real parser here would be more code to
//! be wrong in, for a file whose only interesting content is a list of directories.

/// Extracts the value of every `"path"` key, in file order, with Steam's doubled backslashes
/// unescaped.
pub fn library_paths(text: &str) -> Vec<String> {
    let mut out = Vec::new();
    let b = text.as_bytes();
    let mut i = 0;

    while let Some(k) = find_quoted_key(b, i, b"path") {
        i = k;
        // The value is the next quoted token after the key. Anything between them is whitespace
        // in every file Steam writes; if it is not, skipping to the next quote is still the same
        // behaviour the regex had.
        match next_quoted(b, i) {
            Some((value, end)) => {
                out.push(unescape(value));
                i = end;
            }
            None => break,
        }
    }
    out
}

/// Finds `"<key>"` at or after `from`, returning the index just past its closing quote.
fn find_quoted_key(b: &[u8], from: usize, key: &[u8]) -> Option<usize> {
    let needle_len = key.len() + 2;
    let mut i = from;
    while i + needle_len <= b.len() {
        if b[i] == b'"' && &b[i + 1..i + 1 + key.len()] == key && b[i + 1 + key.len()] == b'"' {
            return Some(i + needle_len);
        }
        i += 1;
    }
    None
}

/// The next `"..."` token at or after `from`: its inner bytes, and the index past its close quote.
fn next_quoted(b: &[u8], from: usize) -> Option<(&str, usize)> {
    let open = b[from..].iter().position(|&c| c == b'"')? + from;
    let close = b[open + 1..].iter().position(|&c| c == b'"')? + open + 1;
    // The file is UTF-8 (Steam writes it as such) and the slice lies on quote boundaries, so this
    // is valid - but a corrupt file must not panic, hence from_utf8 rather than an unchecked cut.
    std::str::from_utf8(&b[open + 1..close]).ok().map(|s| (s, close + 1))
}

/// Steam escapes backslashes by doubling them on Windows. Linux paths contain none.
fn unescape(s: &str) -> String {
    s.replace("\\\\", "\\")
}

#[cfg(test)]
mod tests {
    use super::library_paths;

    // These are the predecessor's own test cases, ported. Its parser was the tested one; this
    // must not regress against it.

    #[test]
    fn extracts_a_single_path() {
        let vdf = r#""libraryfolders"
{
    "0"
    {
        "path"		"C:\\Program Files (x86)\\Steam"
    }
}"#;
        assert_eq!(library_paths(vdf), vec![r"C:\Program Files (x86)\Steam"]);
    }

    #[test]
    fn extracts_multiple_paths_in_order() {
        let vdf = r#"
        "0" { "path" "C:\\Steam" }
        "1" { "path" "D:\\SteamLibrary" }
        "2" { "path" "E:\\Games\\Steam" }
        "#;
        assert_eq!(library_paths(vdf), vec![r"C:\Steam", r"D:\SteamLibrary", r"E:\Games\Steam"]);
    }

    #[test]
    fn linux_forward_slash_paths_survive_untouched() {
        let vdf = r#""path"    "/home/user/.local/share/Steam""#;
        assert_eq!(library_paths(vdf), vec!["/home/user/.local/share/Steam"]);
    }

    #[test]
    fn keys_that_merely_contain_path_are_not_matched() {
        // "contentpath" and "path_override" must not be mistaken for "path" - the old regex
        // anchored on the quotes for exactly this reason, and so does find_quoted_key.
        let vdf = r#"
        "contentpath" "/wrong/one"
        "path_override" "/also/wrong"
        "path" "/right/one"
        "#;
        assert_eq!(library_paths(vdf), vec!["/right/one"]);
    }

    #[test]
    fn no_matches_and_empty_input_yield_nothing() {
        assert!(library_paths("").is_empty());
        assert!(library_paths(r#""apps" { "311210" "12345" }"#).is_empty());
    }

    #[test]
    fn a_truncated_file_does_not_panic_or_hang() {
        for bad in [r#""path""#, r#""path" ""#, r#""path" "unterminated"#, "\"pa", "\"\"\"\"\""] {
            let _ = library_paths(bad);
        }
    }

    #[test]
    fn non_utf8_and_binary_noise_are_survivable() {
        // library_paths takes &str so the bytes are already valid UTF-8, but the content can
        // still be arbitrary - including quotes in odd places.
        let noise: String = (0u32..2000).map(|i| if i % 7 == 0 { '"' } else { '\u{263A}' }).collect();
        let _ = library_paths(&noise);
    }
}
