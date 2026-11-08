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

"""License-header and upstream-provenance gate.

Non-writing check of the hand-written project sources for the
binding GPL-3.0-or-later project header, plus a version-aware,
manifest-based check of the original upstream Fortran sources.

Header categories (explicit, reproducible path rules):
  A  files derived directly from XFOIL 6.99: project header additionally
     with the derived sentence
  B  project-side infrastructure, tests, tools: project header without
     derived sentence
  C  original Fortran sources under fortran-src/: never a header check or
     injection; only non-injection is checked here, provenance is checked
     through tools/fortran-provenance.json
  D  data, goldens, references, samples, generated files, binaries:
     explicitly excluded

Upstream provenance (manifest-based, per version):
  * versions/roots come from tools/fortran-provenance.json;
  * every manifest file must exist as a regular file below its version
    root, must not be a symlink, must match its SHA-256 and its explicit
    notice expectation;
  * every regular file below a version root must be listed as an upstream
    file or as an explicitly allowed metadata file;
  * nested version roots are supported: a more specific root owns its
    subtree and is excluded from the parent scan;
  * paths must be relative, must not traverse ("..") and must not be
    absolute; symlinks anywhere below a version root are rejected;
  * a file may be assigned to exactly one version.

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
import hashlib
import io
import json
import os
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

MANIFEST_REL = "tools/fortran-provenance.json"
SUPPORTED_SCHEMA_VERSION = 1
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")

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
# Single-file header check
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


# ---------------------------------------------------------------------------
# Upstream provenance manifest
# ---------------------------------------------------------------------------

class ManifestError(Exception):
    """Structural manifest problem (setup/configuration error)."""


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 16), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _manifest_rel_path(value, label):
    """Validate and return a normalized relative POSIX path."""
    if not isinstance(value, str) or value == "":
        raise ManifestError(f"{label}: expected a non-empty string")
    if "\x00" in value:
        raise ManifestError(f"{label}: NUL byte in path")
    if "\\" in value:
        raise ManifestError(
            f"{label}: backslash path separator not allowed: {value!r}")
    if value.startswith("/"):
        raise ManifestError(f"{label}: absolute path not allowed: {value!r}")
    if re.match(r"^[A-Za-z]:", value):
        raise ManifestError(f"{label}: absolute path not allowed: {value!r}")
    parts = value.split("/")
    if any(part in ("", ".", "..") for part in parts):
        raise ManifestError(
            f"{label}: empty, '.' or '..' path component not allowed: "
            f"{value!r}")
    return "/".join(parts)


def _path_parts(value):
    return tuple(value.split("/"))


def _is_strict_ancestor(ancestor, descendant):
    pa = _path_parts(ancestor)
    pd = _path_parts(descendant)
    return len(pa) < len(pd) and pd[:len(pa)] == pa


def _is_within_child(child, parent):
    """True when resolved Path child is parent or below parent."""
    try:
        child.relative_to(parent)
        return True
    except ValueError:
        return False


def _symlink_component(root_path, rel):
    """Return the first symlink on the path from root_path to rel, if any."""
    current = root_path
    parts = rel.split("/")
    for part in parts[:-1]:
        current = current / part
        if current.is_symlink():
            return current
    final = root_path / rel
    if final.is_symlink():
        return final
    return None


def load_manifest(root):
    """Load and structurally validate the provenance manifest."""
    manifest_path = root / MANIFEST_REL
    if not manifest_path.is_file():
        raise ManifestError(f"manifest not found: {MANIFEST_REL}")
    try:
        text = manifest_path.read_text(encoding="utf-8")
    except OSError as error:
        raise ManifestError(f"manifest not readable: {error}")
    try:
        data = json.loads(text)
    except (ValueError, TypeError) as error:
        raise ManifestError(f"invalid manifest JSON: {error}")
    if not isinstance(data, dict):
        raise ManifestError("manifest root must be a JSON object")
    if data.get("schema_version") != SUPPORTED_SCHEMA_VERSION:
        raise ManifestError(
            "unsupported schema_version: "
            f"{data.get('schema_version')!r} "
            f"(expected {SUPPORTED_SCHEMA_VERSION})")
    versions = data.get("versions")
    if not isinstance(versions, list) or not versions:
        raise ManifestError("manifest must contain a non-empty 'versions' list")

    parsed = []
    seen_ids = set()
    seen_roots = {}
    for index, version in enumerate(versions):
        label = f"versions[{index}]"
        if not isinstance(version, dict):
            raise ManifestError(f"{label}: expected a JSON object")
        version_id = version.get("id")
        if not isinstance(version_id, str) or not version_id:
            raise ManifestError(f"{label}: 'id' must be a non-empty string")
        if version_id in seen_ids:
            raise ManifestError(f"duplicate version id: {version_id}")
        seen_ids.add(version_id)
        label = f"{version_id}"

        root_rel = _manifest_rel_path(version.get("root"), f"{label}: root")
        if root_rel in seen_roots:
            raise ManifestError(
                f"overlapping version roots: {root_rel!r} is used by "
                f"{seen_roots[root_rel]} and {version_id}")
        seen_roots[root_rel] = version_id

        notice_pattern = version.get("notice_pattern")
        if not isinstance(notice_pattern, str) or not notice_pattern:
            raise ManifestError(
                f"{label}: 'notice_pattern' must be a non-empty string")

        metadata = version.get("metadata_paths", [])
        if not isinstance(metadata, list):
            raise ManifestError(
                f"{label}: 'metadata_paths' must be a list")
        metadata_paths = []
        for value in metadata:
            metadata_paths.append(
                _manifest_rel_path(value, f"{label}: metadata path"))
        if len(set(metadata_paths)) != len(metadata_paths):
            raise ManifestError(f"{label}: duplicate metadata path")

        files = version.get("files")
        if not isinstance(files, list) or not files:
            raise ManifestError(
                f"{label}: 'files' must be a non-empty list")
        file_entries = []
        seen_paths = set()
        for file_index, entry in enumerate(files):
            flabel = f"{label}: files[{file_index}]"
            if not isinstance(entry, dict):
                raise ManifestError(f"{flabel}: expected a JSON object")
            rel = _manifest_rel_path(entry.get("path"), f"{flabel}: path")
            if rel in seen_paths:
                raise ManifestError(
                    f"{label}: duplicate manifest entry: {rel}")
            if rel in metadata_paths:
                raise ManifestError(
                    f"{label}: path is both a source file and metadata: {rel}")
            seen_paths.add(rel)
            digest = entry.get("sha256")
            if not isinstance(digest, str) or not SHA256_RE.match(digest):
                raise ManifestError(
                    f"{flabel}: invalid SHA-256 value: {digest!r}")
            notice_expected = entry.get("notice_expected")
            if not isinstance(notice_expected, bool):
                raise ManifestError(
                    f"{flabel}: 'notice_expected' must be a boolean")
            file_entries.append({
                "path": rel,
                "sha256": digest,
                "notice_expected": notice_expected,
            })

        parsed.append({
            "id": version_id,
            "root": root_rel,
            "metadata_paths": metadata_paths,
            "notice_pattern": notice_pattern,
            "files": file_entries,
        })

    # A file must belong to exactly one version. A less specific root may
    # not claim a file that lives below a more specific (nested) root.
    assignments = {}
    for version in parsed:
        for entry in version["files"]:
            repo_rel = f"{version['root']}/{entry['path']}"
            if repo_rel in assignments:
                raise ManifestError(
                    "file assigned to multiple versions: "
                    f"{repo_rel} ({assignments[repo_rel]}, {version['id']})")
            assignments[repo_rel] = version["id"]
            for other in parsed:
                if other is version:
                    continue
                if _is_strict_ancestor(version["root"], other["root"]):
                    if _is_strict_ancestor(
                            other["root"], repo_rel) or repo_rel.startswith(
                                other["root"] + "/"):
                        raise ManifestError(
                            f"{version['id']}: file belongs to nested "
                            f"version root {other['id']}: {repo_rel}")

    return {
        "schema_version": SUPPORTED_SCHEMA_VERSION,
        "versions": parsed,
    }


def check_provenance(root, manifest):
    """Manifest-based per-version check. Returns (summaries, problems)."""
    problems = []
    summaries = []
    versions = manifest["versions"]

    for version in versions:
        version_id = version["id"]
        root_rel = version["root"]
        notice_pattern = version["notice_pattern"]
        root_path = root / root_rel

        manifest_files = {entry["path"]: entry for entry in version["files"]}
        metadata_paths = list(version["metadata_paths"])
        metadata_set = set(metadata_paths)

        summaries.append((
            version_id,
            len(version["files"]),
            sum(1 for entry in version["files"]
                if entry["notice_expected"]),
        ))

        if root_path.is_symlink():
            problems.append(
                f"{version_id}: version root is a symlink: {root_rel}")
            continue
        if not root_path.is_dir():
            problems.append(
                f"{version_id}: version root missing: {root_rel}")
            continue

        # Explicitly allowed repository-owned metadata files.
        for metadata_rel in metadata_paths:
            metadata_path = root_path / metadata_rel
            if metadata_path.is_symlink():
                problems.append(
                    f"{version_id}: metadata file is a symlink: {metadata_rel}")
            elif not metadata_path.exists():
                problems.append(
                    f"{version_id}: missing metadata file: {metadata_rel}")
            elif not metadata_path.is_file():
                problems.append(
                    f"{version_id}: metadata path is not a regular file: "
                    f"{metadata_rel}")

        # Manifest-declared upstream files.
        for rel, entry in manifest_files.items():
            path = root_path / rel
            symlink = _symlink_component(root_path, rel)
            if symlink is not None:
                problems.append(
                    f"{version_id}: upstream path contains a symlink: {rel}")
                continue
            if not path.exists():
                problems.append(
                    f"{version_id}: missing upstream file: {rel}")
                continue
            if not path.is_file():
                problems.append(
                    f"{version_id}: upstream file is not a regular file: {rel}")
                continue
            actual = sha256_file(path)
            if actual != entry["sha256"]:
                problems.append(
                    f"{version_id}: SHA-256 mismatch: {rel}")
            try:
                text = path.read_text(encoding="utf-8", errors="replace")
            except OSError as error:
                problems.append(
                    f"{version_id}: unreadable upstream file: {rel} ({error})")
                continue
            notice_present = notice_pattern in text
            if entry["notice_expected"] and not notice_present:
                problems.append(
                    f"{version_id}: expected upstream notice missing: {rel}")
            if not entry["notice_expected"] and notice_present:
                problems.append(
                    f"{version_id}: unexpected upstream notice: {rel}")

        # Nested version roots are owned by the more specific version.
        nested_roots = []
        for other in versions:
            if other is version:
                continue
            if _is_strict_ancestor(root_rel, other["root"]):
                nested_roots.append(
                    other["root"][len(root_rel) + 1:])

        # Every remaining regular file must be declared, and no symlink may
        # appear anywhere below the version root.
        for dirpath, dirnames, filenames in os.walk(root_path,
                                                    followlinks=False):
            directory = pathlib.Path(dirpath)
            kept_dirs = []
            for name in dirnames:
                child = directory / name
                rel_dir = child.relative_to(root_path).as_posix()
                if child.is_symlink():
                    problems.append(
                        f"{version_id}: symlink inside version root: "
                        f"{rel_dir} -> {os.readlink(child)}")
                    continue
                if rel_dir in nested_roots:
                    continue
                kept_dirs.append(name)
            dirnames[:] = kept_dirs

            for name in filenames:
                child = directory / name
                rel = child.relative_to(root_path).as_posix()
                if child.is_symlink():
                    problems.append(
                        f"{version_id}: symlink inside version root: "
                        f"{rel} -> {os.readlink(child)}")
                    continue
                if rel in metadata_set:
                    continue
                if rel not in manifest_files:
                    problems.append(
                        f"{version_id}: unexpected upstream file: {rel}")

    return summaries, problems


def check_fortran_injection(root):
    """Reject license-header injection into the original Fortran sources."""
    problems = []
    src = root / "fortran-src"
    if not src.is_dir():
        return problems
    injected = []
    for path in sorted(p for p in src.rglob("*") if p.is_file()):
        if path.is_symlink():
            continue
        rel = path.relative_to(root).as_posix()
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError as error:
            problems.append(f"{rel}: not readable ({error})")
            continue
        if "SPDX-License-Identifier" in text:
            injected.append(rel)
    if injected:
        problems.append(
            "fortran-src/: impermissible license header injection in: "
            + ", ".join(injected))
    return problems


def check_tree(root, manifest):
    problems = []
    checked = 0
    for path in sorted(root.rglob("*")):
        if not path.is_file() or path.is_symlink():
            continue
        rel = path.relative_to(root).as_posix()
        if classify(rel) is None:
            continue
        checked += 1
        problems.extend(check_file(root, rel))
    problems.extend(check_fortran_injection(root))
    summaries, provenance_problems = check_provenance(root, manifest)
    problems.extend(provenance_problems)
    return checked, problems, summaries


def list_inventory(root, out=None):
    """Print one inventory line per checked file: path, style, category."""
    if out is None:
        out = sys.stdout
    for path in sorted(root.rglob("*")):
        if not path.is_file() or path.is_symlink():
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

        def write(base, rel, text):
            path = base / rel
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text, encoding="utf-8")

        def body(rel):
            return f"// File: {rel}\nint main() {{ return 0; }}\n"

        # ------------------------------------------------------------------
        # Header checks (category A/B/C/D)
        # ------------------------------------------------------------------
        cpp_a = expected_block("//", DERIVED_CPP)
        cpp_b = expected_block("//", None)
        hash_b = expected_block("#", None)
        fortran_b = expected_block("C", None)
        fortran_a = expected_block("C", DERIVED_FORTRAN)

        header_root = root / "header"
        write(header_root, "src/core/x.cpp", cpp_a + body("src/core/x.cpp"))
        write(header_root, "src/main.cpp", cpp_b + body("src/main.cpp"))
        write(header_root, "tools/t.py",
              "#!/usr/bin/env python3\n" + hash_b + "print(1)\n")
        write(header_root, "tests/fortran-reference/harnesses/h/h.f",
              fortran_b + "      PROGRAM H\n      END\n")
        write(header_root, "tests/fortran-reference/harnesses/c11a/"
              "c11a_init_harness.f",
              fortran_a + "      PROGRAM H\n      END\n")
        write(header_root, "Makefile", hash_b + "all:\n\t@true\n")
        a_text = ("C     Copyright (C) 2000   Mark Drela\n"
                  "      PROGRAM A\n      END\n")
        write(header_root, "fortran-src/a.f", a_text)
        write(header_root, "tests/golden/cli/x.stdout.txt",
              "old Banner without Header\n")
        write(header_root, "tests/data/core-x-reference/ref.txt", "data\n")
        write(header_root, "sample/CLARK_Y.dat", "CLARK Y\n0.0 0.0\n")
        write(header_root, "build/generated.cpp",
              "generated without header\n")
        write(header_root, MANIFEST_REL, json.dumps({
            "schema_version": 1,
            "versions": [{
                "id": "header-test-6.99",
                "root": "fortran-src",
                "metadata_paths": [],
                "notice_pattern": "Copyright (C) 2000",
                "files": [{
                    "path": "a.f",
                    "sha256": hashlib.sha256(
                        a_text.encode("utf-8")).hexdigest(),
                    "notice_expected": True,
                }],
            }],
        }))
        header_manifest = load_manifest(header_root)

        checked, problems, _ = check_tree(header_root, header_manifest)
        expect("clean diagnostic copy stays error-free", problems == [])
        expect("goldens/data/samples/build remain outside the check",
               checked == 6)

        # Negative case: missing header.
        write(header_root, "src/main.cpp", body("src/main.cpp"))
        _, problems, _ = check_tree(header_root, header_manifest)
        expect("missing Header is reported",
               any("src/main.cpp" in p for p in problems))
        write(header_root, "src/main.cpp", cpp_b + body("src/main.cpp"))

        # Negative case: wrong SPDX-Identifier.
        write(header_root, "src/main.cpp",
              cpp_b.replace(SPDX_LINE, "SPDX-License-Identifier: WITH")
              + body("src/main.cpp"))
        _, problems, _ = check_tree(header_root, header_manifest)
        expect("wrong SPDX identifier is reported",
               any("wrong SPDX identifier" in p for p in problems))
        write(header_root, "src/main.cpp", cpp_b + body("src/main.cpp"))

        # Negative case: missing derived sentence in category A.
        write(header_root, "src/core/x.cpp", cpp_b + body("src/core/x.cpp"))
        _, problems, _ = check_tree(header_root, header_manifest)
        expect("missing derived sentence in A is reported",
               any("src/core/x.cpp" in p and "derived sentence" in p
                   for p in problems))
        write(header_root, "src/core/x.cpp", cpp_a + body("src/core/x.cpp"))

        # Negative case: impermissible derived sentence in category B.
        write(header_root, "src/main.cpp", cpp_a + body("src/main.cpp"))
        _, problems, _ = check_tree(header_root, header_manifest)
        expect("derived sentence in B is reported",
               any("src/main.cpp" in p
                   and "may not contain a derived sentence" in p
                   for p in problems))
        write(header_root, "src/main.cpp", cpp_b + body("src/main.cpp"))

        # Negative case: shebang not in line 1.
        write(header_root, "tools/t.py",
              "\n#!/usr/bin/env python3\n" + hash_b + "print(1)\n")
        _, problems, _ = check_tree(header_root, header_manifest)
        expect("shebang outside line 1 is reported",
               any("shebang not in line 1" in p for p in problems))
        write(header_root, "tools/t.py",
              "#!/usr/bin/env python3\n" + hash_b + "print(1)\n")

        # Negative case: header injected into fortran-src.
        write(header_root, "fortran-src/a.f",
              hash_b + "C     Copyright (C) 2000   Mark Drela\n"
              "      PROGRAM A\n      END\n")
        _, problems, _ = check_tree(header_root, header_manifest)
        expect("injection in fortran-src is reported",
               any("license header injection" in p for p in problems))

        # Positive: data/golden/generated files remain ignored.
        write(header_root, "tests/golden/cli/x.stdout.txt", hash_b + "banner\n")
        write(header_root, "build/generated.cpp", hash_b + "generated\n")
        _, problems, _ = check_tree(header_root, header_manifest)
        expect("golden and build files remain ignored",
               not any("tests/golden" in p or "build/" in p
                       for p in problems))

        # Inventory listing: exercises the same runtime path as --list.
        buffer = io.StringIO()
        list_inventory(header_root, buffer)
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

        # ------------------------------------------------------------------
        # Provenance manifest fixtures
        # ------------------------------------------------------------------
        notice = "C     Copyright (C) 2000   Mark Drela\n"
        no_notice = "C     no upstream notice\n"
        pattern = "Copyright (C) 2000"

        def sha_text(text):
            return hashlib.sha256(text.encode("utf-8")).hexdigest()

        case_counter = [0]

        def build_case(layout):
            case_counter[0] += 1
            base = root / f"prov-{case_counter[0]}"
            base.mkdir(parents=True, exist_ok=True)
            versions = []
            for version_id, version_root, metadata, files in layout:
                version_path = base / version_root
                version_path.mkdir(parents=True, exist_ok=True)
                entries = []
                for rel, text, notice_expected in files:
                    write(version_path, rel, text)
                    entries.append({
                        "path": rel,
                        "sha256": sha_text(text),
                        "notice_expected": notice_expected,
                    })
                for metadata_rel in metadata:
                    write(version_path, metadata_rel, "metadata\n")
                versions.append({
                    "id": version_id,
                    "root": version_root,
                    "metadata_paths": list(metadata),
                    "notice_pattern": pattern,
                    "files": entries,
                })
            write(base, MANIFEST_REL, json.dumps({
                "schema_version": 1,
                "versions": versions,
            }))
            return base

        def run_case(base):
            return check_provenance(base, load_manifest(base))

        def nested_layout():
            return [
                ("xfoil-6.99", "fortran-src", ["SHA256SUMS"], [
                    ("a.f", notice, True),
                    ("b.f", no_notice, False),
                ]),
                ("xfoil-6.996", "fortran-src/xfoil-6.996", [], [
                    ("a.f", notice, True),
                    ("b.f", no_notice, False),
                ]),
            ]

        def expect_manifest_error(label, data, base_name):
            base = root / base_name
            base.mkdir(parents=True, exist_ok=True)
            write(base, MANIFEST_REL, json.dumps(data))
            try:
                load_manifest(base)
            except ManifestError:
                expect(label, True)
            else:
                expect(label, False)

        # Two valid nested version roots (current transition state).
        base = build_case(nested_layout())
        summaries, problems = run_case(base)
        expect("two valid nested version roots", problems == [])
        expect("nested summary counts",
               summaries == [("xfoil-6.99", 2, 1), ("xfoil-6.996", 2, 1)])
        expect("declared metadata file is allowed",
               not any("SHA256SUMS" in p for p in problems))

        # Later separated sibling roots.
        base = build_case([
            ("xfoil-6.99", "fortran-src/xfoil-6.99", [], [
                ("a.f", notice, True)]),
            ("xfoil-6.996", "fortran-src/xfoil-6.996", [], [
                ("a.f", notice, True)]),
        ])
        _, problems = run_case(base)
        expect("separate sibling version roots", problems == [])

        # Missing upstream file in 6.99.
        base = build_case(nested_layout())
        (base / "fortran-src/a.f").unlink()
        _, problems = run_case(base)
        expect("missing file in 6.99 is reported",
               any("xfoil-6.99: missing upstream file: a.f" in p
                   for p in problems))

        # Missing upstream file in 6.996.
        base = build_case(nested_layout())
        (base / "fortran-src/xfoil-6.996/a.f").unlink()
        _, problems = run_case(base)
        expect("missing file in 6.996 is reported",
               any("xfoil-6.996: missing upstream file: a.f" in p
                   for p in problems))

        # Additional file in 6.99.
        base = build_case(nested_layout())
        write(base / "fortran-src", "extra.f", no_notice)
        _, problems = run_case(base)
        expect("additional file in 6.99 is reported",
               any("xfoil-6.99: unexpected upstream file: extra.f" in p
                   for p in problems))

        # Additional file in 6.996.
        base = build_case(nested_layout())
        write(base / "fortran-src/xfoil-6.996", "extra.f", no_notice)
        _, problems = run_case(base)
        expect("additional file in 6.996 is reported",
               any("xfoil-6.996: unexpected upstream file: extra.f" in p
                   for p in problems))

        # SHA-256 mismatch.
        base = build_case(nested_layout())
        write(base / "fortran-src", "b.f", "C     changed content\n")
        _, problems = run_case(base)
        expect("SHA-256 mismatch is reported",
               any("xfoil-6.99: SHA-256 mismatch: b.f" in p
                   for p in problems))

        # Expected notice missing (hash matches, notice absent).
        base = build_case([
            ("V", "fortran-src", [], [("n.f", no_notice, True)])])
        _, problems = run_case(base)
        expect("missing expected notice is reported",
               any("V: expected upstream notice missing: n.f" in p
                   for p in problems))

        # Unexpected notice present (hash matches, notice not expected).
        base = build_case([
            ("V", "fortran-src", [], [("n.f", notice, False)])])
        _, problems = run_case(base)
        expect("unexpected notice is reported",
               any("V: unexpected upstream notice: n.f" in p
                   for p in problems))

        # Wrong notice assignment with an unchanged total notice count.
        base = build_case([
            ("A", "fortran-src/a", [], [("f.f", notice, False)]),
            ("B", "fortran-src/b", [], [("f.f", no_notice, True)]),
        ])
        summaries, problems = run_case(base)
        expect("wrong version notice assignment is reported",
               any("A: unexpected upstream notice: f.f" in p
                   for p in problems)
               and any("B: expected upstream notice missing: f.f" in p
                       for p in problems))
        expect("wrong version notice assignment keeps total notice count",
               sum(item[2] for item in summaries) == 1)

        # A less specific root may not claim a nested file.
        base = build_case([
            ("xfoil-6.99", "fortran-src", [], [
                ("xfoil-6.996/a.f", notice, True)]),
            ("xfoil-6.996", "fortran-src/xfoil-6.996", [], [
                ("a.f", notice, True)]),
        ])
        expect("file assigned to a nested version is rejected",
               _manifest_error_raised(base))

        # Duplicate manifest path.
        base = build_case([
            ("V", "fortran-src", [], [
                ("a.f", notice, True), ("a.f", notice, True)])])
        expect("duplicate manifest entry is rejected",
               _manifest_error_raised(base))

        # Duplicate version id.
        base = build_case([
            ("V", "fortran-src/a", [], [("a.f", notice, True)]),
            ("V", "fortran-src/b", [], [("a.f", notice, True)]),
        ])
        expect("duplicate version id is rejected",
               _manifest_error_raised(base))

        # Duplicate/overlapping version roots.
        base = build_case([
            ("A", "fortran-src", [], [("a.f", notice, True)]),
            ("B", "fortran-src", [], [("b.f", notice, True)]),
        ])
        expect("overlapping version roots are rejected",
               _manifest_error_raised(base))

        # Absolute root.
        expect_manifest_error(
            "absolute version root is rejected",
            {"schema_version": 1, "versions": [{
                "id": "V", "root": "/abs", "metadata_paths": [],
                "notice_pattern": pattern, "files": [{
                    "path": "a.f", "sha256": sha_text(notice),
                    "notice_expected": True}]}]},
            "merr-abs-root")

        # Absolute file path.
        expect_manifest_error(
            "absolute file path is rejected",
            {"schema_version": 1, "versions": [{
                "id": "V", "root": "fortran-src", "metadata_paths": [],
                "notice_pattern": pattern, "files": [{
                    "path": "/abs.f", "sha256": sha_text(notice),
                    "notice_expected": True}]}]},
            "merr-abs-file")

        # Parent traversal in root.
        expect_manifest_error(
            "root traversal is rejected",
            {"schema_version": 1, "versions": [{
                "id": "V", "root": "../outside", "metadata_paths": [],
                "notice_pattern": pattern, "files": [{
                    "path": "a.f", "sha256": sha_text(notice),
                    "notice_expected": True}]}]},
            "merr-dotdot-root")

        # Parent traversal in file path.
        expect_manifest_error(
            "file traversal is rejected",
            {"schema_version": 1, "versions": [{
                "id": "V", "root": "fortran-src", "metadata_paths": [],
                "notice_pattern": pattern, "files": [{
                    "path": "../escape.f", "sha256": sha_text(notice),
                    "notice_expected": True}]}]},
            "merr-dotdot-file")

        # Symlink inside a version root.
        base = build_case([
            ("V", "fortran-src", [], [("a.f", notice, True)])])
        os.symlink(base / "fortran-src/a.f", base / "fortran-src/link.f")
        _, problems = run_case(base)
        expect("symlink inside version root is reported",
               any("V: symlink inside version root: link.f" in p
                   for p in problems))

        # Symlink escaping the version root.
        base = build_case([
            ("V", "fortran-src", [], [("a.f", notice, True)])])
        outside = base / "outside.f"
        outside.write_text(no_notice, encoding="utf-8")
        os.symlink(outside, base / "fortran-src/escape.f")
        _, problems = run_case(base)
        expect("symlink escaping the version root is reported",
               any("V: symlink inside version root: escape.f" in p
                   for p in problems))

        # Missing metadata file.
        base = build_case(nested_layout())
        (base / "fortran-src/SHA256SUMS").unlink()
        _, problems = run_case(base)
        expect("missing declared metadata file is reported",
               any("xfoil-6.99: missing metadata file: SHA256SUMS" in p
                   for p in problems))

        # Additional undeclared metadata-like file is rejected.
        base = build_case(nested_layout())
        write(base / "fortran-src", "extra.meta", "metadata\n")
        _, problems = run_case(base)
        expect("undeclared metadata file is rejected",
               any("xfoil-6.99: unexpected upstream file: extra.meta" in p
                   for p in problems))

        # Missing manifest.
        base = root / "prov-missing-manifest"
        base.mkdir(parents=True, exist_ok=True)
        expect("missing manifest is reported",
               _manifest_error_raised(base, load=False))

        # Invalid JSON.
        base = root / "prov-invalid-json"
        base.mkdir(parents=True, exist_ok=True)
        write(base, MANIFEST_REL, "{broken")
        expect("invalid manifest JSON is reported",
               _manifest_error_raised(base, load=False))

        # Unknown schema version.
        expect_manifest_error(
            "unknown schema version is rejected",
            {"schema_version": 999, "versions": []},
            "merr-schema")

    return 0 if ok else 1


def _manifest_error_raised(base, load=True):
    try:
        load_manifest(base)
    except ManifestError:
        return True
    return False


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="License header and upstream provenance gate.")
    parser.add_argument("--root", default=None,
                        help="check root (default: working directory)")
    parser.add_argument("--self-test", action="store_true",
                        help="self-test with isolated diagnostic copies")
    parser.add_argument("--list", action="store_true",
                        help="list the checked inventory")
    args = parser.parse_args(argv)

    if args.self_test:
        return self_test()

    root = (pathlib.Path(args.root) if args.root
            else pathlib.Path.cwd()).resolve()
    if not root.is_dir():
        print(f"setup error: check root does not exist: {root}",
              file=sys.stderr)
        return 2

    if args.list:
        list_inventory(root)
        return 0

    try:
        manifest = load_manifest(root)
    except ManifestError as error:
        print(f"License header gate: manifest error: {error}",
              file=sys.stderr)
        return 2

    checked, problems, summaries = check_tree(root, manifest)
    for version_id, file_count, notice_count in summaries:
        print(f"{version_id}: {file_count} files, "
              f"{notice_count} notice-bearing files")
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
