# injector/ — the T7Patch installer tool

The injector, as **Tauri 2 (Rust + system webview) with a React frontend**. `./dev/build --injector`
builds it; CI's `injector-linux` / `injector-win` jobs build it, and the `release` job publishes
both on a `v*` tag.

It began as an evaluation against the previous .NET + Eto.Forms implementation, and most of this
file is still the measurements from that comparison — they are kept because they are the honest
record of what this stack costs, not just what it saves.

**It is not yet at parity with what it replaced.** `PARITY-TODO.md` lists what is still missing,
from an audit whose verifiers were told to refute each gap and refuted none. The old
implementation is parked, unbuilt, in `../LeaveJustInCase/injector/` and is the reference for
closing them.

## What it measured

| Build | Linux x64 | Notes |
| --- | ---: | --- |
| .NET 10 + Eto.Forms (the previous implementation, now parked) | **42,057,057 B** | ~42 MB |
| Tauri + React, as first built (local, mingw payload) | **9,518,040 B** | ~9.5 MB |
| Tauri + React, after the size pass (local, mingw payload) | **5,404,320 B** | ~5.4 MB |

**Read the local figures carefully — most of that drop does NOT transfer to the release build.**

| Change | Saved locally | Transfers to the release build? |
| --- | ---: | --- |
| `--strip-all` on the embedded DLL pair before `include_bytes!` | 1,713,704 | **Largely no** |
| Replaced `regex` with a ~40-line hand-written VDF scanner | ~1,201,712 | Yes |
| Dropped `ureq`/`rustls`/`ring`; update check is `fetch()` in the frontend | ~1,191,360 | Yes |

The strip saving is an artefact of the **mingw** pair `dev/build` stages locally, which carries
DWARF (`.debug_info`, `.debug_line`, `.debug_loclists`) straight inside the DLL. The shipped
payload is MSVC-built: `patch/T7Patch.vcxproj` sets `GenerateDebugInformation`, which writes a
separate `.pdb`, and CI publishes only the `.dll`. So there is very little in it to strip, and the
release binary will land far closer to **~7.1 MB** than to 5.4.

The honest, portable saving is the other two: **~2.4 MB**, and `core`'s entire dependency list is
now `serde` (plus `windows-sys` on Windows). Stripping is kept because it is free and verified
safe, not because it is worth 1.7 MB where it counts.

The other trade, stated because it is not free: moving the update check into the webview widens
the CSP to permit `api.github.com`.

**The DLL payload stays embedded, and that is not negotiable.** `core/src/lib.rs` pulls both DLLs
in with `include_bytes!`, so a user downloads one executable and needs nothing else — no separate
DLL download, no archive to unpack. Every size change above was checked against that: stripping
removes debug sections the loader never reads, and the exports survive (verified after every
strip in `dev/build`). Nothing here splits the payload out of the binary, and nothing should.

**React costs nothing here.** A hand-written HTML/JS variant was built alongside this one and
measured 10,156,184 B — 4 KB *larger* than the React build then, which is noise, not a result. A 148 KB
JS bundle (48 KB gzipped) disappears against the Rust and webview-glue code that dominates the
binary. Since the frontend choice was worth nothing on size, the vanilla variant was deleted and
React kept: same bytes, better to write. That measurement is the reason this repo has one
frontend rather than two.

## The catch: what this stack costs

The size win is real. It is also not the number that decides this. Compare what each binary
requires from the machine at load time (`ldd`, link-time `DT_NEEDED` entries):

- **Tauri:** `libwebkit2gtk-4.1`, `libjavascriptcoregtk-4.1`, `libsoup-3.0`, `libgtk-3`, cairo,
  dbus — all **link-time**.
- **.NET + Eto.Forms:** `libc`, `libm`, `libdl`, `libpthread`, `librt`, `libgcc_s`, `libstdc++` — the
  glibc baseline. GTK is loaded later, via `dlopen`, from managed code.

That difference is the whole argument. A Tauri binary on a machine without webkit2gtk **does not
start** — it dies in `ld.so` before `main()`, with no window and no message. The .NET build gets
far enough to catch the failure and print "the system GTK3 runtime is missing. Install it with
your distro's package manager", plus an error log next to the exe. For a tool whose distribution
promise is *one file a game modder downloads and double-clicks*, on Windows **and** on Proton,
that is the difference between a bug report and a silent failure.

For scale: on Arch, `webkit2gtk-4.1` has ~145 reverse-dependencies; `gtk3` has ~806. WebKitGTK is
not reliably present on a gaming box. On Windows the equivalent question is WebView2, which is
bundled with Windows 11 and evergreen-installed on most Windows 10 machines — the Windows side of
this is genuinely fine; Linux is where it breaks.

## Layout

```
core/         everything the installer DOES. A plain library crate with no Tauri, WebKit or GTK
              dependency at all, so it builds and tests standalone.
src-tauri/    the Tauri shell: the window and the command bindings, nothing else.
ui/           the React + Vite frontend, built with bun and embedded at compile time.
assets/       t7patch.dll + dsound.dll (gitignored - produced by ./dev/build).
```

The split is the point, not decoration. `core/` having no path to the UI is what makes it
impossible for a frontend change to alter installer behaviour, and it is why `cargo test
-p t7patch-core` runs in a second with no webview toolchain present — which is also how CI gates
the build before paying for a full webview link.

`core/` holds config load/save (atomic temp+rename, in the format `patch/Protection.cpp` itself
reads), Steam library discovery (registry on Windows, `$HOME` probing plus VDF parsing on Linux),
install/uninstall of the embedded DLLs, a PE export-directory walk with RVA resolution, Steam
launch, elevation, and the Windows inject-and-activate path. The GitHub update check is *not*
here any more: it moved to `ui/src/lib/updates.js` (see the size table above), and `lib.rs`
keeps a tombstone saying so.

Reimplementing all of that is the honest cost of this stack: ~13 native integrations rewritten and
re-tested on two platforms. `PARITY-TODO.md` is what is left of that bill.

## Building

```bash
./dev/build --injector        # -> dev/out/T7Patch-linux-x64
```

That is one command from a clean checkout: `--injector` forces the DLL build (the Rust core embeds
the pair with `include_bytes!`), stages it into `assets/`, and then runs the Tauri build in a
**second container image** — a Rust toolchain plus the WebKitGTK *development* headers, roughly
2 GB — that is built only the first time you pass `--injector`. A `./dev/build --dll` run never
downloads it, which is the point: a DLL-only build should not carry the tool's toolchain. Combine
flags freely; `--dll --injector` (also what a bare `./dev/build` means) keeps the loose DLLs in
`dev/out/` as well as embedding them, from one DLL build.

`--shell --injector` drops into that image (cargo, bun) instead of the default one.

The frontend is **Vite 8 built with bun**, and there is no Node in the image at all — Vite 8
bundles with rolldown and runs under bun, so the JS runtime is one pinned binary rather than a
NodeSource apt repo. Both are pinned (bun 1.4.2, Rust 1.90) for the same reason the DLL build pins
its toolchain. Against the Vite 5 + npm setup this replaced: 147.4 KB bundled instead of 148.7 KB,
in 66 ms instead of 363 ms, from 27 installed packages instead of npm's tree. None of that touches
the shipped binary — the size figures above are unchanged by it.

To build **without** the container you need `cargo` (1.90+), `bun` (1.4+), and the WebKitGTK
development packages installed on the host (`libwebkit2gtk-4.1-dev` on Debian/Ubuntu,
`webkit2gtk-4.1` on Arch) — needing those at all is itself a data point about this stack:

```bash
cd injector/ui && bun install && bun run build && cd ..
cargo build --release -p t7patch-ui     # -> target/release/T7Patch
cargo build --release -p t7patch-core      # the core alone, no UI stack in its tree
```

`assets/` must contain `t7patch.dll` and `dsound.dll` before building — `core` embeds them with
`include_bytes!`, so unlike the .NET side (whose `EmbeddedAssets/` is optional and degrades to a
file picker) a **missing payload here is a compile error**. `dev/build --injector` stages them there
for you, from that same run's freshly built pair - only a manual, no-container build has to do it
by hand.

**Windows is cross-compiled too**, so `--injector` produces both platforms from one Linux
container, as the .NET build did: `dev/out/T7Patch-win-x64.exe`. It goes through `cargo-xwin`, which
fetches Microsoft's own CRT and Windows SDK on first use and targets `x86_64-pc-windows-msvc` (the
MSVC ABI, which is what WebView2 expects — mingw is not a supported path here), with clang, lld and
llvm-rc supplying the compiler, linker and resource compiler.

Two caveats, and they are the point of the comparison. It costs ~700 MB of extra image and four
extra tools to reach where the .NET build gets with no additional toolchain at all. And **this leg
has never been executed** — `dev/build --injector` therefore treats a Windows failure as non-fatal and
still delivers the Linux binary, rather than throwing away a good build for the least proven part
of the setup.

## Bugs found and fixed in this pass

Every one of these was found by reading, not by running, and all but the first were latent behind
the first. The Rust is now compile-checked and unit-tested (see "verified" below).

1. **The binary never loaded its own frontend.** `tauri-macros` decides dev-vs-production with
   `dev: cfg!(not(feature = "custom-protocol"))` — a cargo feature, **not** the profile and not
   `debug_assertions`. `tauri-codegen` then takes the `dev && dev_url.is_some()` branch, embeds
   `EmbeddedAssets::default()` (an *empty* asset set) and points the window at
   `http://localhost:5173`. The `tauri` CLI passes that feature on `tauri build`; a plain
   `cargo build` does not. Symptom: a window that opened, said *"Could not connect to localhost:
   Connection refused"*, and exited. Fixed by making it a default feature, and guarded with a
   `compile_error!` so a release build can never lose it again. **Verified by A/B**: with the
   feature the frontend is in the binary, without it it is not.
2. **The CSP would have blocked every `invoke()`.** `default-src 'self'` with no `connect-src`
   does not permit the IPC origin. Tauri only augments CSP for the isolation pattern, which is not
   enabled here. Now `default-src 'self'; connect-src ipc: http://ipc.localhost; style-src 'self'
   'unsafe-inline'`, matching Tauri's own documented example, and confirmed present in the built
   binary.
3. **The config format was wrong** — see the parity section. It would have written a file the game
   silently ignores.
4. **Elevation could never trigger.** The Rust `install` returns the sentinel `NEEDS_ELEVATION`;
   the frontend tested `String(e).includes('permission')`, which that string does not contain. Now
   a shared constant compared exactly.
5. **The PE parser could kill the app.** Under `panic = "abort"` an out-of-range slice is an
   instant silent death, and `pe_exports` indexed unchecked at attacker-controlled offsets. An
   88-byte file that passes its own header guard was **reproduced panicking**. Every read is now
   bounds-checked, with tests covering truncated, zeroed and garbage input plus the real embedded
   `t7patch.dll`.
6. **Injection proceeded when the module list was unreadable.** `module_loaded` returning `None`
   means "could not tell", not "not loaded"; injecting anyway risks the double-load that
   `dllmain.cpp` warns is not clean. Now refuses, matching the .NET injector.
7. **The "Launch game" button did nothing** but write a log line. Now calls `steam://run/311210`
   through the same reasoning as the .NET `GameLauncher`: a direct `BlackOps3.exe` launch skips
   Steam's launch-option environment, including the `WINEDLLOVERRIDES` the Linux hijack needs.
8. **The Linux launch option had no UI at all**, though it is required for the hijack to work
   under Proton. Now shown with a Copy button, Linux-only.
9. **Unhandled promise rejections** in the folder lookup, uninstall and clipboard paths showed the
   user nothing on failure. All now report into the status log.
10. **`tauri-plugin-opener` was linked, initialised and permissioned but never called.** Removed —
    worth **636 KB** of binary on its own.


## Parity with the implementation it replaced

**`PARITY-TODO.md` is the single source of truth for this.** It is generated from an audit whose
verifiers were told to refute each gap and refuted none, and it is kept current as gaps close.

This section used to duplicate a snapshot of that list, and duplication is exactly how it drifted:
it went on asserting that injection never activates the patch for some time after that was fixed.
Two documents both claiming to list the open gaps is a bug in the documentation, so there is now
one.

The reference implementation is parked, unbuilt, in `../LeaveJustInCase/injector/`.

## What has and has not been verified

**Verified here**, by bootstrapping a Rust and C toolchain plus the WebKitGTK headers into the
sandbox: the **whole Rust tree compiles**, including the Tauri shell, and links to a real 9,515,928
byte release binary. The core's unit tests (`cargo test -p t7patch-core`) cover the config format
taken from `patch/Protection.cpp` itself, the VDF scanner, and the PE parser against truncated,
zeroed and garbage input plus the real embedded `t7patch.dll`; the frontend has its own suite
(`cd ui && bun test`). No count is given here on purpose: this paragraph used to say "7 unit
tests" long after there were more, so run the suites rather than trusting a number in prose. The frontend-embedding fix is confirmed by A/B on the built binary. The compiled-in
CSP was read back out of it. `core` has zero Tauri/WebKit/GTK dependencies. The React frontend
builds under Vite 8 + bun.

**Not verified:** no Tauri binary has been run to a usable window from this repo. Running the
release binary here dies in `ld.so` on a missing `libfribidi.so.0` before `main()` is ever entered -
which is a live demonstration of the link-time argument above rather than a defect in the app, and
is why the panic hook cannot help in that case. The Windows cross-build leg (`cargo-xwin`) has
never been executed and is the least proven part of the setup - `dev/build --injector` treats its
failure as non-fatal for exactly that reason. Nothing has been run on Windows at all.

`main.rs` also sets `WEBKIT_DISABLE_DMABUF_RENDERER=1` and `WEBKIT_DISABLE_COMPOSITING_MODE=1` on
Linux unless the user has. That was first added as a guess at the "opens then closes" report and
was **wrong** - the cause was bug 1 above. It is kept as hardening because the failure it does
prevent is real and has the same silent shape: WebKitGTK's DMABUF renderer kills its own web
process on many Linux desktops, taking the window with it. A GTK build has no second process to
lose, which is a difference worth keeping in view.

## macOS

There is no macOS build here, and there cannot be one from Docker, for either stack. Building for
macOS needs the macOS SDK, which Apple licenses for use on Apple hardware only and does not permit
redistributing in a container image, plus `codesign`/`notarytool` for anything a user can open
without fighting Gatekeeper. `osxcross` can carry a user-supplied SDK for plain Rust, but Tauri's
macOS backend links WKWebView through Objective-C and bundles a `.app`, which is past what that
supports in practice. The .NET tool does not build for macOS either - `dev/build` produces win-x64
and linux-x64 only - so this is a new requirement, not a regression. A `macos-latest` CI runner is
the realistic route for both stacks.

While this was a stack comparison, the React frontend deliberately had no styling ambition:
making it prettier than the Eto UI would have turned it into a design comparison and invited the
wrong conclusion. That comparison is settled, and the UI has since been given its own theme
(`ui/src/theme.css`) and per-component styles under `ui/src/components/`.
