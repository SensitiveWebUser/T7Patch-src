# T7Patch rebuild: working plan

Status as of 2026-11-12, branch `injector-modernization`. **This plan is read-only output.** Nothing below has been run: no files were changed, nothing was staged or committed. Every Windows or in-game claim is unverified, because no BlackOps3.exe of either build is available in this environment.

---

## 0. Interim: what to do right now, before any code changes

- **The game is currently running unpatched.** On the new build the gate refuses, and `RunPatching` returns before it installs hooks, Arxan patches, `Protection::install` or the config watcher. That means there is no packet filtering, no network password and no friends-only filter. The protection is gone; it is not partly working.
- Until a verified build ships:
  - play offline, or only with trusted friends;
  - avoid public matchmaking.
- Removing `dsound.dll` from the game folder gets rid of the box. You lose nothing by doing it, because the protection is already off.
- Post a pinned notice (fork README / release notes): the new game build is unsupported, and "verify game files" will not help. This prevents duplicate reports.
- **Do not hand-edit `kSupportedBuilds` to make the box go away** (see §2).

---

## 1. Situation

### What exists

| Area | Contents | State |
|---|---|---|
| `patch/` → `t7patch.dll` | The C++ patch. MSVC `T7Patch.vcxproj` (v145, /MD, x64 only, `TargetName t7patch`) and vendored prebuilt MinHook. Exports 6 functions. | Staged move from the repo root. Most files are R100. `Protection.cpp`/`.h` are **R096/R097**: the move also deletes the `ZBR_INJECTOR_PASSWORD_MAXLEN` #define and a ZLOG **warning** that fired when a configured password was longer than the old .NET injector's 15-char box. Passwords up to 1023 chars were already accepted, so this is not a behaviour change. |
| `dsound-proxy/dsound_proxy.cpp` | Written from scratch. 12 DirectSound trampolines forward to System32 dsound. `DllMain` loads `t7patch.dll`, pins itself and starts `ActivationThreadProc`. That thread waits for a visible window (120 s cap), waits another 3 s, then calls `EnableInjectorlessInstall` and `zbr_run_gamemode_lui("serious_anticrash_2023")`. | Staged, never committed |
| `dev/build`, `dev/README.md` | Container build: mingw DLLs, plus a Rust/bun image for the tool. Export checks run after link and again after strip. | Staged |
| `.github/workflows/build.yml` | Jobs: dll-msvc (this produces the shipped DLL), dll-mingw-test, dsound-mingw, injector-linux, injector-win, and `release`, which runs only on `v*` tags (`:431`). | Staged, **has never run** |
| `injector/` | Tauri 2 + Rust core (`lib.rs`, `config.rs`, `vdf.rs`) and a React/Vite UI. Embeds both DLLs with `include_bytes!`. | Staged. 5 UI files also have unstaged edits (+46/-9). |
| Local-only (gitignored) | `AGENTS.md` (~500 lines, §0–§13; §13 is "Working conventions for agents"), `CLAUDE.md`, `LeaveJustInCase/` (the parked .NET injector), `.claude/skills/taste-skill`, `patch/.idea` | Exists **only on this disk** |
| Build output (gitignored) | `injector/target` (~5.6 GB), `ui/node_modules`, `dev/out` (Sep 7 mingw build plus T7Patch tool binaries), `injector/assets/*.dll`, `ui/dist`, `src-tauri/gen` | Can be regenerated, **but `dev/out` and `assets/` may be the evidence of which binary produced the report** (see Phase 0) |

### Timeline of what the user has done

1. **2026-04 to 07, upstream Scroptss/T7Patch-src.** The first upload already included the 1365-entry `Arxan.h` and `offsets.h`, with no generator and no provenance. Later commits: "Add Patches from v2.04", then `2e818a3` (3.04; 2026-07-30).
2. **2026-07-25/30.** The user's upstream PR #2 (`4da7e9e`): packet-validation fixes.
3. **2026-08-18/22.** The `hardening` branch:
   - the build gate;
   - packet, Steam and config hardening;
   - Wine exception-hook verification;
   - password canonicalisation;
   - Detours dropped;
   - the trace log.

   `5eeb0c1` (2026-08-18) added the closed-source `redist/t7dwidm_protect.exe`. Four stashes are still there: `stash@{0}` sits on hardening and **contains 5eeb0c1 in its ancestry**; `stash@{1..3}` sit on main `2e818a3`.
4. **2026-08-23.** Fork PR #1 was squash-merged as `46424cd` (HEAD == main == origin/main). Its version string is "Patch 3.05".
5. **2026-08-24 to 09-07, staged but not committed:**
   - the `patch/` move;
   - the dsound proxy;
   - `dev/build` and the CI;
   - a .NET injector, written and then parked;
   - the Tauri rewrite;
   - a parity audit and a refactor plan.

   The last local build is from Sep 7.
6. **After that.** Five unstaged UI polish edits, not finished. `Wizard.jsx:125` still checks `busy === 'save'`, but App.jsx renamed that key to `'saveSettings'`.
7. **Now.** The game refuses the build. The reported exe is linked "2026-08-11", but that date comes from OCR (see §2).

**Open timeline question.** The gate (08-18) and the rest of the hardening (to 08-22) were written against 0x693D731E. If the new exe really shipped on Steam around 08-11, this fork's HEAD may **never have run live** on any build it actually supports. Under §0 the "known-good shipped behaviour" would then be upstream's released 3.04/3.05, not fork HEAD. Check SteamDB for when the new depot was released, and record which tree was last live-verified (Phase 3).

### Branch and remote state

- `git diff --cached`: 80 files, about +13.8k lines. `git diff`: 5 files.
- `origin` has `main` and `hardening`.
- `upstream` is **Scroptss/T7Patch-src**, a source-only repo whose last commit is 2026-07-30. It has no push URL override, so `git push upstream` would actually try to push.
- Release DLLs are published in a **different** repo, Scroptss/T7Patch.
- **Exposure of the closed exe.** `5eeb0c1` can be reached through `hardening`. It also stays reachable through fork PR #1's `refs/pull/1/head`, because GitHub keeps PR refs. That is GitHub's documented behaviour; I have not checked it from here. Deleting the branch does **not** remove the exe from GitHub. Removing it for real needs GitHub Support, or deleting and recreating the repo. The user decides whether this matters.

---

## 2. Why the game says "unsupported build", and what fixes it

### What happened

- `patch/Arxan.cpp` `kSupportedBuilds` contains only `{0x693D731E, 0x1D74B000, 0x02BD40FC}`, which is the 2025-12-13 build.
- **All three reported values came through OCR** ("Ox6A7B6355", "0x1D75BC00", "0x02BD334C"):
  - `SizeOfImage 0x1D75BC00` is not page-aligned, so it is certainly misread;
  - TimeDateStamp and EntryPoint are unverified as well.
- Until they are read from `t7patch_trace.log` (`debuglog=1`, line `arxan: UNSUPPORTED BUILD …`) or from the exe header, **do not name files, branches or docs after 0x6A7B6355**. This plan says "the new build" and uses `<new-ts>` as a placeholder.
- What happened at runtime:
  1. `VerifyGameBuild` found no match and showed the MessageBox.
  2. `RunPatching` returned before `g_patched` was set.
  3. Before that return, `bind_game_pointers()` and `MH_Initialize()` had already run, and `CustomName` had been set to its default (`dllmain.cpp:581-602`).
- The delivery chain still works on the new build: the dsound hijack fires and the activation fires.

### Why this is not a one-line fix

Every entry below is an address or layout taken from the *old* image:

| Category | Count | Where |
|---|---|---|
| Arxan CRC-guard neutralising writes | 1365 (259 + 259 + 847) | `patch/Arxan.h`; the byte shapes are in Arxan.cpp |
| MinHook hook targets | 40 | `Hooks.cpp` (plus `__report_gsfailure`, which is in our own CRT) |
| Game functions we call | ~45 | `offsets.h`, Protection.cpp, inline literals |
| Data globals | ~26 | `offsets.h` (STEAMAPI_*), Protection.cpp, Hooks.cpp, dllmain.cpp |
| Exception fault sites / resume RIPs | 30 / 14 | `dllmain.cpp`, `Protection.cpp` |
| Code byte patches | 8 | `Hooks.cpp` + `Cmp_TokenizeStringInternal+3` |
| Return address / gadgets | 1 / 2 | `0x1EEBF74`, `ROP_RETN 0x1048`, `INT3 0x1059` |
| InspectLM stack-frame offsets | ~6 | `Protection.cpp` |
| Struct field offsets, not pinned by asserts | ~75 | Protection.cpp, Hooks.cpp |
| Steam vtable slots | 9 | `Protection.h`. These belong to steam_api64.dll, not the exe. |

Three things make the port harder:

- Nothing is resolved at runtime.
- Nothing is byte-checked before it is written.
- Several RVAs are written out in more than one place: 0x1EEA560 three times, the 0x22C965C/0x22C9686 and LuaCrash1-3 pairs, and 0x131E350 twice.

**Two shortcuts are ruled out:**

- adding the new triple to `kSupportedBuilds` on its own;
- shifting the old RVAs by a constant delta.

Either one writes 1365 patches plus the hooks blind into code that has moved. The structural floor check (`SizeOfImage > 0x1D6692FA+16`) passes on the new exe, so the triple comparison is the only thing protecting the user.

### Who does what

| Work | Where | Who |
|---|---|---|
| Exact header triple (trace log or exe header) | Windows/Proton | User |
| Copies of the new BlackOps3.exe and steam_api64.dll, with SHA-256 | Windows | User |
| **The old 0x693D731E build.** Download the *whole* previous depot set (every depot manifest for that build, not only the exe) with DepotDownloader (app 311210, previous manifests from SteamDB; needs an account that owns the game) into a **separate directory** | Windows | User. This is the most valuable input: it is the reference that new results are checked against. |
| Activation-time dumps (t7probe) and menu dumps of both builds | Windows | User |
| Disassembler work (IDA/Ghidra + BinDiff/Diaphora), MSVC VS2026 build, live tests | Windows (+Proton) | User, with Claude writing the scripts |
| Preservation commits, docs, lint/manifest tooling, wire characterisation tests, injector build detection, CI fixes | **Linux, here** | Claude, once the user approves commits |
| Running `t7sig` on dumps | Either, once the dumps are local | User/Claude |
| Any claim that "it works in-game" | **Never from Linux** | User's live test |

**Before any reverse engineering, check whether someone has already done it:**

1. `git fetch upstream`. This shows only *source* changes in Scroptss/T7Patch-src, and its last commit predates the new build.
2. Check the **Scroptss/T7Patch releases** page separately. If a release DLL supports the new triple, a `tools/` step can pull its `kSupportedBuilds` and CRC RVA arrays out of the binary. The RVAs are stored in plain form; the patch bytes are XOR 0x69. Those tables are a *candidate* map and a second reference. Every entry must still pass byte verification.

---

## 3. Target architecture (end state, reached in Phase 6, after a minimal port has shipped)

The behaviour, wire format and activation chain stay the same. What changes is how the code is tied to a particular game build.

### `patch/` (MSVC /MD v145 remains the only shipped t7patch.dll)

1. **`patch/game/GameProfile.h`.** One struct per build, selected by the existing triple. It has named fields:
   - `fn.*`, `data.*`, `gadget.*`;
   - `site.*` entries stored as `{rva, expected_bytes, new_bytes}`;
   - `frame.*`, plus an `inspectlm_enabled` flag that can be set per build;
   - `layout.*`;
   - `crc[3][]` stored as `{rva, expected_original_bytes}`.
2. **Generated profiles** go in `patch/game/profiles/build_<ts>.gen.h`. Each one starts with a provenance header (dump SHA-256, tool revision).
3. **`Resolve.cpp`:**
   - Selects the profile by triple. If nothing matches, it shows today's exact MessageBox and returns, leaving the game vanilla.
   - **VerifyProfile.** All-or-nothing memcmp of every CRC site, byte patch, 16-byte hook prologue and fault/resume instruction before anything is written. On failure it names the first site that failed.
   - Writes one trace line on refusal unconditionally, even with debuglog=0, because the MessageBox is often hidden under Proton fullscreen.
   - Computes `kHighestPatchedRva` as constexpr and checks it with static_assert.
4. **Pointers are bound explicitly** after the profile is selected. Two kinds of pointer are kept separate on purpose:
   - `raw_*` (the hooked address; AGENTS §9 needs `Protection::` parsers to re-enter the detours);
   - `orig_*` (the MinHook trampoline).

   Asserts check `raw_X == base+rva` and `orig_X != raw_X`.
5. **The exception path** uses only precomputed plain arrays: no allocation, no locks.
6. **RCDATA `T7P_BUILDS`** lists the supported triples so the injector can read them. It is a resource, not an export, so the six-export ABI does not change.
7. **Optional, later:** an in-process AOB resolver for `fn`/`data` only, still behind VerifyProfile. It never touches CRC, site, frame or layout entries, and there is no runtime CRC scanning.

**Ordering that must be kept in `RunPatching`:** `zbr_enable_trace_from_config` → `bind_game_pointers` → `MH_Initialize` (which tolerates `MH_ERROR_ALREADY_INITIALIZED`) → default `CustomName` → gate → `g_patched = true` → patches and hooks. A refused build therefore leaves MinHook initialised and `g_patched` unset. A retry in the same process still works, because ALREADY_INITIALIZED is accepted. VerifyProfile goes in the current `PatchChecksumComparisons_Precomputed` position.

### `tools/` (offline, never shipped; dumps live in a gitignored `evidence/`)

- **`tools/lint_offsets.py`.** Inventories every REBASE/OFFSET literal and the Arxan tables. Flags a literal whose duplicate copy disagrees with it, and a stale `kHighestPatchedRva`. Writes `baseline_693D731E.json`.
- **`tools/t7probe/`.** A dev-only DLL named `t7patch.dll`, loaded by the **real dsound proxy**. On activation it dumps the image and its triple at the exact moment `RunPatching` would run, then patches nothing. It can optionally dump a second time later.
- **`tools/t7sig/`** (Python, capstone/LIEF):
  - `extract-old` captures expected bytes and their context.
  - `verify --dump old` must reproduce the old table exactly: 259/259/847, with zero extras.
  - `generate --dump new` writes a `.gen.h` plus a `report.md`.
  - Unit tests use synthetic PE fixtures only.
- **`tools/verify-profile`.** A native exe that runs the same check as VerifyProfile against a dump, and also produces a dry-run address/byte log.

### `injector/`

- `core::game_build_ids(path)` reads the triple from the first ~4 KB using the existing bounds-checked helpers.
- `core::payload_supported_builds()` reads the RCDATA resource.
- `InstallState.game_build` drives a "game build not supported yet" badge that is never green, with a report block the user can copy.
- One `VERSION` source feeds Cargo, tauri.conf, `core::CURRENT_VERSION` and `ui/lib/updates.js`.

### `docs/` (tracked)

- `ENGINEERING.md` holds the durable content of AGENTS §0 and §3–§12, with §0 reworded to "shipped behaviour **per build** is the spec; the game can update".
- `WIRE.md`, `PORTING.md`, `SUPPORTED_BUILDS.md`, `LIVE-TEST.md`.
- AGENTS §13 (agent working conventions) stays in the agent-only file. AGENTS.md and CLAUDE.md shrink and point to `docs/`.

### CI additions

- `bun test`, host C++ wire tests and the offsets lint.
- Upload `.pdb` as an artifact.
- Re-pin `softprops/action-gh-release` (release prerequisite).
- A release guard that fails a `v*` tag unless a `live_verified` profile exists for the current build.

---

## 4. Phased plan

Phases 0–2 run on Linux and can proceed while the user works through Phase 3 on Windows. **The minimal port (Phase 4) and its release (Phase 5) come before the big refactor (Phase 6).** That restores protection sooner, and it keeps the refactor from going ahead without a live baseline it can be checked against.

### Phase 0: Preserve everything (first, before any cleanup)

1. **Back up outside the repo.** None of these steps change the repo.
   - `git diff --cached --binary > ~/t7-backup/staged-20261112.patch`
   - `git diff --binary > ~/t7-backup/unstaged-20261112.patch`
   - Also keep a real object: `git update-ref refs/backup/staged-20261112 $(git write-tree)` (local only). Back up the unstaged changes as an object too, with `git stash create`.
   - Make a tar of the full worktree, including `LeaveJustInCase/`, `AGENTS.md`, `CLAUDE.md`, `.claude/skills`, `patch/.idea` and **`dev/out/`, `injector/assets/`**.
   - **Check the backup** by applying both patches in a scratch clone and diffing the result against the worktree. The binaries `icon.ico`, `icon.png` and `libMinHook.x64.lib` must come through.
2. **Record evidence hashes.** `sha256sum dev/out/* injector/assets/*`, plus the `t7patch.dll` and `dsound.dll` currently in the game folder (user). Keep them in `evidence/`.
3. Disable pushing to upstream: `git remote set-url --push upstream DISABLED`.
4. The one-word fix that finishes the user's own edit: `Wizard.jsx:125` `'save'` → `'saveSettings'`.
5. **Commit on `injector-modernization` as logical commits:**
   - (a) the `patch/` move together with the Protection.cpp/.h warning removal, as **one commit**, described as "removed the injector-length mismatch warning (no behaviour change)";
   - (b) `dsound-proxy/`;
   - (c) `dev/`;
   - (d) `.github/workflows/build.yml`;
   - (e) `injector/` core + src-tauri;
   - (f) `injector/ui`;
   - (g) the WIP UI polish + the Wizard fix.

   Then push to origin. Splitting (a) into separate rename and edit commits would mean `git show HEAD:Protection.cpp > patch/Protection.cpp` (and the same for .h), committing, then restoring the edited version. It is not worth doing.
6. **Stashes:**
   - `stash@{1..3}` are based on main `2e818a3`: run `git branch archive/stash-N stash@{N}` and push.
   - **`stash@{0}` must not be pushed as a branch.** Its ancestry includes `5eeb0c1` (the closed exe). Export it off-repo with `git stash show -p --binary --include-untracked stash@{0} > ~/t7-backup/stash0.patch`. If it is needed, re-apply its diff as a single commit on top of main.
   - Before pushing any archive ref, run `git merge-base --is-ancestor 5eeb0c1 <ref>` and refuse the push if it succeeds.
   - Do not drop any stash before reviewing it with `git stash show -p`.
7. Preserve `LeaveJustInCase/` (PARITY-TODO.md cites it). Use an orphan branch `archive/dotnet-injector` (check it does not contain 5eeb0c1) or an off-repo archive, whichever the user chooses.
8. `git fetch upstream` (source only). Separately, check the Scroptss/T7Patch releases: record upstream's latest tag and version string, and whether any release supports the new build.

- **CI risk, corrected.** The expired node20 pin on `softprops/action-gh-release` is used only by the tag-gated `release` job, so a branch push cannot fail because of it. The first push CI can still fail for reasons that have never been tested: the floating `@v7`/`@v8` action majors, the `windows-2025-vs2026` image, cargo-xwin and so on. Treat a failure as information. **Do not tag.** The re-pin is a Phase 5 release prerequisite.
- **Exit:** `git status` is clean apart from ignored output. Origin has every commit plus the safe archive branches. The off-repo backups have been verified, and the evidence hashes are recorded.
- **Needs from user:** approval to commit and push; where LeaveJustInCase should go.

### Phase 1: Make the docs true (no behaviour change)

**AGENTS.md:**
- §0: the game can update, and RVAs are per build.
- §1 vs §12: calling the two activation exports **is** load-bearing (`DllMain` only calls `DisableThreadLibraryCalls`). What is redundant is delivering settings through the exports.
- Remove stale claims: dev/ is gitignored, Detours, Win32 configs, "Linux users never run the injector".
- §11: record that a newer unsupported build exists (triple pending confirmation), and that its triple must not simply be added to the list.
- Resolve the 3.05/3.06 discrepancy (see the version decision in Phase 5).

**Names:** rename `tauri-win`/`tauri-linux` → `injector-win`/`injector-linux` and `windows-latest` → `windows-2025-vs2026`, in CLAUDE.md, dev/README.md, injector/README.md and dev/build.

**Text fixes:**
- CLAUDE.md "~800 lines" → ~500.
- dev/build: the garbled sentence (~:132-133) and the stale line reference (~:248).
- injector/README.md: the "--injector --injector" damage, the test counts, the duplicated paragraphs.
- build.yml: the .NET leftovers (~:239-241, :291).
- Release README.txt / build.yml (~:498-501): "both log lines present" does not mean the patch is working. A T7Patch box that lists TimeDateStamp means the build is unsupported.

**CI pins:** check that the floating action majors exist. Re-pin action-gh-release to a v3.x SHA now, or at the latest before Phase 5.

**PARITY-TODO and REFACTOR-PLAN:** re-triage `injector/PARITY-TODO.md` against the current code. Archive `REFACTOR-PLAN.md` to `docs/history/`.

**Dead code:** add tombstone comments (comments only, nothing deleted) on the 20 unused `PTR_*` entries in offsets.h, the `BG_Cache_*` hooks and `MSG_HostHeartbeat_Inspect`, marking each "not ported".

**Finish the UI polish:**
- CopyButton branches on the boolean `onCopy` returns, and App.jsx returns `copyText`'s result.
- The Wizard intro and Finish text respect `!launchOption`.
- Move the 2 en.js keys into their sections.

- **Exit:** `git grep` finds no `tauri-win`, `EmbeddedAssets` or ".NET jobs". `git diff -w` on `patch/*.cpp` shows only comment changes.
- **Needs from user:** whether AGENTS/CLAUDE content may become tracked `docs/`; which repo the update check should poll.

### Phase 2: Characterisation harness and static inventory (Linux)

1. `tools/lint_offsets.py` → `baseline_693D731E.json`. It fails when a duplicate copy disagrees or `kHighestPatchedRva` is stale.
2. `tests/patch_host/` (mingw in dev/build, native in CI) pins these against fixed vectors:
   - `canon_hash64` and `canon_hash("serious_anticrash_2023")`;
   - `trim_config_field` and `patch_config::loadfrom`;
   - the default conf bytes;
   - prefix-byte derivation;
   - checksum XOR/verify;
   - the 1500 ms grace window.

   Code is pulled out only where the move is purely mechanical.
3. A shared `tests/vectors/conf_cases.txt` is also run by a `config.rs` test. Differences are recorded rather than silently aligned, because the DLL is the spec. The known ones: case-insensitive keys, and an empty playername overwriting.
4. An export snapshot test: exactly the 6 names.
5. CI runs `bun test`, the host tests and the lint.
6. `docs/LIVE-TEST.md`: the golden checklist.

- **Exit:** CI is green on all suites, and the baseline JSON is committed. **Needs from user:** nothing.

### Phase 3: Evidence capture and update assessment (Windows, user-run)

1. **Exact triple** from the trace (`debuglog=1`) or from the exe header (PowerShell / `objdump -p`). This confirms or corrects all three OCR'd values.
2. Copy the new exe and steam_api64.dll into a local `evidence/` with SHA-256. Never commit them.
3. **Old build.** Download the full previous depot set into a **separate install directory**, never over the live Steam install, which auto-update or validation would overwrite. Check that its triple matches exactly.
4. **Dumps, timed to match what VerifyProfile will see.** Put `tools/t7probe` in as `t7patch.dll` and keep the real dsound proxy. Take the dump at the activation moment (visible window + 3 s), and a second dump at the main menu. Repeat over **2 or more launches** of each build. Only bytes that are stable across every dump become `expected_bytes`. Do not "rename dsound.dll" to disable the patch: that would also remove the probe.
5. **If the old build will not run** (content mismatch, Steam offline problems): fall back to static analysis of the on-disk old exe for the `fn`/`data`/`site` entries, and take CRC expected bytes from the on-disk image, since the Arxan guard code is present on disk. Record that this gives lower confidence.
6. Check whether the on-disk header triple equals the in-memory one. This decides whether the injector's pre-check is valid.
7. Compare the steam_api64.dll hashes. If they are equal, the 9 vtable slots need no work.
8. **Assess what the update changed:**
   - the official patch notes and SteamDB depot changes;
   - diffs of the import tables and section lists of the old and new exe (new modules or sections, a changed Arxan stub count);
   - whether the protocol/changelist/netchecksum constants changed;
   - any new anti-cheat or integrity checks.

   Then classify each existing protection as **still-needed**, **now-redundant** (Treyarch fixed it) or **conflicting**.
9. **Timeline:** use the SteamDB release date of the new depot to settle which tree was last live-verified (upstream 3.04, upstream 3.05 or fork HEAD). Phase 4 equivalence is based on that tree.
10. **InspectLM liveness trace on the old build:** only feasible with **two old-build peers on a private LAN or lobby**, because the old build likely cannot matchmake against current servers. Otherwise drop this step and treat InspectLM liveness as an open question (AGENTS §10).

- **Exit:** `evidence/` holds the confirmed triple, both exes, the stable-byte dumps, the hashes, the on-disk/in-memory note, the update assessment and the timeline answer.
- **Needs from user:** all of the above: a Steam account that owns BO3, IDA/Ghidra, BinDiff/Diaphora, x64dbg and VS2026.
- **Go/no-go:** if the old build cannot be recovered at all, CRC regeneration has no reference. Then either use upstream's binary tables or hand-derive the guard templates at much lower confidence. The user decides explicitly.

### Phase 4: Minimal port to the new build (keep the existing literal structure)

Substitute new values into the existing `Arxan.h`, `offsets.h` and inline literals. Do not restructure anything.

| Class | Method | Acceptance |
|---|---|---|
| Functions (40 + ~45) | BinDiff/Diaphora match; string-xref anchors where the match is weak | High similarity **and** an identical 16-byte prologue; still hookable by MinHook |
| Globals (~26) | Decode the RIP-relative disp32 in matched users | At least 2 xref sites agree |
| Fault/resume, byte patches, 0x1EEBF74 | function+offset, re-located by instruction sequence | Byte-identical instructions, otherwise **BLOCKED** until a person reviews it |
| Gadgets 0x1048/0x1059 | Read the bytes | Still `C3` / `CC CC` |
| InspectLM frame | Prologue of the function that owns the return site | If it cannot be proven exactly, disable InspectLM for this build and do not arm the `0xFFEEDDCC44332212` sentinel. A wrong value is remote memory corruption. |
| Struct offsets (~75) | Compare with the game's own `MSG_*_Package` / LobbyMsgRW in both dumps | Confirmed |
| CRC sites | `t7sig` templates validated on the old dump (exactly 259/259/847, zero extras), then applied to the new one | The user accepts the new count, and any template with an odd count is reviewed by hand. **Never diff CRC RVAs between builds.** |
| Steam slots | Compare steam_api64 hashes | Re-derive only if the DLL changed |

- Any disagreement with upstream's tables, if they exist, goes to manual review.
- Protections classed **conflicting** or **now-redundant** in Phase 3.8 go to the user for a decision. They are not silently dropped.
- **Verify offline:** `tools/verify-profile`, driven by the new tables, passes every site on the new stable-byte dumps. Driven by the old tables, it passes on the old dumps. The existing gate stays as it is.
- Add the confirmed triple to `kSupportedBuilds` **in the same commit** as the verified tables. Keep the old build's entries if both can be supported side by side; otherwise keep them in git history.

- **Exit:** every report item is resolved or explicitly disabled with a note, and nothing is guessed.
- **Needs from user:** the reverse-engineering time, and a decision on each site that cannot be ported.

### Phase 5: Live verification and release

1. **Choose the version first.** HEAD already says "Patch 3.05" (`framework.h:268`, `T7Patch.rc:9-11`), and AGENTS §5 says this fork is wire-incompatible with released upstream 3.05. That is already a collision. Upstream will probably call its own release for this game update "3.06". Pick a scheme that cannot collide (a fork suffix or a separate major), and tie it to the update-check repo decision. `updates.js` currently compares against Scroptss/T7Patch tags. Apply the version to `framework.h`, `T7Patch.rc`, `lib.rs` CURRENT_VERSION, `updates.js`, `tauri.conf.json` and `Cargo.toml` together.
2. Prerequisites: action-gh-release re-pinned; a green CI dll-msvc build with the PDB as an artifact. No tag yet.
3. **Run the first live sessions on a secondary Steam account**, in case the update added anti-cheat that reacts to code patching.
4. **Tier 1, solo, Windows and Proton, `debuglog=1`:**
   - no gate box;
   - all hooks and patches logged as installed;
   - conf hot-reload works;
   - CP, MP and ZM each load a map;
   - 15 minutes with no Arxan tamper symptom.
5. **Tier 2, interop:**
   - two patched peers: the same password joins, a different password is rejected, a rotation within the 1500 ms grace window keeps the session, friends-only rejects a non-friend, chat sanitising works, the name spoof is visible;
   - **no password against an unpatched vanilla peer**, and **no password against an upstream-latest peer**: the join succeeds in both directions and nothing is dropped wrongly.
6. **Tier 3 (optional):** malformed lobby packets are dropped, and grep the trace for `inspect`.
7. **Tier 4:** after `Unload()` the game process stays alive, and the game exits cleanly.
8. Tag. The release notes list the supported triple(s).

- **Exit:** Tier 1 and Tier 2 pass on both platforms, with traces archived in `evidence/`.
- **Needs from user:** two machines or accounts, Proton, the go/no-go decision and permission to tag.

### Phase 6: Centralise and add the safety net (after a live-verified baseline exists)

1. Generate `build_<ts>.gen.h` profiles for every supported build from the lint baseline and the stable-byte dumps.
2. Replace literals with profile fields **one file per commit**, with explicit `raw_`/`orig_` binding.
3. Add the in-DLL VerifyProfile at the current gate position. Keep the `RunPatching` ordering from §3, the `g_patched` latch, the Wine read-back and the exception path.
4. **Equivalence without a live MP trace:** a dry-run mode logs every resolved address and every byte it would write, and that log is diffed against the baseline JSON for **both** builds. After that, a Tier 1 smoke test on the live build.
5. RCDATA `T7P_BUILDS`, and the CI release guard.

- **Risk:** swapping the trampoline and raw pointers silently disables InspectorScope masking. The §3.4 asserts catch it.

### Phase 7: Installer awareness and injector gaps

1. `game_build_ids`, `payload_supported_builds`, `InstallState.game_build` and the non-green badge, with tests.
2. The Linux install writes the DLLs as temp-then-rename, keeping the payload-first order. Today `lib.rs:263-264` truncates a DLL that Wine may have mapped.
3. Multi-root Steam discovery, a lossy vdf read, and typed errors for uninstall/save_config.
4. **Decisions needed:**
   - live-injection UI: build it, or remove the backend;
   - the do-nothing `toolautolaunchgame` / `toolcheckforupdatesonlaunch` checkboxes: wire them up or remove them;
   - elevation that loses state.

### Phase 8: Survive the next update

- AOB/string-xref signatures for `fn`/`data`, checked to be unique in every dump.
- An optional runtime resolver behind VerifyProfile.
- A t7probe "report" mode, so users send a report file instead of a screenshot.
- CRC, site, frame and layout entries stay per-build and are generated offline.

---

## 5. Cleanup

### (a) Safe after Phase 0 has been committed, pushed and its backups verified

| Path | Action | Reason |
|---|---|---|
| `injector/ui/src/Wizard.jsx:125` | `'save'` → `'saveSettings'` | Finishes the user's rename |
| `injector/target/` | delete | Regenerated by cargo |
| `injector/ui/node_modules/`, `ui/dist/`, `src-tauri/gen/` | delete | Regenerated |
| `injector/examples/` | delete | Empty, untracked |
| `.claude/scheduled_tasks.lock` | delete | Stale |
| Stale prose | fix (Phase 1) | Includes the `.gitignore` references to a non-existent NOTES.md |
| Dead offsets / BG_Cache / HostHeartbeat_Inspect | tombstone, do not delete | Saves reverse-engineering effort |

### (b) Conditional, or needs a user decision

| Path | Proposed | Condition / why |
|---|---|---|
| `dev/out/`, `injector/assets/*.dll` | delete | **Only after** question 4 is answered and their hashes are in `evidence/`. They may be the binaries that produced the report. Deleting `assets/` breaks the cargo build until `./dev/build --dll` runs. |
| `patch/T7Patch.slnx` x86 platform, vcxproj `<PrecompiledHeaderFile>`, `.filters` adding the .rc | defer or drop | These edit the shipping MSVC project and cannot be verified from Linux. Only do them after Phase 5, behind a green dll-msvc run that compares exports and imports before and after. They are cosmetic. |
| `build.yml` dll-msvc | add the `t7patch.pdb` artifact | Needed for symbolising crashes on the ported build (Phase 5 prerequisite) |
| Staged index + 5 UI files | commit and push (Phase 0) | Only the user authorises commits |
| `stash@{0..3}` | 1–3 → archive branches; **0 → off-repo patch only** | stash@{0} includes 5eeb0c1 |
| `LeaveJustInCase/` | orphan branch or off-repo archive | This is the only copy, and PARITY-TODO cites it |
| `AGENTS.md`, `CLAUDE.md` | move the durable content into tracked `docs/` | Gitignored on purpose today |
| `hardening` (local + origin) | delete once preserved, if the user wants | This only stops advertising the branch. 5eeb0c1 stays reachable through PR #1's refs until GitHub Support purges it or the repo is recreated. The user decides whether to pursue that. |
| `.claude/skills/taste-skill/` | move to `~/.claude/skills` | Unrelated to the patch |
| `patch/.idea/` | leave | IDE layout |
| Version string | non-colliding scheme (Phase 5.1) | User's call |
| `ui/src/lib/updates.js` repo | fork or upstream | Product decision |
| `Hooks.cpp` Info_ValueForKey pass-through; dllmain.cpp duplicate fix-ups | keep | §0: needs a live test first |

---

## 6. Must preserve / explicitly not doing

### Must preserve exactly (AGENTS §0, §3–§12, and the wire contract)

1. **Password hash:**
   - FNV-1a-64 (offset 14695981039346656037, prime 1099511628211);
   - lowercase A–Z only;
   - `& 0x7FFFFFFFFFFFFFFF`;
   - applied after `trim_config_field` (strip BOM, then `" \t\r\n"`);
   - empty → 0, meaning no password.
2. **Rotation:** `PrivatePassword[0]=prev, [1]=cur, [2]=tick`. The grace window requires prev ≠ 0 **and** now ≤ tick+1500 ms.
3. **Prefix:**
   - two bytes, `(h>>16)&0xFF` then `(h>>24)&0xFF`;
   - written after `LobbyMsgRW_PrepWriteMsg` and read after PrepReadMsg;
   - both sides use the `ZBR_HAS_PASSWORD` predicate;
   - on a mismatch, set `readcount=cursize` and **return true**.
4. **Checksum:** outgoing `^ (u16)h`. Incoming accepts the current or the previous key and returns len-2, otherwise -1.
5. **ABI:**
   - six exports;
   - the `canon_hash` gate (seed 0x4B9ACE2F) on `"serious_anticrash_2023"`;
   - lowercase `t7patch.dll`;
   - the proxy's delayed activation (visible window + 3 s);
   - PIN, the System32 path, no DETACH handling.
6. **t7patch.conf:**
   - keys `playername` (15 B), `isfriendsonly` (tri-state), `networkpassword` (1023 B; empty clears), `debuglog`;
   - empty playername/isfriendsonly keeps the current value;
   - the exact default file bytes;
   - 1 s mtime poll;
   - `zbr_enable_trace_from_config` runs before the gate.
7. **Drop policy:**
   - P2P `data[5]` 102/101/109 always, plus 104 when friends-only;
   - IM `'1'`+0x65/0x6D/0x66;
   - lobby 0x0F/0x16/0x17/0x18/0x1E;
   - inspectors 0x02/0x03/0x07/0x10 with limits 18/216/settingssize 1..0xC000;
   - lobbytype 0..1;
   - nattype clamped to 4.
8. **RunPatching ordering:**
   - trace → bind_game_pointers → MH_Initialize (ALREADY_INITIALIZED tolerated) → default name → gate → latch;
   - a refused build leaves MinHook initialised and can be retried in the same process.
9. **Load-bearing oddities:**
   - InspectorScope masks only PackageInt;
   - `raw` Protection:: pointers;
   - no `packageOK&&` on structural checks;
   - the LobbyStateGame theater path;
   - the CheckPendingInfoRequests validation/cast;
   - one shared GetOwnsContent cache;
   - AreExtraSlotsPurchased=true;
   - SEH >4095 → 0;
   - the CL_GetConfigString CP bypass;
   - the VMT hook through a private vtable copy;
   - the Windows/Wine dispatcher split with read-back;
   - the single-owner crash dump + SuspendProcess freeze;
   - the dead `…2211` check;
   - the unreachable second 0x1A604C4 branch;
   - the SetNetworkPassword stack scrub;
   - the Unload ordering.
10. **Build tooling:**
    - MSVC /MD, v145, `windows-2025-vs2026` for the shipped build; mingw for testing only;
    - export checks after link and after strip;
    - the VCRUNTIME import assertion;
    - `custom-protocol` + `compile_error!`;
    - the CSP IPC origin;
    - the panic hook;
    - leak the remote page after an inject timeout.

### Explicitly not doing

- Adding any new triple to `kSupportedBuilds` without verified tables, or naming anything after an unconfirmed OCR'd value.
- Loosening or "degrading" the gate, or applying a constant RVA delta.
- Runtime CRC scanning, or diffing CRC RVAs between builds.
- Signature-scanning frame or struct offsets, or sites in the middle of a function, without a person signing off. Guessing InspectLM offsets; disable it instead.
- Changing the wire format, conf semantics, exports or passphrase, or adding exports.
- Changing the CRT or toolset, or shipping a mingw t7patch.dll.
- "Fixing" any AGENTS §7–§11 entry during the port. Removing pass-through hooks or duplicate fix-ups without a live trace.
- Doing the large GameProfile refactor before a live-verified port exists.
- Committing or uploading any exe, steam_api64.dll or dump. Pushing any ref whose ancestry contains 5eeb0c1.
- Tagging `v*` before a `live_verified` build exists. Running the old build over the live Steam install.
- Deleting stashes, LeaveJustInCase, AGENTS.md, `hardening`, `dev/out` or `assets/` before they are preserved and hashed.
- Rewriting the Tauri injector or the DLL in another language.
- Claiming anything was tested in-game from Linux.

---

## 7. Open questions for the user

1. May I commit the staged work and the 5 UI edits on `injector-modernization`, split as in Phase 0, and push to origin? May I disable push to `upstream`?
2. Can you recover the **old 2025-12-13 build** (the full depot set, into a separate folder)?
3. What are the exact header values from `t7patch_trace.log` (`debuglog=1`) or the exe header? All three, not only SizeOfImage.
4. Which binary produced the MessageBox: a CI/MSVC build, or the local mingw `dev/out` build? On Windows or Proton? What are the hashes of the DLLs in your game folder?
5. Should the AGENTS.md/CLAUDE.md engineering content become tracked `docs/`?
6. Which repo should the update check poll? Is this fork the release channel now?
7. Which version scheme avoids colliding with upstream (HEAD's "3.05" already collides; upstream will likely use "3.06")?
8. What should happen on an unsupported build: keep "box + run vanilla", block play, or show a persistent warning in the injector?
9. Live-injection UI: build it or remove the backend? Auto-launch and update-on-launch checkboxes: implement or remove?
10. Can you run two-peer tests, including against an unpatched peer and an upstream-patched peer? Do you have a secondary Steam account for the first live runs?
11. After preservation, may I delete `hardening` (local and origin), knowing this does not remove 5eeb0c1 from GitHub? Do you want to pursue a GitHub Support purge? Where should `LeaveJustInCase/` and `taste-skill` go?
12. If a hook, crash-fix site or InspectLM cannot be verified on the new build (or turns out to conflict with the update), is shipping with that feature disabled acceptable?

---

## Review log

- APPLIED: backups use `--binary`, plus real git objects, a tarball and a scratch-clone apply check (verified: icon.ico/icon.png/libMinHook show `- -` in numstat).
- APPLIED: the node20 pin affects only the tag-gated `release` job (verified at `build.yml:431`); the CI risk is restated and the re-pin moved to the release prerequisites.
- APPLIED (merged with the duplicate): deleting `hardening` does not purge 5eeb0c1, because PR #1 refs keep it reachable. This is GitHub's documented behaviour and was not verified from here.
- APPLIED: version collision; a non-colliding scheme is now decided before release and tied to the update-check repo.
- APPLIED: the old build may not run. Use a full depot set in a separate directory, with a static on-disk fallback; the old-build MP trace needs two old-build peers or is dropped.
- APPLIED: Protection.cpp/.h are R096/R097 and the removed code is a warning, not a cap (verified); the move and the edit stay in one commit.
- APPLIED: AGENTS.md runs §0–§13 (verified at line 490); §13 stays agent-only.
- APPLIED: `upstream` is source-only Scroptss/T7Patch-src, and releases are checked separately. `upstream` has no pushurl override (verified), so push is disabled in Phase 0.
- PARTIALLY APPLIED: the MH_Initialize-before-gate ordering is now in the preserve list. REJECTED: the claim that a retry would fail with MH_ERROR_ALREADY_INITIALIZED, because `dllmain.cpp:589` explicitly tolerates it.
- APPLIED: stash@{0} contains 5eeb0c1 (verified with `merge-base --is-ancestor`); it is exported off-repo only, and there is an ancestry check before every push.
- APPLIED: `dev/out` and `assets/` are hashed and backed up before any deletion; deletion is gated on question 4.
- APPLIED: added the §0 Interim section (run vanilla/offline, notice to other users).
- APPLIED: added the update-change and anti-cheat assessment (Phase 3.8) and the secondary-account requirement for the first live runs.
- APPLIED: all three header values are treated as OCR-unverified, and nothing is named after 0x6A7B6355 until confirmed.
- APPLIED: timeline question (was HEAD ever live on a supported build?) added to §1 and Phase 3.9.
- APPLIED: dumps are taken with t7probe at activation time, with the real proxy kept, over 2+ launches using stable bytes only; the "rename dsound.dll" instruction is removed.
- APPLIED: the old build is never run over the live install; equivalence uses a dry-run address/byte-log diff instead of MP traces.
- APPLIED: re-ordered to minimal port → live release → refactor (Phases 4/5/6).
- APPLIED: Tier 2 adds no-password interop against vanilla and upstream peers, and records upstream's latest tag in Phase 0.
- APPLIED: MSVC project cosmetic edits moved out of "safe now", deferred behind a green dll-msvc comparison, or dropped.
