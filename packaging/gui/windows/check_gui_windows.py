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

"""Validate the Windows GUI ZIP, its PE dependencies and clean-PATH startup."""

import argparse
import os
import pathlib
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import zipfile

REQUIRED = {
    "xfoil.next.exe",
    "Install XFOIL.next GUI.cmd",
    "Install-XFOIL.next-GUI.ps1",
    "Uninstall XFOIL.next GUI.cmd",
    "Uninstall-XFOIL.next-GUI.ps1",
    "README-Windows.txt",
    "LICENSE",
    "NOTICE",
    "SOURCE-INFO.txt",
    "Qt6Core.dll",
    "Qt6Gui.dll",
    "Qt6Widgets.dll",
    "Qt6Svg.dll",
    "Qt6PrintSupport.dll",
    "libstdc++-6.dll",
    "libgcc_s_seh-1.dll",
    "libwinpthread-1.dll",
    "platforms/qwindows.dll",
    "imageformats/qjpeg.dll",
    "imageformats/qgif.dll",
    "imageformats/qico.dll",
    "imageformats/qsvg.dll",
    "iconengines/qsvgicon.dll",
    "licenses/Qt-LGPLv3-GPLv3.txt",
    "licenses/QCustomPlot-GPL-3.0.txt",
    "licenses/GCC-RUNTIME-EXCEPTION.txt",
    "licenses/MINGW-W64-RUNTIME-NOTICES.txt",
    "licenses/WINPTHREADS-MIT.txt",
    "licenses/BSD-3-Clause-Clear.txt",
    "licenses/MINGW-W64-ZPL-2.1.txt",
}
LOCAL_IMPORT_PREFIXES = (
    "qt6",
    "lib",
    "zlib",
)


class CheckError(Exception):
    pass


def pe_subsystem(source):
    """Return the PE optional-header subsystem value."""
    if isinstance(source, (str, pathlib.Path)):
        data = pathlib.Path(source).read_bytes()
    else:
        data = source
    if len(data) < 64 or data[:2] != b"MZ":
        raise CheckError("not a PE image")
    pe_offset = struct.unpack_from("<I", data, 0x3C)[0]
    if pe_offset + 24 + 70 > len(data) or data[pe_offset:pe_offset + 4] != b"PE\0\0":
        raise CheckError("invalid PE header")
    optional_magic = struct.unpack_from("<H", data, pe_offset + 24)[0]
    if optional_magic not in (0x10B, 0x20B):
        raise CheckError(f"unexpected PE optional magic: 0x{optional_magic:04x}")
    return struct.unpack_from("<H", data, pe_offset + 24 + 68)[0]


def check_archive(path, expected_version, epoch=None):
    expected_time = None
    if epoch is not None:
        expected_time = list(time.gmtime(epoch)[:6])
        expected_time[5] &= ~1
        expected_time = tuple(expected_time)
    with zipfile.ZipFile(path) as archive:
        infos = archive.infolist()
        if not infos:
            raise CheckError("ZIP is empty")
        names = [info.filename for info in infos]
        if len(names) != len(set(names)):
            raise CheckError("ZIP contains duplicate entries")
        roots = {name.split("/", 1)[0] for name in names if name}
        if len(roots) != 1:
            raise CheckError(f"ZIP must have exactly one top-level directory: {roots}")
        top = roots.pop()
        if not re.fullmatch(r"XFOIL\.next-GUI-[A-Za-z0-9._-]+-windows-x86_64", top):
            raise CheckError(f"unexpected top-level directory: {top}")
        relative = set()
        for info in infos:
            name = info.filename
            if name.startswith("/") or "\\" in name or ".." in name.split("/"):
                raise CheckError(f"unsafe ZIP entry: {name}")
            if not name.startswith(top + "/"):
                raise CheckError(f"entry outside top-level directory: {name}")
            rel = name[len(top) + 1:]
            if rel and not rel.endswith("/"):
                relative.add(rel)
                if info.compress_type not in (
                        zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED):
                    raise CheckError(f"unexpected compression for {rel}")
                mode = (info.external_attr >> 16) & 0xFFFF
                wanted = 0o100755 if rel.endswith(".exe") else 0o100644
                if mode != wanted:
                    raise CheckError(
                        f"unexpected mode for {rel}: {mode:o}, expected {wanted:o}")
                if expected_time is not None and info.date_time != expected_time:
                    raise CheckError(
                        f"unexpected timestamp for {rel}: {info.date_time}, "
                        f"expected {expected_time}")
        missing = sorted(REQUIRED - relative)
        if missing:
            raise CheckError(f"required ZIP entries missing: {missing}")
        for rel in relative:
            name = rel.rsplit("/", 1)[-1]
            if name == ".xfoil-next-gui-install":
                raise CheckError(
                    "the installation marker must be created by the installer, "
                    "not shipped in the ZIP")
            if name in ("xfoil.next-gui.exe", "xfoil.next-gui",
                        "xfoil.next-cli.exe", "xfoil.next-cli",
                        "qmake.exe", "moc.exe", "uic.exe", "rcc.exe",
                        "windeployqt.exe", "Qt6Test.dll"):
                raise CheckError(f"forbidden ZIP entry: {rel}")
            if rel.lower().endswith((".pdb", ".lib", ".a", ".h", ".hpp",
                                     ".cpp", ".o", ".obj", ".log")):
                raise CheckError(f"development artifact in ZIP: {rel}")
        with archive.open(top + "/xfoil.next.exe") as handle:
            data = handle.read()
        if expected_version.encode("utf-8") not in data:
            raise CheckError("product version not found in GUI executable")
        subsystem = pe_subsystem(data)
        if subsystem != 2:
            raise CheckError(
                f"GUI executable subsystem is {subsystem}, expected 2 "
                "(IMAGE_SUBSYSTEM_WINDOWS_GUI)")
        return top, relative


def extract(path, target):
    with zipfile.ZipFile(path) as archive:
        archive.extractall(target)


def imported_dlls(exe):
    objdump = shutil.which("objdump")
    if not objdump:
        return []
    result = subprocess.run(
        [objdump, "-p", str(exe)], check=False, text=True,
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if result.returncode != 0:
        raise CheckError(f"objdump failed for {exe}:\n{result.stdout}")
    return re.findall(r"DLL Name:\s*(\S+)", result.stdout)


def check_dependencies(root, relative):
    objdump = shutil.which("objdump")
    if objdump:
        result = subprocess.run(
            [objdump, "-h", str(root / "xfoil.next.exe")],
            check=False, text=True,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if result.returncode != 0 or ".rsrc" not in result.stdout:
            raise CheckError("GUI executable has no native .rsrc icon section")
    present = {name.rsplit("/", 1)[-1].lower() for name in relative}
    for exe in sorted(root.rglob("*")):
        if exe.suffix.lower() not in (".exe", ".dll") or not exe.is_file():
            continue
        for dll in imported_dlls(exe):
            lowered = dll.lower()
            if lowered.startswith(LOCAL_IMPORT_PREFIXES) and lowered not in present:
                raise CheckError(
                    f"missing bundled dependency {dll} imported by "
                    f"{exe.relative_to(root)}")
    if not imported_dlls(root / "xfoil.next.exe"):
        raise CheckError("GUI executable has no readable PE import table")
    if (root / "xfoil.next.exe").read_bytes().find(b"\x00") < 0:
        raise CheckError("GUI executable is not a PE image")


def pe_version_strings(path):
    """Read the native VS_VERSIONINFO strings on Windows."""
    if os.name != "nt":
        return {}
    import ctypes
    from ctypes import wintypes

    version = ctypes.WinDLL("version")
    version.GetFileVersionInfoSizeW.argtypes = [
        wintypes.LPCWSTR, ctypes.POINTER(wintypes.DWORD)]
    version.GetFileVersionInfoSizeW.restype = wintypes.DWORD
    version.GetFileVersionInfoW.argtypes = [
        wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p]
    version.GetFileVersionInfoW.restype = wintypes.BOOL
    version.VerQueryValueW.argtypes = [
        ctypes.c_void_p, wintypes.LPCWSTR,
        ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(wintypes.UINT)]
    version.VerQueryValueW.restype = wintypes.BOOL

    ignored = wintypes.DWORD()
    size = version.GetFileVersionInfoSizeW(str(path), ctypes.byref(ignored))
    if not size:
        raise CheckError(f"no native Windows version resource in {path}")
    buffer = ctypes.create_string_buffer(size)
    if not version.GetFileVersionInfoW(str(path), 0, size, buffer):
        raise CheckError(f"cannot read Windows version resource from {path}")
    pointer = ctypes.c_void_p()
    length = wintypes.UINT()
    if not version.VerQueryValueW(
            buffer, "\\VarFileInfo\\Translation",
            ctypes.byref(pointer), ctypes.byref(length)):
        raise CheckError("version resource has no translation table")
    if length.value < 4:
        raise CheckError("version translation table is truncated")
    language, codepage = struct.unpack_from(
        "<HH", ctypes.string_at(pointer.value, 4))
    values = {}
    for name in ("FileDescription", "ProductName", "InternalName",
                 "OriginalFilename", "FileVersion", "ProductVersion"):
        key = (f"\\StringFileInfo\\{language:04x}{codepage:04x}\\{name}")
        if not version.VerQueryValueW(
                buffer, key, ctypes.byref(pointer), ctypes.byref(length)):
            values[name] = None
            continue
        values[name] = ctypes.wstring_at(
            pointer.value, length.value).rstrip("\0")
    return values


def check_version_metadata(root, expected_version):
    values = pe_version_strings(root / "xfoil.next.exe")
    expected = {
        "FileDescription": "XFOIL.next GUI",
        "ProductName": "XFOIL.next GUI",
        "InternalName": "XFOIL.next GUI",
        "OriginalFilename": "xfoil.next.exe",
    }
    for name, wanted in expected.items():
        if values.get(name) != wanted:
            raise CheckError(
                f"PE {name} is {values.get(name)!r}, expected {wanted!r}")
    for name in ("FileVersion", "ProductVersion"):
        actual = values.get(name) or ""
        if not actual.startswith(expected_version):
            raise CheckError(
                f"PE {name} is {actual!r}, expected {expected_version!r}")


def window_titles(pid):
    import ctypes
    from ctypes import wintypes
    titles = []
    user32 = ctypes.windll.user32
    user32.GetWindowTextLengthW.argtypes = [wintypes.HWND]
    user32.GetWindowTextLengthW.restype = ctypes.c_int
    user32.GetWindowTextW.argtypes = [
        wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
    user32.GetWindowTextW.restype = ctypes.c_int
    user32.GetWindowThreadProcessId.argtypes = [
        wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
    user32.GetWindowThreadProcessId.restype = wintypes.DWORD
    callback_type = ctypes.WINFUNCTYPE(
        wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)

    @callback_type
    def callback(hwnd, _):
        process_id = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(process_id))
        if process_id.value != pid:
            return True
        length = user32.GetWindowTextLengthW(hwnd)
        if length <= 0:
            return True
        buffer = ctypes.create_unicode_buffer(length + 1)
        user32.GetWindowTextW(hwnd, buffer, length + 1)
        titles.append(buffer.value)
        return True

    user32.EnumWindows(callback, 0)
    return titles


def check_clean_run(root):
    if os.name != "nt":
        raise CheckError("clean-PATH startup requires Windows")
    exe = root / "xfoil.next.exe"
    with tempfile.TemporaryDirectory(prefix="xfoil-gui-run-") as temp:
        home = pathlib.Path(temp)
        env = {
            "SystemRoot": os.environ.get("SystemRoot", r"C:\Windows"),
            "WINDIR": os.environ.get("WINDIR", r"C:\Windows"),
            "PATH": r"C:\Windows\System32;C:\Windows",
            "TEMP": str(home / "temp"),
            "TMP": str(home / "temp"),
            "USERPROFILE": str(home / "profile"),
            "APPDATA": str(home / "appdata"),
            "LOCALAPPDATA": str(home / "localappdata"),
            "QT_QPA_PLATFORM": "windows",
            "LANG": "C",
        }
        (home / "temp").mkdir()
        (home / "profile").mkdir()
        (home / "appdata").mkdir()
        (home / "localappdata").mkdir()
        log = home / "run.log"
        with open(log, "wb") as handle:
            process = subprocess.Popen(
                [str(exe)], cwd=str(root), env=env,
                stdout=handle, stderr=subprocess.STDOUT)
            try:
                time.sleep(3)
                if process.poll() is not None:
                    handle.flush()
                    raise CheckError(
                        "GUI exited during clean-PATH startup "
                        f"(exit code {process.returncode}); log:\n" +
                        log.read_text(encoding="utf-8", errors="replace"))
                titles = window_titles(process.pid)
                if not any("XFOIL.next" in title for title in titles):
                    raise CheckError(
                        "GUI process is running but no XFOIL.next window was "
                        f"found; titles={titles}")
            finally:
                if process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=10)
                    except subprocess.TimeoutExpired:
                        process.kill()


def main():
    parser = argparse.ArgumentParser(
        description="Validate an XFOIL.next Windows GUI ZIP")
    parser.add_argument("--archive", required=True)
    parser.add_argument("--expected-version", required=True)
    parser.add_argument("--clean-run", action="store_true")
    parser.add_argument("--epoch", type=int)
    args = parser.parse_args()
    try:
        top, relative = check_archive(pathlib.Path(args.archive),
                                      args.expected_version, args.epoch)
        with tempfile.TemporaryDirectory(prefix="xfoil-gui-zip-") as temp:
            root = pathlib.Path(temp) / top
            extract(args.archive, temp)
            check_dependencies(root, relative)
            check_version_metadata(root, args.expected_version)
            if args.clean_run:
                check_clean_run(root)
        print(f"Windows GUI ZIP: PASS ({len(relative)} files)")
    except CheckError as error:
        print(f"check_gui_windows.py: error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
