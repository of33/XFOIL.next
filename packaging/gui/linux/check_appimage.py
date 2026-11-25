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

"""Audit an XFOIL.next Linux GUI AppDir or extracted AppImage."""

import argparse
import configparser
import hashlib
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from icon_checks import IconError, check_png

REQUIRED_FILES = (
    "AppRun",
    ".DirIcon",
    "xfoil.next-gui.desktop",
    "xfoil-next.png",
    "usr/bin/xfoil.next-gui",
    "usr/share/icons/hicolor/512x512/apps/xfoil-next.png",
    "LICENSE",
    "NOTICE",
    "SOURCE-INFO.txt",
)
REQUIRED_QT_LIBS = (
    "libQt6Core.so.6",
    "libQt6Gui.so.6",
    "libQt6Widgets.so.6",
    "libQt6Svg.so.6",
    "libQt6PrintSupport.so.6",
)
REQUIRED_IMAGE_PLUGINS = (
    "libqjpeg.so",
    "libqgif.so",
    "libqico.so",
    "libqsvg.so",
)
REQUIRED_LICENSE_FILES = (
    "licenses/Qt-LGPLv3-GPLv3.txt",
    "licenses/QCustomPlot-GPL-3.0.txt",
    "licenses/GCC-RUNTIME-EXCEPTION.txt",
    "licenses/AppImage-type2-runtime-NOTICE.txt",
)
FORBIDDEN_NAMES = {
    "xfoil.next",
    "xfoil.next.exe",
    "qmake",
    "qmake6",
    "moc",
    "moc-qt6",
    "uic",
    "uic-qt6",
    "rcc",
    "rcc-qt6",
    "windeployqt",
    "windeployqt.exe",
}
FORBIDDEN_SUFFIXES = (
    ".a",
    ".h",
    ".hpp",
    ".hh",
    ".cpp",
    ".cc",
    ".cxx",
    ".pc",
    ".prl",
    ".cmake",
    ".debug",
    ".dwp",
)
FORBIDDEN_SYSTEM_LIBS = (
    "ld-linux-x86-64.so.2",
    "libc.so.6",
    "libm.so.6",
    "libpthread.so.0",
    "libdl.so.2",
    "librt.so.1",
    "libresolv.so.2",
)
GLIBC_RE = re.compile(r"GLIBC_([0-9]+)\.([0-9]+)")
GLIBCXX_RE = re.compile(r"GLIBCXX_([0-9]+)\.([0-9]+)(?:\.([0-9]+))?")
CXXABI_RE = re.compile(r"CXXABI_([0-9]+)\.([0-9]+)")


class CheckError(Exception):
    pass


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def run(command, check=True, env=None):
    actual_env = os.environ.copy()
    actual_env["LC_ALL"] = "C"
    if env:
        actual_env.update(env)
    result = subprocess.run(
        command,
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        env=actual_env)
    if check and result.returncode != 0:
        raise CheckError(
            f"command failed ({result.returncode}): {' '.join(command)}\n"
            f"{result.stdout}")
    return result


def find_file(appdir, name, subdir=None):
    if subdir:
        roots = [appdir / "usr/plugins" / subdir,
                 appdir / "usr/lib/plugins" / subdir,
                 appdir / "usr/lib/qt6/plugins" / subdir]
    else:
        roots = [appdir / "usr/lib", appdir / "usr/lib64"]
    for root in roots:
        if not root.is_dir():
            continue
        for path in root.rglob(name):
            if path.is_file():
                return path
    return None


def check_required(appdir):
    for relative in REQUIRED_FILES:
        path = appdir / relative
        if not path.exists():
            raise CheckError(f"required AppDir entry missing: {relative}")
        if relative not in ("AppRun",) and not path.is_file():
            raise CheckError(f"required AppDir entry is not a regular file: {relative}")
    apprun = appdir / "AppRun"
    if not os.access(apprun, os.X_OK):
        raise CheckError("AppRun is not executable")
    for name in REQUIRED_QT_LIBS:
        if find_file(appdir, name) is None:
            raise CheckError(f"required Qt library missing: {name}")
    if find_file(appdir, "libqxcb.so", "platforms") is None:
        raise CheckError("required Qt platform plugin missing: platforms/libqxcb.so")
    for name in REQUIRED_IMAGE_PLUGINS:
        if find_file(appdir, name, "imageformats") is None:
            raise CheckError(f"required Qt image plugin missing: imageformats/{name}")
    if find_file(appdir, "libqsvgicon.so", "iconengines") is None:
        raise CheckError(
            "required Qt icon plugin missing: iconengines/libqsvgicon.so")
    for relative in REQUIRED_LICENSE_FILES:
        if not (appdir / relative).is_file():
            raise CheckError(f"required license/notice file missing: {relative}")


def check_desktop(appdir):
    path = appdir / "xfoil.next-gui.desktop"
    parser = configparser.ConfigParser(interpolation=None)
    try:
        parser.read(path, encoding="utf-8")
    except configparser.Error as error:
        raise CheckError(f"cannot parse desktop file: {error}")
    section = "Desktop Entry"
    if not parser.has_section(section):
        raise CheckError("desktop file has no [Desktop Entry] section")
    values = {key.lower(): value for key, value in parser[section].items()}
    expected = {
        "type": "Application",
        "name": "XFOIL.next GUI",
        "exec": "xfoil.next-gui",
        "icon": "xfoil-next",
        "terminal": "false",
    }
    for key, wanted in expected.items():
        if values.get(key) != wanted:
            raise CheckError(
                f"desktop {key} is {values.get(key)!r}, expected {wanted!r}")
    categories = {item for item in values.get("categories", "").split(";") if item}
    for wanted in ("Science", "Engineering"):
        if wanted not in categories:
            raise CheckError(f"desktop Categories is missing {wanted}")
    if shutil.which("desktop-file-validate"):
        result = run(["desktop-file-validate", str(path)], check=False)
        if result.returncode != 0:
            raise CheckError(f"desktop-file-validate failed:\n{result.stdout}")


def check_icon(appdir):
    path = appdir / "xfoil-next.png"
    try:
        width, height, _colours, _transparent = check_png(path, 512, 512)
    except IconError as error:
        raise CheckError(f"corrected GUI icon check failed: {error}")
    installed = appdir / "usr/share/icons/hicolor/512x512/apps/xfoil-next.png"
    if sha256(path) != sha256(installed):
        raise CheckError("AppDir root icon and installed icon differ")
    dir_icon = appdir / ".DirIcon"
    if sha256(dir_icon) != sha256(path):
        raise CheckError(".DirIcon does not resolve to the corrected GUI icon")
    return width


def check_symlinks(appdir):
    root = appdir.resolve()
    for path in appdir.rglob("*"):
        if path.is_symlink():
            target = os.readlink(path)
            if os.path.isabs(target):
                raise CheckError(
                    f"absolute symlink is not allowed: {path.relative_to(appdir)}"
                    f" -> {target}")
            resolved = (path.parent / target).resolve()
            if root not in resolved.parents and resolved != root:
                raise CheckError(
                    f"symlink escapes AppDir: {path.relative_to(appdir)}"
                    f" -> {target}")


def check_forbidden(appdir):
    for path in appdir.rglob("*"):
        if not path.is_file() and not path.is_symlink():
            continue
        relative = path.relative_to(appdir).as_posix()
        name = path.name
        if name in FORBIDDEN_NAMES:
            raise CheckError(f"forbidden CLI/development artifact: {relative}")
        if name.startswith("libQt6Test"):
            raise CheckError(f"forbidden Qt Test library: {relative}")
        if name in FORBIDDEN_SYSTEM_LIBS:
            raise CheckError(f"forbidden system/loader library: {relative}")
        if name.lower().startswith("ld-linux"):
            raise CheckError(f"forbidden ELF loader: {relative}")
        if name.endswith(FORBIDDEN_SUFFIXES):
            raise CheckError(f"forbidden development artifact: {relative}")
        if relative.startswith("tests/") or "/tests/" in relative:
            raise CheckError(f"forbidden test artifact: {relative}")
        lowered = relative.lower()
        if "qcustomplot" in lowered and (
                lowered.endswith((".cpp", ".h", ".o", ".a"))):
            raise CheckError(f"forbidden QCustomPlot source/object: {relative}")


def is_elf(path):
    try:
        return path.read_bytes()[:4] == b"\x7fELF"
    except OSError:
        return False


def parse_versions(output):
    values = {"glibc": [], "glibcxx": [], "cxxabi": []}
    for match in GLIBC_RE.finditer(output):
        values["glibc"].append(tuple(int(part) for part in match.groups()))
    for match in GLIBCXX_RE.finditer(output):
        parts = tuple(int(part) for part in match.groups() if part is not None)
        values["glibcxx"].append(parts)
    for match in CXXABI_RE.finditer(output):
        values["cxxabi"].append(tuple(int(part) for part in match.groups()))
    return values


def version_text(parts):
    return ".".join(str(part) for part in parts) if parts else "none"


def check_elf(path, appdir):
    header = run(["readelf", "-W", "-h", str(path)]).stdout
    if "ELF64" not in header or "Advanced Micro Devices X86-64" not in header:
        raise CheckError(f"not an ELF64 x86-64 file: {path}")
    dynamic = run(["readelf", "-W", "-d", str(path)]).stdout
    needed = re.findall(r"\(NEEDED\)\s+Shared library: \[([^\]]+)\]", dynamic)
    runpath = re.findall(r"\((?:RPATH|RUNPATH)\)\s+Library r(?:un)?path: \[([^\]]+)\]",
                        dynamic)
    for value in runpath:
        for part in value.split(":"):
            if not part:
                continue
            if "$ORIGIN" in part or "$APPDIR" in part:
                continue
            if part.startswith("/"):
                raise CheckError(
                    f"absolute RPATH/RUNPATH entry in {path.name}: {part}")
    version_info = run(
        ["readelf", "-W", "--version-info", str(path)]).stdout
    versions = parse_versions(version_info)
    libdir = appdir / "usr/lib"
    env = {"LD_LIBRARY_PATH": str(libdir)}
    ldd = run(["ldd", str(path)], check=False, env=env)
    if ldd.returncode != 0 or "not found" in ldd.stdout:
        raise CheckError(
            f"unresolved dynamic dependency for {path.name}:\n{ldd.stdout}")
    return {
        "needed": sorted(set(needed)),
        "runpath": runpath,
        "max_glibc": version_text(max(versions["glibc"], default=())),
        "max_glibcxx": version_text(max(versions["glibcxx"], default=())),
        "max_cxxabi": version_text(max(versions["cxxabi"], default=())),
    }


def check_build_paths(appdir):
    markers = (
        b"/local/Olaf/Entwicklung/XFOIL.next",
        b"build/portable/gui",
    )
    for path in appdir.rglob("*"):
        if not path.is_file() or path.is_symlink():
            continue
        if path.stat().st_size > 32 * 1024 * 1024:
            continue
        data = path.read_bytes()
        for marker in markers:
            if marker in data:
                raise CheckError(
                    f"embedded build path {marker.decode()!r} in "
                    f"{path.relative_to(appdir)}")


def check_version_markers(appdir, expected_version):
    binary = appdir / "usr/bin/xfoil.next-gui"
    data = binary.read_bytes()
    if expected_version.encode("utf-8") not in data:
        raise CheckError(
            f"product version {expected_version!r} not found in GUI executable")
    if (b"A\x00n\x00a\x00l\x00y\x00s\x00i\x00s\x00" not in data
            and b"Analysis" not in data):
        raise CheckError("Analysis workflow label not found in GUI executable")


def inventory(appdir, expected_version, elf_data):
    root = appdir.resolve()
    files = []
    for path in sorted(appdir.rglob("*")):
        relative = path.relative_to(appdir).as_posix()
        if path.is_symlink():
            files.append({
                "path": relative,
                "type": "symlink",
                "target": os.readlink(path),
            })
        elif path.is_dir():
            files.append({"path": relative, "type": "dir"})
        elif path.is_file():
            entry = {
                "path": relative,
                "type": "file",
                "size": path.stat().st_size,
                "mode": oct(path.stat().st_mode & 0o777),
                "sha256": sha256(path),
            }
            if relative in elf_data:
                entry["elf"] = elf_data[relative]
            files.append(entry)
    glibc = [entry["elf"]["max_glibc"] for entry in files
             if entry.get("elf", {}).get("max_glibc") not in (None, "none")]
    glibcxx = [entry["elf"]["max_glibcxx"] for entry in files
               if entry.get("elf", {}).get("max_glibcxx") not in (None, "none")]
    return {
        "root": str(root),
        "expected_version": expected_version,
        "maximum_glibc": max(glibc, key=lambda value: tuple(
            int(part) for part in value.split("."))) if glibc else "none",
        "maximum_glibcxx": max(glibcxx, key=lambda value: tuple(
            int(part) for part in value.split("."))) if glibcxx else "none",
        "files": files,
    }


def check_tree(appdir, expected_version, write_inventory=None):
    appdir = pathlib.Path(appdir)
    if not appdir.is_dir():
        raise CheckError(f"AppDir not found: {appdir}")
    check_required(appdir)
    check_desktop(appdir)
    width = check_icon(appdir)
    check_symlinks(appdir)
    check_forbidden(appdir)
    check_version_markers(appdir, expected_version)
    elf_data = {}
    for path in sorted(appdir.rglob("*")):
        if path.is_file() and not path.is_symlink() and is_elf(path):
            elf_data[path.relative_to(appdir).as_posix()] = check_elf(
                path, appdir)
    check_build_paths(appdir)
    data = inventory(appdir, expected_version, elf_data)
    data["icon"] = {"file": "xfoil-next.png", "size": width}
    if write_inventory:
        target = pathlib.Path(write_inventory)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(
            json.dumps(data, indent=2, sort_keys=True) + "\n",
            encoding="utf-8")
    return data


def compare_trees(appdir, extracted, expected_version):
    left = check_tree(appdir, expected_version)
    right = check_tree(extracted, expected_version)
    left_files = {entry["path"]: entry for entry in left["files"]}
    right_files = {entry["path"]: entry for entry in right["files"]}
    if set(left_files) != set(right_files):
        missing = sorted(set(left_files) - set(right_files))
        extra = sorted(set(right_files) - set(left_files))
        raise CheckError(
            f"AppDir/AppImage content mismatch; missing={missing}, extra={extra}")
    for name in sorted(left_files):
        a = left_files[name]
        b = right_files[name]
        if a["type"] != b["type"]:
            raise CheckError(f"type mismatch for {name}")
        if a["type"] == "file":
            if a["sha256"] != b["sha256"] or a["size"] != b["size"]:
                raise CheckError(f"content mismatch for {name}")
        elif a["type"] == "symlink" and a["target"] != b["target"]:
            raise CheckError(f"symlink target mismatch for {name}")


def main():
    parser = argparse.ArgumentParser(
        description="Audit an XFOIL.next Linux GUI AppDir or AppImage tree")
    parser.add_argument("--appdir", required=True)
    parser.add_argument("--expected-version", required=True)
    parser.add_argument("--inventory")
    parser.add_argument("--compare-appdir")
    args = parser.parse_args()
    try:
        data = check_tree(args.appdir, args.expected_version, args.inventory)
        if args.compare_appdir:
            compare_trees(pathlib.Path(args.compare_appdir),
                          pathlib.Path(args.appdir),
                          args.expected_version)
        print(
            "GUI AppDir: PASS "
            f"({len(data['files'])} entries, "
            f"max GLIBC {data['maximum_glibc']}, "
            f"max GLIBCXX {data['maximum_glibcxx']})")
    except CheckError as error:
        print(f"check_appimage.py: error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
