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

"""Deterministic Windows GUI ZIP packaging and validation."""

import argparse
import hashlib
import json
import os
import pathlib
import re
import sys
import time
import zipfile

REQUIRED_FILES = (
    "xfoil.next-gui.exe",
    "Install XFOIL.next GUI.cmd",
    "Install-XFOIL.next-GUI.ps1",
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
)
FORBIDDEN_NAMES = {
    "xfoil.next",
    "xfoil.next.exe",
    "xfoil.next-gui.pdb",
    "qmake.exe",
    "qmake6.exe",
    "moc.exe",
    "uic.exe",
    "rcc.exe",
    "windeployqt.exe",
    "Qt6Test.dll",
    "libxfoil_next.dll",
    "libxfoil_next.dll.a",
}
FORBIDDEN_SUFFIXES = (
    ".pdb",
    ".ilk",
    ".exp",
    ".lib",
    ".a",
    ".h",
    ".hpp",
    ".hh",
    ".cpp",
    ".cc",
    ".cxx",
    ".o",
    ".obj",
    ".log",
    ".d",
)
DEBUG_QT_RE = re.compile(r"^Qt6[A-Za-z0-9]+d\.dll$", re.IGNORECASE)
BUILD_PATH_MARKERS = (
    b"C:\\Users\\Agent\\b0",
    b"build\\portable\\gui",
    b"/local/Olaf/Entwicklung/XFOIL.next",
)


class PackageError(Exception):
    pass


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def check_tree(root, expected_version):
    if not root.is_dir():
        raise PackageError(f"staging directory not found: {root}")
    relpaths = set()
    for path in sorted(root.rglob("*")):
        if not path.is_file():
            continue
        relative = path.relative_to(root).as_posix()
        relpaths.add(relative)
        if path.is_symlink():
            raise PackageError(f"symlink is not allowed in Windows ZIP: {relative}")
        if path.name in FORBIDDEN_NAMES:
            raise PackageError(f"forbidden artifact: {relative}")
        if path.name.endswith(FORBIDDEN_SUFFIXES):
            raise PackageError(f"forbidden development artifact: {relative}")
        if DEBUG_QT_RE.fullmatch(path.name):
            raise PackageError(f"debug Qt DLL is not allowed: {relative}")
        lowered = relative.lower()
        if lowered.startswith("tests/") or "/tests/" in lowered:
            raise PackageError(f"forbidden test artifact: {relative}")
        if "qcustomplot" in lowered and lowered.endswith(
                (".cpp", ".h", ".o", ".obj", ".a", ".lib")):
            raise PackageError(f"forbidden QCustomPlot source/object: {relative}")
        if path.stat().st_size <= 32 * 1024 * 1024:
            data = path.read_bytes()
            for marker in BUILD_PATH_MARKERS:
                if marker in data:
                    raise PackageError(
                        f"embedded build path {marker.decode(errors='replace')!r} "
                        f"in {relative}")
    for relative in REQUIRED_FILES:
        if relative not in relpaths:
            raise PackageError(f"required package file missing: {relative}")
    executable = root / "xfoil.next-gui.exe"
    data = executable.read_bytes()
    if expected_version.encode("utf-8") not in data:
        raise PackageError(
            f"product version {expected_version!r} not found in GUI executable")
    if (b"A\x00n\x00a\x00l\x00y\x00s\x00i\x00s\x00" not in data
            and b"Analysis" not in data):
        raise PackageError("Analysis workflow label not found in GUI executable")
    return sorted(relpaths)


def inventory(root, expected_version):
    entries = []
    for path in sorted(root.rglob("*")):
        if not path.is_file():
            continue
        entries.append({
            "path": path.relative_to(root).as_posix(),
            "size": path.stat().st_size,
            "sha256": sha256(path),
        })
    return {
        "expected_version": expected_version,
        "files": entries,
    }


def write_zip(root, target, top_dir, epoch):
    if epoch < 315532800:
        raise PackageError("SOURCE_DATE_EPOCH predates 1980; ZIP cannot represent it")
    stamp = list(time.gmtime(epoch)[:6])
    stamp[5] &= ~1
    stamp = tuple(stamp)
    if not top_dir or "/" in top_dir or "\\" in top_dir or top_dir in (".", ".."):
        raise PackageError(f"invalid top-level directory name: {top_dir!r}")
    target.parent.mkdir(parents=True, exist_ok=True)
    if target.exists():
        target.unlink()
    directories = {top_dir}
    files = []
    for path in sorted(root.rglob("*")):
        if path.is_dir():
            relative = path.relative_to(root).as_posix()
            parts = relative.split("/")
            for index in range(1, len(parts) + 1):
                directories.add(top_dir + "/" + "/".join(parts[:index]))
        elif path.is_file():
            files.append(path)
    with zipfile.ZipFile(
            target, "w", compression=zipfile.ZIP_DEFLATED,
            compresslevel=9, allowZip64=True) as archive:
        for directory in sorted(directories):
            info = zipfile.ZipInfo(directory + "/", date_time=stamp)
            info.compress_type = zipfile.ZIP_STORED
            info.create_system = 3
            info.external_attr = (0o040755 << 16) | 0x10
            info.flag_bits = 0
            archive.writestr(info, b"")
        for path in files:
            relative = path.relative_to(root).as_posix()
            name = top_dir + "/" + relative
            mode = 0o100755 if relative.endswith(".exe") else 0o100644
            info = zipfile.ZipInfo(name, date_time=stamp)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.create_system = 3
            info.external_attr = mode << 16
            info.flag_bits = 0
            archive.writestr(
                info, path.read_bytes(),
                compress_type=zipfile.ZIP_DEFLATED,
                compresslevel=9)


def package(args):
    root = pathlib.Path(args.staging_dir)
    expected = args.expected_version
    files = check_tree(root, expected)
    data = inventory(root, expected)
    inventory_path = pathlib.Path(args.inventory)
    inventory_path.parent.mkdir(parents=True, exist_ok=True)
    inventory_path.write_text(
        json.dumps(data, indent=2, sort_keys=True) + "\n",
        encoding="utf-8")
    target = pathlib.Path(args.output)
    write_zip(root, target, args.top_dir, args.epoch)
    digest = sha256(target)
    print(f"{digest}  {target}")
    print(f"packaged files: {len(files)}")
    return 0


def main():
    parser = argparse.ArgumentParser(
        description="Deterministic XFOIL.next Windows GUI packaging")
    parser.add_argument("--staging-dir", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--top-dir", required=True)
    parser.add_argument("--epoch", type=int, required=True)
    parser.add_argument("--expected-version", required=True)
    parser.add_argument("--inventory", required=True)
    args = parser.parse_args()
    try:
        return package(args)
    except PackageError as error:
        print(f"package_gui_windows.py: error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
