#!/usr/bin/env python3
"""Inventory and lint every BlackOps3.exe address constant in patch/.

Every RVA literal in patch/ is a February-2026-build RVA. patch/GameBuild.h decides at runtime
which supported build is running and, through bo3::address() (which OFFSET/REBASE call), moves
RVAs that fall inside one range of the code section by a fixed delta for the September 2026 build.
The ways a change goes wrong quietly are:

  * a constant that exists in two places (an offsets.h symbol and the hook that targets it, a
    PTR_X define and its X function pointer) is changed in one place and not the other;
  * an address is formed as image_base() + rva directly, skipping bo3::address(), and so is not
    shifted on the September build even though it lies in the shifted range;
  * GameBuild.h's build fingerprints, shift range or static_asserts stop agreeing with each other;
  * the Arxan checksum patterns, their patches or the expected match counts change.

This tool reads the source as text (stdlib only, no compiler) and reports all of these. --check
also compares against the committed baseline; see tools/README.md for when to regenerate it.

  python3 tools/lint_offsets.py                   summary
  python3 tools/lint_offsets.py --list            full inventory with file:line
  python3 tools/lint_offsets.py --check           checks + baseline comparison, exit 1 on failure
  python3 tools/lint_offsets.py --write-baseline  rewrite the baseline (a deliberate act)
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from collections import Counter, defaultdict
from dataclasses import dataclass, field
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
BASELINE_DIR = Path(__file__).resolve().parent
# Every literal in patch/ is a February-2026 RVA whichever build is running, so there is one
# baseline for the tree rather than one per build. (The previous tree, where each build needed its
# own hard-coded RVAs, kept baseline_<TimeDateStamp>.json per build; that scheme went with it.)
DEFAULT_BASELINE = BASELINE_DIR / "baseline_feb2026_rvas.json"
BASELINE_FORMAT = 2

# Vendored libraries have no game addresses and their own macros would only add noise.
EXCLUDED_DIRS = {"minhook", "detours"}

GAMEBUILD_H = "GameBuild.h"
ARXAN_CPP = "Arxan.cpp"
PAGE = 0x1000

# Dropped with the move to upstream's runtime Arxan pattern scan (patch/Arxan.cpp) and
# GameBuild.h's build detection: the 1365-entry crc_patch_* tables in Arxan.h, kSupportedBuilds,
# kHighestPatchedRva and its gate margin no longer exist, so the checks on them (highest RVA,
# table duplicates/overlaps, Apply loops, per-build triples) were removed rather than left to
# report parse errors forever.


# --------------------------------------------------------------------------------------------
# Source handling


def strip_comments(text: str) -> str:
    """Blank out C/C++ comments, keeping every newline so line numbers survive.

    Commented-out hooks are not live addresses, and counting them would make the inventory wrong.
    String and char literal bodies are blanked too: a "//" inside one is not a comment, and a
    REBASE(...) inside one (a log message, say) is not an address.
    """
    out: list[str] = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if c == "/" and nxt == "/":
            j = text.find("\n", i)
            j = n if j == -1 else j
            out.append(" " * (j - i))
            i = j
        elif c == "/" and nxt == "*":
            j = text.find("*/", i + 2)
            j = n if j == -1 else j + 2
            out.append("".join(ch if ch == "\n" else " " for ch in text[i:j]))
            i = j
        elif c in "\"'":
            j = i + 1
            while j < n and text[j] != c and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            j = min(j + 1, n)
            out.append(c + " " * max(j - i - 2, 0) + (text[j - 1] if j - i > 1 else ""))
            i = j
        else:
            out.append(c)
            i += 1
    return "".join(out)


def line_of(text: str, pos: int) -> int:
    return text.count("\n", 0, pos) + 1


def balanced_arg(text: str, open_paren: int) -> tuple[str, int] | None:
    """Return the text inside the parentheses opening at open_paren, and the index after them."""
    depth = 0
    for j in range(open_paren, len(text)):
        if text[j] == "(":
            depth += 1
        elif text[j] == ")":
            depth -= 1
            if depth == 0:
                return text[open_paren + 1 : j], j + 1
    return None


_INT = r"(?:0[xX][0-9A-Fa-f]+|\d+)"
_LITERAL_EXPR = re.compile(rf"^\s*{_INT}(?:\s*[+-]\s*{_INT})*\s*$")
_TERM = re.compile(rf"([+-]?)\s*({_INT})")


def eval_literal(expr: str) -> int | None:
    """Evaluate an integer literal or a +/- chain of them, e.g. "0x1EEACF2 + 1".

    Anything else (a variable or a named constant) returns None; named *Rva constants are
    resolved separately by the caller.
    """
    if not _LITERAL_EXPR.match(expr):
        return None
    total = 0
    for sign, num in _TERM.findall(expr):
        if num[:2].lower() == "0x":
            value = int(num, 16)
        elif len(num) > 1 and num.startswith("0"):
            value = int(num, 8)  # C reads a leading zero as octal
        else:
            value = int(num)
        total += -value if sign == "-" else value
    return total


def hx(value: int) -> str:
    return f"0x{value:X}"


def hx8(value: int) -> str:
    # Build fingerprints are shown 0x%08X, the way users report them, so a report can be matched
    # against the baseline by plain text search.
    return f"0x{value:08X}"


# --------------------------------------------------------------------------------------------
# Inventory


@dataclass
class Site:
    file: str
    line: int
    macro: str
    expr: str
    value: int
    name: str | None = None  # the symbol this site defines or hooks, if any


@dataclass
class Binding:
    """One place that ties a symbol name to an RVA."""

    name: str
    value: int
    file: str
    line: int
    kind: str  # "define", "pointer", "hook-target", "rva-constant"


@dataclass
class RawUse:
    """image_base() + rva: an address formed without bo3::address(), so never shifted."""

    file: str
    line: int
    expr: str
    value: int


@dataclass
class Build:
    name: str
    time_date_stamp: int | None = None
    retail_size: int | None = None
    dump_size: int | None = None


@dataclass
class Shift:
    build_prefix: str  # e.g. "September", from SeptemberShiftStartRva
    start: int
    end: int
    delta: int

    def applies_to(self, build: str) -> bool:
        return build.startswith(self.build_prefix)

    def covers(self, rva: int) -> bool:
        return self.start <= rva < self.end

    def live_rva(self, rva: int, build: str) -> int:
        # Mirrors GameBuild.h get_live_rva().
        return rva - self.delta if self.applies_to(build) and self.covers(rva) else rva


@dataclass
class ArxanCollection:
    """One collect_pattern_patches(pattern, patch, count) call in Arxan.cpp."""

    pattern: str
    patch: str
    count_name: str
    line: int


@dataclass
class Inventory:
    sites: list[Site] = field(default_factory=list)
    bindings: list[Binding] = field(default_factory=list)
    raw_uses: list[RawUse] = field(default_factory=list)
    builds: dict[str, Build] = field(default_factory=dict)
    build_refs: Counter = field(default_factory=Counter)  # fingerprint constant -> uses in GameBuild.h
    shift: Shift | None = None
    shift_asserts: list[tuple[int, str, int, int]] = field(default_factory=list)  # (rva, build, expected, line)
    arxan_arrays: dict[str, tuple[int, list[int], int]] = field(default_factory=dict)  # name -> (declared size, bytes, line)
    arxan_counts: dict[str, int] = field(default_factory=dict)
    arxan_key: int | None = None
    arxan_calls: list[ArxanCollection] = field(default_factory=list)
    unresolved: list[tuple[str, int, str]] = field(default_factory=list)  # REBASE(arg) we could not evaluate
    parse_errors: list[str] = field(default_factory=list)
    # Resolved after every file is scanned, since a name and its target can live in different files.
    aliases: list[tuple[str, str, str, int]] = field(default_factory=list)  # (name, target, file, line)


_MACRO_CALL = re.compile(r"\b(REBASE|OFFSET)\s*\(")
_DEFINE = re.compile(r"^[ \t]*#[ \t]*define[ \t]+(\w+)(\([^)]*\))?[ \t]+(.*)$", re.M)
_POINTER = re.compile(r"\bauto\s*\*?\s*(\w+)\s*=\s*reinterpret_cast\s*<", re.S)
_HOOK = re.compile(r"\b(?:CREATE_HOOK|MH_CreateHook)\s*\(")
_HOOK_ORIGINAL = re.compile(r"&\s*(\w+)\s*\)?\s*$")
# Named game RVAs (LuaSymbols.cpp, Arxan.cpp's kReadySignalRva). GameBuild.h's shift bounds also
# end in "Rva" but are parsed as the shift, not as game addresses.
_RVA_CONST = re.compile(r"\bconstexpr\s+(?:std::)?uintptr_t\s+(\w*Rva)\s*=\s*(0[xX][0-9A-Fa-f]+)\s*;")
_RAW_USE = re.compile(r"\bimage_base\s*\(\s*\)\s*\+\s*(\w+)")


def _statement_end(text: str, start: int) -> int:
    end = text.find(";", start)
    return len(text) if end == -1 else end


def scan_file(rel: str, raw: str, inv: Inventory, constants: dict[str, int]) -> None:
    text = strip_comments(raw)
    is_gamebuild = Path(rel).name == GAMEBUILD_H

    def resolve(expr: str) -> int | None:
        value = eval_literal(expr)
        if value is None and expr.strip() in constants:
            value = constants[expr.strip()]
        return value

    def macro_value(stmt: str) -> tuple[int, int] | None:
        hit = _MACRO_CALL.search(stmt)
        if not hit:
            return None
        arg = balanced_arg(stmt, hit.end() - 1)
        value = resolve(arg[0]) if arg else None
        return None if value is None else (value, hit.start())

    # Each named binding is recorded once, and its REBASE/OFFSET site is labelled with that name
    # so the inventory reads as "what this address is", not just a bare number.
    names_at: dict[int, str] = {}
    aliases: list[tuple[str, str, int]] = []

    if not is_gamebuild:
        for m in _RVA_CONST.finditer(text):
            inv.bindings.append(Binding(m.group(1), int(m.group(2), 16), rel, line_of(text, m.start()), "rva-constant"))

    for m in _DEFINE.finditer(text):
        name, params, body = m.group(1), m.group(2), m.group(3)
        if name in ("REBASE", "OFFSET"):
            continue
        hit = macro_value(body)
        if hit:
            inv.bindings.append(Binding(name, hit[0], rel, line_of(text, m.start()), "define"))
            names_at[m.start(3) + hit[1]] = name
        elif params is None and re.fullmatch(r"\s*\w+\s*", body):
            aliases.append((name, body.strip(), line_of(text, m.start())))

    for m in _POINTER.finditer(text):
        name = m.group(1)
        stmt = text[m.end() : _statement_end(text, m.end())]
        hit = macro_value(stmt)
        if hit:
            inv.bindings.append(Binding(name, hit[0], rel, line_of(text, m.start()), "pointer"))
            names_at[m.end() + hit[1]] = name
        else:
            # reinterpret_cast<...>PTR_X: the pointer inherits PTR_X's address, so record it as an
            # alias and resolve once every file is scanned.
            alias = re.search(r">\s*\(?\s*(\w+)\s*\)?\s*$", stmt)
            if alias:
                aliases.append((name, alias.group(1), line_of(text, m.start())))

    for m in _HOOK.finditer(text):
        arg = balanced_arg(text, m.end() - 1)
        if not arg:
            continue
        hit = macro_value(arg[0])
        original = _HOOK_ORIGINAL.search(arg[0])
        if hit and original:
            inv.bindings.append(Binding(original.group(1), hit[0], rel, line_of(text, m.start()), "hook-target"))
            names_at[m.end() + hit[1]] = original.group(1)

    for m in _MACRO_CALL.finditer(text):
        if is_gamebuild:
            break
        arg = balanced_arg(text, m.end() - 1)
        if not arg:
            continue
        value = resolve(arg[0])
        if value is None:
            # REBASE(x) inside the OFFSET/REBASE #defines themselves is the macro, not a site.
            if not re.match(r"^[ \t]*#[ \t]*define\b", text[text.rfind("\n", 0, m.start()) + 1 : m.start()]):
                inv.unresolved.append((rel, line_of(text, m.start()), arg[0].strip()))
            continue
        label = names_at.get(m.start())
        if label is None and arg[0].strip() in constants:
            label = arg[0].strip()
        inv.sites.append(Site(rel, line_of(text, m.start()), m.group(1), arg[0].strip(), value, label))

    if not is_gamebuild:
        for m in _RAW_USE.finditer(text):
            value = resolve(m.group(1))
            if value is not None:
                inv.raw_uses.append(RawUse(rel, line_of(text, m.start()), m.group(1), value))

    inv.aliases += [(a, t, rel, ln) for a, t, ln in aliases]

    if is_gamebuild:
        _scan_gamebuild(rel, text, inv)
    if Path(rel).name == ARXAN_CPP:
        _scan_arxan(rel, text, inv)


_FINGERPRINT = re.compile(
    r"\bconstexpr\s+(?:std::)?uint32_t\s+(\w+?)(TimeDateStamp|RetailImageSize|DumpImageSize)\s*=\s*(0[xX][0-9A-Fa-f]+)"
)
_SHIFT_PART = re.compile(r"\bconstexpr\s+(?:std::)?uintptr_t\s+(\w+?ShiftStartRva|PrimaryCodeEndRva|\w+?CodeDelta)\s*=\s*(0[xX][0-9A-Fa-f]+)")
_SHIFT_ASSERT = re.compile(
    r"static_assert\s*\(\s*get_live_rva\s*\(\s*(0[xX][0-9A-Fa-f]+)\s*,\s*Build::(\w+)\s*\)\s*==\s*(0[xX][0-9A-Fa-f]+)\s*\)"
)


def _scan_gamebuild(rel: str, text: str, inv: Inventory) -> None:
    fields = {"TimeDateStamp": "time_date_stamp", "RetailImageSize": "retail_size", "DumpImageSize": "dump_size"}
    for m in _FINGERPRINT.finditer(text):
        build = inv.builds.setdefault(m.group(1), Build(m.group(1)))
        setattr(build, fields[m.group(2)], int(m.group(3), 16))
        const = m.group(1) + m.group(2)
        # Everything after the definition: a fingerprint nobody compares against is never accepted.
        inv.build_refs[const] = len(re.findall(rf"\b{const}\b", text[m.end() :]))
    if not inv.builds:
        inv.parse_errors.append(f"{rel}: no <Build>TimeDateStamp/RetailImageSize/DumpImageSize constants found")

    parts: dict[str, list[tuple[str, int]]] = defaultdict(list)
    for m in _SHIFT_PART.finditer(text):
        kind = "start" if m.group(1).endswith("ShiftStartRva") else "end" if m.group(1) == "PrimaryCodeEndRva" else "delta"
        parts[kind].append((m.group(1), int(m.group(2), 16)))
    if all(len(parts[k]) == 1 for k in ("start", "end", "delta")):
        prefix = parts["start"][0][0][: -len("ShiftStartRva")]
        inv.shift = Shift(prefix, parts["start"][0][1], parts["end"][0][1], parts["delta"][0][1])
    else:
        # The lint models GameBuild.h's single shifted range; a second one needs the model extended.
        inv.parse_errors.append(
            f"{rel}: expected exactly one *ShiftStartRva, PrimaryCodeEndRva and *CodeDelta, found "
            + ", ".join(f"{k}={[n for n, _ in parts[k]]}" for k in ("start", "end", "delta"))
        )

    for m in _SHIFT_ASSERT.finditer(text):
        inv.shift_asserts.append((int(m.group(1), 16), m.group(2), int(m.group(3), 16), line_of(text, m.start())))


_ARRAY = re.compile(r"\bstd::array\s*<\s*(?:std::)?uint8_t\s*,\s*(\d+)\s*>\s*(k\w+)\s*=\s*\{([^}]*)\}", re.S)
_COUNT = re.compile(r"\b(kCrc\d+ExpectedCount)\s*=\s*(\d+)")
_KEY = re.compile(r"\bkPatchKey\s*=\s*(0[xX][0-9A-Fa-f]+)")
_COLLECT = re.compile(r"\bcollect_pattern_patches\s*\(\s*(k\w+)\s*,\s*(k\w+)\s*,\s*(k\w+)")
_HEX = re.compile(r"0[xX][0-9A-Fa-f]+")


def _scan_arxan(rel: str, text: str, inv: Inventory) -> None:
    for m in _ARRAY.finditer(text):
        inv.arxan_arrays[m.group(2)] = (int(m.group(1)), [int(h, 16) for h in _HEX.findall(m.group(3))], line_of(text, m.start()))
    for m in _COUNT.finditer(text):
        inv.arxan_counts[m.group(1)] = int(m.group(2))
    m = _KEY.search(text)
    if m:
        inv.arxan_key = int(m.group(1), 16)
    else:
        inv.parse_errors.append(f"{rel}: could not find kPatchKey")
    for m in _COLLECT.finditer(text):
        inv.arxan_calls.append(ArxanCollection(m.group(1), m.group(2), m.group(3), line_of(text, m.start())))
    if not inv.arxan_calls:
        inv.parse_errors.append(f"{rel}: no collect_pattern_patches(pattern, patch, count) calls found")


def scan(root: Path) -> Inventory:
    inv = Inventory()
    patch_dir = root / "patch"
    if not patch_dir.is_dir():
        inv.parse_errors.append(f"{patch_dir}: not a directory")
        return inv
    files = sorted(
        p
        for p in patch_dir.rglob("*")
        if p.suffix in (".cpp", ".h") and not (EXCLUDED_DIRS & set(p.relative_to(patch_dir).parts[:-1]))
    )
    texts = {path.relative_to(root).as_posix(): path.read_text(encoding="utf-8", errors="replace") for path in files}

    # Named *Rva constants are file-local (anonymous namespaces), but resolving them globally as a
    # fallback costs nothing and catches a constant moved into a header.
    global_consts: dict[str, int] = {}
    local_consts: dict[str, dict[str, int]] = {}
    for rel, raw in texts.items():
        if Path(rel).name == GAMEBUILD_H:
            continue
        found = {m.group(1): int(m.group(2), 16) for m in _RVA_CONST.finditer(strip_comments(raw))}
        local_consts[rel] = found
        for k, v in found.items():
            global_consts.setdefault(k, v)

    for rel, raw in texts.items():
        scan_file(rel, raw, inv, {**global_consts, **local_consts.get(rel, {})})

    _resolve_aliases(inv)
    for name in (GAMEBUILD_H, ARXAN_CPP):
        if not (patch_dir / name).is_file():
            inv.parse_errors.append(f"patch/{name}: missing")
    return inv


def _resolve_aliases(inv: Inventory) -> None:
    by_name: dict[str, int] = {}
    for b in inv.bindings:
        if b.kind in ("define", "pointer"):
            by_name.setdefault(b.name, b.value)
    for alias, target, rel, line in inv.aliases:
        if target in by_name:
            inv.bindings.append(Binding(alias, by_name[target], rel, line, f"pointer (via {target})"))


def classify(inv: Inventory, rva: int) -> str:
    return "shifted" if inv.shift and inv.shift.covers(rva) else "unshifted"


# --------------------------------------------------------------------------------------------
# Checks


def _symbol_key(name: str) -> str:
    # offsets.h keeps both "PTR_X" (a REBASE'd address) and "X" (a typed pointer to it) for many
    # functions; they name the same address and must move together.
    return name[4:] if name.startswith("PTR_") else name


def check_symbol_consistency(inv: Inventory) -> list[str]:
    groups: dict[str, list[Binding]] = defaultdict(list)
    for b in inv.bindings:
        groups[_symbol_key(b.name)].append(b)
    errors = []
    for key, bindings in sorted(groups.items()):
        if len({b.value for b in bindings}) > 1:
            where = ", ".join(f"{b.name}={hx(b.value)} ({b.kind}, {b.file}:{b.line})" for b in bindings)
            errors.append(f"symbol '{key}' has disagreeing RVAs: {where}")
    return errors


def check_duplicate_definitions(inv: Inventory) -> list[str]:
    # The same name defined twice (two #defines, two *Rva constants) compiles only if the copies are
    # in different translation units, and then each file silently uses its own copy.
    seen: dict[tuple[str, str], list[Binding]] = defaultdict(list)
    for b in inv.bindings:
        if b.kind in ("define", "rva-constant"):
            seen[(b.kind, b.name)].append(b)
    return [
        f"{kind} '{name}' is defined {len(bs)} times: " + ", ".join(f"{b.file}:{b.line}" for b in bs)
        for (kind, name), bs in sorted(seen.items())
        if len(bs) > 1
    ]


def check_raw_uses(inv: Inventory) -> list[str]:
    if inv.shift is None:
        return []
    return [
        f"{u.file}:{u.line}: image_base() + {u.expr} ({hx(u.value)}) is inside the "
        f"{inv.shift.build_prefix} shift range {hx(inv.shift.start)}..{hx(inv.shift.end)} but bypasses "
        "bo3::address(), so it is not shifted on that build; use REBASE/bo3::address()"
        for u in inv.raw_uses
        if inv.shift.covers(u.value)
    ]


def check_gamebuild(inv: Inventory) -> list[str]:
    errors: list[str] = []
    for b in inv.builds.values():
        for attr, label in (("time_date_stamp", "TimeDateStamp"), ("retail_size", "RetailImageSize"), ("dump_size", "DumpImageSize")):
            if getattr(b, attr) is None:
                errors.append(f"build {b.name}: no {b.name}{label} constant")
            elif inv.build_refs[b.name + label] == 0:
                errors.append(f"build {b.name}: {b.name}{label} is defined but never compared against, so it is never accepted")
        # A reconstructed memory dump rounds SizeOfImage up to the next page; the retail exe keeps
        # its 0x200 file alignment. The two fingerprints must describe the same image.
        if b.retail_size is not None and b.dump_size is not None:
            rounded = -(-b.retail_size // PAGE) * PAGE
            if b.dump_size != rounded:
                errors.append(
                    f"build {b.name}: DumpImageSize {hx8(b.dump_size)} is not RetailImageSize "
                    f"{hx8(b.retail_size)} rounded up to a page ({hx8(rounded)})"
                )
    stamps = Counter(b.time_date_stamp for b in inv.builds.values() if b.time_date_stamp is not None)
    errors += [f"two builds share TimeDateStamp {hx8(t)}" for t, c in stamps.items() if c > 1]

    if inv.shift:
        if inv.shift.start >= inv.shift.end:
            errors.append(f"shift range is empty: start {hx(inv.shift.start)} >= end {hx(inv.shift.end)}")
        if not any(inv.shift.applies_to(name) for name in inv.builds):
            errors.append(f"shift prefix '{inv.shift.build_prefix}' matches no build in {GAMEBUILD_H}")
        for rva, build, expected, line in inv.shift_asserts:
            got = inv.shift.live_rva(rva, build)
            if got != expected:
                errors.append(
                    f"patch/{GAMEBUILD_H}:{line}: static_assert says get_live_rva({hx(rva)}, {build}) == "
                    f"{hx(expected)}, but the parsed shift gives {hx(got)}"
                )
    return errors


def decrypted(inv: Inventory, name: str) -> list[int] | None:
    arr = inv.arxan_arrays.get(name)
    if arr is None or inv.arxan_key is None:
        return None
    return [b ^ inv.arxan_key for b in arr[1]]


def check_arxan(inv: Inventory) -> list[str]:
    errors: list[str] = []
    for name, (size, data, line) in sorted(inv.arxan_arrays.items()):
        if size != len(data):
            errors.append(f"patch/{ARXAN_CPP}:{line}: {name} is declared with {size} bytes but lists {len(data)}")
    used_patterns = Counter(c.pattern for c in inv.arxan_calls)
    for name in sorted(n for n in inv.arxan_arrays if n.startswith("kChecksumPattern")):
        if used_patterns[name] == 0:
            errors.append(f"{name} is defined but never passed to collect_pattern_patches")
    for name, count in sorted(used_patterns.items()):
        if count > 1:
            errors.append(f"{name} is collected {count} times; its sites would be patched twice")
    for c in inv.arxan_calls:
        for ref in (c.pattern, c.patch):
            if ref not in inv.arxan_arrays:
                errors.append(f"patch/{ARXAN_CPP}:{c.line}: collect_pattern_patches uses unknown array {ref}")
        if c.count_name not in inv.arxan_counts:
            errors.append(f"patch/{ARXAN_CPP}:{c.line}: collect_pattern_patches uses unknown count {c.count_name}")
        if c.pattern in inv.arxan_arrays and c.patch in inv.arxan_arrays:
            if inv.arxan_arrays[c.patch][0] > inv.arxan_arrays[c.pattern][0]:
                errors.append(f"{c.patch} is longer than {c.pattern}; it would write past the matched bytes")
    # external_code_patches_installed() counts replacement1 matches as >= kCrc1 + kCrc2 because
    # "the first six bytes of replacement2 are replacement1". Hold the code to that comment.
    p1, p2 = decrypted(inv, "kEncryptedPatch1"), decrypted(inv, "kEncryptedPatch2")
    if p1 is not None and p2 is not None and p2[: len(p1)] != p1:
        errors.append(
            "decrypted kEncryptedPatch2 no longer starts with decrypted kEncryptedPatch1, so "
            "external_code_patches_installed()'s kCrc1 + kCrc2 count is wrong"
        )
    return errors


def run_checks(inv: Inventory) -> list[str]:
    return (
        list(inv.parse_errors)
        + check_symbol_consistency(inv)
        + check_duplicate_definitions(inv)
        + check_raw_uses(inv)
        + check_gamebuild(inv)
        + check_arxan(inv)
    )


# --------------------------------------------------------------------------------------------
# Baseline


def _bytes(data: list[int] | None) -> str | None:
    return None if data is None else " ".join(f"{b:02X}" for b in data)


def build_baseline(inv: Inventory) -> dict:
    # Line numbers are left out on purpose: comment-only edits to patch/ shift them constantly and
    # must not read as drift. --list prints the live file:line inventory when it is needed.
    site_counts = Counter((s.file, s.value) for s in inv.sites)
    symbols: dict[str, str] = {}
    for b in inv.bindings:
        if b.kind != "hook-target":
            symbols.setdefault(b.name, hx(b.value))
    by_class = Counter(classify(inv, s.value) for s in inv.sites)
    distinct = Counter(classify(inv, v) for v in {s.value for s in inv.sites})
    return {
        "format": BASELINE_FORMAT,
        "comment": "Address constants in patch/ (all February-2026 RVAs; GameBuild.h shifts them at "
        "runtime). Regenerate only as part of a deliberate change; see tools/README.md.",
        "builds": [
            {
                "name": b.name,
                "time_date_stamp": hx8(b.time_date_stamp) if b.time_date_stamp is not None else None,
                "retail_size_of_image": hx8(b.retail_size) if b.retail_size is not None else None,
                "dump_size_of_image": hx8(b.dump_size) if b.dump_size is not None else None,
            }
            for b in sorted(inv.builds.values(), key=lambda b: b.name)
        ],
        "shift": None
        if inv.shift is None
        else {"build": inv.shift.build_prefix, "start": hx(inv.shift.start), "end": hx(inv.shift.end), "delta": hx(inv.shift.delta)},
        "arxan": {
            "patch_key": hx(inv.arxan_key) if inv.arxan_key is not None else None,
            "collections": [
                {
                    "pattern": c.pattern,
                    "pattern_bytes": _bytes(inv.arxan_arrays.get(c.pattern, (0, None, 0))[1]),
                    "patch": c.patch,
                    "patch_bytes_decrypted": _bytes(decrypted(inv, c.patch)),
                    "expected_count_name": c.count_name,
                    "expected_count": inv.arxan_counts.get(c.count_name),
                }
                for c in inv.arxan_calls
            ],
        },
        "classification": {
            "sites_shifted": by_class["shifted"],
            "sites_unshifted": by_class["unshifted"],
            "distinct_rvas_shifted": distinct["shifted"],
            "distinct_rvas_unshifted": distinct["unshifted"],
        },
        "symbols": dict(sorted(symbols.items())),
        "sites": [
            {"file": f, "rva": hx(v), "count": c, "class": classify(inv, v)}
            for (f, v), c in sorted(site_counts.items())
        ],
        "raw_uses": sorted({(u.file, hx(u.value)) for u in inv.raw_uses}),
    }


def compare_baseline(current: dict, baseline: dict) -> list[str]:
    if baseline.get("format") != BASELINE_FORMAT:
        return [f"baseline format {baseline.get('format')!r} is not {BASELINE_FORMAT}; regenerate it"]
    errors: list[str] = []
    for key, label in (("builds", "GameBuild.h build fingerprints"), ("shift", "GameBuild.h shift range")):
        if current[key] != baseline[key]:
            errors.append(f"{label} changed: baseline {baseline[key]}, now {current[key]}")

    old_a, new_a = baseline["arxan"], current["arxan"]
    if old_a["patch_key"] != new_a["patch_key"]:
        errors.append(f"Arxan kPatchKey changed: baseline {old_a['patch_key']}, now {new_a['patch_key']}")
    old_c = {c["pattern"]: c for c in old_a["collections"]}
    new_c = {c["pattern"]: c for c in new_a["collections"]}
    for name in sorted(set(old_c) | set(new_c)):
        if name not in new_c:
            errors.append(f"Arxan pattern {name} is no longer collected")
        elif name not in old_c:
            errors.append(f"Arxan pattern {name} is newly collected")
        elif old_c[name]["expected_count"] != new_c[name]["expected_count"]:
            errors.append(
                f"Arxan {name}: expected count {old_c[name]['expected_count']} in baseline, now {new_c[name]['expected_count']}"
            )
        elif old_c[name] != new_c[name]:
            changed = sorted(k for k in old_c[name] if old_c[name][k] != new_c[name].get(k))
            errors.append(f"Arxan {name}: {', '.join(changed)} changed")

    old_s, new_s = baseline["symbols"], current["symbols"]
    for name in sorted(set(old_s) | set(new_s)):
        if old_s.get(name) != new_s.get(name):
            errors.append(f"symbol {name}: baseline {old_s.get(name)}, now {new_s.get(name)}")

    old_sites = {(s["file"], s["rva"]): s["count"] for s in baseline["sites"]}
    new_sites = {(s["file"], s["rva"]): s["count"] for s in current["sites"]}
    for key in sorted(set(old_sites) | set(new_sites)):
        o, n = old_sites.get(key, 0), new_sites.get(key, 0)
        if o != n:
            errors.append(f"{key[0]}: {key[1]} used {n} time(s), baseline {o}")

    old_raw = {tuple(r) for r in baseline["raw_uses"]}
    new_raw = {tuple(r) for r in current["raw_uses"]}
    for f, v in sorted(new_raw - old_raw):
        errors.append(f"{f}: new raw image_base() + {v}")
    for f, v in sorted(old_raw - new_raw):
        errors.append(f"{f}: raw image_base() + {v} is gone")
    return errors


# --------------------------------------------------------------------------------------------
# Output


def summary(inv: Inventory) -> str:
    out = []
    per_file = Counter(s.file for s in inv.sites)
    values = {s.value for s in inv.sites}
    out.append(f"REBASE/OFFSET sites: {len(inv.sites)} ({len(values)} distinct February RVAs)")
    for f, c in sorted(per_file.items()):
        out.append(f"  {f}: {c}")
    if inv.shift:
        sc = Counter(classify(inv, s.value) for s in inv.sites)
        dc = Counter(classify(inv, v) for v in values)
        out.append(
            f"shift ({inv.shift.build_prefix} build, {hx(inv.shift.start)}..{hx(inv.shift.end)} moves by "
            f"-{hx(inv.shift.delta)}): {sc['shifted']} sites / {dc['shifted']} RVAs shifted, "
            f"{sc['unshifted']} sites / {dc['unshifted']} RVAs unshifted"
        )
    out.append(f"raw image_base() + rva uses: {len(inv.raw_uses)}")
    if inv.unresolved:
        out.append(f"REBASE/OFFSET with a non-constant argument (not inventoried): {len(inv.unresolved)}")
    out.append(f"named bindings: {len(inv.bindings)} across {len({_symbol_key(b.name) for b in inv.bindings})} symbols")
    out.append("builds (GameBuild.h):")
    for b in sorted(inv.builds.values(), key=lambda b: b.name):
        out.append(
            f"  {b.name}: TimeDateStamp {hx8(b.time_date_stamp or 0)}  SizeOfImage retail "
            f"{hx8(b.retail_size or 0)} / dump {hx8(b.dump_size or 0)}"
        )
    out.append("Arxan checksum patterns:")
    total = 0
    for c in inv.arxan_calls:
        count = inv.arxan_counts.get(c.count_name)
        total += count or 0
        out.append(f"  {c.pattern} -> {c.patch} ({_bytes(decrypted(inv, c.patch))}), expect {count}")
    out.append(f"  total expected sites: {total}")
    return "\n".join(out)


def listing(inv: Inventory) -> str:
    out = []
    for s in sorted(inv.sites, key=lambda s: (s.file, s.line)):
        label = f"  {s.name}" if s.name else ""
        out.append(f"{s.file}:{s.line}: {s.macro}({s.expr}) = {hx(s.value)} [{classify(inv, s.value)}]{label}")
    for u in sorted(inv.raw_uses, key=lambda u: (u.file, u.line)):
        out.append(f"{u.file}:{u.line}: image_base() + {u.expr} = {hx(u.value)} [raw, {classify(inv, u.value)} range]")
    for f, line, expr in inv.unresolved:
        out.append(f"{f}:{line}: REBASE/OFFSET({expr}) [not a constant]")
    return "\n".join(out)


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    mode = ap.add_mutually_exclusive_group()
    mode.add_argument("--check", action="store_true", help="run checks and compare to the baseline; exit 1 on any failure")
    mode.add_argument("--write-baseline", action="store_true", help="rewrite the baseline from the current source")
    mode.add_argument("--list", action="store_true", help="print every site with file:line")
    ap.add_argument("--root", type=Path, default=REPO_ROOT, help="repository root (default: this checkout)")
    ap.add_argument("--baseline", type=Path, default=DEFAULT_BASELINE, help=f"baseline JSON path (default: {DEFAULT_BASELINE.name})")
    args = ap.parse_args(argv)

    inv = scan(args.root)
    problems = run_checks(inv)

    if args.list:
        print(listing(inv))
        return 0

    if args.write_baseline:
        if problems:
            # A baseline written over a failing tree would bless the failure.
            for p in problems:
                print(f"FAIL: {p}", file=sys.stderr)
            print("refusing to write a baseline while checks fail", file=sys.stderr)
            return 1
        args.baseline.write_text(json.dumps(build_baseline(inv), indent=2) + "\n", encoding="utf-8")
        print(f"wrote {args.baseline}")
        return 0

    if args.check:
        try:
            baseline = json.loads(args.baseline.read_text(encoding="utf-8"))
        except FileNotFoundError:
            problems.append(f"baseline {args.baseline} not found; run --write-baseline")
        except json.JSONDecodeError as e:
            problems.append(f"baseline {args.baseline} is not valid JSON: {e}")
        else:
            problems += [f"drift from baseline: {d}" for d in compare_baseline(build_baseline(inv), baseline)]
        for p in problems:
            print(f"FAIL: {p}", file=sys.stderr)
        if problems:
            print(f"{len(problems)} problem(s)", file=sys.stderr)
            return 1
        print("ok: checks pass and patch/ matches the baseline")
        return 0

    print(summary(inv))
    if problems:
        print()
        for p in problems:
            print(f"FAIL: {p}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
