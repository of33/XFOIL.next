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

# -*- coding: utf-8 -*-
"""Non-mutating dependency check for the xfoil::core.

The check has two tasks:

1. manifest completeness: the files explicitly declared in a core
   source manifest are compared with the actual file set under the
   exclusive new core roots. The three gate input manifests
   (core-sources.txt, io-sources.txt, session-sources.txt) live in
   tools/dependency-manifests/; no other location is read:

       include/xfoil/core/
       src/core/foundation/

   Every actual core file must appear exactly once in the manifest, and
   every manifest file must exist. There may be no duplicates, no
   absolute paths, no '..' components, no empty path components and no
   non-normalized spelling. The resolved path (realpath) must lie under
   the repository root and under one of the two core roots (symlinks
   pointing outside are rejected). Only header and source suffixes
   (.h, .cpp) are allowed. Additional or missing files as well as
   structural manifest errors lead to a non-zero exit code
   (structural: 2).

2. Forbidden dependencies: every (validated) core file is checked for

   - the public facade header (xfoil/XFoil.h),
   - app/CLI includes (app/, CommandInterpreter),
   - test includes (tests/, TestSupport.h),
   - direct use of the facade class as a complete identifier
     (XFoil, ::XFoil, xfoil::XFoil), without falsely hitting a longer
     identifier that merely contains the character sequence 'XFoil',
   - globally mutable solver state (static, heuristic).

The script is strictly non-mutating: it changes no file. On at least
one violation it returns a non-zero exit code and reports file, line
and violation. Otherwise it returns 0.

Exit codes:
    0  - no violations, manifest complete and consistent
    1  - at least one dependency violation
    2  - structural manifest error (missing manifest, invalid path,
         duplicate, missing/too many files, symlink pointing outside)

Uses only the Python standard library, runnable from Python 3.8.
"""

import argparse
import os
import re
import sys


# Permanent, phase-neutral location of the gate input manifests.
# The manifests are tool inputs, not documentation; they live next to the
# tool that consumes them. There is deliberately no fallback search at any
# other location: a missing manifest below tools/dependency-manifests/ is a
# hard structural error (exit code 2).
MANIFEST_RELATIVE_PATH = os.path.join(
    "tools", "dependency-manifests", "core-sources.txt")

# Roots of the 0-based core. Files below other src/core/
# subdirectories (e.g. XFoilFacade.cpp, math/, geometry/) are
# session/facade/helper layers and do not belong to the core roots.
CORE_ROOTS = (
    os.path.join("include", "xfoil", "core"),
    os.path.join("src", "core", "foundation"),
    os.path.join("src", "core", "numerics"),
    os.path.join("src", "core", "geometry_core"),
    os.path.join("src", "core", "paneling"),
    os.path.join("src", "core", "inviscid_core"),
    os.path.join("src", "core", "boundary_layer_core"),
    os.path.join("src", "core", "boundary_layer_solver"),
    os.path.join("src", "core", "geometry_design"),
    os.path.join("src", "core", "inverse_design"),
    os.path.join("src", "core", "polar"),
    os.path.join("src", "core", "export_data"),
)

# header/source suffixes allowed for the core.
ALLOWED_SUFFIXES = (".h", ".cpp")

# Source roots of the core in slash spelling (subgroup of CORE_ROOTS,
# only src paths; include/xfoil/core contains header-only files). Serves
# the Makefile synchronization check (: sources instead of object literals).
CORE_SOURCE_ROOT_PREFIXES = tuple(
    core_root.replace(os.sep, "/")
    for core_root in CORE_ROOTS
    if core_root.replace(os.sep, "/").startswith("src/")
)

# Public facade headers that the core may not include.
# The rule is restricted to the concrete, known headers and does not
# block arbitrary future file names merely because of a suffix.
FORBIDDEN_FACADE_INCLUDES = (
    os.path.join("xfoil", "XFoil.h"),
)

INCLUDE_RE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]')
# Whole-identifier use of the facade class XFoil. The negative
# lookbehind prevents matches in longer identifiers (myXFoil) as
# well as in member accesses (foo.XFoil). The forms XFoil, ::XFoil
# and xfoil::XFoil all end in 'XFoil' after a ':' or space and
# are therefore captured.
XFOIL_USAGE_RE = re.compile(r'(?<![\w.])XFoil\b')
# static declaration (leading whitespace allowed) that is neither const
# nor constexpr. static_assert/static_cast are not captured by the
# word-boundary pattern.
STATIC_DECL_RE = re.compile(
    r'^\s*static\s+(?!const\b|constexpr\b|assert\b|cast\b)')


def normalize_include_target(target):
    """Normalize an include target for comparison (defensive)."""
    target = target.strip().replace("\\", "/")
    while target.startswith("./"):
        target = target[2:]
    return target


def classify_include(target):
    """Return the list of violation reasons for an include target."""
    normalized = normalize_include_target(target)
    reasons = []
    if normalized in FORBIDDEN_FACADE_INCLUDES:
        reasons.append(
            "forbidden facade include (%s)" % normalized)
    if normalized.startswith("app/") or "/app/" in normalized:
        reasons.append("forbidden App-Include (app/)")
    if "CommandInterpreter" in normalized:
        reasons.append("forbidden CLI-Include (CommandInterpreter)")
    if (normalized.startswith("tests/") or "/tests/" in normalized
            or normalized == "TestSupport.h"):
        reasons.append("forbidden Test-Include (tests/)")
    return reasons


def strip_comments_and_strings(text):
    """Replace comments and string/char literals by spaces.

    Line breaks are preserved so that the line numbers stay stable.
    Brackets ({}[]) outside comments/strings are kept so that the
    scope depth can be computed.
    """
    result = []
    i = 0
    n = len(text)
    state = "code"
    while i < n:
        c = text[i]
        if state == "code":
            if c == "/" and i + 1 < n and text[i + 1] == "/":
                state = "line_comment"
                result.append("  ")
                i += 2
                continue
            if c == "/" and i + 1 < n and text[i + 1] == "*":
                state = "block_comment"
                result.append("  ")
                i += 2
                continue
            if c == '"':
                state = "string"
                result.append(" ")
                i += 1
                continue
            if c == "'":
                state = "char"
                result.append(" ")
                i += 1
                continue
            result.append(c)
            i += 1
        elif state == "line_comment":
            if c == "\n":
                state = "code"
                result.append("\n")
            else:
                result.append(" ")
            i += 1
        elif state == "block_comment":
            if c == "*" and i + 1 < n and text[i + 1] == "/":
                state = "code"
                result.append("  ")
                i += 2
                continue
            result.append("\n" if c == "\n" else " ")
            i += 1
        elif state == "string":
            if c == "\\" and i + 1 < n:
                result.append("  ")
                i += 2
                continue
            if c == '"':
                state = "code"
                result.append(" ")
            else:
                result.append("\n" if c == "\n" else " ")
                if c == "\n":
                    state = "code"
            i += 1
        elif state == "char":
            if c == "\\" and i + 1 < n:
                result.append("  ")
                i += 2
                continue
            if c == "'":
                state = "code"
                result.append(" ")
            else:
                result.append("\n" if c == "\n" else " ")
                if c == "\n":
                    state = "code"
            i += 1
    return "".join(result)


def namespace_depth_per_line(stripped_text):
    """Compute for each line the scope depth at the start of the line.

    Only functions/blocks (braces not opened by a 'namespace' line)
    raise the depth. Namespace braces are transparent, so that a
    'static' in namespace scope (also within 'namespace { ... }') is
    recognized as global and a 'static' in function/block scope is not
    reported as namespace state.

    Returns a list of integer values (one per line).
    """
    lines = stripped_text.splitlines()
    depths = []
    depth = 0
    for line in lines:
        depths.append(depth)
        stripped_line = line.strip()
        # 'namespace' as keyword: the line begins with 'namespace'
        # followed by space or '{' (not a longer identifier such as
        # namespaceFoo).
        is_namespace = re.match(r'^namespace(\s|$)', stripped_line) is not None
        for ch in line:
            if ch == "{":
                if not is_namespace:
                    depth += 1
            elif ch == "}":
                if depth > 0:
                    depth -= 1
    return depths


def read_manifest(manifest_path):
    """Read the raw entries of the core manifest (without comments/blank lines)."""
    with open(manifest_path, "rb") as handle:
        data = handle.read()
    files = []
    for raw in data.decode("utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        files.append(line)
    return files


def validate_manifest_entry(root, entry):
    """Validate a single manifest entry at path level.

    Returns (ok, error message); for ok the message is None.
    """
    if not entry:
        return False, "empty manifest entry"
    if "\\" in entry:
        return False, "backslash in manifest path: %s" % entry
    if entry.startswith("/"):
        return False, "absolute manifest path: %s" % entry
    if entry.startswith("./"):
        return False, (
            "non-normalized manifest path spelling (/.): %s" % entry)
    parts = entry.split("/")
    if any(part == "" for part in parts):
        return False, "empty path component in manifest path: %s" % entry
    if ".." in parts:
        return False, "'..' component in manifest path: %s" % entry
    if not any(entry == r or entry.startswith(r + "/") for r in CORE_ROOTS):
        return False, (
            "manifest path lies outside the core roots: %s" % entry)
    if not any(entry.endswith(s) for s in ALLOWED_SUFFIXES):
        return False, (
            "file suffix not allowed (only .h/.cpp): %s" % entry)
    full_path = os.path.join(root, entry)
    if not os.path.isfile(full_path):
        return False, "manifest file missing: %s" % entry
    real_root = os.path.realpath(root)
    real_full = os.path.realpath(full_path)
    if not (real_full == real_root
            or real_full.startswith(real_root + os.sep)):
        return False, (
            "realpath lies outside the repository root: %s" % entry)
    under_core = False
    for core_root in CORE_ROOTS:
        real_core = os.path.realpath(os.path.join(root, core_root))
        if real_full == real_core or real_full.startswith(real_core + os.sep):
            under_core = True
            break
    if not under_core:
        return False, (
            "realpath lies outside the allowed core roots "
            "(symlink pointing outside?): %s" % entry)
    return True, None


def scan_core_files(root):
    """Collect the actual set of files under the core roots.

    Returns a set of normalized paths given relative to the root.
    """
    found = set()
    for core_root in CORE_ROOTS:
        abs_root = os.path.join(root, core_root)
        if not os.path.isdir(abs_root):
            continue
        for dirpath, _dirnames, filenames in os.walk(abs_root):
            for name in filenames:
                full = os.path.join(dirpath, name)
                rel = os.path.relpath(full, root).replace(os.sep, "/")
                found.add(rel)
    return found


def check_makefile_sync(root, manifest_entries):
    """Checks that every core .cpp source listed in the manifest appears
    as a source in a productive source group of the Makefile.

    The Makefile derives its object lists mechanically per patsubst from the
    source groups; therefore the source reference (e.g.
    src/core/foundation/Matrix.cpp) is the authoritative synchronization anchor
    and no longer a single-object literal. Deliberately without a full
    Makefile parser: only the reference to the corresponding source is
    searched for. Headers are header-only and are not checked. Every
    manifest .cpp under all core roots is checked (an extension
    compared with the earlier subset). Returns a list of messages
    (empty on success).
    """
    cpp_entries = [
        entry for entry in manifest_entries if entry.endswith(".cpp")
        and any(entry.startswith(root_prefix + "/")
                for root_prefix in CORE_SOURCE_ROOT_PREFIXES)
    ]
    if not cpp_entries:
        return []
    makefile_path = os.path.join(root, "Makefile")
    if not os.path.isfile(makefile_path):
        return ["Makefile not found: %s" % makefile_path]
    with open(makefile_path, "rb") as handle:
        makefile_text = handle.read().decode("utf-8")
    messages = []
    for entry in cpp_entries:
        if entry not in makefile_text:
            messages.append(
                "core source %s is in the manifest, but is not "
                "listed as a productive source group in the Makefile "
                "(expected: %s)" % (entry, entry))
    return messages


def check_file(path):
    """Check a single core file.

    Returns a list of (line, message) for every violation.
    """
    violations = []
    with open(path, "rb") as handle:
        data = handle.read()
    text = data.decode("utf-8")
    stripped = strip_comments_and_strings(text)
    stripped_lines = stripped.splitlines()
    depths = namespace_depth_per_line(stripped)
    for index, line in enumerate(text.splitlines(), start=1):
        code = stripped_lines[index - 1]
        match = INCLUDE_RE.match(line)
        if match:
            for reason in classify_include(match.group(1)):
                violations.append((index, reason))
        if XFOIL_USAGE_RE.search(code):
            violations.append(
              (index,
                "forbidden use of the facade class "
                "(complete identifier)"))
        if (STATIC_DECL_RE.match(code)
                and depths[index - 1] == 0):
            violations.append(
                (index,
                 "global mutable solver state "
                 "(static, namespace scope)"))
    return violations


def check_manifest(root, manifest_path):
    """Run the complete check (manifest + dependencies).

    Returns (exit_code, messages); messages is a list of
    (relative_path, line, text); line is 0 for structural errors.
    """
    if not os.path.isfile(manifest_path):
        return 2, [("manifest", 0, "Error: manifest not found: %s"
                    % manifest_path)]

    entries = read_manifest(manifest_path)

    # Duplicates.
    seen = set()
    for entry in entries:
        if entry in seen:
            return 2, [("manifest", 0,
                        "duplicate manifest entry: %s" % entry)]
        seen.add(entry)

    # Strict path validation for each entry.
    for entry in entries:
        ok, message = validate_manifest_entry(root, entry)
        if not ok:
            return 2, [("manifest", 0, message)]

    # Completeness: actual core files vs. manifest.
    actual = scan_core_files(root)
    manifest_set = set(entries)
    for missing in sorted(actual - manifest_set):
        return 2, [("manifest", 0,
                    "actual core file missing in the manifest: %s"
                    % missing)]
    for extra in sorted(manifest_set - actual):
        return 2, [("manifest", 0,
                    "manifest entry does not exist as core file: %s"
                    % extra)]

    # Makefile/manifest synchronization: every foundation .cpp in the
    # manifest must also be built as a core object in the Makefile.
    sync_messages = check_makefile_sync(root, entries)
    if sync_messages:
        return 2, [("Makefile/manifest", 0, message)
                   for message in sync_messages]

    # Dependency check of the validated files.
    violations = []
    for rel_path in entries:
        for line_no, message in check_file(os.path.join(root, rel_path)):
            violations.append((rel_path, line_no, message))

    if violations:
        return 1, violations
    return 0, violations


IO_MANIFEST_RELATIVE_PATH = os.path.join(
    "tools", "dependency-manifests", "io-sources.txt")

IO_ROOTS = (
    os.path.join("include", "xfoil", "io"),
    os.path.join("src", "io"),
)

IO_INCLUDE_RE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]')


def classify_io_include(target):
    """Rule for the xfoil::io layer: only xfoil/io and xfoil/core allowed."""
    normalized = normalize_include_target(target)
    reasons = []
    if normalized.startswith("tests/") or normalized.startswith("../tests"):
        reasons.append("forbidden Test-Include in xfoil::io")
    first = normalized.split("/")[0]
    if first == "xfoil":
        second = normalized.split("/")[1] if "/" in normalized else ""
        if second not in ("io", "core"):
            reasons.append(
                "xfoil::io may only include xfoil/io and xfoil core: %s"
                % normalized)
    return reasons


def check_io_file(path):
    """Check an io file against the layer rules."""
    violations = []
    with open(path, "rb") as handle:
        text = handle.read().decode("utf-8")
    for index, line in enumerate(text.splitlines(), start=1):
        match = IO_INCLUDE_RE.match(line)
        if match:
            for reason in classify_io_include(match.group(1)):
                violations.append((index, reason))
    return violations


def check_io_layer(root):
    """manifest, Makefile and include rules for xfoil::io."""
    manifest_path = os.path.join(root, IO_MANIFEST_RELATIVE_PATH)
    if not os.path.isfile(manifest_path):
        return [("io manifest", 0,
                 "Error: io manifest not found: %s" % manifest_path)]
    entries = read_manifest(manifest_path)
    seen = set()
    for entry in entries:
        if entry in seen:
            return [("io manifest", 0, "duplicate io entry: %s" % entry)]
        seen.add(entry)
        if not any(entry == r or entry.startswith(r + "/") for r in IO_ROOTS):
            return [("io manifest", 0,
                     "io path outside the io roots: %s" % entry)]
        if not os.path.isfile(os.path.join(root, entry)):
            return [("io manifest", 0, "io file missing: %s" % entry)]
    actual = set()
    for io_root in IO_ROOTS:
        abs_root = os.path.join(root, io_root)
        if not os.path.isdir(abs_root):
            continue
        for dirpath, _dirnames, filenames in os.walk(abs_root):
            for name in filenames:
                full = os.path.join(dirpath, name)
                rel = os.path.relpath(full, root).replace(os.sep, "/")
                actual.add(rel)
    manifest_set = set(entries)
    for missing in sorted(actual - manifest_set):
        return [("io manifest", 0,
                 "actual io file missing in the manifest: %s" % missing)]
    for extra in sorted(manifest_set - actual):
        return [("io manifest", 0,
                 "io manifest entry does not exist: %s" % extra)]
    makefile_path = os.path.join(root, "Makefile")
    with open(makefile_path, "rb") as handle:
        makefile_text = handle.read().decode("utf-8")
    messages = []
    for entry in entries:
        if not entry.endswith(".cpp"):
            continue
        if entry not in makefile_text:
            messages.append(("io/Makefile", 0,
                             "io source %s is not listed as a "
                             "productive source group in the Makefile"
                             % entry))
    for entry in entries:
        for line_no, message in check_io_file(os.path.join(root, entry)):
            messages.append((entry, line_no, message))
    # Core may not include xfoil/io.
    for rel_path in scan_core_files(root):
        if not rel_path.endswith((".h", ".cpp")):
            continue
        with open(os.path.join(root, rel_path), "rb") as handle:
            text = handle.read().decode("utf-8")
        for index, line in enumerate(text.splitlines(), start=1):
            match = IO_INCLUDE_RE.match(line)
            if match and "xfoil/io/" in normalize_include_target(
                    match.group(1)):
                messages.append((rel_path, index,
                                 "xfoil::core includes xfoil::io"))
    return messages


SESSION_MANIFEST_RELATIVE_PATH = os.path.join(
    "tools", "dependency-manifests", "session-sources.txt")

# Roots of the session/use-case layer.
SESSION_ROOTS = (
    os.path.join("include", "xfoil", "session"),
    os.path.join("src", "session"),
)

# Documented, allowed public value-type headers of the
# session layer (architecture contract):
# - the neutral value-type header xfoil/SessionTypes.h
# (GeometrySetError/GeometrySetResult),
# - public API value types (AnalysisConfig/AnalysisResult/CancellationToken/
# Types/limits, geometry values),
# - the public session API header (xfoil/XFoilSession.h, only in the facade
# implementation).
# The XCP diagnostics quantity is derived from the confirmed 0-based core
# kernel clCalc (LoadResult.centerOfPressureX, additive loop term).
SESSION_ALLOWED_PUBLIC_HEADERS = (
    "xfoil/SessionTypes.h",
    "xfoil/AnalysisConfig.h",
    "xfoil/AnalysisResult.h",
    "xfoil/CancellationToken.h",
    "xfoil/Types.h",
    "xfoil/Limits.h",
    "xfoil/geometry/AirfoilGeometry.h",
    "xfoil/XFoilSession.h",
)

SESSION_INCLUDE_RE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]')


def classify_session_include(target):
    """Rule for the session layer:
    session -> core/io allowed; session -> app/CLI/tests forbidden."""
    normalized = normalize_include_target(target)
    reasons = []
    if normalized.startswith("tests/") or normalized == "TestSupport.h":
        reasons.append("forbidden Test-Include in xfoil::session")
    if normalized.startswith("app/") or normalized.startswith("cli/"):
        reasons.append("forbidden App-/CLI-Include in xfoil::session")
    first = normalized.split("/")[0]
    if first == "xfoil":
        second = normalized.split("/")[1] if "/" in normalized else ""
        if second not in ("session", "core", "io"):
            if normalized not in SESSION_ALLOWED_PUBLIC_HEADERS:
                reasons.append(
                    "xfoil::session may only include xfoil/session, "
                    "xfoil/core, xfoil/io and the documented public "
                    "value-type headers: %s" % normalized)
    elif first == "XFoilSession.h":
        pass
    return reasons


def check_session_file(path):
    """Check a session file against the layer rules."""
    violations = []
    with open(path, "rb") as handle:
        text = handle.read().decode("utf-8")
    for index, line in enumerate(text.splitlines(), start=1):
        match = SESSION_INCLUDE_RE.match(line)
        if match:
            for reason in classify_session_include(match.group(1)):
                violations.append((index, reason))
    return violations


def check_session_layer(root):
    """manifest, Makefile and include rules for xfoil::session."""
    manifest_path = os.path.join(root, SESSION_MANIFEST_RELATIVE_PATH)
    if not os.path.isfile(manifest_path):
        return [("session manifest", 0,
                 "Error: session manifest not found: %s"
                 % manifest_path)]
    entries = read_manifest(manifest_path)
    seen = set()
    for entry in entries:
        if entry in seen:
            return [("session manifest", 0,
                     "duplicate session entry: %s" % entry)]
        seen.add(entry)
        if not any(entry == r or entry.startswith(r + "/")
                   for r in SESSION_ROOTS):
            return [("session manifest", 0,
                     "session path outside the session roots: %s"
                     % entry)]
        if not os.path.isfile(os.path.join(root, entry)):
            return [("session manifest", 0, "session file missing: %s" % entry)]
    actual = set()
    for session_root in SESSION_ROOTS:
        abs_root = os.path.join(root, session_root)
        if not os.path.isdir(abs_root):
            continue
        for dirpath, _dirnames, filenames in os.walk(abs_root):
            for name in filenames:
                full = os.path.join(dirpath, name)
                rel = os.path.relpath(full, root).replace(os.sep, "/")
                actual.add(rel)
    manifest_set = set(entries)
    for missing in sorted(actual - manifest_set):
        return [("session manifest", 0,
                  "actual session file missing in the manifest: %s" % missing)]
    for extra in sorted(manifest_set - actual):
        return [("session manifest", 0,
                 "session manifest entry does not exist: %s" % extra)]
    makefile_path = os.path.join(root, "Makefile")
    with open(makefile_path, "rb") as handle:
        makefile_text = handle.read().decode("utf-8")
    messages = []
    for entry in entries:
        if not entry.endswith(".cpp"):
            continue
        if entry not in makefile_text:
            messages.append(("session/Makefile", 0,
                             "session source %s is not listed as a "
                             "productive source group in the Makefile"
                             % entry))
    for entry in entries:
        for line_no, message in check_session_file(os.path.join(root, entry)):
            messages.append((entry, line_no, message))
    # Core may not include xfoil/session.
    for rel_path in scan_core_files(root):
        if not rel_path.endswith((".h", ".cpp")):
            continue
        with open(os.path.join(root, rel_path), "rb") as handle:
            text = handle.read().decode("utf-8")
        for index, line in enumerate(text.splitlines(), start=1):
            match = SESSION_INCLUDE_RE.match(line)
            if match and "xfoil/session/" in normalize_include_target(
                    match.group(1)):
                messages.append((rel_path, index,
                                 "xfoil::core includes xfoil::session"))
    return messages


def run_self_test():
    """Negative and positive checker tests in temporary directories.

    Works only in fresh temporary directories, cleans them up in
    every case, and changes no production file. Returns 0 if all
    expectations are met.
    """
    import shutil
    import tempfile

    # (name, files, manifest, expected exit code)
    # files is a list of (relative_path, content). manifest is a
    # list of entries (or None for an empty manifest).
    scenarios = (
        ("allowed core include",
         [("include/xfoil/core/probe.h",
           '#include "xfoil/core/Matrix.h"\nint f() { return 1; }\n')],
         ["include/xfoil/core/probe.h"], 0),
        ("forbidden App-Include",
         [("include/xfoil/core/probe.h",
           '#include "app/DatFileReader.h"\nint f() { return 1; }\n')],
          ["include/xfoil/core/probe.h"], 1),
         ("forbidden Test-Include",
          [("include/xfoil/core/probe.h",
            '#include "TestSupport.h"\nint f() { return 1; }\n')],
          ["include/xfoil/core/probe.h"], 1),
         ("forbidden::XFoil-Use",
         [("include/xfoil/core/probe.h",
           'int f() { return ::XFoil::x; }\n')],
         ["include/xfoil/core/probe.h"], 1),
        ("forbidden unqualified XFoil-Use",
         [("include/xfoil/core/probe.h",
           'int f() { XFoil foil; return 1; }\n')],
         ["include/xfoil/core/probe.h"], 1),
        ("forbidden xfoil::XFoil-Use",
         [("include/xfoil/core/probe.h",
           'int f() { xfoil::XFoil foil; return 1; }\n')],
         ["include/xfoil/core/probe.h"], 1),
        ("XFoil in std::unique_ptr",
         [("include/xfoil/core/probe.h",
           'std::unique_ptr<XFoil> f();\n')],
         ["include/xfoil/core/probe.h"], 1),
        ("longer identifier with XFoil stays allowed",
         [("include/xfoil/core/probe.h",
           'int myXFoilValue = 1;\nint f() { return myXFoilValue; }\n')],
         ["include/xfoil/core/probe.h"], 0),
        ("Comment with forbidden character sequence stays allowed",
         [("include/xfoil/core/probe.h",
           '// XFoil ::XFoil xfoil::XFoil #include "app/x.h"\n'
           'int f() { return 1; }\n')],
         ["include/xfoil/core/probe.h"], 0),
        ("String literal with forbidden character sequence stays allowed",
         [("include/xfoil/core/probe.h",
           'const char* s = "XFoil ::XFoil xfoil::XFoil";\n'
           'int f() { return 1; }\n')],
         ["include/xfoil/core/probe.h"], 0),
        ("missing manifest file",
         [("include/xfoil/core/probe.h", 'int f() { return 1; }\n')],
         ["include/xfoil/core/missing.h"], 2),
        ("additional non-manifested core file",
         [("include/xfoil/core/probe.h", 'int f() { return 1; }\n'),
          ("include/xfoil/core/extra.h", 'int g() { return 2; }\n')],
         ["include/xfoil/core/probe.h"], 2),
        ("duplicate manifest entry",
         [("include/xfoil/core/probe.h", 'int f() { return 1; }\n')],
         ["include/xfoil/core/probe.h", "include/xfoil/core/probe.h"], 2),
        ("../-Traversal",
         [("include/xfoil/core/probe.h", 'int f() { return 1; }\n')],
         ["include/xfoil/core/../core/probe.h"], 2),
        ("absolute manifest path",
         [("include/xfoil/core/probe.h", 'int f() { return 1; }\n')],
         ["/absolute/probe.h"], 2),
        ("symlink from core root to outside",
         [("include/xfoil/core/probe.h", 'int f() { return 1; }\n')],
         ["include/xfoil/core/probe.h"], 2),  # special case: built below
        ("globally mutable static state",
         [("include/xfoil/core/probe.h",
           'static int counter = 0;\nint f() { return ++counter; }\n')],
         ["include/xfoil/core/probe.h"], 1),
        ("global state with leading whitespace",
         [("include/xfoil/core/probe.h",
           '   static double cache = 0.0;\nint f() { return 1; }\n')],
         ["include/xfoil/core/probe.h"], 1),
        ("static thread_local as global state",
         [("include/xfoil/core/probe.h",
           'static thread_local int tls = 0;\nint f() { return tls; }\n')],
         ["include/xfoil/core/probe.h"], 1),
        ("allowed static const",
         [("include/xfoil/core/probe.h",
           'static const int limit = 10;\nint f() { return limit; }\n')],
         ["include/xfoil/core/probe.h"], 0),
        ("allowed static constexpr",
         [("include/xfoil/core/probe.h",
           'static constexpr double pi = 3.14159;\n'
           'double f() { return pi; }\n')],
         ["include/xfoil/core/probe.h"], 0),
        ("static local variable stays allowed",
         [("include/xfoil/core/probe.h",
           'int f() { static int local = 0; return ++local; }\n')],
         ["include/xfoil/core/probe.h"], 0),
        ("core source in Makefile source group present",
         [("src/core/foundation/Matrix.cpp", 'int f() { return 1; }\n'),
          ("Makefile",
           "FOUNDATION_SRCS := "
           "src/core/foundation/Matrix.cpp\n")],
         ["src/core/foundation/Matrix.cpp"], 0),
        ("core source missing in Makefile source group",
         [("src/core/foundation/Matrix.cpp", 'int f() { return 1; }\n'),
          ("Makefile", "UNKNOWN:= x\n")],
         ["src/core/foundation/Matrix.cpp"], 2),
        ("extended core source (geometry_design) present in source group",
         [("src/core/geometry_design/CamberSession.cpp",
           'int f() { return 1; }\n'),
          ("Makefile",
           "GEOMETRY_DESIGN_SRCS := "
           "src/core/geometry_design/CamberSession.cpp\n")],
         ["src/core/geometry_design/CamberSession.cpp"], 0),
        ("extended core source (geometry_design) missing in the Makefile",
         [("src/core/geometry_design/CamberSession.cpp",
           'int f() { return 1; }\n'),
          ("Makefile", "UNKNOWN:= x\n")],
         ["src/core/geometry_design/CamberSession.cpp"], 2),
    )

    failures = 0
    tmpdir = tempfile.mkdtemp(prefix="core-dep-selftest-")
    try:
        for name, files, manifest_entries, expected in scenarios:
            # Path-safe directory names: replace every character except
            # alphanumeric, '-' and '_'. Prevents '..'/'/' interactions.
            safe = re.sub(r'[^A-Za-z0-9_-]', '_', name)
            scenario_dir = os.path.join(tmpdir, safe)
            os.makedirs(scenario_dir)
            for rel_path, content in files:
                full = os.path.join(scenario_dir, rel_path)
                os.makedirs(os.path.dirname(full), exist_ok=True)
                with open(full, "w", encoding="utf-8") as handle:
                    handle.write(content)

            # Special case symlink: probe.h points outside.
            if name == "symlink from core root to outside":
                outside = os.path.join(tmpdir, "outside-target.h")
                with open(outside, "w", encoding="utf-8") as handle:
                    handle.write("int f() { return 1; }\n")
                link = os.path.join(scenario_dir, "include/xfoil/core/probe.h")
                os.remove(link)
                os.symlink(outside, link)

            manifest = os.path.join(scenario_dir, "manifest.txt")
            with open(manifest, "w", encoding="utf-8") as handle:
                for entry in manifest_entries:
                    handle.write(entry + "\n")

            exit_code, _ = check_manifest(scenario_dir, manifest)
            if exit_code != expected:
                failures += 1
                sys.stdout.write(
                    "Self-Test ERROR: %s (expected exit %d, got %d)\n"
                    % (name, expected, exit_code))
            else:
                sys.stdout.write("Self-Test OK: %s\n" % name)
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)

    # Default manifest location tests: the three gate manifests
    # are read exclusively from tools/dependency-manifests/. A manifest
    # at the forbidden historical docs/refactoring/core-migration/ location
    # is never used as a fallback.
    def build_default_root(root):
        files = {
            "include/xfoil/core/probe.h": 'int f() { return 1; }\n',
            "include/xfoil/io/Probe.h":
                '#include "xfoil/core/probe.h"\nint f() { return 1; }\n',
            "src/io/Probe.cpp":
                '#include "xfoil/io/Probe.h"\nint f() { return 1; }\n',
            "include/xfoil/session/Probe.h":
                '#include "xfoil/io/Probe.h"\nint f() { return 1; }\n',
            "src/session/Probe.cpp":
                '#include "xfoil/session/Probe.h"\nint f() { return 1; }\n',
            "Makefile":
                "IO_SRCS := src/io/Probe.cpp\n"
                "SESSION_SRCS := src/session/Probe.cpp\n",
            MANIFEST_RELATIVE_PATH: "include/xfoil/core/probe.h\n",
            IO_MANIFEST_RELATIVE_PATH:
                "include/xfoil/io/Probe.h\nsrc/io/Probe.cpp\n",
            SESSION_MANIFEST_RELATIVE_PATH:
                "include/xfoil/session/Probe.h\nsrc/session/Probe.cpp\n",
        }
        for rel, content in files.items():
            full = os.path.join(root, rel)
            os.makedirs(os.path.dirname(full), exist_ok=True)
            with open(full, "w", encoding="utf-8") as handle:
                handle.write(content)

    def move_manifests_to_old_location(root):
        old_dir = os.path.join(root, "docs", "refactoring", "core-migration")
        os.makedirs(old_dir, exist_ok=True)
        pairs = (
            (MANIFEST_RELATIVE_PATH, "core-source-manifest.txt"),
            (IO_MANIFEST_RELATIVE_PATH, "io-source-manifest.txt"),
            (SESSION_MANIFEST_RELATIVE_PATH, "session-source-manifest.txt"),
        )
        for rel, old_name in pairs:
            full = os.path.join(root, rel)
            with open(full, "rb") as handle:
                data = handle.read()
            with open(os.path.join(old_dir, old_name), "wb") as handle:
                handle.write(data)
            os.remove(full)

    def append_line(rel_path):
        def mutate(root):
            with open(os.path.join(root, rel_path), "a",
                      encoding="utf-8") as handle:
                handle.write(added_lines[rel_path])
        return mutate

    added_lines = {
        IO_MANIFEST_RELATIVE_PATH: "include/xfoil/io/Probe.h\n",
        SESSION_MANIFEST_RELATIVE_PATH: "include/xfoil/session/Probe.h\n",
    }

    def write_manifest(rel_path, content):
        def mutate(root):
            with open(os.path.join(root, rel_path), "w",
                      encoding="utf-8") as handle:
                handle.write(content)
        return mutate

    def create_file(rel_path):
        def mutate(root):
            full = os.path.join(root, rel_path)
            os.makedirs(os.path.dirname(full), exist_ok=True)
            with open(full, "w", encoding="utf-8") as handle:
                handle.write('int g() { return 2; }\n')
        return mutate

    def write_makefile(content):
        def mutate(root):
            with open(os.path.join(root, "Makefile"), "w",
                      encoding="utf-8") as handle:
                handle.write(content)
        return mutate

    # (name, mutation, expected (core_exit, io_violation, session_violation))
    default_scenarios = (
        ("all three default manifests present", None, (0, False, False)),
        ("core default manifest missing",
         lambda r: os.remove(os.path.join(r, MANIFEST_RELATIVE_PATH)),
         (2, False, False)),
        ("io default manifest missing",
         lambda r: os.remove(os.path.join(r, IO_MANIFEST_RELATIVE_PATH)),
         (0, True, False)),
        ("session default manifest missing",
         lambda r: os.remove(
             os.path.join(r, SESSION_MANIFEST_RELATIVE_PATH)),
         (0, False, True)),
        ("old manifest location is not a fallback",
         move_manifests_to_old_location,
         (2, True, True)),
        ("duplicate io entry",
         append_line(IO_MANIFEST_RELATIVE_PATH),
         (0, True, False)),
        ("duplicate session entry",
         append_line(SESSION_MANIFEST_RELATIVE_PATH),
         (0, False, True)),
        ("io manifest entry file missing",
         write_manifest(IO_MANIFEST_RELATIVE_PATH,
                        "include/xfoil/io/Probe.h\nsrc/io/Probe.cpp\n"
                        "include/xfoil/io/Gone.h\n"),
         (0, True, False)),
        ("session manifest entry file missing",
         write_manifest(SESSION_MANIFEST_RELATIVE_PATH,
                        "include/xfoil/session/Probe.h\n"
                        "src/session/Probe.cpp\n"
                        "include/xfoil/session/Gone.h\n"),
         (0, False, True)),
        ("additional non-manifested io file",
         create_file("include/xfoil/io/Extra.h"),
         (0, True, False)),
        ("additional non-manifested session file",
         create_file("include/xfoil/session/Extra.h"),
         (0, False, True)),
        ("io source missing in Makefile source group",
         write_makefile("SESSION_SRCS := src/session/Probe.cpp\n"),
         (0, True, False)),
        ("session source missing in Makefile source group",
         write_makefile("IO_SRCS := src/io/Probe.cpp\n"),
         (0, False, True)),
    )

    default_failures = 0
    default_tmpdir = tempfile.mkdtemp(prefix="default-manifest-selftest-")
    try:
        for name, mutate, expected in default_scenarios:
            safe = re.sub(r'[^A-Za-z0-9_-]', '_', name)
            scenario_dir = os.path.join(default_tmpdir, safe)
            os.makedirs(scenario_dir)
            build_default_root(scenario_dir)
            if mutate is not None:
                mutate(scenario_dir)
            core_exit, _ = check_manifest(
                scenario_dir,
                os.path.join(scenario_dir, MANIFEST_RELATIVE_PATH))
            io_msgs = check_io_layer(scenario_dir)
            session_msgs = check_session_layer(scenario_dir)
            got = (core_exit, bool(io_msgs), bool(session_msgs))
            if got != expected:
                default_failures += 1
                sys.stdout.write(
                    "Self-Test ERROR: %s (expected %s, got %s)\n"
                    % (name, expected, got))
            else:
                sys.stdout.write("Self-Test OK: %s\n" % name)
    finally:
        shutil.rmtree(default_tmpdir, ignore_errors=True)

    # Scope-level gate tests: the durable scope spellings stay green.
    scope_failures = 0
    scope_tmpdir = tempfile.mkdtemp(prefix="scope-selftest-")

    def fresh_scope_root():
        root = tempfile.mkdtemp(prefix="scope-root-", dir=scope_tmpdir)
        build_default_root(root)
        return root

    def run_main(argv):
        try:
            return main(argv)
        except SystemExit as exc:  # argparse controlled argument error
            return exc.code

    try:
        # (name, extra arguments, expected return code)
        scope_cases = (
            ("session scope green", ["--scope=session"], 0),
            ("cli scope green", ["--scope=cli"], 0),
            ("full scope green", ["--scope=full"], 0),
        )
        for name, extra, expected in scope_cases:
            root = fresh_scope_root()
            got = run_main(["--root", root] + extra)
            if got != expected:
                scope_failures += 1
                sys.stdout.write(
                    "Self-Test ERROR: %s (expected %d, got %s)\n"
                    % (name, expected, got))
            else:
                sys.stdout.write("Self-Test OK: %s\n" % name)

        # Forbidden session include: an app include in a session source.
        root = fresh_scope_root()
        with open(os.path.join(root, "src", "session", "Bad.cpp"), "w",
                  encoding="utf-8") as handle:
            handle.write('#include "app/DatFileReader.h"\n'
                         'int f() { return 1; }\n')
        with open(os.path.join(root, SESSION_MANIFEST_RELATIVE_PATH), "a",
                  encoding="utf-8") as handle:
            handle.write("src/session/Bad.cpp\n")
        session_layer_msgs = check_session_layer(root)
        if not any("App-" in message
                   for _r, _l, message in session_layer_msgs):
            scope_failures += 1
            sys.stdout.write("Self-Test ERROR: forbidden session include "
                             "not reported\n")
        else:
            sys.stdout.write("Self-Test OK: forbidden session include "
                             "reported\n")
    finally:
        shutil.rmtree(scope_tmpdir, ignore_errors=True)

    return 1 if (failures or default_failures
                 or scope_failures) else 0


def default_root():
    """Repository root: parent directory of tools/."""
    script_dir = os.path.dirname(os.path.abspath(__file__))
    return os.path.dirname(script_dir)


def main(argv):
    parser = argparse.ArgumentParser(
        description="Dependency check for the new xfoil::core.")
    parser.add_argument(
        "--root", default=None,
        help="repository root (default: parent directory of tools/).")
    parser.add_argument(
        "--manifest", default=None,
        help="path to the core source manifest (default: %s relative to the "
             "root)." % MANIFEST_RELATIVE_PATH)
    parser.add_argument(
        "--self-test", action="store_true",
        help="run the negative and positive checker tests in a "
              "temporary directory.")
    parser.add_argument(
        "--scope", default="full",
        choices=("full", "session", "cli"),
        help="check scope: 'full' = all layer gates (core manifest and "
              "dependencies, io layer, session layer; default), "
              "'session' = the same layer gates reported with the session "
              "layer as the deepest checked layer, 'cli' = the same layer "
              "gates reported from the CLI-facing perspective. All scopes "
              "run the current layer gates.")
    args = parser.parse_args(argv)

    if args.self_test:
        return run_self_test()

    scope = args.scope

    root = args.root if args.root is not None else default_root()
    if args.manifest is not None:
        manifest_path = args.manifest
    else:
        manifest_path = os.path.join(root, MANIFEST_RELATIVE_PATH)

    exit_code, messages = check_manifest(root, manifest_path)

    for rel_path, line_no, message in messages:
        if exit_code == 2:
            sys.stderr.write("%s\n" % message)
        else:
            sys.stdout.write("%s:%d: %s\n" % (rel_path, line_no, message))

    io_messages = check_io_layer(root)
    for rel_path, line_no, message in io_messages:
        sys.stdout.write("io:%s:%d: %s\n" % (rel_path, line_no, message))

    session_messages = check_session_layer(root)
    for rel_path, line_no, message in session_messages:
        sys.stdout.write("session:%s:%d: %s\n"
                         % (rel_path, line_no, message))

    if exit_code == 1 or io_messages or session_messages:
        sys.stdout.write(
            "Dependency check: %d %s.\n"
            % (len(messages) + len(io_messages) + len(session_messages),
               "violation" if (len(messages) + len(io_messages)
                               + len(session_messages)) == 1
               else "violations"))
        return 1
    if exit_code == 2:
        return 2

    if scope == "session":
        # Session gate: the session-layer include and manifest rules.
        sys.stdout.write(
            "session dependency check: OK (core, io and session layer "
            "gates green).\n")
        return 0

    if scope == "cli":
        # CLI-facing gate: the layer gates that keep the CLI layer free
        # of inbound core/io/session dependencies.
        sys.stdout.write(
            "CLI dependency check: OK (core, io and session layer "
            "gates green).\n")
        return 0

    sys.stdout.write(
        "Dependency check: OK (no forbidden dependency, "
        "manifest complete, all layer gates green).\n")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
