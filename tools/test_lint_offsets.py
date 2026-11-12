"""Tests for lint_offsets.py against small synthetic patch/ trees, never the real one.

Run from the repo root:  python3 -B -I -m unittest discover -s tools -p 'test_*.py'
"""

import contextlib
import io
import json
import sys
import tempfile
import textwrap
import unittest
from unittest import mock
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import lint_offsets as lint  # noqa: E402

ARXAN_H = """\
#pragma once
static uintptr_t crc_patch_1[] = {
0x1000,
0x2000,
};
static uintptr_t crc_patch_2[] = {
0x3000,
};
static uintptr_t crc_patch_3[] = {
0x4000,
0x5000,
0x6000,
};
"""

ARXAN_CPP = """\
static const SupportedBuild kSupportedBuilds[] =
{
\t{ 0x693D731E, 0x1D74B000, 0x02BD40FC }, // retail
};
static const DWORD kHighestPatchedRva = 0x6000;
static bool VerifyGameBuild()
{
\tif (sizeOfImage <= kHighestPatchedRva + 16) return false;
}
static constexpr ObfuscatedPatch patch_crc_1({ 0x48, 0x31, 0xC9, 0x90, 0x90, 0x90 });
static constexpr ObfuscatedPatch patch_crc_2({ 0x48, 0x31, 0xC9, 0x90, 0x90, 0x90, 0x90, 0x90 });
static constexpr ObfuscatedPatch patch_crc_3({ 0x48, 0x31, 0xC0, 0x48, 0x31, 0xD2 });
bool PatchChecksumComparisons_Precomputed()
{
\tfor (auto rva : crc_patch_1)
\t\tpatch_crc_1.Apply((uint8_t*)(REBASE(rva)), PatchAddress);
\tfor (auto rva : crc_patch_2)
\t\tpatch_crc_2.Apply((uint8_t*)(REBASE(rva)), PatchAddress);
\tfor (auto rva : crc_patch_3)
\t\tpatch_crc_3.Apply((uint8_t*)(REBASE(rva)), PatchAddress);
}
"""

OFFSETS_H = """\
#define OFFSET(x) ((INT64)GetModuleHandleA(NULL) + (INT64)x)
#define REBASE(x) OFFSET(x)
#define PTR_Foo REBASE(0x100)
#define PTR_Bar REBASE(0x200)
#define Tokenize REBASE(0x20E2A3E + 0x3)
#define Sys_Checksum(msg, size) ((unsigned __int16(__fastcall*)(const unsigned char*, __int32))REBASE(0x211F430))(msg, size)
const static auto Foo = reinterpret_cast<bool(__fastcall*)(int)>OFFSET(0x100);
const static auto Bar = reinterpret_cast<bool(__fastcall*)(int)>PTR_Bar;
"""

HOOKS_CPP = """\
void Install()
{
\tCREATE_HOOK((LPVOID)REBASE(0x100), functions::hkFoo, (LPVOID*)&Foo);
\tCREATE_HOOK((LPVOID)REBASE(0x200), functions::hkBar, (LPVOID*)&Bar);
\t/*CREATE_HOOK((LPVOID)REBASE(0x999), functions::hkOld, (LPVOID*)&Foo);*/
\tint x = *(int*)REBASE(0x300); // REBASE(0x998)
\tconst char* s = "// not a comment REBASE(0x997)";
}
"""


class Tree:
    """A throwaway repo root with a patch/ directory built from the strings above."""

    def __init__(self, **overrides):
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name)
        files = {
            "Arxan.h": ARXAN_H,
            "Arxan.cpp": ARXAN_CPP,
            "offsets.h": OFFSETS_H,
            "Hooks.cpp": HOOKS_CPP,
            "minhook/hook.c": "",
            "minhook/src/x.h": "#define X REBASE(0xDEAD)\n",
        }
        files.update(overrides)
        for rel, body in files.items():
            if body is None:
                continue
            path = self.root / "patch" / rel
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(body, encoding="utf-8")
        self.baseline = self.root / "baseline.json"

    def run(self, *args):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = lint.main(["--root", str(self.root), "--baseline", str(self.baseline), *args])
        return code, out.getvalue(), err.getvalue()

    def run_default(self, *args):
        """Like run(), but without --baseline, so the tool picks the path itself."""
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = lint.main(["--root", str(self.root), *args])
        return code, out.getvalue(), err.getvalue()

    def edit(self, rel, old, new):
        path = self.root / "patch" / rel
        text = path.read_text(encoding="utf-8")
        assert old in text, old
        path.write_text(text.replace(old, new), encoding="utf-8")

    def close(self):
        self._tmp.cleanup()


class LintTestCase(unittest.TestCase):
    def tree(self, **overrides):
        t = Tree(**overrides)
        self.addCleanup(t.close)
        return t


class TestHelpers(unittest.TestCase):
    def test_eval_literal(self):
        self.assertEqual(lint.eval_literal("0x20E2A3E + 0x3"), 0x20E2A41)
        self.assertEqual(lint.eval_literal(" 0x10 - 2 "), 14)
        self.assertEqual(lint.eval_literal("010"), 8)
        self.assertEqual(lint.eval_literal("0"), 0)
        self.assertIsNone(lint.eval_literal("rva"))
        self.assertIsNone(lint.eval_literal("0x10 * 2"))

    def test_strip_comments_keeps_lines_and_blanks_strings(self):
        # The "//" inside the string must not swallow the rest of the line ("c" survives).
        src = 'a // x\n/* y\nz */ b "//q\\"" c \'/\'\n'
        out = lint.strip_comments(src)
        self.assertEqual(len(out), len(src))
        self.assertEqual(out.count("\n"), src.count("\n"))
        for gone in ("x", "y", "z", "q"):
            self.assertNotIn(gone, out)
        self.assertIn(" b ", out)
        self.assertIn(" c ", out)


class TestInventory(LintTestCase):
    def test_sites_tables_and_builds(self):
        inv = lint.scan(self.tree().root)
        self.assertEqual(inv.parse_errors, [])
        values = sorted(s.value for s in inv.sites)
        # Comments, the string literal, minhook and the non-literal REBASE(rva) are all excluded.
        self.assertEqual(values, sorted([0x100, 0x200, 0x20E2A41, 0x211F430, 0x100, 0x100, 0x200, 0x300]))
        self.assertEqual({k: len(t.rvas) for k, t in inv.tables.items()},
                         {"crc_patch_1": 2, "crc_patch_2": 1, "crc_patch_3": 3})
        self.assertEqual({k: t.patch_len for k, t in inv.tables.items()},
                         {"crc_patch_1": 6, "crc_patch_2": 8, "crc_patch_3": 6})
        self.assertEqual(inv.supported_builds, [(0x693D731E, 0x1D74B000, 0x02BD40FC)])
        self.assertEqual(inv.highest_patched_rva, 0x6000)
        self.assertEqual(inv.gate_margin, 16)

    def test_line_numbers_and_names(self):
        inv = lint.scan(self.tree().root)
        hook = [s for s in inv.sites if s.file == "patch/Hooks.cpp" and s.value == 0x200]
        self.assertEqual([(s.line, s.name) for s in hook], [(4, "Bar")])
        tok = [s for s in inv.sites if s.value == 0x20E2A41]
        self.assertEqual((tok[0].line, tok[0].name), (5, "Tokenize"))

    def test_alias_pointer_resolves(self):
        inv = lint.scan(self.tree().root)
        kinds = {(b.name, b.kind) for b in inv.bindings if b.name == "Bar"}
        self.assertIn(("Bar", "pointer (via PTR_Bar)"), kinds)

    def test_clean_tree_passes(self):
        t = self.tree()
        code, out, _ = t.run()
        self.assertEqual(code, 0, out)
        self.assertIn("crc_patch_3: 3 sites", out)
        self.assertIn("TimeDateStamp 0x693D731E", out)


class TestChecks(LintTestCase):
    def failures(self, tree):
        return lint.run_checks(lint.scan(tree.root))

    def test_stale_highest_rva(self):
        t = self.tree()
        t.edit("Arxan.h", "0x6000,", "0x7000,")
        self.assertTrue(any("kHighestPatchedRva is 0x6000" in f and "0x7000" in f for f in self.failures(t)))

    def test_highest_rva_not_the_true_max(self):
        t = self.tree()
        t.edit("Arxan.cpp", "kHighestPatchedRva = 0x6000", "kHighestPatchedRva = 0x5000")
        self.assertTrue(any("kHighestPatchedRva is 0x5000" in f for f in self.failures(t)))

    def test_margin_smaller_than_patch(self):
        t = self.tree()
        t.edit("Arxan.cpp", "kHighestPatchedRva + 16", "kHighestPatchedRva + 4")
        self.assertTrue(any("gate margin" in f for f in self.failures(t)))

    def test_duplicate_in_table(self):
        t = self.tree()
        t.edit("Arxan.h", "0x2000,", "0x2000,\n0x1000,")
        self.assertTrue(any("crc_patch_1: RVA 0x1000 appears 2 times" in f for f in self.failures(t)))

    def test_overlap_across_tables(self):
        t = self.tree()
        t.edit("Arxan.h", "0x3000,", "0x1004,")
        self.assertTrue(any("overlapping" in f for f in self.failures(t)))

    def test_disagreeing_hook_target(self):
        t = self.tree()
        t.edit("Hooks.cpp", "REBASE(0x100), functions::hkFoo", "REBASE(0x104), functions::hkFoo")
        found = [f for f in self.failures(t) if "symbol 'Foo'" in f]
        self.assertEqual(len(found), 1)
        self.assertIn("0x104", found[0])
        self.assertIn("patch/Hooks.cpp:3", found[0])

    def test_disagreeing_ptr_and_pointer(self):
        t = self.tree()
        t.edit("offsets.h", "#define PTR_Foo REBASE(0x100)", "#define PTR_Foo REBASE(0x108)")
        self.assertTrue(any("symbol 'Foo'" in f for f in self.failures(t)))

    def test_alias_follows_its_target(self):
        # Bar is "reinterpret_cast<...>PTR_Bar", so porting PTR_Bar alone must still flag the hook.
        t = self.tree()
        t.edit("offsets.h", "#define PTR_Bar REBASE(0x200)", "#define PTR_Bar REBASE(0x208)")
        found = [f for f in self.failures(t) if "symbol 'Bar'" in f]
        self.assertEqual(len(found), 1)
        self.assertIn("hook-target", found[0])

    def test_same_literal_two_names_is_fine(self):
        t = self.tree(**{"offsets.h": OFFSETS_H + "#define Alias1 REBASE(0x500)\n#define Alias2 REBASE(0x500)\n"})
        self.assertEqual(self.failures(t), [])

    def test_missing_arxan_pieces_are_errors(self):
        t = self.tree(**{"Arxan.cpp": "// nothing here\n"})
        fails = self.failures(t)
        for needle in ("kSupportedBuilds", "kHighestPatchedRva", "gate margin", "Apply' loop"):
            self.assertTrue(any(needle in f for f in fails), needle)

    def test_no_patch_dir(self):
        with tempfile.TemporaryDirectory() as d:
            inv = lint.scan(Path(d))
        self.assertTrue(inv.parse_errors)


class TestBaseline(LintTestCase):
    def test_round_trip(self):
        t = self.tree()
        self.assertEqual(t.run("--write-baseline")[0], 0)
        data = json.loads(t.baseline.read_text())
        self.assertEqual(data["arxan_tables"]["crc_patch_3"]["count"], 3)
        self.assertEqual(data["supported_builds"][0]["entry_point"], "0x02BD40FC")
        code, out, err = t.run("--check")
        self.assertEqual(code, 0, err)
        self.assertIn("ok", out)

    def test_comment_only_edit_is_not_drift(self):
        t = self.tree()
        t.run("--write-baseline")
        t.edit("Hooks.cpp", "void Install()\n", "// a new comment\n\n// pushing lines down\nvoid Install()\n")
        self.assertEqual(t.run("--check")[0], 0)

    def test_count_change_is_drift(self):
        t = self.tree()
        t.run("--write-baseline")
        t.edit("Arxan.h", "0x2000,", "0x2000,\n0x2800,")
        code, _, err = t.run("--check")
        self.assertEqual(code, 1)
        self.assertIn("crc_patch_1: 2 entries in baseline, now 3", err)

    def test_same_count_different_rva_is_drift(self):
        t = self.tree()
        t.run("--write-baseline")
        t.edit("Arxan.h", "0x2000,", "0x2800,")
        code, _, err = t.run("--check")
        self.assertEqual(code, 1)
        self.assertIn("crc_patch_1: same count but", err)

    def test_supported_builds_change_is_drift(self):
        t = self.tree()
        t.run("--write-baseline")
        t.edit("Arxan.cpp", "0x02BD40FC }, // retail", "0x02BD40FC },\n\t{ 0x11111111, 0x22222000, 0x00001000 },")
        code, _, err = t.run("--check")
        self.assertEqual(code, 1)
        self.assertIn("kSupportedBuilds changed", err)

    def test_moved_literal_is_drift(self):
        t = self.tree()
        t.run("--write-baseline")
        t.edit("Hooks.cpp", "REBASE(0x300)", "REBASE(0x304)")
        code, _, err = t.run("--check")
        self.assertEqual(code, 1)
        self.assertIn("patch/Hooks.cpp: 0x300 used 0 time(s), baseline 1", err)
        self.assertIn("patch/Hooks.cpp: 0x304 used 1 time(s), baseline 0", err)

    def test_failing_check_also_fails_check_mode(self):
        t = self.tree()
        t.run("--write-baseline")
        t.edit("Arxan.cpp", "kHighestPatchedRva = 0x6000", "kHighestPatchedRva = 0x5000")
        code, _, err = t.run("--check")
        self.assertEqual(code, 1)
        self.assertIn("kHighestPatchedRva is 0x5000", err)

    def test_missing_baseline(self):
        code, _, err = self.tree().run("--check")
        self.assertEqual(code, 1)
        self.assertIn("not found", err)

    def test_default_write_target_is_named_after_the_build(self):
        t = self.tree()
        with mock.patch.object(lint, "BASELINE_DIR", t.root):
            code, out, _ = t.run_default("--write-baseline")
        self.assertEqual(code, 0, out)
        written = t.root / "baseline_693D731E.json"
        self.assertIn(str(written), out)
        self.assertIn("0x693D731E", json.loads(written.read_text(encoding="utf-8"))["comment"])

    def test_default_write_does_not_overwrite_another_builds_baseline(self):
        t = self.tree()
        t.edit("Arxan.cpp", "0x693D731E, 0x1D74B000, 0x02BD40FC", "0x11111111, 0x22222000, 0x00001000")
        old = t.root / "baseline_693D731E.json"
        old.write_text("{}\n", encoding="utf-8")
        with mock.patch.object(lint, "BASELINE_DIR", t.root):
            code, out, _ = t.run_default("--write-baseline")
        self.assertEqual(code, 0)
        self.assertEqual(old.read_text(encoding="utf-8"), "{}\n")
        self.assertTrue((t.root / "baseline_11111111.json").exists())

    def test_default_write_refuses_a_mismatched_triple(self):
        t = self.tree()
        with mock.patch.object(lint, "BASELINE_DIR", t.root):
            t.run_default("--write-baseline")
            t.edit("Arxan.cpp", "0x1D74B000, 0x02BD40FC", "0x1D74C000, 0x02BD40FC")
            before = (t.root / "baseline_693D731E.json").read_text(encoding="utf-8")
            code, _, err = t.run_default("--write-baseline")
        self.assertEqual(code, 1)
        self.assertIn("pass --baseline explicitly", err)
        self.assertEqual((t.root / "baseline_693D731E.json").read_text(encoding="utf-8"), before)

    def test_default_write_refuses_multiple_builds(self):
        t = self.tree()
        t.edit("Arxan.cpp", "0x02BD40FC }, // retail", "0x02BD40FC },\n\t{ 0x11111111, 0x22222000, 0x00001000 },")
        with mock.patch.object(lint, "BASELINE_DIR", t.root):
            code, _, err = t.run_default("--write-baseline")
        self.assertEqual(code, 1)
        self.assertIn("2 entries", err)
        self.assertEqual(list(t.root.glob("baseline_*.json")), [])
        # An explicit path is the escape hatch.
        self.assertEqual(t.run("--write-baseline")[0], 0)

    def test_refuses_to_bless_a_failing_tree(self):
        t = self.tree()
        t.edit("Arxan.h", "0x2000,", "0x2000,\n0x1000,")
        code, _, err = t.run("--write-baseline")
        self.assertEqual(code, 1)
        self.assertFalse(t.baseline.exists())
        self.assertIn("refusing", err)


if __name__ == "__main__":
    unittest.main()
