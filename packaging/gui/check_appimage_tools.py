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

"""Verify the pinned AppImage tool set without downloading anything."""

import argparse
import hashlib
import pathlib
import sys

EXPECTED = {
    "linuxdeploy-x86_64.AppImage": (
        "c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d"),
    "linuxdeploy-plugin-qt-x86_64.AppImage": (
        "cfc1055b2b9dbc08412b579f20990b7b41a17b61beaa5847dc9477c96c9e9617"),
    "appimagetool-x86_64.AppImage": (
        "ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0"),
    "runtime-x86_64": (
        "2fca8b443c92510f1483a883f60061ad09b46b978b2631c807cd873a47ec260d"),
}


class ToolError(Exception):
    pass


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def verify_tools(tools_dir, expected=None):
    root = pathlib.Path(tools_dir)
    if not root.is_dir():
        raise ToolError(f"AppImage tools directory not found: {root}")
    hashes = EXPECTED if expected is None else expected
    for name, wanted in sorted(hashes.items()):
        path = root / name
        if not path.is_file():
            raise ToolError(f"required AppImage tool is missing: {path}")
        actual = sha256(path)
        if actual != wanted:
            raise ToolError(
                f"SHA-256 mismatch for {path}\n"
                f"  expected {wanted}\n"
                f"  actual   {actual}")
    return root


def main():
    parser = argparse.ArgumentParser(
        description="Verify the pinned XFOIL.next AppImage tool set")
    parser.add_argument("--tools-dir", required=True)
    args = parser.parse_args()
    try:
        root = verify_tools(args.tools_dir)
    except ToolError as error:
        print(f"check_appimage_tools.py: error: {error}", file=sys.stderr)
        return 1
    print(f"AppImage tools: PASS ({root})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
