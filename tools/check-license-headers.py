#!/usr/bin/env python3
# XFOIL.next
#
# Copyright (C) 2026 Olaf Frommann
# https://github.com/of33/XFOIL.next
#
# XFOIL.next is based on XFOIL 6.99 by Mark Drela and
# Harold Youngren.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.
#
# SPDX-License-Identifier: GPL-3.0-or-later

"""License-header gate.

Non-writing check of the hand-written project sources for the
binding GPL-3.0-or-later project header.

Categories (explicit, reproducible path rules):
  A  files derived directly from XFOIL 6.99: project header additionally
     with the derived sentence
  B  project-side infrastructure, tests, tools: project header without
     derived sentence
  C  original Fortran sources under fortran-src/: never a header check or
     injection; only non-injection and the unchanged
     original notice presence are checked
  D  data, goldens, references, samples, generated files, binaries:
     explicitly excluded

Rules:
  * header exactly at the top of the file (C/C++, make, fixed-form Fortran) or
    after line 1 (shebang tools); the shebang stays on line 1
  * exactly one SPDX identifier per file, correct GPL-3.0-or-later
  * derived sentence exactly once in category A, forbidden in category B
   * fixed-form headers: 'C' in column 1, line length <= 72

Usage:
  tools/check-license-headers.py [--root DIR] [--self-test] [--list]

Exit: 0 = everything in order, 1 = violation, 2 = invocation/setup error.
The tool needs no Git history and changes no file.
"""

import argparse
import io
import pathlib
import re
import sys
import tempfile

SPDX_LINE = "SPDX-License-Identifier: GPL-3.0-or-later"
SPDX_DECLARATION = re.compile(r"^\s*(?://|#|C|\*)\s*SPDX-License-Identifier:")

DERIVED_CPP = ("This file contains a C++ implementation "
               "derived from XFOIL 6.99.")
DERIVED_FORTRAN = ("This file contains Fortran code "
                   "derived from XFOIL 6.99.")

ATTRIBUTION = [
    "{p} XFOIL.next",
    "{p}",
    "{p} Copyright (C) 2026 Olaf Frommann",
    "{p} https://github.com/of33/XFOIL.next",
    "{p}",
    "{p} XFOIL.next is based on XFOIL 6.99 by Mark Drela and",
    "{p} Harold Youngren.",
]
GPL_BODY = [
    "{p} This program is free software: you can redistribute it and/or modify",
    "{p} it under the terms of the GNU General Public License as published by",
    "{p} the Free Software Foundation, either version 3 of the License, or",
    "{p} (at your option) any later version.",
    "{p}",
    "{p} This program is distributed in the hope that it will be useful,",
    "{p} but WITHOUT ANY WARRANTY; without even the implied warranty of",
    "{p} MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the",
    "{p} GNU General Public License for more details.",
    "{p}",
    "{p} You should have received a copy of the GNU General Public License",
    "{p} along with this program. If not, see <https://www.gnu.org/licenses/>.",
    "{p}",
    "{p} SPDX-License-Identifier: GPL-3.0-or-later",
]


def expected_block(prefix, derived):
    """Exact header text including the trailing blank line."""
    lines = [line.format(p=prefix) for line in ATTRIBUTION]
    if derived is not None:
        lines.append("{p}".format(p=prefix))
        lines.append("{p} {d}".format(p=prefix, d=derived))
    lines.append("{p}".format(p=prefix))
    lines.extend(line.format(p=prefix) for line in GPL_BODY)
    return "\n".join(lines) + "\n\n"


# ---------------------------------------------------------------------------
# category and exclusion rules (explicit, file-exact)
# ---------------------------------------------------------------------------

CATEGORY_A_DIR_PREFIXES = (
    "src/core/",
    "include/xfoil/core/",
    "include/xfoil/math/",
    "include/xfoil/geometry/",
    "include/xfoil/inviscid/",
    "include/xfoil/boundary_layer/",
    "include/xfoil/inverse/",
)

CATEGORY_A_EXPLICIT = frozenset({
    "src/io/AddedCamberCodec.cpp",
    "src/io/DefaultsCodec.cpp",
    "src/io/DumpCodec.cpp",
    "src/io/ExportFormats.cpp",
    "src/io/MdesInverseFileCodec.cpp",
    "src/io/MsesMultielementCodec.cpp",
    "src/io/OrrSommerfeldCodec.cpp",
    "src/io/PolarText.cpp",
    "src/io/TextFormat.cpp",
    "src/io/ThicknessCamberCodec.cpp",
    "include/xfoil/io/AddedCamberCodec.h",
    "include/xfoil/io/DefaultsCodec.h",
    "include/xfoil/io/DumpCodec.h",
    "include/xfoil/io/ExportFormats.h",
    "include/xfoil/io/MdesInverseFileCodec.h",
    "include/xfoil/io/MsesMultielementCodec.h",
    "include/xfoil/io/OrrSommerfeldCodec.h",
    "include/xfoil/io/PolarText.h",
    "include/xfoil/io/TextFormat.h",
    "include/xfoil/io/ThicknessCamberCodec.h",
    "src/app/DatFileReader.cpp",
    "src/app/DatFileReader.h",
    "src/app/AirfoilWriter.cpp",
    "src/app/AirfoilWriter.h",
    "src/session/SessionOps.cpp",
    "src/app/cli/CliFortranText.h",
    "src/app/cli/CommandInterpreter.cpp",
    "src/app/cli/CommandInterpreter.h",
    "src/app/cli/CoreGeometryHandlers.cpp",
    "src/app/cli/CoreGeometryHandlers.h",
    "src/app/cli/CoreInverseDesignHandlers.cpp",
    "src/app/cli/CoreInverseDesignHandlers.h",
    "src/app/cli/CoreOperHandlers.cpp",
    "src/app/cli/CoreOperHandlers.h",
    "src/app/cli/CoreVparHandlers.cpp",
    "src/app/cli/CoreVparHandlers.h",
})

# harness Fortran with substantially adopted XFOIL routines
# (inline VISCAL-/CAMB-/GDES-/GOFINI sections).
HARNESS_DERIVED = frozenset({
    "tests/fortran-reference/harnesses/c11a/c11a_alfa_diag_harness.f",
    "tests/fortran-reference/harnesses/c11a/c11a_batch_stall_harness.f",
    "tests/fortran-reference/harnesses/c11a/c11a_cseq_harness.f",
    "tests/fortran-reference/harnesses/c11a/c11a_cseq_nseqex_harness.f",
    "tests/fortran-reference/harnesses/c11a/c11a_init_harness.f",
    "tests/fortran-reference/harnesses/c11a/c11a_lrecalc_harness.f",
    "tests/fortran-reference/harnesses/c11a/c11a_oper_singlepoint_harness.f",
    "tests/fortran-reference/harnesses/geometry-design/camb_session_harness.f",
    "tests/fortran-reference/harnesses/geometry-design/gdes_ops_harness.f",
    "tests/fortran-reference/harnesses/geometry-design/gofini_harness.f",
})

# Excluded from the header check (category D and original Fortran sources, category C).
EXCLUDED_DIR_PREFIXES = (
    "build/",
    "fortran-src/",
    "fortran-bin/",
    "tests/golden/",
    "tests/data/",
    "tests/fortran-reference/cases/",
    "sample/",
    "docs/",
    ".agents/",
    ".git/",
    ".git_custom/",
)

EXCLUDED_SUFFIXES = (
    ".pol", ".dump", ".cp", ".bl", ".dat", ".o", ".a", ".so", ".png",
    ".jpg", ".gz", ".tgz", ".pdf",
)

EXCLUDED_NAMES = frozenset({
    "SHA256SUMS",
    "manifest.tsv",
    "LICENSE",
    "NOTICE",
})

EXPECTED_ORIGINAL_NOTICE_FILES = 23
FULL_TREE_MIN_FORTRAN_FILES = 30


def is_excluded(rel):
    if any(rel.startswith(prefix) for prefix in EXCLUDED_DIR_PREFIXES):
        return True
    if rel.endswith(EXCLUDED_SUFFIXES):
        return True
    if pathlib.PurePosixPath(rel).name in EXCLUDED_NAMES:
        return True
    return False


def classify(rel):
    """(style, derived) for the header check, otherwise None."""
    if is_excluded(rel):
        return None
    if rel.endswith((".cpp", ".h")):
        derived = rel in CATEGORY_A_EXPLICIT or any(
            rel.startswith(prefix) for prefix in CATEGORY_A_DIR_PREFIXES)
        return ("cpp", derived)
    if rel.endswith(".f"):
        if not rel.startswith("tests/fortran-reference/harnesses/"):
            return None
        return ("fortran", rel in HARNESS_DERIVED)
    if rel.endswith((".py", ".sh")):
        if not rel.startswith("tools/"):
            return None
        return ("hash", False)
    if rel == "Makefile":
        return ("hash", False)
    return None


# ---------------------------------------------------------------------------
# Single-file check
# ---------------------------------------------------------------------------

def check_file(root, rel):
    problems = []
    style, derived = classify(rel)
    path = root / rel
    try:
        text = path.read_text(encoding="utf-8")
    except UnicodeDecodeError:
        return [f"{rel}: binary file inside the header check (not allowed)"]
    except OSError as error:
        return [f"{rel}: not readable ({error})"]

    lines = text.splitlines()

    # Shebang rule: the shebang must be on line 1; a shebang on a
    # later line of the header is a violation.
    has_shebang = bool(lines) and lines[0].startswith("#!")
    if style == "hash":
        for index, line in enumerate(lines[:4]):
            if index > 0 and line.startswith("#!"):
                problems.append(
                    f"{rel}: shebang not in line 1 (line {index + 1})")
        prefix = "#"
    elif style == "fortran":
        prefix = "C"
    else:
        prefix = "//"

    expected = expected_block(
        prefix,
        (DERIVED_CPP if style == "cpp" else DERIVED_FORTRAN)
        if derived else None)

    if has_shebang:
        first, _, rest = text.partition("\n")
        if not rest.startswith(expected):
            problems.append(
                f"{rel}: project header missing or differs from expectation "
                f"(expected directly behind the shebang line)")
    elif not text.startswith(expected):
        problems.append(
            f"{rel}: project header missing or differs at the top of the file")

    # SPDX exactly once and correct. Only real SPDX declaration lines
    # (comment line carrying the identifier) are counted; plain
    # text mentions in code or documentation do not count.
    spdx_lines = [line for line in lines if SPDX_DECLARATION.match(line)]
    if len(spdx_lines) != 1:
        problems.append(
            f"{rel}: exactly one SPDX identifier expected, found: "
            f"{len(spdx_lines)}")
    elif SPDX_LINE not in spdx_lines[0]:
        problems.append(f"{rel}: wrong SPDX identifier: {spdx_lines[0].strip()}")

    # Derived sentence: exactly once in category A, not permitted in category B.
    derived_hits = sum(1 for line in lines
                       if DERIVED_CPP in line or DERIVED_FORTRAN in line)
    if derived and derived_hits != 1:
        problems.append(
            f"{rel}: category A expects exactly one derived sentence, "
            f"found: {derived_hits}")
    if not derived and derived_hits != 0:
        problems.append(
            f"{rel}: category B may not contain a derived sentence")

    # Fixed-form header: 'C' in column 1, length <= 72.
    if style == "fortran":
        for line in expected.splitlines():
            if not line:
                continue
            if not line.startswith("C"):
                problems.append(f"{rel}: fixed-form header does not begin with C")
                break
            if len(line) > 72:
                problems.append(
                    f"{rel}: fixed-form header line over 72 columns: {line}")

    return problems


def check_fortran_src(root):
    """Original Fortran sources: only the non-injection and
    notice-immutability checks (category C)."""
    problems = []
    src = root / "fortran-src"
    if not src.is_dir():
        return problems
    fortran_files = sorted(p for p in src.rglob("*") if p.is_file())
    injected = []
    notices = 0
    for path in fortran_files:
        rel = path.relative_to(root).as_posix()
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError as error:
            problems.append(f"{rel}: not readable ({error})")
            continue
        if "SPDX-License-Identifier" in text:
            injected.append(rel)
        if "Copyright (C) 2000" in text:
            notices += 1
    if injected:
        problems.append(
            "fortran-src/: impermissible license header injection in: "
            + ", ".join(injected))
    if len(fortran_files) >= FULL_TREE_MIN_FORTRAN_FILES:
        if notices != EXPECTED_ORIGINAL_NOTICE_FILES:
            problems.append(
                "fortran-src/: original-notice inventory changed "
                f"(expected {EXPECTED_ORIGINAL_NOTICE_FILES} files with "
                f"'Copyright (C) 2000', found: {notices})")
    return problems


def check_tree(root):
    problems = []
    checked = 0
    for path in sorted(root.rglob("*")):
        if not path.is_file():
            continue
        rel = path.relative_to(root).as_posix()
        if classify(rel) is None:
            continue
        checked += 1
        problems.extend(check_file(root, rel))
    problems.extend(check_fortran_src(root))
    return checked, problems


def list_inventory(root, out=None):
    """Print one inventory line per checked file: path, style, category."""
    if out is None:
        out = sys.stdout
    for path in sorted(root.rglob("*")):
        if not path.is_file():
            continue
        rel = path.relative_to(root).as_posix()
        kind = classify(rel)
        if kind is not None:
            print(f"{rel}\t{kind[0]}\t{'A' if kind[1] else 'B'}", file=out)


# ---------------------------------------------------------------------------
# Self-test with isolated diagnostic copies
# ---------------------------------------------------------------------------

def self_test():
    ok = True

    def expect(label, condition):
        nonlocal ok
        if not condition:
            ok = False
            print(f"SELFTEST FAILED: {label}")
        else:
            print(f"selftest OK: {label}")

    with tempfile.TemporaryDirectory(prefix="xfoil-license-selftest-") as tmp:
        root = pathlib.Path(tmp)

        def write(rel, text):
            path = root / rel
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text, encoding="utf-8")

        def body(rel):
            return f"// File: {rel}\nint main() {{ return 0; }}\n"

        cpp_a = expected_block("//", DERIVED_CPP)
        cpp_b = expected_block("//", None)
        hash_b = expected_block("#", None)
        fortran_b = expected_block("C", None)
        fortran_a = expected_block("C", DERIVED_FORTRAN)

        write("src/core/x.cpp", cpp_a + body("src/core/x.cpp"))
        write("src/main.cpp", cpp_b + body("src/main.cpp"))
        write("tools/t.py", "#!/usr/bin/env python3\n" + hash_b + "print(1)\n")
        write("tests/fortran-reference/harnesses/h/h.f",
              fortran_b + "      PROGRAM H\n      END\n")
        write("tests/fortran-reference/harnesses/c11a/c11a_init_harness.f",
              fortran_a + "      PROGRAM H\n      END\n")
        write("Makefile", hash_b + "all:\n\t@true\n")
        write("fortran-src/a.f",
              "C     Copyright (C) 2000   Mark Drela\n      PROGRAM A\n"
              "      END\n")
        write("tests/golden/cli/x.stdout.txt", "old Banner without Header\n")
        write("tests/data/core-x-reference/ref.txt", "data\n")
        write("sample/CLARK_Y.dat", "CLARK Y\n0.0 0.0\n")
        write("build/generated.cpp", "generated without header\n")

        checked, problems = check_tree(root)
        expect("clean diagnostic copy stays error-free", problems == [])
        expect("goldens/data/samples/build remain outside the check",
               checked == 6)

        # Negative case: missing header.
        write("src/main.cpp", body("src/main.cpp"))
        _, problems = check_tree(root)
        expect("missing Header is reported",
               any("src/main.cpp" in p for p in problems))
        write("src/main.cpp", cpp_b + body("src/main.cpp"))

        # Negative case: wrong SPDX-Identifier.
        write("src/main.cpp",
              cpp_b.replace(SPDX_LINE, "SPDX-License-Identifier: WITH")
              + body("src/main.cpp"))
        _, problems = check_tree(root)
        expect("wrong SPDX identifier is reported",
               any("wrong SPDX identifier" in p for p in problems))
        write("src/main.cpp", cpp_b + body("src/main.cpp"))

        # Negative case: missing derived sentence in category A.
        write("src/core/x.cpp", cpp_b + body("src/core/x.cpp"))
        _, problems = check_tree(root)
        expect("missing derived sentence in A is reported",
               any("src/core/x.cpp" in p and "derived sentence" in p
                   for p in problems))
        write("src/core/x.cpp", cpp_a + body("src/core/x.cpp"))

        # Negative case: impermissible derived sentence in category B.
        write("src/main.cpp", cpp_a + body("src/main.cpp"))
        _, problems = check_tree(root)
        expect("derived sentence in B is reported",
               any("src/main.cpp" in p
                   and "may not contain a derived sentence" in p
                   for p in problems))
        write("src/main.cpp", cpp_b + body("src/main.cpp"))

        # Negative case: shebang not in line 1.
        write("tools/t.py", "\n#!/usr/bin/env python3\n" + hash_b + "print(1)\n")
        _, problems = check_tree(root)
        expect("shebang outside line 1 is reported",
               any("shebang not in line 1" in p for p in problems))
        write("tools/t.py", "#!/usr/bin/env python3\n" + hash_b + "print(1)\n")

        # Negative case: header injected into fortran-src.
        write("fortran-src/a.f",
              hash_b + "C     Copyright (C) 2000   Mark Drela\n"
              "      PROGRAM A\n      END\n")
        _, problems = check_tree(root)
        expect("injection in fortran-src is reported",
               any("license header injection" in p for p in problems))

        # Positive: data/golden/generated files remain ignored.
        write("tests/golden/cli/x.stdout.txt", hash_b + "banner\n")
        write("build/generated.cpp", hash_b + "generated\n")
        _, problems = check_tree(root)
        expect("golden and build files remain ignored",
               not any("tests/golden" in p or "build/" in p
                       for p in problems))

        # Inventory listing: exercises the same runtime path as --list.
        buffer = io.StringIO()
        list_inventory(root, buffer)
        lines = buffer.getvalue().splitlines()
        expected_lines = [
            "Makefile\thash\tB",
            "src/core/x.cpp\tcpp\tA",
            "src/main.cpp\tcpp\tB",
            ("tests/fortran-reference/harnesses/c11a/"
             "c11a_init_harness.f\tfortran\tA"),
            "tests/fortran-reference/harnesses/h/h.f\tfortran\tB",
            "tools/t.py\thash\tB",
        ]
        expect("inventory lists exactly the checked files",
               lines == expected_lines)

    return 0 if ok else 1


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="License header gate for hand-written project sources.")
    parser.add_argument("--root", default=None,
                        help="check root (default: working directory)")
    parser.add_argument("--self-test", action="store_true",
                        help="self-test with isolated diagnostic copies")
    parser.add_argument("--list", action="store_true",
                        help="list the checked inventory")
    args = parser.parse_args(argv)

    if args.self_test:
        return self_test()

    root = pathlib.Path(args.root) if args.root else pathlib.Path.cwd()
    if not root.is_dir():
        print(f"setup error: check root does not exist: {root}",
              file=sys.stderr)
        return 2

    if args.list:
        list_inventory(root)
        return 0

    checked, problems = check_tree(root)
    if problems:
        print(f"License header gate: {len(problems)} violation(s) "
              f"for {checked} checked files:")
        for problem in problems:
            print(f"  {problem}")
        return 1
    print(f"License header gate: OK ({checked} hand-written source files "
          f"checked, original Fortran sources unchanged)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
