# injector/PARITY-TODO.md

Generated from a 14-agent audit that compared this implementation against the .NET one domain
by domain, with an adversarial verifier per domain whose job was to REFUTE each claimed gap by
finding the capability in the Tauri source. It refuted **none of them**: 4 blockers, 14 major
and 52 minor gaps, all confirmed. Some have since been fixed; the rest are below.

**Line numbers in the citations below are stale.** They were taken before `injector-tauri/` became
`injector/`, before the react/src-tauri flatten, and before the UI was rewritten. The file paths
and symbol names still resolve; treat the numbers as a hint, not an address.

The reference implementation is parked in `LeaveJustInCase/injector/` and is the only place
several of these behaviours still exist in working form. Read it before reimplementing.

## Blockers - must close before this is the only tool (1 open of 4)

### [inject] Injection is unreachable from the Tauri UI — the command is dead code with no PID source

- **Impact:** The feature does not exist for users of the Tauri build. Anyone who cannot use the dsound.dll hijack — the exact audience the live-injection path exists for, per Strings.resx:75 "Only needed if you have not installed T7Patch above." — has no way to get the patch running at all. Deleting injector/ deletes the only shipping implementation of this capability.
- **Reference:** LeaveJustInCase/injector/T7Patch.Injector.UI/MainForm.cs:552-561 builds a real "Inject into running game" button in a "Live injection" group on the Advanced page; MainForm.cs:907-992 wires it, finds the process, and runs it off the UI thread with the button disabled meanwhile
- **Here:** injector/ui/src/App.jsx:24 defines `inject: (pid) => invoke('inject_into', { pid })` and nothing else in the file ever calls `CMD.inject` — grep for inject/Inject across injector/ui/src/ returns that single line. There is no process enumeration, no PID input, and no button; grep for pid|process in App.jsx returns only line 5 (plugin-process import) and line 24.
- **Fix:** Add a Windows-only "Live injection" section to the Advanced page. It needs a backend command that enumerates BlackOps3.exe processes and returns verified (pid, main-module path) pairs, plus a file picker for the DLL, then calls inject_into with both — mirroring MainForm.cs:907-963. Gate its visibility on a `cfg!(windows)` command the way linux_launch_option already is (main.rs:74-81).

## Major (7 open of 14)

### [locate] Windows: only the first registry hit is used and there is no Program Files fallback, so one bad value means zero detection

- **Impact:** Any Windows machine where HKCU `SteamPath` is present but empty or stale, or where the per-user hive is not the one Steam wrote to (running the tool elevated as a different admin account switches HKCU), gets "Game folder not found automatically" even though Steam sits at the default `C:\Program Files (x86)\Steam`, which .NET would have found from its hardcoded fallback. With an empty value it is worse than not-found: the search silently walks `steamapps\common\...` relative to the exe's working directory.
- **Reference:** LeaveJustInCase/injector/T7Patch.Injector.Core/GameLocator.cs:141-159 — all three registry sources are *appended* to a list (AddWindowsRegistrySteamRoots, GameLocator.cs:182-212), then `%ProgramFiles(x86)%\Steam` and `%ProgramFiles%\Steam` are appended as further candidates; every one of them is searched (GameLocator.cs:107-126). Each registry value is also guarded by `path.Length > 0` (GameLocator.cs:198).
- **Here:** injector/core/src/lib.rs:27-32 — an `.or_else` chain: the first key that reads successfully wins and the other two are never consulted; there is no Program Files candidate anywhere in the file. injector/core/src/lib.rs:47 then returns an *empty* library list if that single lookup is `None`, so `find_game` (lib.rs:59-65) returns `None` outright. There is also no `path.is_empty()` guard: lib.rs:23-25 returns `Some(String)` whenever `rc == 0`, and for an empty `REG_SZ` value `position(|&c| c == 0)` yields 0, so an empty-string root is accepted, `.or_else` stops, and every subsequent `join` (lib.rs:49, 61) becomes a path relative to the process working directory.
- **Fix:** Make the Windows branch return `Vec<PathBuf>` like `DefaultSteamRoots()` does: collect all three registry values (rejecting empty strings, and normalising `/` to `\` as GameLocator.cs:204 does), then push `%ProgramFiles(x86)%\Steam` and `%ProgramFiles%\Steam`; have `steam_libraries` iterate every root that `is_dir()` and union their vdf entries.

### [locate] Linux: the Flatpak path differs from .NET's and the Snap path is missing entirely

- **Impact:** Flatpak Steam is the default Steam on the immutable distros this patch's Linux/Proton audience uses, and Snap Steam is the default on Ubuntu. On those machines the Tauri build reports "Game folder not found automatically" and pushes every user through the unvalidated browse flow (see the first gap), where .NET finds the install unaided. Note the two Flatpak spellings cannot both be right and neither implementation probes both — whichever is correct, the Tauri list is missing at least one real-world root that .NET covers.
- **Reference:** LeaveJustInCase/injector/T7Patch.Injector.Core/GameLocator.cs:168-172 — probes `~/.steam/steam`, `~/.var/app/com.valvesoftware.Steam/.local/share/Steam` (commented "Flatpak Steam, common on immutable/atomic distros") and `~/snap/steam/common/.local/share/Steam` (commented "Snap Steam"), in addition to `~/.local/share/Steam` (GameLocator.cs:166).
- **Here:** injector/core/src/lib.rs:38 — the whole candidate list is `[".steam/steam", ".local/share/Steam", ".var/app/com.valvesoftware.Steam/data/Steam"]`. The Flatpak entry points at `.var/app/com.valvesoftware.Steam/data/Steam`, a *different* directory from the `.var/app/com.valvesoftware.Steam/.local/share/Steam` .NET probes (the Flatpak package overrides `$HOME` to `~/.var/app/com.valvesoftware.Steam`, so Steam's own `$HOME/.local/share/Steam` lands at the .NET path). There is no Snap candidate at all.
- **Fix:** Adopt .NET's list verbatim as the Linux candidate set — `.local/share/Steam`, `.steam/steam`, `.var/app/com.valvesoftware.Steam/.local/share/Steam`, `snap/steam/common/.local/share/Steam` — and additionally keep `.var/app/com.valvesoftware.Steam/data/Steam` if a real Flatpak install is confirmed to use it; probing an extra non-existent path costs one `is_dir()`.

### [install] Install/uninstall errors lose every piece of actionable guidance

> **Partially addressed.** `install` now returns GAME_IS_RUNNING / NEEDS_ELEVATION /
> EMBEDDED_PAYLOAD_INVALID sentinels and the frontend owns the wording. `uninstall` and
> `save_config` still return a raw `io::Error` string, so their catalogue entries are unreachable.

- **Impact:** Installing while the game is running is the single most common failure. On Windows that is ERROR_SHARING_VIOLATION, which is not ErrorKind::PermissionDenied, so it neither elevates nor explains: the user sees 'The process cannot access the file because it is being used by another process. (os error 32)' with no filename, no folder and no 'close the game'. A Flatpak-sandboxed Steam library on Linux gives 'Permission denied (os error 13)' and, since relaunch_elevated is a hardcoded false off Windows (lib.rs:226-227), the only follow-up is 'Could not restart with elevated rights.'
- **Reference:** LeaveJustInCase/injector/T7Patch.Injector.Core/Installer.cs:190-211 and LeaveJustInCase/injector/T7Patch.Injector.Core/Resources/Strings.resx:25-27,31-32 (game-running, permission-denied naming the directory with Program Files/Flatpak guidance, and folder-no-longer-exists)
- **Here:** injector/core/src/lib.rs:143-147 (no classification at all) and :160-162 (is_permission_denied only matches ErrorKind::PermissionDenied); injector/src-tauri/src/main.rs:53 (Err(e) => e.to_string())
- **Fix:** Classify the io::Error per file (sharing violation via raw_os_error()==32 on Windows, PermissionDenied, NotFound) and format messages carrying the file name and directory, mirroring the five Installer/Strings.resx cases.

### [inject] Error-reporting fidelity: nine distinguished outcomes with Win32 error codes collapse to six opaque strings with no GetLastError

> **Partially addressed.** Every failure path now appends GetLastError via `last_error()`. They
> are still prose strings rather than nine typed outcomes the UI can branch on.

- **Impact:** The distinct failures need distinct user actions — CannotOpenProcess means "the game is elevated and this tool isn't" (Strings.resx:109) and calls for the elevation path; LoadFailed means the DLL is bad and calls for a reinstall. "OpenProcess failed" with no code tells the user which to try only by accident, and a support request cannot be triaged without the Win32 error. The .NET class comment names this as the specific thing it was written to avoid.
- **Reference:** LeaveJustInCase/injector/T7Patch.Injector.Core/WindowsInjector.cs:10-45 (nine InjectionOutcome members), :47-61 (InjectionResult carries Win32Error, with the comment "this is what stops six distinct failure modes from collapsing into one bare false"), every P/Invoke declared SetLastError = true (:131-178), and LeaveJustInCase/injector/T7Patch.Injector.UI/MainForm.cs:998-1031 rendering each to its own sentence from Strings.resx:103-116
- **Here:** injector/core/src/lib.rs:271, 276, 278, 280, 282, 285, 291 — seven bare `Err("OpenProcess failed")`-style strings, none of which call GetLastError; injector/src-tauri/src/main.rs:98-99 adds two more. No timeout case, no ModuleNotFoundInTarget, no ActivationExportsMissing, and no error code anywhere.
- **Fix:** Introduce an enum mirroring InjectionOutcome and pair each variant with `GetLastError()` captured immediately after the failing call (windows_sys::Win32::Foundation::GetLastError). Serialise both across the Tauri boundary and render per-variant text in App.jsx, the way the NEEDS_ELEVATION sentinel is already handled (App.jsx:12, :142).

### [update] No launch-time automatic check; toolcheckforupdatesonlaunch is parsed, saved, and ignored

> **Partially addressed.** The Advanced page now has the checkbox, bound to
> `cfg.toolcheckforupdatesonlaunch` in `ui/src/App.jsx`. Nothing reads the value at startup yet,
> so the box still controls a check that never runs.

- **Impact:** Users who never open the Advanced page and click "Check now" are never told about a new release, which is the case the silent launch check exists for. Worse, the preference is visible in t7patch.conf and defaults to enabled, so the file asserts a behaviour the tool does not have — and the UI offers no checkbox to turn the (non-existent) behaviour off.
- **Reference:** LeaveJustInCase/injector/T7Patch.Injector.UI/MainForm.cs:285-288 runs CheckForUpdates(silent: true) at startup; the preference has a real checkbox at MainForm.cs:33, loaded at :689 and saved at :732, defaulting true at LeaveJustInCase/injector/T7Patch.Injector.Core/PatchConfig.cs:31
- **Here:** injector/core/src/config.rs:33,47,132-136,163-166 declares/defaults/parses/writes toolcheckforupdatesonlaunch, but injector/ui/src/App.jsx:99-111 (the only mount effect) only locates the game and loads config — no update call — and no occurrence of toolcheckforupdatesonlaunch exists anywhere in injector/ui/src
- **Fix:** After the mount effect loads cfg, if cfg.toolcheckforupdatesonlaunch is true fire the same check with a silent flag that suppresses the up-to-date/failure log lines, and add the checkbox to the Advanced page next to the existing Updates fieldset (App.jsx:266-278).

### [ui] Live injection is unreachable - the Windows inject path has no UI

- **Impact:** A Windows user with the game already running has no way to inject; the whole 'Live injection' group is gone. The Rust command also takes a pid the UI has no way to obtain - there is no process lookup on the frontend, so even wiring a button up would need backend work first.
- **Reference:** LeaveJustInCase/injector/T7Patch.Injector.UI/MainForm.cs:552-561 (Group_LiveInjection, hint, _injectButton) and MainForm.cs:907-1031 (DLL picker, process resolution, MainModule verification, off-thread inject, nine distinct outcome messages); strings at LeaveJustInCase/injector/T7Patch.Injector.UI/Resources/Strings.resx:102-116
- **Here:** injector/ui/src/App.jsx:24 defines `inject: (pid) => invoke('inject_into', { pid })` and nothing in the file ever calls it - grep for `CMD.inject` in App.jsx returns only line 24; no button, no group, no pid discovery
- **Fix:** Add a Windows-only group mirroring MainForm.cs:552-561, plus a Rust command that resolves the BlackOps3.exe pid and verifies its MainModule path the way MainForm.cs:958-963 does, and map inject errors onto distinct messages. Note the README (injector/README.md:176-182) separately records that the Rust inject never activates the patch, so the button would report success and do nothing until that is fixed too.

### [ui] Installing never writes t7patch.conf

- **Impact:** A user who clicks through the wizard or presses Install without editing a field ends up with the two DLLs and no t7patch.conf. Nothing they saw on the Settings step was persisted, the debounce (App.jsx:116-124) only fires on an actual edit, and the 'conf exists = setup done' signal every other part of this tool keys on is never created.
- **Reference:** LeaveJustInCase/injector/T7Patch.Injector.UI/MainForm.cs:806-812 and 816-828 (both install paths call SaveSettings() straight after the install); LeaveJustInCase/injector/T7Patch.Injector.UI/Wizard/SetupSession.cs:578-580 (RunInstall does InstallFromEmbeddedAssets then WriteConfig)
- **Here:** injector/ui/src/App.jsx:134-149 (doInstall calls CMD.install(dir) only; no CMD.save anywhere in the function) and App.jsx:184 (the wizard's Install step calls the same doInstall)
- **Fix:** In doInstall, after a successful CMD.install, call CMD.save(dir, cfg) and log both outcomes - matching MainForm.cs:810-811 and the two-part result string at Strings.resx:92-94.

## Minor (44 open of 52)

- **[config]** The tool's own three preference keys are written but never read or surfaced — no auto-launch, no launch-time update check, no last-seen-tag suppression
- **[config]** Empty `playername=` overwrites the name instead of preserving it, dropping the DLL's deliberate asymmetry
- **[config]** Config keys are matched case-insensitively, but the DLL matches them case-sensitively
- **[config]** Save's temp file is named per-process, not per-call, and is never cleaned up when the save fails
- **[config]** Save failures surface as raw OS error strings instead of the two causes that actually happen
- **[locate]** A hand-picked game folder is never validated — silent install into the wrong directory
- **[locate]** Linux: the first existing root wins, so a second Steam installation is never searched
- **[locate]** The game folder name is hardcoded and the executable check is case-sensitive, instead of enumerating steamapps/common
- **[locate]** No deduplication of Steam roots and library paths
- **[locate]** A libraryfolders.vdf that is not valid UTF-8 drops every library silently
- **[locate]** Locator paths are lossily converted to String at the Tauri boundary
- **[locate]** No test coverage of Steam-library discovery on the Rust side
- **[install]** Install reports failure after it has already installed both files
- **[install]** Uninstall has no elevation handling (the confirmation half is fixed - see below)
- **[install]** Installing never writes t7patch.conf, while the wizard claims it did
- **[install]** The Linux launch option is absent from the install result (the wizard half is fixed - see below)
- **[install]** No fallback when the payload is absent - and no way to install a hand-picked DLL pair
- **[install]** Export verification checks 1 of the 6 exports, and not the two the loader actually calls
- **[install]** Elevation is triggered without asking, and exits with a pending config save in flight
- **[install]** A vanished or unreachable game folder is not detected before writing
- **[inject]** A timed-out load is reported as success — STILL_ACTIVE is not excluded
- **[inject]** No verification the target PID is actually Black Ops III
- **[inject]** No PE export verification of the DLL before it is mapped into the game
- **[inject]** AlreadyLoaded — a deliberate non-failure — is surfaced as an error
- **[inject]** Load wait is 10 s where the .NET side deliberately uses 30 s under the target's loader lock
- **[inject]** module_loaded matches by ends_with and silently caps at 1024 modules
- **[update]** Loose-version parsing and the "3.05.0" == "3.05" normalisation have no Rust counterpart or test
- **[update]** A failed check is reported to the user as "No update found"
- **[update]** html_url is fetched but never surfaced, and has no https/github.com validation for when it is
- **[update]** LastSeenUpstreamTag is round-tripped in the config but never read or written by the update flow
- **[update]** Release tag is accepted with no length or control-character validation
- **[update]** "Check now" has no in-flight guard and no minimum visible duration
- **[update]** Response body cap is 10x looser than the .NET side's
- **[ui]** 'Auto-launch on startup' setting has no UI and no behaviour
- **[ui]** Elevation happens without asking the user, contradicting the code's own comment
- **[ui]** Layout ignores the desktop font and the window is a fixed pixel size
- **[ui]** Picking a folder with no BlackOps3.exe in it produces no warning
- **[ui]** Wizard Settings step is missing two of the four settings it should surface
- **[ui]** No manual file-picker install path
- **[ui]** Pending debounced save is lost on window close and on the elevate path
- **[ui]** 'Check now' can be re-entered and swallows failures
- **[ui]** Actions with no game folder fail silently instead of saying why
- **[ui]** No tooltips and no live-region announcement for status
- **[ui]** Version label and window identity are hardcoded and divergent

## Fixed since the audit

Re-triaged against the code during the rebuild plan's Phase 1 (`docs/REBUILD-PLAN.md`). Only
items whose fix is visible in the source are listed; anything partly done stays open above with a
note.

- **[ui] Wizard's Done step reports success even when the install failed** - the final step of
  `ui/src/Wizard.jsx` (`id: 'play'`) shows `onboard.doneBody` only behind `allDone`, which is the
  same three probed install-state booleans the step ticks are drawn from, not the outcome of a
  button press.
- **[ui] Wizard Done step omits the Linux Steam launch-option field** - that same step renders the
  launch option with a Copy button whenever `launchOption` is non-null, and refuses to tick green
  on Linux for that reason.
- **[install]/[config]/[ui] Uninstall leaves t7patch.conf, the logs and scratch .tmp files
  behind** - `uninstall_hijack` (`core/src/lib.rs`) removes every name in `INSTALLED_FILES`
  (`t7patch.conf`, `t7patch-dsound.log`, both trace logs, `crashes.log`, ...) and sweeps our own
  `*.tmp` leftovers. The UI asks `confirmUser` before calling it.
- **[install] Copy order is reversed** - `install_hijack` writes `t7patch.dll` before `dsound.dll`,
  with a comment saying why.
- **[install] A hand-picked folder is never checked for BlackOps3.exe** - `InstallReport` carries
  `not_a_game_dir` and the UI logs `warn.installedIntoNonGameDir`. It warns rather than refuses;
  the separate "[ui] no warning at pick time" item stays open.
- **[ui] No localisation seam** - every string goes through `t()` from `ui/src/i18n/`, which has
  its own README on adding a catalogue.
- **[ui] Status log has no Clear button and does not scroll to the newest line** -
  `ui/src/components/StatusLog.jsx` takes `onClear` (wired in App.jsx) and pins `scrollTop` to the
  bottom.
- **[ui] Generated password uses a biased modulo** - `ui/src/lib/password.js` uses rejection
  sampling, and says why in its comment.

- **[config] A conf that is not valid UTF-8 silently reset every setting** - `load` read with
  `read_to_string`, which errors on a single bad byte, and every error mapped to `default()`; the
  debounced auto-save then wrote that reset over the user's real file. Now reads bytes and decodes
  lossily, so only the undecodable characters are affected. Two regression tests.
- **No MSVC payload path exists for the Tauri build, and no CI builds it at all** - CI now has injector-linux and injector-win jobs that stage the MSVC pair from dll-msvc, hard-fail on a missing or zero-byte artifact, and run `cargo test -p t7patch-core` before building.
- **VirtualFreeEx runs after an unchecked wait** - The wait result is checked; on timeout the page is deliberately leaked rather than decommitted under a running remote thread.
- **Tauri injection never activates the patch** - inject() now resolves EnableInjectorlessInstall and zbr_run_gamemode_lui by RVA and calls both after the load.
- **First-run setup wizard never appears when the game folder is found** - Onboarding is driven by probed install state rather than by lookup failure.
- **Injects a bare filename** - inject_into takes an absolute dll_path.
- **No version comparison at all** - isNewerThan lives in ui/src/lib/updates.js with tests; only a strictly newer tag reports as an update.

- **[ui]** First-run setup wizard never appears when the game folder is found
- **[inject]** Tauri injection never activates the patch — it maps an inert DLL and reports success
- **[inject]** Injects a bare filename, not the user's chosen path — LoadLibraryW resolves it through the target's DLL search order
- **[update]** No version comparison at all: any upstream tag reports as an available update
- **[ui]** Update check does no version comparison, cannot open the release page, and has no on-launch option
