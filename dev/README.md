# dev/

Local build tooling, not part of the shipped patch. Two files — `build` and this
— and both are tracked, so a container-based local build works out of the box on
a fresh clone. Everything a build *produces* (`dev/out/`) is gitignored. No
binary drop-ins are needed any more: every artifact here is built from source in
this repo.

```
./dev/build                  the DLLs, then the injector tool  ->  dev/out/
./dev/build --dll            just the DLLs (all a Linux/Proton install needs)
./dev/build --injector          just the injector tool (linux-x64 + win-x64)
./dev/build --debug          -Og -g instead of -O2
./dev/build --release        -O2 (the default; only useful to override --debug)
./dev/build --shell          drop into the container
./dev/build --clean          remove everything the script generates
./dev/build --help           print this usage block
```

## Setup

The build runs entirely inside a Linux container, so the toolchain is identical
on every host. All the host needs is a container engine (Docker or Podman) and a
`bash` to launch it — the script itself is host-portable.

| Host | Works? | How |
| ---- | ------ | --- |
| **Linux** | yes | `sudo pacman -S --needed docker` (or apt/dnf), then `systemctl enable --now docker` and add yourself to the `docker` group. |
| **macOS** | yes | Docker Desktop (`brew install --cask docker`) or Colima (`brew install colima docker && colima start`). Runs under the stock `/bin/bash` — no Homebrew bash needed. |
| **Windows** | yes, via WSL2 | Install Docker Desktop with the WSL2 backend, then run `./dev/build` from inside your WSL2 distro (Ubuntu), not PowerShell. This is the normal Docker-on-Windows setup, so it is not an extra step. |

Native Windows shells (PowerShell, `cmd`, bare Git Bash) are **not** supported:
the script is bash, and Git Bash mangles the container bind-mount paths. Use WSL2.

The first run builds the image (a few minutes); after that a full build is seconds
and needs no network. The image is tagged with a hash of the Dockerfile text
embedded in `build`, so editing it rebuilds automatically.

The container runs as your UID/GID, so nothing in `dev/out/` is root-owned.

A `--dll` build writes only to `dev/out/`. Every intermediate — object files, the
mingw header shims, the scratch home — goes to a throwaway path inside the
container, which `--rm` discards on exit. `--injector` is the exception: cargo and bun
leave `target/`, `node_modules/` and `dist/` under `injector/` on the host
(gitignored, not root-owned, otherwise harmless) — `--clean` removes those too,
along with the DLL pair it staged into `injector/assets/`.

## Test build, not a release build

mingw-w64 is a different compiler and a different C runtime from the MSVC build
that ships, and this codebase depends on precisely what the shipped build does —
see "The one rule that overrides everything else below" in AGENTS.md. **Publish
only MSVC builds from Windows.**

Most likely divergences: the C runtime (mingw's, not the UCRT, including the ~30
`*_s` calls), unwinding around the `KiUserExceptionDispatcher` hook, and inlining
in the packet paths. Struct layouts are covered by `static_assert`s, so a padding
difference fails the build rather than corrupting a packet.

**One hook is inert here.** `__report_gsfailure` is an MSVC CRT internal that
`Hooks.cpp` detours; mingw has no equivalent, so a stub stands in. Nothing else
is affected, but stack-cookie behaviour needs an MSVC build to test.

Two other deliberate differences from the vcxproj:

- **MinHook is built from source** — the vendored `libMinHook.x64.lib` is
  MSVC-built and will not link against the mingw CRT. The vendored copy is
  untouched.
- **`<Winternl.h>` and `<TlHelp32.h>` get case-correcting symlinks.** MSVC's SDK
  is case-insensitive; mingw ships them lowercase and Linux is not.

After linking, the script reads the PE export directory and checks the six names
the loader and injector resolve. A DLL missing them loads fine and silently does
nothing, which is indistinguishable from the patch not working. `dsound.dll` gets
the same treatment further down the build: the script checks it exports all 12 of
the DirectSound entry points it proxies (see the dsound.dll section below).

## dsound.dll (proxy)

`dsound-proxy/dsound_proxy.cpp` is a from-source replacement for what used to be a real,
patched-in-place Microsoft `dsound.dll` binary (import-table injection: a real system
`dsound.dll`, with one forced import added to force-load a separate loader DLL as a side
effect of the game loading real DirectSound). Reverse-engineering and rebuilding *that* isn't
something this repo does — it would mean reimplementing Microsoft's actual DirectSound COM
subsystem, which is both an enormous undertaking and Microsoft's copyrighted code. Instead,
`dsound-proxy/` is a small, original, from-scratch proxy DLL: it re-exports the same 12
DirectSound entry points as thin trampolines forwarding to the real system `dsound.dll`
(loaded explicitly from `%WINDIR%\System32`, never a bare filename, so it can never resolve
back to itself), and its own `DllMain` does what the old separate loader DLL used to —
load `t7patch.dll`, from an absolute path built from the proxy's own module location so that
"the one sitting next to me" is what actually gets asked for. (It used to pass the bare
filename and rely on the default search order; that worked, but not for the reason the
comments claimed — an unqualified `LoadLibraryA` name is searched for in the *process
executable's* directory, not the calling module's, and those are the same directory here only
because the hijack requires it.) See that file's own remarks for the full reasoning, including
why parameters are passed through as opaque pointers rather than needing `dsound.h`.

`./dev/build` (or `--dll`) builds this alongside `t7patch.dll` into `dev/out/dsound.dll` —
no drop-in files needed for it any more.

## The injector tool

`./dev/build --injector` builds the tool in `injector/` (Tauri 2 + Rust + React) for both
platforms in one run: `dev/out/T7Patch-linux-x64` and `dev/out/T7Patch-win-x64.exe`.
Those two are all a `--injector` run leaves behind — both DLLs are already embedded, so the loose
pair is cleared out of `dev/out/`. Ask for `--dll` (or run with no flags, which is
`--dll --injector`) and the raw `dev/out/dsound.dll` and `dev/out/t7patch.dll` stay put as well.

The Windows leg cross-compiles from the same Linux container via `cargo-xwin`, against Microsoft's
own CRT and Windows SDK. CI does not use it — `tauri-win` runs on `windows-latest` with the real
MSVC toolchain, because a release should not depend on a reimplementation of the linker
environment when a real one is free there.

**The DLL pair it embeds is a mingw test build.** `--injector` forces the DLL build and stages the
result into `injector/assets/`, where `include_bytes!` bakes it in at compile time — there
is no runtime fallback and no file picker. That makes the MSVC-vs-mingw distinction sharper here
than it was for the .NET tool, whose payload was an optional embedded resource. CI stages the
MSVC `t7patch.dll` from the `dll-msvc` job instead; see `.github/workflows/build.yml`.

Two things it needs from the user's machine that the previous .NET tool did not:

- **Linux:** `webkit2gtk` (`libwebkit2gtk-4.1-0`, or `webkit2gtk-4.1` on Arch). It is a *link-time*
  dependency, so without it the binary does not start at all — it dies in `ld.so` before `main()`,
  with no window and no message.
- **Windows:** nothing extra. `injector/.cargo/config.toml` statically links the MSVC CRT so
  the exe does not need the VC++ redistributable, and CI asserts that.

`injector/PARITY-TODO.md` lists what this tool still does not do that the previous .NET
implementation did. That implementation is parked, unbuilt, in `LeaveJustInCase/injector/`.

## Installing

- **Linux/Proton** — copy `dev/out/t7patch.dll` and `dev/out/dsound.dll` next to
  `BlackOps3.exe`, keeping the lowercase names, and set
  `WINEDLLOVERRIDES="dsound=n,b"`. Or run `dev/out/T7Patch-linux-x64` and let it
  do that for you.
- **Windows** — run `dev/out/T7Patch-win-x64.exe`, or copy both DLLs for a
  manual `dsound.dll` install.

See AGENTS.md for why the lowercase name matters and for the trace-log workflow.
