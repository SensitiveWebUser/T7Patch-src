# Injector refactor plan

From a 9-agent design workflow: four research agents (UI options, binary size, OS-action audit,
current-UI inventory) feeding three independent UI proposals written from deliberately opposed
premises. Every size figure below was **measured**, not estimated — variants were built with the
repo's own toolchain and compressed with brotli q11, because Tauri stores embedded assets
brotli-compressed (`brotli` sits under `tauri-codegen` in `Cargo.lock`, and a string literal from
`App.jsx` cannot be found in the shipped binary).

## The finding that reframes everything

**The frontend cannot be optimised for size, because it is already noise.**

The entire frontend is **42,330 B brotli inside a 9,509,480 B binary — 0.445%**. The most
expensive component library measured (Chakra) would add 0.83% of the binary. The size argument
that justified leaving .NET does not transfer to this layer.

So the UI stack should be chosen on maintenance burden and accessibility. And the binary should be
attacked somewhere else entirely — where the bytes actually are.

## Order of work

The UI redesign is third. Two things come before it.

### 1. Fix what is actively wrong (highest priority)

The OS-action audit found **10 genuinely wrong** behaviours in 20 operations. Four can destroy
user data:

| # | Defect | What the user loses |
| --- | --- | --- |
| 1 | A `t7patch.conf` that is not valid UTF-8 silently resets every setting, and the debounced auto-save then writes that reset over the real file on the first keystroke | Their lobby password. The DLL itself can produce such a file — it round-trips `playername` as raw bytes and truncates at byte 15 |
| 2 | `uninstall_hijack` deletes `dsound.dll` unconditionally, without checking it is ours | `dsound.dll` is one of the most common wrapper names in PC gaming — DSOAL, dgVoodoo, some ReShade setups. Install overwrites it with no backup; uninstall then deletes it |
| 3 | A hand-picked folder is adopted with **zero** validation | Exactly the bug `WarnIfNotGameDirectory` was added to fix in the predecessor, reintroduced without even the soft warning. Mis-pick and you get "Installed. 6 exports verified." and a patch that never loads |
| 4 | On Linux, installing while the game runs truncates a mapped DLL in place | `fs::write` is `O_TRUNC`; Wine has it mapped. Windows refuses this; Linux zeroes pages the game is executing from — SIGBUS with no connection to the button pressed |

Plus: install reports failure *after* having installed (and verifies `SetPlayerName`, not the two
exports the loader actually calls); elevation discards the hand-picked folder and the typed
settings; `is_permission_denied` misses `ERROR_SHARING_VIOLATION`, the single most common failure,
so it neither elevates nor explains; an empty registry value poisons the Steam search into
CWD-relative paths.

None of this is UI work, and all of it outranks UI work.

### 2. Take the binary from 9.5 MB to ~4.7 MB

Four measured, independently rebuilt changes remove **4,811,304 B — 50.5%**. None needs nightly
and none adds a dependency:

| Change | Saved |
| --- | ---: |
| Strip the embedded DLLs — they currently carry DWARF and a mingw COFF symbol table | 1,713,704 |
| Delete the one `regex` call in `core` (a single VDF pattern; hand-parse it) | 1,201,712 |
| Drop `ureq`/`rustls`/`ring` — a whole TLS stack for one GitHub API call | 1,191,360 |
| Drop `tauri-plugin-clipboard-manager` | 694,872 |

The last two are only unlockable *from the frontend*: the update check becomes `fetch()` and the
copy buttons use the DOM. That is the strongest argument for touching the UI at all, and it is
worth roughly 194× what the redesign itself costs in bytes.

Caveats, stated because they are easy to lose:

- The DLL-stripping figure is against the **mingw** pair `dev/build` stages locally. CI ships the
  **MSVC** pair, which may differ — re-measure there before quoting the number.
- `fetch()` to `api.github.com` needs a `connect-src` CSP entry, which moves network egress into
  the webview. That is a real trade, not a free win.
- Two commonly-suggested levers were measured and are **dead**: `-C force-unwind-tables=no`
  changes nothing (the target forces them), and `opt-level = "s"` is **414,024 B bigger** than the
  current `"z"`.
- Nightly `build-std` + `panic=immediate-abort` does save 976,184 B, but disables the panic hook
  `main.rs` installs so that a panic is never silent. **Rejected** — that hook exists because of a
  real incident.
- The payload is ~74% compressible, and a brotli encoder *and* decoder are already in the graph
  via Tauri's `compression` feature — so compressing it costs no new crate.

### 3. The UI itself

All three proposals independently converged, which is worth more than any one of them:

- **No component library.** Measured cost is not the reason — ownership is. The app's only hard
  widget is the modal, and `@tauri-apps/plugin-dialog` already exports native `ask`/`confirm`,
  which is the better desktop UX anyway. Everything else is native HTML the app already uses.
  Explicitly **not** shadcn/ui: its `cn()` helper alone (clsx + tailwind-merge) is +7,734 B —
  more than Tailwind itself — for components you then maintain by hand.
- **No router.** react-router is +25,175 B, 59% of the entire current frontend, to model two tabs
  and a wizard that are already two `useState` values. No URL bar, no back button, no deep links.
- **No React 19.** Measured **+11,558 B for nothing this app uses.**
- **i18n: hand-rolled dictionary.** The complete inventory is 127 strings / 6,599 chars, all
  `{0}`-positional, zero ICU plurals. A ~10-line `t()` plus the catalogue is +2,881 B, nearly all
  of it the catalogue. Lingui (+5,377) is the right upgrade *if* plurals ever appear; i18next
  (+16,593) and react-intl (+14,329) buy nothing this app can use.
- **One open question: Radix Tabs (+6,971) or ~40 hand-written lines.** There is a real
  accessibility defect today — the *selected* tab is `disabled` (`App.jsx:205-206`), so it is
  unreachable by keyboard and announced as unavailable. Radix is correct out of the box; hand-
  written will not be correct first try. Either is defensible.

**The i18n work is mostly a Rust refactor, and that is the part people underestimate.** Every
user-facing failure today is English prose returned as a `String` from Rust (`main.rs:48,50,72`,
`lib.rs:387,405,416,476,488,496`). A JS-side i18n layer localises the chrome and leaves every
error in English. `PARITY-TODO.md` already wants structured injection outcomes for triage reasons
— the predecessor had nine `InjectionOutcome` members. Do both as one change: Rust returns
`{code, args}`, the frontend owns all prose.

### Onboarding in the main window

Today the wizard replaces the entire window (`App.jsx:158` returns early). The proposal worth
adopting reframes it: **stop declaring outcomes and start reflecting disk state.** `App.jsx:168`
renders "T7Patch is installed and your settings have been saved" as an unconditional literal,
because the wizard is a step counter with no idea what actually happened. Replace the counter with
a probe of the game folder, and the whole class of bug where the UI disagrees with reality
disappears instead of being fixed case by case.

Onboarding then becomes a strip inside the persistent frame — both tab panels stay mounted and are
toggled with `hidden`, the frame never scrolls, only the content pane does.

That is the same reasoning as the `ContentFit` rule this project already learned the hard way:
never ask the UI what it thinks; measure the real thing.

## What this plan does not do

- No macOS. Unchanged and still impossible from Docker.
- No preact swap yet. It is the single largest frontend lever (−30,805 B, −73% of the frontend)
  but only −0.32% of the binary, and it forecloses Radix. Revisit after the four size changes
  above, when the binary is half the size and the percentage means more.
