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

"""Validate the Qt kit used by the optional XFOIL.next GUI build.

The check is intentionally independent of the local repository layout. It
accepts an explicit --qt-root, QT_ROOT, qmake on PATH, or pkg-config. It
rejects Qt versions other than 6.12.0, missing build tools/modules/libraries,
and obviously incompatible kits.

Exit codes: 0 = OK, 1 = check failed, 2 = usage/setup error.
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

EXPECTED_VERSION = "6.12.0"
REQUIRED_MODULES = ("Core", "Gui", "Widgets", "PrintSupport", "Svg", "Test")
REQUIRED_LIBS = ("Qt6Core", "Qt6Gui", "Qt6Widgets", "Qt6PrintSupport",
                 "Qt6Svg", "Qt6Test")
TOOL_NAMES = ("moc", "uic", "rcc")


class CheckError(Exception):
    """A user-facing Qt kit check failure."""


def fail(summary: str, searched: str | None = None, found: str | None = None,
         expected: str | None = None, fix: str | None = None) -> None:
    lines = [summary]
    if searched:
        lines.append("  searched: " + searched)
    if found:
        lines.append("  found:    " + found)
    if expected:
        lines.append("  expected: " + expected)
    if fix:
        lines.append("  fix:      " + fix)
    raise CheckError("\n".join(lines))


def run(command: list[str]) -> subprocess.CompletedProcess[str] | None:
    try:
        return subprocess.run(
            command,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=20,
            check=False,
        )
    except (OSError, subprocess.SubprocessError):
        return None


def query(qmake: Path, key: str) -> str:
    result = run([str(qmake), "-query", key])
    if result is None or result.returncode != 0:
        return ""
    return result.stdout.strip()


def first_file(paths: list[Path]) -> Path | None:
    seen: set[Path] = set()
    for path in paths:
        try:
            resolved = path.expanduser().resolve()
        except OSError:
            continue
        if resolved in seen:
            continue
        seen.add(resolved)
        if resolved.is_file():
            return resolved
    return None


def find_qmake(root: Path) -> Path:
    candidates = [
        root / "bin" / "qmake6",
        root / "bin" / "qmake6.exe",
        root / "bin" / "qmake",
        root / "bin" / "qmake.exe",
    ]
    qmake = first_file(candidates)
    if qmake is None:
        fail(
            "qmake was not found in the Qt root",
            searched=", ".join(str(path) for path in candidates),
            found=str(root),
            expected="a Qt 6.12.0 installation with qmake6 or qmake",
            fix="set QT_ROOT to the Qt kit prefix (the directory containing bin/qmake6)",
        )
    return qmake


def root_from_qmake(qmake: Path) -> Path:
    value = query(qmake, "QT_INSTALL_PREFIX")
    if not value:
        fail(
            "qmake did not report QT_INSTALL_PREFIX",
            found=str(qmake),
            fix="set QT_ROOT explicitly to the Qt kit prefix",
        )
    root = Path(value).expanduser().resolve()
    if not root.is_dir():
        fail(
            "qmake reported a Qt prefix that does not exist",
            found=str(root),
            expected="an existing Qt 6.12.0 kit prefix",
            fix="set QT_ROOT explicitly to the existing Qt kit prefix",
        )
    return root


def resolve_root(args: argparse.Namespace) -> tuple[Path, str]:
    explicit = args.qt_root or os.environ.get("QT_ROOT", "")
    if explicit:
        root = Path(explicit).expanduser().resolve()
        if not root.is_dir():
            fail(
                "QT_ROOT does not point to an existing Qt kit",
                searched=f"QT_ROOT={explicit}",
                found=str(root),
                expected="an existing Qt 6.12.0 kit prefix",
                fix="export QT_ROOT=/path/to/Qt/6.12.0/gcc_64 "
                    "(or the matching MSYS2/MinGW kit)",
            )
        return root, "QT_ROOT" if args.qt_root is None else "--qt-root"

    if args.no_discovery:
        fail(
            "no Qt kit was supplied",
            searched="QT_ROOT, --qt-root; discovery disabled by --no-discovery",
            found="nothing",
            expected="Qt 6.12.0",
            fix="set QT_ROOT to the Qt kit prefix",
        )

    discovered: list[str] = []
    for name in ("qmake6", "qmake"):
        found = shutil.which(name)
        if found:
            qmake = Path(found).resolve()
            try:
                return root_from_qmake(qmake), f"qmake on PATH ({qmake})"
            except CheckError as error:
                discovered.append(f"qmake at {qmake}: {str(error).splitlines()[0]}")

    pkg_config = shutil.which("pkg-config")
    if pkg_config:
        result = run([pkg_config, "--variable=prefix", "Qt6Core"])
        if result is not None and result.returncode == 0:
            value = result.stdout.strip()
            if value:
                root = Path(value).expanduser().resolve()
                if root.is_dir():
                    return root, "pkg-config Qt6Core"
                fail(
                    "pkg-config reported a Qt6Core prefix that does not exist",
                    found=str(root),
                    expected="an existing Qt 6.12.0 kit prefix",
                    fix="set QT_ROOT explicitly to the Qt kit prefix",
                )

    fail(
        "no Qt 6.12.0 kit was found",
        searched="QT_ROOT, --qt-root, qmake6/qmake on PATH, pkg-config Qt6Core",
        found="; ".join(discovered) if discovered else "nothing",
        expected="Qt 6.12.0 with qmake6, moc, uic and rcc",
        fix="set QT_ROOT to the Qt kit prefix, for example "
            "QT_ROOT=/path/to/Qt/6.12.0/gcc_64",
    )
    raise AssertionError("unreachable")


def tool_path(root: Path, qmake: Path, name: str) -> Path:
    query_keys = ("QT_HOST_BINS", "QT_INSTALL_LIBEXECS", "QT_INSTALL_BINS")
    directories = [root / "bin", root / "libexec", root / "share" / "qt6" / "bin"]
    for key in query_keys:
        value = query(qmake, key)
        if value:
            directories.append(Path(value))
    candidates: list[Path] = []
    for directory in directories:
        candidates.extend([directory / name, directory / (name + ".exe")])
    tool = first_file(candidates)
    if tool is None:
        fail(
            f"required Qt tool {name} was not found",
            searched=", ".join(str(path) for path in candidates),
            found=str(root),
            expected=f"the {name} tool belonging to Qt {EXPECTED_VERSION}",
            fix="install a complete Qt 6.12.0 kit or set QT_ROOT to it",
        )
    return tool


def tool_version(tool: Path, name: str, root: Path) -> str:
    options = ("--version", "-v") if name == "rcc" else ("-v", "--version")
    outputs: list[str] = []
    for option in options:
        result = run([str(tool), option])
        if result is not None:
            outputs.append(result.stdout.strip())
            outputs.append(result.stderr.strip())
    text = "\n".join(part for part in outputs if part)
    if EXPECTED_VERSION in text:
        return EXPECTED_VERSION
    # Some Windows Qt tool builds do not print a version for -v/--version.
    # The qmake version and the tool path are then the available metadata.
    if not text:
        try:
            tool.relative_to(root)
            return EXPECTED_VERSION
        except ValueError:
            pass
    fail(
        f"{name} did not report Qt {EXPECTED_VERSION}",
        found=text or str(tool),
        expected=f"a {name} tool from Qt {EXPECTED_VERSION}",
        fix="set QT_ROOT to the Qt 6.12.0 kit, not a mixed or older install",
    )
    raise AssertionError("unreachable")


def check_libraries(lib_dir: Path) -> None:
    for library in REQUIRED_LIBS:
        candidates = list(lib_dir.glob(f"lib{library}.*"))
        candidates += list(lib_dir.glob(f"{library}.lib"))
        candidates += list(lib_dir.glob(f"{library}.a"))
        candidates += list(lib_dir.glob(f"{library}.dll.a"))
        if not candidates:
            fail(
                f"required Qt library {library} was not found",
                searched=str(lib_dir),
                found="none",
                expected=f"{library} from Qt {EXPECTED_VERSION}",
                fix="install the complete Qt kit or set QT_ROOT to it",
            )


def platform_name(args: argparse.Namespace) -> str:
    if args.platform != "auto":
        return args.platform
    if (os.name == "nt" or sys.platform in ("win32", "cygwin", "msys")
            or os.environ.get("MSYSTEM")):
        return "windows"
    return "linux"


def check_kit(root: Path, qmake: Path, platform: str) -> tuple[str, str]:
    version = query(qmake, "QT_VERSION")
    if version != EXPECTED_VERSION:
        fail(
            "unsupported Qt version",
            found=version or "unknown",
            expected=EXPECTED_VERSION,
            fix="set QT_ROOT to a Qt 6.12.0 kit",
        )
    xspec = query(qmake, "QMAKE_XSPEC")
    lowered = xspec.lower()
    if platform == "windows":
        if os.environ.get("MSYSTEM") and os.environ["MSYSTEM"] != "UCRT64":
            fail(
                "the active MSYS2 environment is not UCRT64",
                found=os.environ["MSYSTEM"],
                expected="MSYSTEM=UCRT64 for the XFOIL.next Windows GUI baseline",
                fix="open the MSYS2 UCRT64 shell and set QT_ROOT to its Qt 6.12.0 kit",
            )
        if not lowered.startswith("win32-g++"):
            fail(
                "the Qt kit is not a Windows MinGW kit",
                found=xspec or "unknown",
                expected="QMAKE_XSPEC=win32-g++ (MSYS2 UCRT64/MinGW)",
                fix="set QT_ROOT to a Qt 6.12.0 MinGW kit; MSVC kits are not supported",
            )
    else:
        if not lowered.startswith("linux-g++") or any(
                token in lowered for token in
                ("android", "wasm", "win32", "macx", "ios", "msvc")):
            fail(
                "the Qt kit is not a Linux GCC desktop kit",
                found=xspec or "unknown",
                expected="QMAKE_XSPEC=linux-g++ (desktop GCC build)",
                fix="set QT_ROOT to a Qt 6.12.0 gcc_64 desktop kit",
            )
    return version, xspec


def query_dir(qmake: Path, key: str, fallback: Path) -> Path:
    value = query(qmake, key)
    if value:
        candidate = Path(value).expanduser()
        if candidate.is_dir():
            return candidate.resolve()
        match = re.match(r"^([A-Za-z]):[\\/](.*)$", value)
        if match:
            converted = Path("/" + match.group(1).lower() + "/" +
                             match.group(2).replace("\\", "/"))
            if converted.is_dir():
                return converted.resolve()
    if fallback.is_dir():
        return fallback.resolve()
    return candidate if value else fallback


def check_windres(command: str) -> str:
    candidate = shutil.which(command) if command else None
    if candidate is None and command:
        path = Path(command).expanduser()
        if path.is_file():
            candidate = str(path.resolve())
    if candidate is None:
        fail(
            "the Windows resource compiler windres was not found",
            searched=command or "windres on PATH",
            found="nothing",
            expected="windres from the MSYS2 UCRT64/MinGW toolchain",
            fix="open the MSYS2 UCRT64 shell or set WINDRES to windres.exe",
        )
    return candidate


def check_qt(root: Path, platform: str) -> dict[str, str]:
    if not root.is_dir():
        fail(
            "the Qt root does not exist",
            found=str(root),
            expected="an existing Qt 6.12.0 kit prefix",
            fix="set QT_ROOT to the existing Qt kit prefix",
        )
    qmake = find_qmake(root)
    version, xspec = check_kit(root, qmake, platform)
    include_dir = query_dir(qmake, "QT_INSTALL_HEADERS", root / "include")
    lib_dir = query_dir(qmake, "QT_INSTALL_LIBS", root / "lib")
    plugin_dir = query_dir(qmake, "QT_INSTALL_PLUGINS", root / "plugins")
    for module in REQUIRED_MODULES:
        module_dir = include_dir / f"Qt{module}"
        if not module_dir.is_dir():
            fail(
                f"required Qt module header directory is missing: Qt{module}",
                searched=str(module_dir),
                found="missing",
                expected=f"headers for Qt {EXPECTED_VERSION} {module}",
                fix="install the complete Qt kit or set QT_ROOT to it",
            )
    check_libraries(lib_dir)
    if not (plugin_dir / "platforms").is_dir():
        fail(
            "Qt platform plugins were not found",
            searched=str(plugin_dir / "platforms"),
            found="missing",
            expected=f"Qt {EXPECTED_VERSION} platform plugins",
            fix="install the complete Qt kit or set QT_ROOT to it",
        )
    tools: dict[str, Path] = {}
    for name in TOOL_NAMES:
        tool = tool_path(root, qmake, name)
        tool_version(tool, name, root)
        tools[name] = tool
    return {
        "root": str(root),
        "version": version,
        "qmake": str(qmake),
        "moc": str(tools["moc"]),
        "uic": str(tools["uic"]),
        "rcc": str(tools["rcc"]),
        "xspec": xspec,
        "platform": "Windows MinGW" if platform == "windows" else "Linux GCC",
        "include": str(include_dir),
        "libs": str(lib_dir),
        "plugins": str(plugin_dir),
    }


def print_result(result: dict[str, str]) -> None:
    print("Qt check: OK")
    print(f"Qt root:   {result['root']}")
    print(f"Qt version: {result['version']}")
    print(f"qmake:     {result['qmake']}")
    print(f"moc:       {result['moc']}")
    print(f"uic:       {result['uic']}")
    print(f"rcc:       {result['rcc']}")
    print(f"platform:  {result['platform']} ({result['xspec']})")


def write_executable(path: Path, text: str) -> None:
    path.write_text(text, encoding="utf-8")
    path.chmod(0o755)


def make_fake_qt(base: Path, version: str = EXPECTED_VERSION,
                 xspec: str = "linux-g++",
                 omit: str | None = None) -> Path:
    root = base / "qt"
    for module in REQUIRED_MODULES:
        (root / "include" / f"Qt{module}").mkdir(parents=True, exist_ok=True)
    (root / "lib").mkdir(parents=True, exist_ok=True)
    (root / "bin").mkdir(parents=True, exist_ok=True)
    (root / "plugins" / "platforms").mkdir(parents=True, exist_ok=True)
    for library in REQUIRED_LIBS:
        (root / "lib" / f"lib{library}.so").touch()
    qmake = root / "bin" / "qmake6"
    write_executable(
        qmake,
        "#!/bin/sh\n"
        "case \"$1:$2\" in\n"
        f"  -query:QT_VERSION) echo {version} ;;\n"
        f"  -query:QT_INSTALL_PREFIX) echo {root} ;;\n"
        f"  -query:QMAKE_XSPEC) echo {xspec} ;;\n"
        f"  -query:QT_HOST_BINS) echo {root}/bin ;;\n"
        f"  -query:QT_INSTALL_LIBEXECS) echo {root}/libexec ;;\n"
        f"  -query:QT_INSTALL_BINS) echo {root}/bin ;;\n"
        f"  -query:QT_INSTALL_HEADERS) echo {root}/include ;;\n"
        f"  -query:QT_INSTALL_LIBS) echo {root}/lib ;;\n"
        f"  -query:QT_INSTALL_PLUGINS) echo {root}/plugins ;;\n"
        "  *) exit 1 ;;\n"
        "esac\n",
    )
    for name in TOOL_NAMES:
        if name == omit:
            continue
        option = "--version" if name == "rcc" else "-v"
        write_executable(
            root / "bin" / name,
            "#!/bin/sh\n"
            f"if [ \"$1\" = \"{option}\" ]; then echo {name} {version}; "
            "else exit 1; fi\n",
        )
    return root


def self_test() -> int:
    if os.name != "posix":
        print("check-qt.py self-test: skipped (POSIX-only fake tools)")
        return 0
    failures = 0
    with tempfile.TemporaryDirectory(prefix="xfoil-check-qt-") as temp:
        base = Path(temp)
        valid = make_fake_qt(base / "valid")
        try:
            check_qt(valid, "linux")
            print("self-test: valid Qt accepted")
        except CheckError as error:
            failures += 1
            print("self-test: valid Qt rejected:", error)
        cases = [
            ("missing root", base / "missing", "linux", "exists"),
            ("wrong version", make_fake_qt(base / "old", "6.11.0"), "linux",
             "version"),
            ("missing tool", make_fake_qt(base / "no-moc", omit="moc"),
             "linux", "moc"),
            ("unsuitable kit", make_fake_qt(base / "android",
                                             xspec="android-clang"),
             "linux", "kit"),
        ]
        for label, root, platform, expected_text in cases:
            try:
                check_qt(root, platform)
            except CheckError as error:
                if expected_text.lower() in str(error).lower() or (
                        label == "missing root" and "does not exist" in str(error)):
                    print(f"self-test: {label} rejected")
                else:
                    failures += 1
                    print(f"self-test: {label} rejected with unexpected text:",
                          error)
            else:
                failures += 1
                print(f"self-test: {label} was accepted")
    if failures:
        print(f"check-qt.py self-test: FAIL ({failures})")
        return 1
    print("check-qt.py self-test: OK")
    return 0


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Validate a Qt 6.12.0 kit for the XFOIL.next GUI build")
    parser.add_argument("--qt-root", default="",
                        help="explicit Qt kit prefix (overrides QT_ROOT)")
    parser.add_argument("--platform", choices=("auto", "linux", "windows"),
                        default="auto", help="expected host platform")
    parser.add_argument("--cxx", default="",
                        help="C++ compiler command (used for diagnostics)")
    parser.add_argument("--windres", default="",
                        help="Windows resource compiler (used for the native icon)")
    parser.add_argument("--no-discovery", action="store_true",
                        help="do not search PATH or pkg-config")
    parser.add_argument("--self-test", action="store_true",
                        help="run isolated check-qt.py self-tests")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    if args.self_test:
        return self_test()
    try:
        root, source = resolve_root(args)
        platform = platform_name(args)
        result = check_qt(root, platform)
    except CheckError as error:
        print("ERROR: " + str(error), file=sys.stderr)
        return 1
    print_result(result)
    print(f"source:    {source}")
    if args.cxx:
        print(f"CXX:       {args.cxx}")
    if args.windres:
        print(f"windres:   {check_windres(args.windres)}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
