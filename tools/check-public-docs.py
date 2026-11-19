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

# Non-writing consistency gate for the permanent public documentation.
# It checks that the public documents exist, contain no internal migration
# references, no removed-contract terms, no stale counts, no German prose,
# and that their relative links and referenced paths resolve.
#
# Exit codes: 0 = all checks pass, 1 = a check failed, 2 = usage/setup error.

import argparse
import os
import re
import sys

PUBLIC_DOCS = [
    "README.md",
    "CONTRIBUTING.md",
    "SECURITY.md",
    "CHANGELOG.md",
    "docs/architecture.md",
    "docs/building-and-testing.md",
    "docs/cli.md",
    "docs/batch-mode.md",
    "docs/public-api.md",
    "docs/numerical-validation.md",
    "docs/fortran-reference.md",
    "docs/known-differences.md",
    "fortran-src/xfoil-6.99/README.md",
    "fortran-bin/README.txt",
    "tests/fortran-reference/README.md",
    "tests/golden/metadata.md",
    "sample/README.md",
]

# Substrings that must never appear in public documentation.
FORBIDDEN_SUBSTRINGS = [
    "docs/refactoring",  # forbidden historical internal path
    "OpenSourceMigration",  # forbidden historical internal path
    "OSM-",
    "Deperrois",
    "BatchApplication",
    "clark_y_polar.csv",
    "converged after",
    "alpha : ",
    "migration in progress",
    "core-unit",  # forbidden internal target name
]

# Stale point-in-time counts that must not be presented as current.
FORBIDDEN_STALE = [
    "1831", "1580", "1625", "1909", "1919", "1876",
    "232/232", "64/64", "64/65", "46/46",
]

# Old command-line option token for the removed batch path.
FORBIDDEN_TOKENS = ["--job"]

# German umlauts and unambiguous German function/domain words.
GERMAN_CHARS = set("äöüÄÖÜß")
GERMAN_WORDS = [
    "und", "nicht", "ist", "sind", "diese", "dieser", "dieses",
    "der", "die", "das", "ein", "eine", "kein", "keine",
    "auch", "oder", "aber", "wenn", "dann", "noch", "sehr",
    "kann", "muss", "soll", "will", "für", "mit", "von", "auf",
    "siehe", "gemäß", "laut", "abschnitt", "tabelle", "datei",
    "prüfung", "fälle", "fall", "zählung", "verzeichnis", "bezug",
    "unten", "oben", "hier", "dort", "werden", "wurde", "haben",
    "hat", "war", "wird", "kann", "gegen", "zwischen", "innerhalb",
    "außerhalb", "unverändert", "kanonisch", "referenz",
]

C_STEP_RE = re.compile(r"\bC\d{2}")
MD_LINK_RE = re.compile(r"\[([^\]]*)\]\(([^)]+)\)")
BACKTICK_PATH_RE = re.compile(r"`([A-Za-z0-9_./-]+)`")
PATH_PREFIXES = (
    "docs/", "fortran-src/", "fortran-bin/", "tests/", "sample/",
    "include/", "src/", "tools/",
)


def check_file(root, rel, problems):
    path = os.path.join(root, rel)
    if not os.path.isfile(path):
        problems.append(f"{rel}: missing public document")
        return
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        text = handle.read()

    for token in FORBIDDEN_SUBSTRINGS:
        if token in text:
            problems.append(f"{rel}: forbidden internal/removed term: {token!r}")

    for token in FORBIDDEN_TOKENS:
        if token in text:
            problems.append(f"{rel}: forbidden removed option: {token!r}")

    for token in FORBIDDEN_STALE:
        if token in text:
            problems.append(f"{rel}: stale count presented as current: {token!r}")

    for match in C_STEP_RE.finditer(text):
        problems.append(
            f"{rel}: internal migration step term: {match.group(0)!r}"
        )

    for line_no, line in enumerate(text.splitlines(), start=1):
        bad_chars = sorted(set(line) & GERMAN_CHARS)
        if bad_chars:
            problems.append(
                f"{rel}:{line_no}: German characters: "
                + " ".join(bad_chars)
            )
        for word in GERMAN_WORDS:
            if re.search(rf"(?<![0-9A-Za-z]){re.escape(word)}(?![0-9A-Za-z])",
                         line):
                problems.append(
                    f"{rel}:{line_no}: German word: {word!r}"
                )

    base_dir = os.path.dirname(path)
    for _label, target in MD_LINK_RE.findall(text):
        check_link(root, base_dir, rel, target, problems)

    for token in BACKTICK_PATH_RE.findall(text):
        if token.startswith(PATH_PREFIXES) and "<" not in token \
                and "*" not in token and "." in token:
            if not os.path.exists(os.path.join(root, token)):
                problems.append(f"{rel}: referenced path missing: {token!r}")


def check_link(root, base_dir, rel, target, problems):
    if target.startswith(("http://", "https://", "mailto:", "#")):
        return
    if "://" in target:
        return
    path_part = target.split("#", 1)[0]
    if not path_part:
        return
    if path_part.startswith("/"):
        problems.append(f"{rel}: absolute link path: {target!r}")
        return
    resolved = os.path.normpath(os.path.join(base_dir, path_part))
    if not os.path.exists(resolved):
        problems.append(f"{rel}: broken relative link: {target!r}")


def main(argv):
    parser = argparse.ArgumentParser(
        description="Check the permanent public documentation."
    )
    parser.add_argument(
        "--root", default=".", help="repository root (default: current dir)"
    )
    args = parser.parse_args(argv[1:])

    root = os.path.abspath(args.root)
    if not os.path.isdir(root):
        print(f"ERROR: not a directory: {root}", file=sys.stderr)
        return 2

    problems = []
    for rel in PUBLIC_DOCS:
        check_file(root, rel, problems)

    if problems:
        for problem in problems:
            print(f"FAIL: {problem}")
        print(f"Public documentation gate: {len(problems)} problem(s)")
        return 1

    print(
        "Public documentation gate: OK "
        f"({len(PUBLIC_DOCS)} public documents checked)"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
