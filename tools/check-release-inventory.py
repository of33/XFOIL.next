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

# file: tools/check-release-inventory.py
# Read-only gate for the public release inventory.
#
# Checks the public release inventory:
# - required files present,
# - dependency-gate manifests present at tools/dependency-manifests/ and
#   no reappearing copies at the forbidden historical docs/refactoring location,
# - no root runtime artifacts,
# - no cache artifacts outside ignored build directories,
# - OSMAP selection (active/inactive) and its SHA integrity,
# - osmap_DP.f in no productive source group / no builder,
# - the seven .bl references present and not hidden by .gitignore,
# - no missing IoTests reference path,
# - file modes (no unexpected executable bits; expected tools/binaries
# executable).
#
# The tool changes no file. It only reads.
#
# Invocation:
# tools/check-release-inventory.py [--root DIR]
#
# Exit: 0 = inventory in order, 1 = violation, 2 = invocation/setup error.

import argparse
import fnmatch
import hashlib
import os
import re
import stat
import sys


EXPECTED_OSMAP_SHA = {
    "fortran-src/xfoil-6.99/osmap.f":
        "3c806b1d5bd62b80d2a2180b93ea2db95cb5f56c5ec7e55c07868fa20065e719",
    "fortran-src/xfoil-6.99/osmap.dat":
        "744d0e5a6948de9a5864a04177735d9620ccbbcb1abcb8fd1af284fd52a9b949",
    "fortran-src/xfoil-6.99/osmap_DP.f":
        "175c83c50ee53a08c03b70f6a5053048fcd6dac5f70ffd50cb216506ef7ddda9",
    "fortran-src/xfoil-6.99/osmapDP.dat":
        "2f6b3c63461d71da9b6cb9ca1340d77cff0cfbe767679d15b5b8556b45d948c4",
}

REQUIRED_FILES = [
    "LICENSE",
    "NOTICE",
    "fortran-src/xfoil-6.99/SHA256SUMS",
    "fortran-bin/xfoil",
    "sample/CLARK_Y.dat",
    "sample/NACA2312.dat",
]

# Permanent dependency-gate manifests. They are inputs of
# tools/check-core-dependencies.py and must stay at this neutral,
# phase-free location.
DEPENDENCY_MANIFESTS = [
    "tools/dependency-manifests/core-sources.txt",
    "tools/dependency-manifests/io-sources.txt",
    "tools/dependency-manifests/session-sources.txt",
]

# Historical manifest location. The manifests were relocated out of the
# documentation tree; reappearing copies at the old location would create
# a second, non-authoritative copy and are rejected.
FORBIDDEN_OLD_DEPENDENCY_MANIFESTS = [
    "docs/refactoring/core-migration/core-source-manifest.txt",  # forbidden historical path
    "docs/refactoring/core-migration/io-source-manifest.txt",  # forbidden historical path
    "docs/refactoring/core-migration/session-source-manifest.txt",  # forbidden historical path
]

# Runtime artifacts in the repository root (fnmatch patterns).
FORBIDDEN_ROOT_PATTERNS = [
    "CLARK_Y.cp",
    "PGET",
    "PWRT",
    "fort.*",
    ":00.bl",   # POSIX runtime residue: XFOIL 6.99 VISCAL (xoper.f:3061-3077)
    "100.bl",   # portable name of the same BL residue (PUB-W1)
    "plot.ps",
    "xplot11_logfile",
    "xfoil.def",
    "*.pol",
    "*.dump",
    "*.cp",
    "transcript.txt",
    "results.dat",
]

BL_REFERENCES = [
    "tests/data/core-polar-io-reference/bldump_case_reference.bl",
    "tests/data/core-polar-io-reference/bldump_case_csv_reference.bl",
    "tests/data/core-polar-io-reference/bldump_case_tab_reference.bl",
    "tests/data/core-polar-io-reference/bldump_case_bad_reference.bl",
    "tests/data/core-polar-io-reference/bldump_case_inviscid_reference.bl",
    "tests/data/core-polar-io-reference/bldump2_case_reference.bl",
    "tests/data/core-polar-io-reference/bldump2_case_inviscid_reference.bl",
]

# directory names skipped during tree traversals.
SKIP_DIRS = {".git", ".git_custom", ".agents", "build"}

# Expected executable files (must have the executable bit set).
EXPECTED_EXECUTABLES = [
    "fortran-bin/xfoil",
]


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()


def is_executable(path):
    mode = os.stat(path).st_mode
    return bool(mode & (stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH))


def has_shebang(path):
    try:
        with open(path, "rb") as fh:
            return fh.read(2) == b"#!"
    except OSError:
        return False


def is_binary(path):
    try:
        with open(path, "rb") as fh:
            chunk = fh.read(4096)
        return b"\0" in chunk
    except OSError:
        return False


_COMPILER_RE = re.compile(r"\$\{?FC\}?|\bgfortran\b")


def _read(path):
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        return fh.read()


def _strip_makefile_comments(text):
    """Makefile lines with comments stripped ('#' until end of line)."""
    out = []
    for line in text.splitlines():
        idx = line.find("#")
        if idx != -1:
            line = line[:idx]
        out.append(line)
    return out


def _strip_shell_comments(text):
    """Strip bash comments: '#' at the line start or after whitespace
    begins a comment."""
    out = []
    for line in text.splitlines():
        cut = len(line)
        for i, c in enumerate(line):
            if c == "#" and (i == 0 or line[i - 1] in " \t"):
                cut = i
                break
        out.append(line[:cut])
    return out



# --- minimal .gitignore semantics (last-match-wins, without Git) -------------

def _pattern_to_regex(pattern):
    """Translate a .gitignore pattern into a regular expression.

    Returns (negate, regex, anchored). Supported leading '!',
    leading '/', trailing '/' (directories only), '*', '?'
    and '**'. '*' and '?' never match a '/'.
    """
    negate = False
    if pattern.startswith("!"):
        negate = True
        pattern = pattern[1:]
    if pattern.endswith("/"):
        pattern = pattern[:-1]
        dir_only = True
    else:
        dir_only = False
    anchored = "/" in pattern
    if pattern.startswith("/"):
        pattern = pattern[1:]

    regex = ""
    i = 0
    n = len(pattern)
    while i < n:
        c = pattern[i]
        if c == "*":
            if i + 1 < n and pattern[i + 1] == "*":
                # '**' -> matches all incl.. '/'
                if i + 2 < n and pattern[i + 2] == "/":
                    regex += "(?:.*/)?"
                    i += 3
                    continue
                regex += ".*"
                i += 2
                continue
            regex += "[^/]*"
            i += 1
            continue
        if c == "?":
            regex += "[^/]"
            i += 1
            continue
        regex += re.escape(c)
        i += 1

    if dir_only:
        regex += "(?:/.*)?$"
    else:
        regex += "$"

    if anchored:
        full = "^" + regex
    else:
        full = "^(?:.*/)?" + regex
    return negate, re.compile(full)


def load_gitignore(root):
    path = os.path.join(root, ".gitignore")
    patterns = []
    if os.path.isfile(path):
        with open(path, "r", encoding="utf-8", errors="replace") as fh:
            for raw in fh:
                line = raw.rstrip("\n").rstrip("\r")
                if not line or line.startswith("#"):
                    continue
                patterns.append(_pattern_to_regex(line))
    return patterns


def is_ignored(relpath, patterns):
    """True if the file would be ignored by the .gitignore patterns."""
    rel = relpath.replace(os.sep, "/")
    ignored = False
    for negate, rx in patterns:
        if rx.match(rel):
            ignored = not negate
    return ignored


# --- Checks -----------------------------------------------------------

class Checker:
    def __init__(self, root):
        self.root = root
        self.failures = []
        self.notes = []

    def fail(self, msg):
        self.failures.append(msg)

    def note(self, msg):
        self.notes.append(msg)

    def p(self, rel):
        return os.path.join(self.root, rel)

    def check_required_files(self):
        for rel in REQUIRED_FILES:
            if not os.path.isfile(self.p(rel)):
                self.fail("required file missing: %s" % rel)

    def check_dependency_manifests(self):
        for rel in DEPENDENCY_MANIFESTS:
            if not os.path.isfile(self.p(rel)):
                self.fail("dependency manifest missing: %s" % rel)
        for rel in FORBIDDEN_OLD_DEPENDENCY_MANIFESTS:
            if os.path.exists(self.p(rel)):
                self.fail("old dependency manifest reappeared: %s" % rel)

    def check_forbidden_root_artifacts(self):
        try:
            entries = os.listdir(self.root)
        except OSError as exc:
            self.fail("Root not readable: %s" % exc)
            return
        for name in entries:
            full = os.path.join(self.root, name)
            if not os.path.isfile(full):
                continue
            for pat in FORBIDDEN_ROOT_PATTERNS:
                if fnmatch.fnmatch(name, pat):
                    self.fail("forbidden runtime artifact in the "
                              "repository root: %s" % name)
                    break

    def check_cache_artifacts(self):
        for dirpath, dirnames, filenames in os.walk(self.root):
            rel_dir = os.path.relpath(dirpath, self.root)
            rel_parts = [] if rel_dir == "." else rel_dir.split(os.sep)
            if any(part in SKIP_DIRS for part in rel_parts):
                dirnames[:] = []
                continue
            if "__pycache__" in dirnames:
                self.fail("cache directory __pycache__: %s"
                          % os.path.join(rel_dir, "__pycache__"))
            for fn in filenames:
                if fn.endswith((".pyc", ".pyo")):
                    self.fail("cache artifact: %s"
                              % os.path.join(rel_dir, fn))

    def check_osmap(self):
        for rel, expected in EXPECTED_OSMAP_SHA.items():
            full = self.p(rel)
            if not os.path.isfile(full):
                self.fail("OSMAP artifact missing: %s" % rel)
                continue
            actual = sha256_file(full)
            if actual != expected:
                self.fail("OSMAP-SHA wrong: %s (expected %s, found %s)"
                          % (rel, expected, actual))
        # Negative: osmap_DP.f may not be compiled in any productive
        # source group and in no harness builder. Comments and explicit
        # skip lists (guards) are allowed; only an actual
        # compilation/rule reference is recognized (compiler token on the
        # same line, non-comment in the Makefile).
        makefile = self.p("Makefile")
        if os.path.isfile(makefile):
            for line in _strip_makefile_comments(_read(makefile)):
                if "osmap_DP" in line:
                    self.fail("osmap_DP in Makefile source group/rule: %s"
                              % line.strip())
        tools_dir = self.p("tools")
        if os.path.isdir(tools_dir):
            for fn in sorted(os.listdir(tools_dir)):
                if fn.startswith("build-") and fn.endswith("-harnesses.sh"):
                    for line in _strip_shell_comments(
                            _read(os.path.join(tools_dir, fn))):
                        if "osmap_DP" in line and _COMPILER_RE.search(line):
                            self.fail("osmap_DP compilation in harness builder:"
                                      " tools/%s: %s" % (fn, line.strip()))
        if not os.path.isfile(self.p("tools/check-osmap-selection.sh")):
            self.fail("tools/check-osmap-selection.sh missing")

    def check_bl_references(self):
        patterns = load_gitignore(self.root)
        for rel in BL_REFERENCES:
            if not os.path.isfile(self.p(rel)):
                self.fail(".bl reference missing: %s" % rel)
                continue
            if is_ignored(rel, patterns):
                self.fail(".bl reference hidden by .gitignore: %s" % rel)

    def check_iotests_references(self):
        iotests = self.p("tests/core/IoTests.cpp")
        if not os.path.isfile(iotests):
            self.fail("tests/core/IoTests.cpp missing")
            return
        with open(iotests, "r", encoding="utf-8", errors="replace") as fh:
            text = fh.read()
        refs = set(re.findall(r'"(tests/[A-Za-z0-9_./+:-]+\.[A-Za-z0-9]+)"',
                              text))
        for rel in sorted(refs):
            if not os.path.isfile(self.p(rel)):
                self.fail("missing IoTests reference path: %s" % rel)

    def check_modes(self):
        # Expected executables must be executable.
        for rel in EXPECTED_EXECUTABLES:
            full = self.p(rel)
            if os.path.isfile(full) and not is_executable(full):
                self.fail("expected file not executable: %s" % rel)
        # All tools/*.sh must be executable (direct invocation).
        tools_dir = self.p("tools")
        if os.path.isdir(tools_dir):
            for fn in sorted(os.listdir(tools_dir)):
                if fn.endswith(".sh"):
                    full = os.path.join(tools_dir, fn)
                    if os.path.isfile(full) and not is_executable(full):
                        self.fail("shell tool not executable: tools/%s"
                                  % fn)
        # No unexpected executable bits: an executable file must
        # be a binary or a shebang script.
        for dirpath, dirnames, filenames in os.walk(self.root):
            rel_dir = os.path.relpath(dirpath, self.root)
            rel_parts = [] if rel_dir == "." else rel_dir.split(os.sep)
            if any(part in SKIP_DIRS for part in rel_parts):
                dirnames[:] = []
                continue
            for fn in filenames:
                full = os.path.join(dirpath, fn)
                if not os.path.isfile(full) or os.path.islink(full):
                    continue
                if not is_executable(full):
                    continue
                if is_binary(full) or has_shebang(full):
                    continue
                rel = os.path.join(rel_dir, fn) if rel_dir != "." else fn
                self.fail("unexpected executable bit: %s" % rel)

    def run(self):
        self.check_required_files()
        self.check_dependency_manifests()
        self.check_forbidden_root_artifacts()
        self.check_cache_artifacts()
        self.check_osmap()
        self.check_bl_references()
        self.check_iotests_references()
        self.check_modes()
        return self.failures


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Non-mutating release inventory gate.")
    parser.add_argument("--root", default=None,
                        help="repository root (default: parent of tools/)")
    args = parser.parse_args(argv)

    if args.root is None:
        root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    else:
        root = os.path.abspath(args.root)
    if not os.path.isdir(root):
        print("ERROR: root directory not found: %s" % root,
              file=sys.stderr)
        return 2

    checker = Checker(root)
    failures = checker.run()

    if failures:
        print("Release-Inventory: ERROR (%d)" % len(failures))
        for f in failures:
            print("  - %s" % f)
        return 1
    print("Release-Inventory: OK (root: %s)" % root)
    return 0


if __name__ == "__main__":
    sys.exit(main())
