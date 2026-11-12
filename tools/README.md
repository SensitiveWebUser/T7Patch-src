# tools/

Offline developer tooling. Nothing here is built into or shipped with either DLL or the installer.

## `lint_offsets.py`

Every RVA in `patch/` belongs to exactly one `BlackOps3.exe` build. This script (Python 3.10+,
stdlib only) reads the source as text and inventories all of them: the `REBASE(...)`/`OFFSET(...)`
literals, the three Arxan CRC tables in `patch/Arxan.h`, `kSupportedBuilds` and
`kHighestPatchedRva`. It then checks the things a port can get quietly wrong:

- `kHighestPatchedRva` equals the highest RVA in the CRC tables, and the build gate's
  `kHighestPatchedRva + N` margin covers the longest CRC patch;
- no CRC table lists the same RVA twice, and no two tables write overlapping bytes;
- a symbol bound in more than one place (`PTR_X`, the `X` function pointer, and the
  `CREATE_HOOK` target that installs over it) has the same RVA everywhere;
- with `--check`, nothing has drifted from the committed baseline: CRC table counts and contents,
  the build triple(s), the named symbols, and how often each RVA appears in each file.

```bash
python3 tools/lint_offsets.py                  # summary
python3 tools/lint_offsets.py --list           # every site, with file:line
python3 tools/lint_offsets.py --check          # exit 1 on a failed check or any drift
python3 -B -I -m unittest discover -s tools -p 'test_*.py'   # -B: write no __pycache__
```

Comments are stripped before scanning, so commented-out hooks are not counted, and line numbers
are left out of the baseline, so comment-only edits to `patch/` are not drift.

### The baseline is build 0x693D731E

`baseline_693D731E.json` records the constants for the build in `kSupportedBuilds` today
(TimeDateStamp `0x693D731E`, SizeOfImage `0x1D74B000`, EntryPoint `0x02BD40FC`): 259/259/847 CRC
sites. It is the reference a port is measured against, not a cache.

**Regenerating it (`--write-baseline`) is a deliberate act, tied to porting `patch/` to a new
build** and done in the same change as the re-derived offsets, after they have been verified
against that build (see `docs/REBUILD-PLAN.md`). Do not regenerate it just to make `--check` pass:
drift is what the check exists to report. `--write-baseline` refuses to run while any check fails,
so it cannot bless a stale `kHighestPatchedRva` or a disagreeing duplicate.

Each build gets its own file. Without `--baseline`, `--write-baseline` writes
`baseline_<TimeDateStamp>.json` for the single `kSupportedBuilds` entry, so porting leaves this
0x693D731E reference in place rather than overwriting it with new offsets under the old name. It
refuses, unless `--baseline` names the target explicitly, when `kSupportedBuilds` has more than one
entry or the target file already records a different triple. `--check` keeps reading
`baseline_693D731E.json` until a port repoints `DEFAULT_BASELINE` at the new file in the same
change.
