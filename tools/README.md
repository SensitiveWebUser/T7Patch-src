# tools/

Offline developer tooling. Nothing here is built into or shipped with either DLL or the installer.

## `lint_offsets.py`

Every address literal in `patch/` is a **February 2026 build** RVA. `patch/GameBuild.h` detects
which supported `BlackOps3.exe` is running and, through `bo3::address()` (which `OFFSET`/`REBASE`
call), moves RVAs inside one range of the code section by a fixed delta on the September 2026
build. This script (Python 3.10+, stdlib only) reads the source as text, inventories every address
constant, and checks what a change can get quietly wrong:

- a symbol bound in more than one place (`PTR_X`, the `X` function pointer, the
  `MH_CreateHook`/`CREATE_HOOK` target that installs over it, a named `*Rva` constant) has the same
  RVA everywhere, and no `#define` or `*Rva` constant is defined twice;
- no address is formed as `image_base() + rva` for an RVA inside the shifted range, since that
  skips `bo3::address()` and would not be shifted on the September build;
- `GameBuild.h` agrees with itself: each build's dump `SizeOfImage` is its retail one rounded up to
  a page, every fingerprint is actually compared in `detect_build()`, and its
  `static_assert(get_live_rva(...))` lines match the parsed shift range;
- the Arxan checksum scan in `patch/Arxan.cpp` is consistent: declared array sizes match their
  bytes, every pattern is collected once with a patch no longer than itself, and decrypted
  `kEncryptedPatch2` still starts with `kEncryptedPatch1` (which
  `external_code_patches_installed()` relies on);
- with `--check`, nothing has drifted from the committed baseline: build fingerprints, the shift
  range, the Arxan patterns, patches and expected counts (259/259/847), the named symbols, how
  often each RVA appears in each file, and every raw `image_base() +` use.

```bash
python3 tools/lint_offsets.py                  # summary, including shifted/unshifted counts
python3 tools/lint_offsets.py --list           # every site, with file:line and its class
python3 tools/lint_offsets.py --check          # exit 1 on a failed check or any drift
python3 -B -I -m unittest discover -s tools -p 'test_*.py'   # -B: write no __pycache__
```

Comments are stripped before scanning, so commented-out hooks are not counted, and line numbers
are left out of the baseline, so comment-only edits to `patch/` are not drift. `REBASE(x)` with a
non-constant argument (a loop variable, say) is listed by `--list` but not inventoried.

### The baseline

`baseline_feb2026_rvas.json` records the tree as it is on upstream `main`. There is one baseline,
not one per game build, because the literals stay February RVAs whichever build runs; supporting a
new build changes `GameBuild.h`, which the baseline also records.

**Regenerating it (`--write-baseline`) is a deliberate act**, done in the same change as the
`patch/` edit it reflects (a new build, a new hook, a pulled upstream update), after reviewing what
`--check` reported. Do not regenerate it just to make `--check` pass: drift is what the check
exists to report. `--write-baseline` refuses to run while any check fails, so it cannot bless a
disagreeing duplicate or an unshifted raw address.

The previous tree (hard-coded per-build Arxan tables in `Arxan.h`, `kSupportedBuilds` and
`kHighestPatchedRva` in `Arxan.cpp`) had a per-build `baseline_693D731E.json` and checks for those
tables. Upstream replaced that design, so those checks and that baseline were removed.
