#!/usr/bin/env python3
"""Inventory and lint every BlackOps3.exe build-specific constant in patch/.

Every RVA in patch/ is valid for exactly one BlackOps3.exe build. When the game updates, all of
them have to be re-derived, and the two ways that goes wrong quietly are:

  * a constant that exists in two places (an offsets.h symbol and the hook that targets it, a
    PTR_X define and its X function pointer) gets ported in one place and not the other;
  * the Arxan CRC tables change but kHighestPatchedRva, which the build gate's structural floor
    relies on, does not.

This tool reads the source as text (stdlib only, no compiler) and reports both. The baseline
--check compares against describes build 0x693D731E; see tools/README.md for when to regenerate it.

  python3 tools/lint_offsets.py                   summary
  python3 tools/lint_offsets.py --list            full inventory with file:line
  python3 tools/lint_offsets.py --check           checks + baseline comparison, exit 1 on failure
  python3 tools/lint_offsets.py --write-baseline  rewrite the baseline (a deliberate porting act)
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from collections import Counter, defaultdict
from dataclasses import dataclass, field
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
BASELINE_DIR = Path(__file__).resolve().parent
# What --check measures against. A port writes a new per-build file rather than overwriting this
# one, then repoints this name at it in the same change.
DEFAULT_BASELINE = BASELINE_DIR / "baseline_693D731E.json"
BASELINE_FORMAT = 1

# Vendored MinHook has no game addresses and its own macros would only add noise.
EXCLUDED_DIRS = {"minhook"}

ARXAN_H = "Arxan.h"
ARXAN_CPP = "Arxan.cpp"


# --------------------------------------------------------------------------------------------
# Source handling


def strip_comments(text: str) -> str:
    """Blank out C/C++ comments, keeping every newline so line numbers survive.

    Commented-out hooks (Hooks.cpp keeps a block of disabled MH_CreateHook calls) are not live
    addresses, and counting them would make a port look incomplete when it is not. String and char
    literal bodies are blanked too: a "//" inside one is not a comment, and a REBASE(...) inside one
    (a log message, say) is not an address.
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
    """Evaluate an integer literal or a +/- chain of them, e.g. "0x20E2A3E + 0x3".

    Anything else (a variable such as Arxan.cpp's loop "rva") is not a build constant at this site
    and returns None.
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
    # The build triple is always shown the way the DLL's refusal MessageBox prints it (0x%08X), so
    # a user's report can be matched against the baseline by plain text search.
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
    kind: str  # "define", "pointer", "hook-target"


@dataclass
class ArxanTable:
    name: str
    file: str
    line: int
    rvas: list[int]
    patch: str | None = None  # the ObfuscatedPatch applied to every entry
    patch_len: int | None = None


@dataclass
class Inventory:
    sites: list[Site] = field(default_factory=list)
    bindings: list[Binding] = field(default_factory=list)
    tables: dict[str, ArxanTable] = field(default_factory=dict)
    supported_builds: list[tuple[int, int, int]] = field(default_factory=list)
    highest_patched_rva: int | None = None
    gate_margin: int | None = None
    parse_errors: list[str] = field(default_factory=list)
    # Resolved after every file is scanned, since a name and its target can live in different files.
    aliases: list[tuple[str, str, str, int]] = field(default_factory=list)  # (name, target, file, line)
    apply_loops: list[tuple[str, str, int | None]] = field(default_factory=list)  # (table, patch, len)


_MACRO_CALL = re.compile(r"\b(REBASE|OFFSET)\s*\(")
_DEFINE = re.compile(r"^[ \t]*#[ \t]*define[ \t]+(\w+)(\([^)]*\))?[ \t]+(.*)$", re.M)
_POINTER = re.compile(r"\bauto\s+(\w+)\s*=\s*reinterpret_cast\s*<", re.S)
_HOOK = re.compile(r"\bCREATE_HOOK\s*\(")
_HOOK_ORIGINAL = re.compile(r"&\s*(\w+)\s*\)?\s*$")


def _first_macro_value(stmt: str) -> tuple[str, int] | None:
    m = _MACRO_CALL.search(stmt)
    if not m:
        return None
    arg = balanced_arg(stmt, m.end() - 1)
    if not arg:
        return None
    value = eval_literal(arg[0])
    return None if value is None else (m.group(1), value)


def _statement_end(text: str, start: int) -> int:
    end = text.find(";", start)
    return len(text) if end == -1 else end


def scan_file(rel: str, raw: str, inv: Inventory) -> None:
    text = strip_comments(raw)

    # Each named binding is recorded once, and its REBASE/OFFSET site is labelled with that name
    # so the inventory reads as "what this address is", not just a bare number.
    names_at: dict[int, str] = {}

    defines_with_alias: list[tuple[str, str, int]] = []
    for m in _DEFINE.finditer(text):
        name, params, body = m.group(1), m.group(2), m.group(3)
        if name in ("REBASE", "OFFSET"):
            continue
        hit = _MACRO_CALL.search(body)
        if hit:
            arg = balanced_arg(body, hit.end() - 1)
            value = eval_literal(arg[0]) if arg else None
            if value is not None:
                inv.bindings.append(Binding(name, value, rel, line_of(text, m.start()), "define"))
                names_at[m.start(3) + hit.start()] = name
        elif params is None and re.fullmatch(r"\s*\w+\s*", body):
            defines_with_alias.append((name, body.strip(), line_of(text, m.start())))

    for m in _POINTER.finditer(text):
        name = m.group(1)
        stmt_end = _statement_end(text, m.end())
        stmt = text[m.end() : stmt_end]
        hit = _MACRO_CALL.search(stmt)
        if hit:
            arg = balanced_arg(stmt, hit.end() - 1)
            value = eval_literal(arg[0]) if arg else None
            if value is not None:
                inv.bindings.append(Binding(name, value, rel, line_of(text, m.start()), "pointer"))
                names_at[m.end() + hit.start()] = name
        else:
            # reinterpret_cast<...>PTR_X: the pointer inherits PTR_X's address, so record it as an
            # alias and resolve once every file is scanned.
            alias = re.search(r">\s*\(?\s*(\w+)\s*\)?\s*$", stmt)
            if alias:
                defines_with_alias.append((name, alias.group(1), line_of(text, m.start())))

    for m in _HOOK.finditer(text):
        arg = balanced_arg(text, m.end() - 1)
        if not arg:
            continue
        hit = _MACRO_CALL.search(arg[0])
        original = _HOOK_ORIGINAL.search(arg[0])
        if not hit or not original:
            continue
        inner = balanced_arg(arg[0], hit.end() - 1)
        value = eval_literal(inner[0]) if inner else None
        if value is not None:
            inv.bindings.append(
                Binding(original.group(1), value, rel, line_of(text, m.start()), "hook-target")
            )
            names_at[m.end() + hit.start()] = original.group(1)

    for m in _MACRO_CALL.finditer(text):
        arg = balanced_arg(text, m.end() - 1)
        if not arg:
            continue
        value = eval_literal(arg[0])
        if value is None:
            continue
        inv.sites.append(
            Site(rel, line_of(text, m.start()), m.group(1), arg[0].strip(), value, names_at.get(m.start()))
        )

    inv.aliases += [(a, t, rel, ln) for a, t, ln in defines_with_alias]

    if Path(rel).name == ARXAN_H:
        _scan_arxan_tables(rel, text, inv)
    if Path(rel).name == ARXAN_CPP:
        _scan_arxan_cpp(rel, text, inv)


_TABLE = re.compile(r"\b(\w+)\s*\[\s*\]\s*=\s*\{([^}]*)\}", re.S)
_HEX = re.compile(r"0[xX][0-9A-Fa-f]+")


def _scan_arxan_tables(rel: str, text: str, inv: Inventory) -> None:
    for m in _TABLE.finditer(text):
        if not m.group(1).startswith("crc_patch"):
            continue
        rvas = [int(h, 16) for h in _HEX.findall(m.group(2))]
        inv.tables[m.group(1)] = ArxanTable(m.group(1), rel, line_of(text, m.start()), rvas)


_BUILDS = re.compile(r"\bkSupportedBuilds\s*\[\s*\]\s*=\s*\{(.*?)\}\s*;", re.S)
_TRIPLE = re.compile(r"\{\s*(0[xX][0-9A-Fa-f]+)\s*,\s*(0[xX][0-9A-Fa-f]+)\s*,\s*(0[xX][0-9A-Fa-f]+)\s*\}")
_HIGHEST = re.compile(r"\bkHighestPatchedRva\s*=\s*(0[xX][0-9A-Fa-f]+)")
_MARGIN = re.compile(r"\bkHighestPatchedRva\s*\+\s*(\d+|0[xX][0-9A-Fa-f]+)")
_OBF_PATCH = re.compile(r"\bObfuscatedPatch\s+(\w+)\s*\(\s*\{([^}]*)\}")
_APPLY_LOOP = re.compile(r"for\s*\(\s*auto\s+\w+\s*:\s*(\w+)\s*\)\s*(\w+)\s*\.\s*Apply")


def _scan_arxan_cpp(rel: str, text: str, inv: Inventory) -> None:
    m = _BUILDS.search(text)
    if m:
        inv.supported_builds = [tuple(int(v, 16) for v in t) for t in _TRIPLE.findall(m.group(1))]
    else:
        inv.parse_errors.append(f"{rel}: could not find kSupportedBuilds[]")

    m = _HIGHEST.search(text)
    if m:
        inv.highest_patched_rva = int(m.group(1), 16)
    else:
        inv.parse_errors.append(f"{rel}: could not find kHighestPatchedRva")

    m = _MARGIN.search(text)
    if m:
        inv.gate_margin = int(m.group(1), 0)
    else:
        inv.parse_errors.append(f"{rel}: could not find the 'kHighestPatchedRva + N' gate margin")

    patch_lens = {p.group(1): len(_HEX.findall(p.group(2))) for p in _OBF_PATCH.finditer(text)}
    inv.apply_loops += [(t, p, patch_lens.get(p)) for t, p in _APPLY_LOOP.findall(text)]


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
    for path in files:
        rel = path.relative_to(root).as_posix()
        scan_file(rel, path.read_text(encoding="utf-8", errors="replace"), inv)

    _resolve_aliases(inv)

    for table, patch, length in inv.apply_loops:
        if table in inv.tables:
            inv.tables[table].patch = patch
            inv.tables[table].patch_len = length
    for name in (ARXAN_H, ARXAN_CPP):
        if not (patch_dir / name).is_file():
            inv.parse_errors.append(f"patch/{name}: missing")
    if (patch_dir / ARXAN_H).is_file() and not inv.tables:
        inv.parse_errors.append(f"patch/{ARXAN_H}: no crc_patch_* tables found")
    return inv


def _resolve_aliases(inv: Inventory) -> None:
    by_name: dict[str, int] = {}
    for b in inv.bindings:
        if b.kind in ("define", "pointer"):
            by_name.setdefault(b.name, b.value)
    for alias, target, rel, line in inv.aliases:
        if target in by_name:
            inv.bindings.append(Binding(alias, by_name[target], rel, line, f"pointer (via {target})"))


# --------------------------------------------------------------------------------------------
# Checks


def _symbol_key(name: str) -> str:
    # offsets.h keeps both "PTR_X" (a REBASE'd address) and "X" (a typed pointer to it) for many
    # functions; they name the same address and must move together in a port.
    return name[4:] if name.startswith("PTR_") else name


def check_highest_rva(inv: Inventory) -> list[str]:
    errors: list[str] = []
    if inv.highest_patched_rva is None or not inv.tables:
        return errors
    # kHighestPatchedRva is the start of the highest write, not its end: the gate adds a margin
    # instead of the patch length, so the check is that the margin covers the longest patch.
    true_max = max(max(t.rvas) for t in inv.tables.values() if t.rvas)
    if inv.highest_patched_rva != true_max:
        errors.append(
            f"kHighestPatchedRva is {hx(inv.highest_patched_rva)} but the highest Arxan table RVA "
            f"is {hx(true_max)}; update it in patch/{ARXAN_CPP} (the build gate's structural floor "
            f"depends on it)"
        )
    lengths = [t.patch_len for t in inv.tables.values() if t.patch_len]
    if inv.gate_margin is not None and lengths and inv.gate_margin < max(lengths):
        errors.append(
            f"build gate margin 'kHighestPatchedRva + {inv.gate_margin}' is smaller than the "
            f"longest CRC patch ({max(lengths)} bytes)"
        )
    return errors


def check_apply_loops(inv: Inventory) -> list[str]:
    errors: list[str] = []
    for t in inv.tables.values():
        if t.patch is None:
            errors.append(f"{t.name}: no 'for (auto rva : {t.name}) <patch>.Apply' loop found in patch/{ARXAN_CPP}")
        elif t.patch_len is None:
            errors.append(f"{t.name}: applied with {t.patch}, whose byte list was not found")
    return errors


def check_table_duplicates(inv: Inventory) -> list[str]:
    errors: list[str] = []
    for t in inv.tables.values():
        for rva, count in sorted(Counter(t.rvas).items()):
            if count > 1:
                errors.append(f"{t.name}: RVA {hx(rva)} appears {count} times")
    return errors


def check_table_overlaps(inv: Inventory) -> list[str]:
    # Two tables writing overlapping bytes would leave whichever runs last in place, silently
    # corrupting the other patch. Not observed in 0x693D731E, so any hit is new.
    spans = sorted(
        (rva, rva + (t.patch_len or 1), t.name)
        for t in inv.tables.values()
        for rva in set(t.rvas)
    )
    errors = []
    for (a0, a1, an), (b0, _b1, bn) in zip(spans, spans[1:]):
        if b0 < a1:
            errors.append(f"{an} {hx(a0)} and {bn} {hx(b0)} write overlapping bytes")
    return errors


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


def run_checks(inv: Inventory) -> list[str]:
    return (
        list(inv.parse_errors)
        + check_highest_rva(inv)
        + check_apply_loops(inv)
        + check_table_duplicates(inv)
        + check_table_overlaps(inv)
        + check_symbol_consistency(inv)
    )


# --------------------------------------------------------------------------------------------
# Baseline


def _digest(values) -> str:
    return hashlib.sha256("\n".join(hx(v) for v in values).encode()).hexdigest()


def build_baseline(inv: Inventory) -> dict:
    # Line numbers are left out on purpose: comment-only edits to patch/ shift them constantly and
    # must not read as drift. --list prints the live file:line inventory when it is needed.
    site_counts = Counter((s.file, hx(s.value)) for s in inv.sites)
    symbols: dict[str, str] = {}
    for b in inv.bindings:
        if b.kind != "hook-target":
            symbols.setdefault(b.name, hx(b.value))
    return {
        "format": BASELINE_FORMAT,
        "comment": "Build-specific constants in patch/ for BlackOps3.exe "
        + (", ".join(hx8(a) for a, _, _ in inv.supported_builds) or "(no kSupportedBuilds entry)")
        + ". Regenerate only as part of porting to a new build; see tools/README.md.",
        "supported_builds": [
            {"time_date_stamp": hx8(a), "size_of_image": hx8(b), "entry_point": hx8(c)}
            for a, b, c in inv.supported_builds
        ],
        "highest_patched_rva": hx(inv.highest_patched_rva) if inv.highest_patched_rva is not None else None,
        "gate_margin": inv.gate_margin,
        "arxan_tables": {
            t.name: {
                "count": len(t.rvas),
                "min": hx(min(t.rvas)) if t.rvas else None,
                "max": hx(max(t.rvas)) if t.rvas else None,
                "patch": t.patch,
                "patch_len": t.patch_len,
                "sha256": _digest(sorted(t.rvas)),
            }
            for t in sorted(inv.tables.values(), key=lambda t: t.name)
        },
        "symbols": dict(sorted(symbols.items())),
        "sites": [
            {"file": f, "rva": v, "count": c}
            for (f, v), c in sorted(site_counts.items(), key=lambda kv: (kv[0][0], int(kv[0][1], 16)))
        ],
    }


def compare_baseline(current: dict, baseline: dict) -> list[str]:
    errors: list[str] = []
    if baseline.get("format") != BASELINE_FORMAT:
        return [f"baseline format {baseline.get('format')!r} is not {BASELINE_FORMAT}; regenerate it"]

    if current["supported_builds"] != baseline["supported_builds"]:
        errors.append(
            f"kSupportedBuilds changed: baseline {baseline['supported_builds']}, now {current['supported_builds']}"
        )
    for key in ("highest_patched_rva", "gate_margin"):
        if current[key] != baseline[key]:
            errors.append(f"{key} changed: baseline {baseline[key]}, now {current[key]}")

    old_t, new_t = baseline["arxan_tables"], current["arxan_tables"]
    for name in sorted(set(old_t) | set(new_t)):
        if name not in new_t:
            errors.append(f"Arxan table {name} is gone (baseline had {old_t[name]['count']} entries)")
        elif name not in old_t:
            errors.append(f"Arxan table {name} is new ({new_t[name]['count']} entries)")
        elif old_t[name]["count"] != new_t[name]["count"]:
            errors.append(f"Arxan table {name}: {old_t[name]['count']} entries in baseline, now {new_t[name]['count']}")
        elif old_t[name] != new_t[name]:
            changed = sorted(k for k in old_t[name] if old_t[name][k] != new_t[name].get(k))
            errors.append(f"Arxan table {name}: same count but {', '.join(changed)} changed")

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
    return errors


# --------------------------------------------------------------------------------------------
# Output


def summary(inv: Inventory) -> str:
    out = []
    per_file = Counter(s.file for s in inv.sites)
    out.append(f"REBASE/OFFSET literal sites: {len(inv.sites)} ({len({s.value for s in inv.sites})} distinct RVAs)")
    for f, c in sorted(per_file.items()):
        out.append(f"  {f}: {c}")
    out.append(f"named bindings: {len(inv.bindings)} across {len({_symbol_key(b.name) for b in inv.bindings})} symbols")
    out.append("Arxan CRC tables:")
    for t in sorted(inv.tables.values(), key=lambda t: t.name):
        rng = f"{hx(min(t.rvas))}..{hx(max(t.rvas))}" if t.rvas else "empty"
        out.append(f"  {t.name}: {len(t.rvas)} sites, {rng}, patch {t.patch} ({t.patch_len} bytes)")
    out.append(f"  total: {sum(len(t.rvas) for t in inv.tables.values())}")
    out.append("kSupportedBuilds:")
    for a, b, c in inv.supported_builds:
        out.append(f"  TimeDateStamp {hx8(a)}  SizeOfImage {hx8(b)}  EntryPoint {hx8(c)}")
    if inv.highest_patched_rva is not None:
        out.append(f"kHighestPatchedRva: {hx(inv.highest_patched_rva)} (gate margin +{inv.gate_margin})")
    return "\n".join(out)


def listing(inv: Inventory) -> str:
    out = []
    for s in sorted(inv.sites, key=lambda s: (s.file, s.line)):
        label = f"  {s.name}" if s.name else ""
        out.append(f"{s.file}:{s.line}: {s.macro}({s.expr}) = {hx(s.value)}{label}")
    for t in sorted(inv.tables.values(), key=lambda t: t.name):
        out.append(f"{t.file}:{t.line}: {t.name}[] {len(t.rvas)} entries")
    return "\n".join(out)


def _triples(doc: dict) -> list[tuple[str, str, str]]:
    return [(b.get("time_date_stamp"), b.get("size_of_image"), b.get("entry_point")) for b in doc.get("supported_builds", [])]


def resolve_write_target(inv: Inventory, explicit: Path | None) -> tuple[Path | None, str | None]:
    """Where --write-baseline may write, or why it may not.

    Each baseline belongs to one build, so the default target is named after the build being
    written, and a port leaves the previous build's reference intact instead of overwriting it
    with new offsets under the old ID. Anything ambiguous needs an explicit --baseline.
    """
    if explicit is not None:
        return explicit, None
    if len(inv.supported_builds) != 1:
        return None, (
            f"kSupportedBuilds has {len(inv.supported_builds)} entries, so there is no single build to "
            "name the baseline after; pass --baseline explicitly"
        )
    tds, soi, ep = inv.supported_builds[0]
    target = BASELINE_DIR / f"baseline_{tds:08X}.json"
    if target.exists():
        try:
            existing = _triples(json.loads(target.read_text(encoding="utf-8")))
        except (json.JSONDecodeError, AttributeError) as e:
            return None, f"{target} exists but is not a readable baseline ({e}); pass --baseline explicitly"
        if existing != [(hx8(tds), hx8(soi), hx8(ep))]:
            return None, (
                f"{target} records {existing}, not the current kSupportedBuilds triple; "
                "pass --baseline explicitly to overwrite it"
            )
    return target, None


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    mode = ap.add_mutually_exclusive_group()
    mode.add_argument("--check", action="store_true", help="run checks and compare to the baseline; exit 1 on any failure")
    mode.add_argument("--write-baseline", action="store_true", help="rewrite the baseline from the current source")
    mode.add_argument("--list", action="store_true", help="print every site with file:line")
    ap.add_argument("--root", type=Path, default=REPO_ROOT, help="repository root (default: this checkout)")
    ap.add_argument(
        "--baseline",
        type=Path,
        default=None,
        help=f"baseline JSON path (--check default: {DEFAULT_BASELINE.name}; "
        "--write-baseline default: baseline_<TimeDateStamp>.json for the single kSupportedBuilds entry)",
    )
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
        target, refusal = resolve_write_target(inv, args.baseline)
        if refusal:
            print(f"refusing to write a baseline: {refusal}", file=sys.stderr)
            return 1
        target.write_text(json.dumps(build_baseline(inv), indent=2) + "\n", encoding="utf-8")
        print(f"wrote {target}")
        return 0

    if args.baseline is None:
        args.baseline = DEFAULT_BASELINE

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
