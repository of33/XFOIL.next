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

"""Tests for the GUI packaging infrastructure."""

import hashlib
import importlib.util
import pathlib
import sys
import tempfile
import unittest
import zipfile

ROOT = pathlib.Path(__file__).resolve().parents[3]
GUI = ROOT / "packaging/gui"


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


TOOLS = load("check_appimage_tools", GUI / "check_appimage_tools.py")
WINDOWS = load("package_gui_windows",
               GUI / "windows/package_gui_windows.py")
CHECK_ZIP = load("check_gui_windows",
                 GUI / "windows/check_gui_windows.py")

REQUIRED = set(WINDOWS.REQUIRED_FILES)


def fill_required(root):
    for relative in REQUIRED:
        path = root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        if relative == "xfoil.next-gui.exe":
            path.write_bytes(b"MZ1.2.1\x00Analysis")
        else:
            path.write_bytes(b"test\n")


class ToolTests(unittest.TestCase):
    def test_missing_tool_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            expected = {"missing.AppImage": "0" * 64}
            with self.assertRaises(TOOLS.ToolError):
                TOOLS.verify_tools(temp, expected)

    def test_hash_mismatch_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            path = pathlib.Path(temp) / "tool.AppImage"
            path.write_bytes(b"not the pinned tool")
            expected = {"tool.AppImage": "0" * 64}
            with self.assertRaises(TOOLS.ToolError):
                TOOLS.verify_tools(temp, expected)

    def test_matching_hash_is_accepted(self):
        with tempfile.TemporaryDirectory() as temp:
            path = pathlib.Path(temp) / "tool.AppImage"
            path.write_bytes(b"pinned")
            expected = {
                "tool.AppImage": hashlib.sha256(b"pinned").hexdigest()}
            self.assertEqual(TOOLS.verify_tools(temp, expected), pathlib.Path(temp))


class WindowsPackageTests(unittest.TestCase):
    def test_deterministic_zip(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp) / "stage"
            fill_required(root)
            (root / "platforms").mkdir(exist_ok=True)
            (root / "platforms/qwindows.dll").write_bytes(b"dll")
            a = pathlib.Path(temp) / "a.zip"
            b = pathlib.Path(temp) / "b.zip"
            top = "XFOIL.next-GUI-1.2.1-local-g8-windows-x86_64"
            WINDOWS.write_zip(root, a, top, 1700000000)
            WINDOWS.write_zip(root, b, top, 1700000000)
            self.assertEqual(a.read_bytes(), b.read_bytes())
            with zipfile.ZipFile(a) as archive:
                names = archive.namelist()
                files = [name for name in names if not name.endswith("/")]
                self.assertEqual(files, sorted(files))
                self.assertTrue(all(name.startswith(top + "/") for name in names))
                self.assertIn(top + "/xfoil.next-gui.exe", names)

    def test_forbidden_cli_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp) / "stage"
            fill_required(root)
            (root / "xfoil.next.exe").write_bytes(b"MZ")
            with self.assertRaises(WINDOWS.PackageError):
                WINDOWS.check_tree(root, "1.2.1")

    def test_archive_check_accepts_required_layout(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp) / "stage"
            fill_required(root)
            archive = pathlib.Path(temp) / "gui.zip"
            top = "XFOIL.next-GUI-1.2.1-local-g8-windows-x86_64"
            WINDOWS.write_zip(root, archive, top, 1700000000)
            actual_top, relative = CHECK_ZIP.check_archive(archive, "1.2.1")
            self.assertEqual(actual_top, top)
            self.assertIn("xfoil.next-gui.exe", relative)

    def test_archive_check_rejects_cli(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp) / "stage"
            fill_required(root)
            (root / "xfoil.next.exe").write_bytes(b"MZ")
            archive = pathlib.Path(temp) / "gui.zip"
            top = "XFOIL.next-GUI-1.2.1-local-g8-windows-x86_64"
            WINDOWS.write_zip(root, archive, top, 1700000000)
            with self.assertRaises(CHECK_ZIP.CheckError):
                CHECK_ZIP.check_archive(archive, "1.2.1")


if __name__ == "__main__":
    unittest.main()
