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

# File: tools/extract-fortran-routines.py
# Extracts named SUBROUTINE blocks byte-faithfully from a canonical
# Fortran source file (fixed form) and writes them below build/.
#
# Purpose: the geometry harnesses need ROTATE and
# TECALC from fortran-src/xfoil.f plus the required xgeom routines.
# fortran-src/xfoil.f contains PROGRAM XFOIL and xgeom.f contains PLTCRV
# with references to a plot library not contained in fortran-src;
# neither is called in the harness and neither may be linked.
# Instead of unused routines with external dependencies, only the
# actually called routine blocks are extracted byte-faithfully.
#
# Behavior:
# - Routine head: a line that (after leading whitespace) begins with
# "SUBROUTINE <Name>" (in fixed form, comment lines begin with
# 'C'/'*' in column 1 and are not matched).
# - Block end: the next line whose directive is exactly END (optionally
# followed by a comment); ENDIF/END IF/ENDDO/END DO do not match.
# - Extraction is byte-faithful (source bytes of the block lines
# including line endings, in the order of the --routines list).
# - Failure (exit 1) for a missing, duplicate or ambiguous routine,
# as well as for a block without END.
# - Documentation of the extracted source lines in a companion file
# (<output>.lines.txt) and on stdout.
# - No Fortran source file is changed.

import argparse
import os
import re
import sys

HEAD_RE = re.compile(
    r"^[ \t]*(?:SUBROUTINE|"
    r"(?:LOGICAL|INTEGER|REAL|DOUBLE[ \t]+PRECISION)[ \t]+FUNCTION|"
    r"FUNCTION)"
    r"[ \t]+([A-Za-z][A-Za-z0-9_]*)",
    re.IGNORECASE)
END_RE = re.compile(r"^[ \t]*END[ \t]*(!.*)$|^[ \t]*END[ \t]*$", re.IGNORECASE)


def fail(message):
    sys.stderr.write("Error: %s\n" % message)
    sys.exit(1)


def find_blocks(lines, source):
    """Return a list of (name_upper, start, end) with 0-based block
    bounds (END line included) for every SUBROUTINE block."""
    blocks = []
    index = 0
    while index < len(lines):
        match = HEAD_RE.match(lines[index])
        if match is None:
            index += 1
            continue
        name = match.group(1).upper()
        end = None
        probe = index + 1
        while probe < len(lines):
            if END_RE.match(lines[probe]) is not None:
                end = probe
                break
            probe += 1
        if end is None:
            fail("Routine '%s' in %s has no END." % (name, source))
        blocks.append((name, index, end))
        index = end + 1
    return blocks


def main():
    parser = argparse.ArgumentParser(
        description="Extract named SUBROUTINE blocks byte-faithfully from "
                    "a canonical Fortran source file.")
    parser.add_argument("--source", required=True,
                        help="canonical Fortran-Source file (only read)")
    parser.add_argument("--routines", required=True, nargs="+",
                        help="routine names to extract")
    parser.add_argument("--output", required=True,
                        help="Output file below build/ (byte-faithful Blocks)")
    args = parser.parse_args()

    if not os.path.isfile(args.source):
        fail("Source file not found: %s" % args.source)

    with open(args.source, "rb") as handle:
        data = handle.read()
    lines = data.splitlines(keepends=True)
    text_lines = [line.decode("latin-1") for line in lines]

    blocks = find_blocks(text_lines, args.source)
    by_name = {}
    for name, start, end in blocks:
        by_name.setdefault(name, []).append((start, end))

    extracted = []
    for requested in args.routines:
        key = requested.upper()
        found = by_name.get(key, [])
        if len(found) == 0:
            fail("Routine '%s' in %s not found." % (requested, args.source))
        if len(found) > 1:
            positions = ", ".join("%d-%d" % (start + 1, end + 1)
                                  for start, end in found)
            fail("Routine '%s' in %s ambiguous/duplicate (%s)."
                 % (requested, args.source, positions))
        start, end = found[0]
        extracted.append((key, start, end))

    out_dir = os.path.dirname(args.output)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)

    # Byte-faithful block chain in the order of the --routines list.
    payload = b"".join(b"".join(lines[start:end + 1])
                       for _name, start, end in extracted)
    with open(args.output, "wb") as handle:
        handle.write(payload)

    # Companion file with the extracted source lines (1-based, inclusive).
    lines_doc = ["Source: %s" % args.source]
    for name, start, end in extracted:
        lines_doc.append("%s: Lines %d-%d" % (name, start + 1, end + 1))
    lines_doc.append("block bytes: %d" % len(payload))
    doc_path = args.output + ".lines.txt"
    with open(doc_path, "w", newline="\n") as handle:
        handle.write("\n".join(lines_doc) + "\n")

    print("Extraction OK: %s" % args.output)
    for name, start, end in extracted:
        print("  %s: Lines %d-%d" % (name, start + 1, end + 1))
    print("Lines documentation: %s" % doc_path)


if __name__ == "__main__":
    main()
