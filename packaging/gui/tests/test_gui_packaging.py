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
ICONS = load("icon_checks", GUI / "icon_checks.py")

REQUIRED = set(WINDOWS.REQUIRED_FILES)
OLD_PNG_SHA256 = "a04555590ece3c54a6ed19972725684064311bbc2b55157868be6bb8ab228dea"
OLD_ICO_SHA256 = "5cb06f57a12a0f65cad644a02e0a98f6f34694259ef9800d1062c66887b46951"
G8A_PNG_SHA256 = "d250aac993d795b39f5e22a98a3f10d907286794506b778208a78c2f068269b2"
G8A_ICO_SHA256 = "53ffac2ae38968ad7e084d2c97a3fd2926859dcc33bbae0bf973618850f0d6cb"
CANONICAL_SHA256 = "9d1abd6e7c5375357bd8caa0f7189138fa744776d835cd7b55138a9f59233218"
CANONICAL = ROOT / "resources/gui/Airfoil.Next_icon.png"


def fake_pe(subsystem=2):
    import struct
    data = bytearray(512)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 0x3C, 0x80)
    data[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<H", data, 0x84, 0x8664)
    struct.pack_into("<H", data, 0x80 + 24, 0x20B)
    struct.pack_into("<H", data, 0x80 + 24 + 68, subsystem)
    data[0x100:0x10D] = b"1.2.1\x00Analysis"
    return bytes(data)


def fill_required(root):
    for relative in REQUIRED:
        path = root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        if relative == "xfoil.next-gui.exe":
            path.write_bytes(fake_pe())
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


class IconTests(unittest.TestCase):
    def test_canonical_source_is_the_owner_file(self):
        self.assertTrue(CANONICAL.is_file())
        self.assertEqual(hashlib.sha256(CANONICAL.read_bytes()).hexdigest(),
                         CANONICAL_SHA256)
        info = ICONS.check_canonical_source(CANONICAL)
        self.assertEqual((info["width"], info["height"]), (4233, 4841))
        self.assertAlmostEqual(info["aspect"], 4233 / 4841, places=8)
        self.assertGreater(info["transparent_fraction"], 0.20)
        self.assertGreater(info["white_fraction"], 0.50)

    def test_corrected_png_and_ico_preserve_source_aspect(self):
        png = ROOT / "resources/gui/xfoil-next-icon.png"
        ico = ROOT / "resources/gui/xfoil-next-icon.ico"
        png_info, sizes, source_info = ICONS.check_pair(png, ico, CANONICAL)
        self.assertEqual(png_info[:2], (512, 512))
        self.assertEqual({(w, h) for w, h in sizes}, set(ICONS.ICO_SIZES))
        self.assertAlmostEqual(source_info["aspect"], 4233 / 4841, places=8)
        png_hash = hashlib.sha256(png.read_bytes()).hexdigest()
        ico_hash = hashlib.sha256(ico.read_bytes()).hexdigest()
        self.assertNotIn(png_hash, (OLD_PNG_SHA256, G8A_PNG_SHA256))
        self.assertNotIn(ico_hash, (OLD_ICO_SHA256, G8A_ICO_SHA256))

    def test_generator_uses_only_new_canonical_source(self):
        text = (ROOT / "tools/generate-gui-icons.py").read_text(
            encoding="utf-8")
        self.assertIn(
            'CANONICAL_SOURCE = "resources/gui/Airfoil.Next_icon.png"', text)
        self.assertNotIn("XFOIL.next.png", text)
        self.assertNotIn("numpy", text.lower())
        self.assertNotIn("scipy", text.lower())
        self.assertIn("Image.Resampling.LANCZOS", text)
        self.assertIn("alpha_composite", text)

    def test_installer_scripts_have_no_network_or_absolute_dev_paths(self):
        ps = (GUI / "windows/Install-XFOIL.next-GUI.ps1").read_text(
            encoding="utf-8")
        cmd = (GUI / "windows/Install XFOIL.next GUI.cmd").read_text(
            encoding="utf-8")
        self.assertIn("LocalApplicationData", ps)
        self.assertIn("'Programs'", ps)
        self.assertIn("'XFOIL.next GUI'", ps)
        self.assertIn("GetFolderPath('Desktop')", ps)
        self.assertIn("GetFolderPath('Programs')", ps)
        self.assertIn("WorkingDirectory", ps)
        self.assertIn("IconLocation", ps)
        self.assertIn("Uninstall XFOIL.next GUI.cmd", ps)
        self.assertIn("StartUninstallLink", ps)
        self.assertIn("marker", ps.lower())
        self.assertIn("-NoProfile", cmd)
        self.assertIn("-ExecutionPolicy Bypass", cmd)
        for text in (ps, cmd):
            self.assertNotIn("C:\\Program Files", text)
            self.assertNotIn("C:\\Windows", text)
            self.assertNotIn("C:\\Users\\Agent", text)
            self.assertNotIn("/local/Olaf", text)
            for token in ("Invoke-WebRequest", "Invoke-RestMethod",
                          "Start-BitsTransfer", "New-ItemProperty",
                          "reg.exe", "Registry::", "Start-Process -Verb RunAs"):
                self.assertNotIn(token, text)

    def test_uninstaller_scripts_are_guarded_and_offline(self):
        ps = (GUI / "windows/Uninstall-XFOIL.next-GUI.ps1").read_text(
            encoding="utf-8")
        cmd = (GUI / "windows/Uninstall XFOIL.next GUI.cmd").read_text(
            encoding="utf-8")
        self.assertIn("MarkerName", ps)
        self.assertIn("ReparsePoint", ps)
        self.assertIn("HelperInstallRoot", ps)
        self.assertIn("Test-IsUnderRoot", ps)
        self.assertIn("The portable folder containing this uninstaller was not changed.", ps)
        self.assertIn("A portable copy is removed by deleting its extracted folder.", ps)
        self.assertIn("Schedule-HelperCleanup", ps)
        self.assertIn("'Uninstall ' + $TargetName", ps)
        self.assertIn("GetFolderPath('Desktop')", ps)
        self.assertIn("GetFolderPath('Programs')", ps)
        self.assertIn("-NoProfile", cmd)
        self.assertIn("-ExecutionPolicy Bypass", cmd)
        self.assertIn("%TEMP%", cmd)
        for text in (ps, cmd):
            self.assertNotIn("C:\\Program Files", text)
            self.assertNotIn("C:\\Windows", text)
            self.assertNotIn("C:\\Users\\Agent", text)
            self.assertNotIn("/local/Olaf", text)
            for token in ("Invoke-WebRequest", "Invoke-RestMethod",
                          "Start-BitsTransfer", "New-ItemProperty",
                          "reg.exe", "Registry::", "Start-Process -Verb RunAs"):
                self.assertNotIn(token, text)

    def test_windows_version_resource_uses_canonical_metadata(self):
        rc = (ROOT / "resources/gui/xfoil-next-icon.rc").read_text(
            encoding="utf-8")
        self.assertIn('VALUE "FileDescription", "XFOIL.next GUI"', rc)
        self.assertIn('VALUE "ProductName", "XFOIL.next GUI"', rc)
        self.assertIn('VALUE "InternalName", "XFOIL.next GUI"', rc)
        self.assertIn('VALUE "OriginalFilename", "xfoil.next-gui.exe"', rc)
        self.assertIn("XFOIL_GUI_VERSION_STRING", rc)

    def test_pe_subsystem_parser(self):
        def pe(subsystem):
            data = bytearray(256)
            data[:2] = b"MZ"
            import struct
            struct.pack_into("<I", data, 0x3C, 0x80)
            data[0x80:0x84] = b"PE\0\0"
            struct.pack_into("<H", data, 0x84, 0x8664)
            struct.pack_into("<H", data, 0x80 + 24, 0x20B)
            struct.pack_into("<H", data, 0x80 + 24 + 68, subsystem)
            return bytes(data)
        self.assertEqual(CHECK_ZIP.pe_subsystem(pe(2)), 2)
        self.assertEqual(CHECK_ZIP.pe_subsystem(pe(3)), 3)


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

    def test_missing_runtime_is_rejected(self):
        for relative in ("Qt6Core.dll", "platforms/qwindows.dll"):
            with tempfile.TemporaryDirectory() as temp:
                root = pathlib.Path(temp) / "stage"
                fill_required(root)
                (root / relative).unlink()
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

    def test_archive_check_rejects_marker(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp) / "stage"
            fill_required(root)
            (root / ".xfoil-next-gui-install").write_text("product\n")
            archive = pathlib.Path(temp) / "gui.zip"
            top = "XFOIL.next-GUI-1.2.1-local-g8-windows-x86_64"
            WINDOWS.write_zip(root, archive, top, 1700000000)
            with self.assertRaises(CHECK_ZIP.CheckError):
                CHECK_ZIP.check_archive(archive, "1.2.1")

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
