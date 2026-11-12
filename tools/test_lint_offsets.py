"""Tests for lint_offsets.py against small synthetic patch/ trees, never the real one.

Run from the repo root:  python3 -B -I -m unittest discover -s tools -p 'test_*.py'
"""

import contextlib
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import lint_offsets as lint  # noqa: E402

# Shaped like upstream's patch/GameBuild.h: two builds, one shifted range, static_asserts.
GAMEBUILD_H = """\
namespace bo3
{
    enum class Build { Unknown, February2026, September2026 };
    inline constexpr std::uint32_t February2026TimeDateStamp = 0x693D731E;
    inline constexpr std::uint32_t February2026RetailImageSize = 0x1D74AC00;
    inline constexpr std::uint32_t February2026DumpImageSize = 0x1D74B000;
    inline constexpr std::uint32_t September2026TimeDateStamp = 0x6A7B6355;
    inline constexpr std::uint32_t September2026RetailImageSize = 0x1D75BC00;
    inline constexpr std::uint32_t September2026DumpImageSize = 0x1D75C000;
    inline constexpr std::uintptr_t SeptemberShiftStartRva = 0x1000;
    inline constexpr std::uintptr_t PrimaryCodeEndRva = 0x5000;
    inline constexpr std::uintptr_t SeptemberCodeDelta = 0x10;
    inline Build detect_build()
    {
        if (timestamp == September2026TimeDateStamp &&
            (imageSize == September2026RetailImageSize || imageSize == September2026DumpImageSize))
            return Build::September2026;
        if (timestamp == February2026TimeDateStamp &&
            (imageSize == February2026RetailImageSize || imageSize == February2026DumpImageSize))
            return Build::February2026;
        return Build::Unknown;
    }
    static_assert(get_live_rva(0x2000, Build::September2026) == 0x1FF0);
    static_assert(get_live_rva(0x9000, Build::September2026) == 0x9000);
    static_assert(get_live_rva(0x2000, Build::February2026) == 0x2000);
}
"""

ARXAN_CPP = """\
namespace
{
    constexpr std::uint8_t kPatchKey = 0x69;
    constexpr std::uintptr_t kReadySignalRva = 0x9948;
    constexpr std::size_t kCrc1ExpectedCount = 3;
    constexpr std::size_t kCrc2ExpectedCount = 2;
    constexpr std::array<std::uint8_t, 2> kEncryptedPatch1 = { 0x21, 0x58 };
    constexpr std::array<std::uint8_t, 3> kEncryptedPatch2 = { 0x21, 0x58, 0xA0 };
    constexpr std::array<std::uint8_t, 2> kChecksumPattern1 = { 0x8B, 0x0C };
    constexpr std::array<std::uint8_t, 3> kChecksumPattern2 = { 0x8B, 0x0C, 0x8B };
    const auto* signal = reinterpret_cast<const std::uintptr_t*>(bo3::image_base() + kReadySignalRva);
}
bool install(std::string& message)
{
    if (!collect_pattern_patches(kChecksumPattern1, kEncryptedPatch1, kCrc1ExpectedCount, message) ||
        !collect_pattern_patches(kChecksumPattern2, kEncryptedPatch2, kCrc2ExpectedCount, message))
        return false;
}
"""

OFFSETS_H = """\
#define OFFSET(x) ((INT64)bo3::address(static_cast<std::uintptr_t>(x)))
#define REBASE(x) OFFSET(x)
#define PTR_Foo REBASE(0x1100)
#define PTR_Bar REBASE(0x200)
#define Tokenize REBASE(0x20E2 + 0x3)
const static auto Foo = reinterpret_cast<bool(__fastcall*)(int)>OFFSET(0x1100);
const static auto Bar = reinterpret_cast<bool(__fastcall*)(int)>PTR_Bar;
"""

HOOKS_CPP = """\
void Install()
{
\tMH_CreateHook((LPVOID)REBASE(0x1100), functions::hkFoo, (LPVOID*)&Foo);
\tCREATE_HOOK((LPVOID)REBASE(0x200), functions::hkBar, (LPVOID*)&Bar);
\t/*MH_CreateHook((LPVOID)REBASE(0x999), functions::hkOld, (LPVOID*)&Foo);*/
\tint x = *(int*)REBASE(0x300); // REBASE(0x998)
\tconst char* s = "// not a comment REBASE(0x997)";
}
"""

LUA_CPP = """\
namespace
{
    constexpr std::uintptr_t HksGetInfoRva = 0x1D00;
}
void InstallLua()
{
    MH_CreateHook(reinterpret_cast<void*>(REBASE(HksGetInfoRva)),
        reinterpret_cast<void*>(&hk), reinterpret_cast<void**>(&orig));
}
"""


class Tree:
    """A throwaway repo root with a patch/ directory built from the strings above."""

    def __init__(self, **overrides):
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name)
        files = {
            "GameBuild.h": GAMEBUILD_H,
            "Arxan.cpp": ARXAN_CPP,
            "offsets.h": OFFSETS_H,
            "Hooks.cpp": HOOKS_CPP,
            "LuaSymbols.cpp": LUA_CPP,
            "minhook/src/x.h": "#define X REBASE(0xDEAD)\n",
            "detours/detours.h": "#define Y REBASE(0xBEEF)\n",
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

    def problems(self, t):
        return lint.run_checks(lint.scan(t.root))

    def assertProblem(self, t, fragment):
        problems = self.problems(t)
        self.assertTrue(any(fragment in p for p in problems), f"{fragment!r} not in {problems}")


class TestHelpers(unittest.TestCase):
    def test_eval_literal(self):
        self.assertEqual(lint.eval_literal("0x10"), 16)
        self.assertEqual(lint.eval_literal(" 0x1EEACF2 + 1 "), 0x1EEACF3)
        self.assertEqual(lint.eval_literal("0x10 - 0x1"), 15)
        self.assertEqual(lint.eval_literal("010"), 8)
        self.assertIsNone(lint.eval_literal("rva"))
        self.assertIsNone(lint.eval_literal("HksGetInfoRva"))

    def test_strip_comments_keeps_lines_and_blanks_strings(self):
        src = 'a // REBASE(1)\n/* REBASE(2)\n */ b "REBASE(3) // x" c\n'
        out = lint.strip_comments(src)
        self.assertEqual(out.count("\n"), src.count("\n"))
        self.assertNotIn("REBASE", out)
        self.assertIn("b", out)
        self.assertIn("c", out)

    def test_shift_model_matches_get_live_rva(self):
        s = lint.Shift("September", 0x1000, 0x5000, 0x10)
        self.assertEqual(s.live_rva(0x1000, "September2026"), 0xFF0)
        self.assertEqual(s.live_rva(0x4FFF, "September2026"), 0x4FEF)
        self.assertEqual(s.live_rva(0x5000, "September2026"), 0x5000)  # end is exclusive
        self.assertEqual(s.live_rva(0xFFF, "September2026"), 0xFFF)
        self.assertEqual(s.live_rva(0x2000, "February2026"), 0x2000)


class TestInventory(LintTestCase):
    def test_sites_builds_shift_and_arxan(self):
        inv = lint.scan(self.tree().root)
        values = sorted(s.value for s in inv.sites)
        # Comments, strings and vendored minhook/detours are excluded; HksGetInfoRva is resolved.
        self.assertEqual(values, [0x200, 0x200, 0x300, 0x1100, 0x1100, 0x1100, 0x1D00, 0x20E5])
        self.assertEqual(sorted(inv.builds), ["February2026", "September2026"])
        self.assertEqual(inv.builds["September2026"].retail_size, 0x1D75BC00)
        self.assertEqual((inv.shift.start, inv.shift.end, inv.shift.delta), (0x1000, 0x5000, 0x10))
        self.assertEqual(len(inv.shift_asserts), 3)
        self.assertEqual([c.pattern for c in inv.arxan_calls], ["kChecksumPattern1", "kChecksumPattern2"])
        self.assertEqual(inv.arxan_counts, {"kCrc1ExpectedCount": 3, "kCrc2ExpectedCount": 2})
        self.assertEqual(lint.decrypted(inv, "kEncryptedPatch2"), [0x48, 0x31, 0xC9])
        self.assertEqual([(u.expr, u.value) for u in inv.raw_uses], [("kReadySignalRva", 0x9948)])
        self.assertEqual(inv.unresolved, [])

    def test_classification(self):
        inv = lint.scan(self.tree().root)
        self.assertEqual(lint.classify(inv, 0x1100), "shifted")
        self.assertEqual(lint.classify(inv, 0x200), "unshifted")
        base = lint.build_baseline(inv)
        self.assertEqual(
            base["classification"],
            {"sites_shifted": 5, "sites_unshifted": 3, "distinct_rvas_shifted": 3, "distinct_rvas_unshifted": 2},
        )

    def test_names_label_sites(self):
        inv = lint.scan(self.tree().root)
        names = {s.name for s in inv.sites if s.name}
        # The hook's original pointer wins over the *Rva constant for a REBASE(HksGetInfoRva) site.
        self.assertTrue({"PTR_Foo", "Foo", "Bar", "orig"} <= names, names)

    def test_alias_pointer_resolves(self):
        inv = lint.scan(self.tree().root)
        bar = [b for b in inv.bindings if b.name == "Bar" and b.kind.startswith("pointer")]
        self.assertEqual([b.value for b in bar], [0x200])

    def test_non_constant_argument_is_reported_not_counted(self):
        t = self.tree(**{"Extra.cpp": "void f(uintptr_t rva) { auto p = REBASE(rva); }\n"})
        inv = lint.scan(t.root)
        self.assertEqual([(f, e) for f, _, e in inv.unresolved], [("patch/Extra.cpp", "rva")])

    def test_clean_tree_passes(self):
        t = self.tree()
        self.assertEqual(self.problems(t), [])
        code, out, _ = t.run()
        self.assertEqual(code, 0, out)
        self.assertIn("total expected sites: 5", out)


class TestChecks(LintTestCase):
    def test_disagreeing_hook_target(self):
        t = self.tree()
        t.edit("Hooks.cpp", "REBASE(0x1100), functions::hkFoo", "REBASE(0x1104), functions::hkFoo")
        self.assertProblem(t, "symbol 'Foo' has disagreeing RVAs")

    def test_disagreeing_ptr_and_pointer(self):
        t = self.tree()
        t.edit("offsets.h", "#define PTR_Foo REBASE(0x1100)", "#define PTR_Foo REBASE(0x1108)")
        self.assertProblem(t, "symbol 'Foo' has disagreeing RVAs")

    def test_duplicate_rva_constant(self):
        t = self.tree(**{"Other.cpp": "constexpr std::uintptr_t HksGetInfoRva = 0x1D00;\n"})
        self.assertProblem(t, "rva-constant 'HksGetInfoRva' is defined 2 times")

    def test_raw_use_in_shift_range(self):
        t = self.tree()
        t.edit("Arxan.cpp", "kReadySignalRva = 0x9948", "kReadySignalRva = 0x2948")
        self.assertProblem(t, "bypasses bo3::address()")

    def test_raw_use_outside_shift_range_is_fine(self):
        self.assertEqual(self.problems(self.tree()), [])

    def test_dump_size_must_be_retail_rounded_to_a_page(self):
        t = self.tree()
        t.edit("GameBuild.h", "September2026DumpImageSize = 0x1D75C000", "September2026DumpImageSize = 0x1D75D000")
        self.assertProblem(t, "not RetailImageSize")

    def test_fingerprint_never_compared(self):
        t = self.tree()
        t.edit("GameBuild.h", " || imageSize == September2026DumpImageSize", "")
        self.assertProblem(t, "September2026DumpImageSize is defined but never compared against")

    def test_static_assert_disagrees_with_shift(self):
        t = self.tree()
        t.edit("GameBuild.h", "SeptemberCodeDelta = 0x10", "SeptemberCodeDelta = 0x20")
        self.assertProblem(t, "static_assert says get_live_rva(0x2000, September2026)")

    def test_two_shift_ranges_are_a_parse_error(self):
        t = self.tree()
        t.edit("GameBuild.h", "SeptemberCodeDelta = 0x10;", "SeptemberCodeDelta = 0x10;\ninline constexpr std::uintptr_t OctoberCodeDelta = 0x8;")
        self.assertProblem(t, "expected exactly one")

    def test_arxan_declared_size_mismatch(self):
        t = self.tree()
        t.edit("Arxan.cpp", "std::array<std::uint8_t, 3> kChecksumPattern2", "std::array<std::uint8_t, 4> kChecksumPattern2")
        self.assertProblem(t, "kChecksumPattern2 is declared with 4 bytes but lists 3")

    def test_arxan_uncollected_pattern(self):
        t = self.tree()
        t.edit("Arxan.cpp", "kChecksumPattern2 = { 0x8B, 0x0C, 0x8B };", "kChecksumPattern2 = { 0x8B, 0x0C, 0x8B };\n    constexpr std::array<std::uint8_t, 1> kChecksumPattern9 = { 0x90 };")
        self.assertProblem(t, "kChecksumPattern9 is defined but never passed")

    def test_arxan_patch_longer_than_pattern(self):
        t = self.tree()
        t.edit("Arxan.cpp", "kChecksumPattern1, kEncryptedPatch1", "kChecksumPattern1, kEncryptedPatch2")
        self.assertProblem(t, "kEncryptedPatch2 is longer than kChecksumPattern1")

    def test_arxan_patch2_prefix_invariant(self):
        t = self.tree()
        t.edit("Arxan.cpp", "kEncryptedPatch2 = { 0x21, 0x58, 0xA0 }", "kEncryptedPatch2 = { 0x21, 0x59, 0xA0 }")
        self.assertProblem(t, "no longer starts with decrypted kEncryptedPatch1")

    def test_missing_files_are_errors(self):
        t = self.tree(**{"GameBuild.h": None, "Arxan.cpp": None})
        problems = self.problems(t)
        self.assertTrue(any("patch/GameBuild.h: missing" in p for p in problems))
        self.assertTrue(any("patch/Arxan.cpp: missing" in p for p in problems))

    def test_no_patch_dir(self):
        with tempfile.TemporaryDirectory() as d:
            inv = lint.scan(Path(d))
            self.assertTrue(any("not a directory" in p for p in inv.parse_errors))


class TestBaseline(LintTestCase):
    def written(self):
        t = self.tree()
        code, _, err = t.run("--write-baseline")
        self.assertEqual(code, 0, err)
        return t

    def test_round_trip(self):
        t = self.written()
        code, out, err = t.run("--check")
        self.assertEqual(code, 0, err)
        self.assertIn("ok:", out)
        doc = json.loads(t.baseline.read_text())
        self.assertEqual(doc["format"], lint.BASELINE_FORMAT)
        self.assertEqual([b["name"] for b in doc["builds"]], ["February2026", "September2026"])

    def test_comment_only_edit_is_not_drift(self):
        t = self.written()
        t.edit("Hooks.cpp", "void Install()", "// a new comment\n// another\nvoid Install()")
        self.assertEqual(t.run("--check")[0], 0)

    def test_expected_count_change_is_drift(self):
        t = self.written()
        t.edit("Arxan.cpp", "kCrc2ExpectedCount = 2", "kCrc2ExpectedCount = 4")
        code, _, err = t.run("--check")
        self.assertEqual(code, 1)
        self.assertIn("expected count 2 in baseline, now 4", err)

    def test_pattern_bytes_change_is_drift(self):
        t = self.written()
        t.edit("Arxan.cpp", "kChecksumPattern1 = { 0x8B, 0x0C }", "kChecksumPattern1 = { 0x8B, 0x0D }")
        code, _, err = t.run("--check")
        self.assertEqual(code, 1)
        self.assertIn("kChecksumPattern1: pattern_bytes changed", err)

    def test_build_fingerprint_change_is_drift(self):
        t = self.written()
        t.edit("GameBuild.h", "September2026TimeDateStamp = 0x6A7B6355", "September2026TimeDateStamp = 0x6A7B6356")
        code, _, err = t.run("--check")
        self.assertEqual(code, 1)
        self.assertIn("build fingerprints changed", err)

    def test_shift_change_is_drift(self):
        t = self.written()
        t.edit("GameBuild.h", "PrimaryCodeEndRva = 0x5000", "PrimaryCodeEndRva = 0x6000")
        code, _, err = t.run("--check")
        self.assertEqual(code, 1)
        self.assertIn("shift range changed", err)

    def test_moved_literal_is_drift(self):
        t = self.written()
        t.edit("Hooks.cpp", "*(int*)REBASE(0x300)", "*(int*)REBASE(0x304)")
        code, _, err = t.run("--check")
        self.assertEqual(code, 1)
        self.assertIn("patch/Hooks.cpp: 0x304 used 1 time(s), baseline 0", err)

    def test_new_raw_use_is_drift(self):
        t = self.written()
        t.edit("Hooks.cpp", "int x =", "auto y = bo3::image_base() + 0x9000;\n\tint x =")
        code, _, err = t.run("--check")
        self.assertEqual(code, 1)
        self.assertIn("new raw image_base() + 0x9000", err)

    def test_failing_check_also_fails_check_mode(self):
        t = self.written()
        t.edit("offsets.h", "#define PTR_Foo REBASE(0x1100)", "#define PTR_Foo REBASE(0x1108)")
        code, _, err = t.run("--check")
        self.assertEqual(code, 1)
        self.assertIn("disagreeing RVAs", err)

    def test_missing_baseline(self):
        t = self.tree()
        code, _, err = t.run("--check")
        self.assertEqual(code, 1)
        self.assertIn("not found", err)

    def test_old_format_baseline_is_rejected(self):
        t = self.tree()
        t.baseline.write_text(json.dumps({"format": 1}))
        code, _, err = t.run("--check")
        self.assertEqual(code, 1)
        self.assertIn("is not 2; regenerate it", err)

    def test_refuses_to_bless_a_failing_tree(self):
        t = self.tree()
        t.edit("offsets.h", "#define PTR_Foo REBASE(0x1100)", "#define PTR_Foo REBASE(0x1108)")
        code, _, err = t.run("--write-baseline")
        self.assertEqual(code, 1)
        self.assertIn("refusing", err)
        self.assertFalse(t.baseline.exists())

    def test_default_baseline_is_the_committed_one(self):
        self.assertEqual(lint.DEFAULT_BASELINE.name, "baseline_feb2026_rvas.json")
        self.assertTrue(lint.DEFAULT_BASELINE.is_file())


if __name__ == "__main__":
    unittest.main()
