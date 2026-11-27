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

"""Validate the portable Linux ELF and Windows PE runtime contracts.

Linux:
  * ELF64 x86-64 PIE/executable;
  * NEEDED is exactly the accepted system glibc set;
  * no RPATH/RUNPATH;
  * no libstdc++/libgcc/project shared library dependency;
  * no CXXABI/GCC dynamic symbol versions;
  * reports the maximum GLIBC and GLIBCXX symbol versions required;
  * --version reports the expected product version.

Windows:
  * PE32+ x86-64 executable with zero PE timestamp;
  * only KERNEL32.dll and api-ms-win-crt-*.dll imports;
  * none of the forbidden MinGW/MSYS/project DLLs;
  * --version reports the expected product version.

Optionally validates the archive layout, modes, timestamps and exact
member names for the corresponding platform.
"""

import argparse
import hashlib
import os
import pathlib
import re
import struct
import subprocess
import sys
import tarfile
import time
import zipfile

ALLOWED_LINUX_NEEDED = {
    "libm.so.6",
    "libc.so.6",
    "ld-linux-x86-64.so.2",
}
FORBIDDEN_LINUX = (
    "libstdc++.so",
    "libgcc_s.so",
    "libxfoil_next.so",
)
FORBIDDEN_VERSION_PREFIXES = ("CXXABI_", "GCC_")
WINDOWS_DLL_RE = re.compile(
    r"^(KERNEL32\.dll|api-ms-win-crt-[a-z0-9-]+\.dll)$", re.IGNORECASE)
WINDOWS_FORBIDDEN = {
    "libstdc++-6.dll",
    "libgcc_s_seh-1.dll",
    "libwinpthread-1.dll",
    "msys-2.0.dll",
    "xfoil_next.dll",
}
GLIBC_RE = re.compile(r"GLIBC_([0-9]+)\.([0-9]+)")
GLIBCXX_RE = re.compile(r"GLIBCXX_([0-9]+)\.([0-9]+)(?:\.([0-9]+))?")


class CheckError(Exception):
    pass


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def run(command, check=True):
    env = os.environ.copy()
    env["LC_ALL"] = "C"
    result = subprocess.run(
        command,
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        env=env)
    if check and result.returncode != 0:
        raise CheckError(
            f"command failed ({result.returncode}): {' '.join(command)}\n"
            f"{result.stdout}")
    return result


def check_version(binary, expected):
    result = run([str(binary), "--version"])
    text = result.stdout.strip()
    expected_text = f"XFOIL.next {expected}"
    if text != expected_text:
        raise CheckError(
            f"--version mismatch: expected {expected_text!r}, got {text!r}")


def check_linux(binary, expected_version, run_version):
    header = run(["readelf", "-W", "-h", str(binary)]).stdout
    if "Class:" not in header or "ELF64" not in header:
        raise CheckError("not an ELF64 executable")
    if "Machine:" not in header or "Advanced Micro Devices X86-64" not in header:
        raise CheckError("not an x86-64 ELF executable")
    if "Type:" not in header or not (
            "EXEC" in header or "DYN" in header):
        raise CheckError("unexpected ELF type")

    dynamic = run(["readelf", "-W", "-d", str(binary)]).stdout
    needed = set(re.findall(r"\(NEEDED\)\s+Shared library: \[([^\]]+)\]",
                            dynamic))
    if needed != ALLOWED_LINUX_NEEDED:
        raise CheckError(
            f"unexpected NEEDED set: {sorted(needed)}; "
            f"expected {sorted(ALLOWED_LINUX_NEEDED)}")
    for forbidden in FORBIDDEN_LINUX:
        if any(forbidden in item for item in needed):
            raise CheckError(f"forbidden runtime dependency: {forbidden}")
    if "(RPATH)" in dynamic or "(RUNPATH)" in dynamic:
        raise CheckError("RPATH/RUNPATH must not be present")

    version_info = run(
        ["readelf", "-W", "--version-info", str(binary)]).stdout
    names = set(re.findall(r"Name:\s+([A-Za-z0-9_.]+)", version_info))
    for name in names:
        if name.startswith(FORBIDDEN_VERSION_PREFIXES):
            raise CheckError(f"forbidden dynamic symbol version: {name}")
    glibc_versions = []
    glibcxx_versions = []
    for name in names:
        match = GLIBC_RE.fullmatch(name)
        if match:
            glibc_versions.append((int(match.group(1)), int(match.group(2))))
            continue
        match = GLIBCXX_RE.fullmatch(name)
        if match:
            glibcxx_versions.append(tuple(
                int(part) for part in match.groups() if part is not None))
    if not glibc_versions:
        raise CheckError("no GLIBC symbol versions found")
    maximum = max(glibc_versions)
    maximum_cxx = max(glibcxx_versions) if glibcxx_versions else None

    if expected_version and run_version:
        check_version(binary, expected_version)
    print("linux: PASS")
    return maximum, maximum_cxx


def pe_fields(binary):
    data = binary.read_bytes()
    if len(data) < 64 or data[:2] != b"MZ":
        raise CheckError("not a PE/COFF image (missing MZ header)")
    pe_offset = struct.unpack_from("<I", data, 0x3C)[0]
    if pe_offset + 24 > len(data) or data[pe_offset:pe_offset + 4] != b"PE\0\0":
        raise CheckError("not a PE/COFF image (missing PE signature)")
    machine = struct.unpack_from("<H", data, pe_offset + 4)[0]
    timestamp = struct.unpack_from("<I", data, pe_offset + 8)[0]
    optional_magic = struct.unpack_from("<H", data, pe_offset + 24)[0]
    return machine, timestamp, optional_magic


def check_windows(binary, expected_version, run_version):
    machine, timestamp, optional_magic = pe_fields(binary)
    if machine != 0x8664:
        raise CheckError(f"unexpected PE machine: 0x{machine:04x}")
    if optional_magic != 0x20B:
        raise CheckError(
            f"unexpected PE optional-header magic: 0x{optional_magic:04x}")
    if timestamp != 0:
        raise CheckError(
            f"PE timestamp is non-zero: 0x{timestamp:08x}; "
            "use -Wl,--no-insert-timestamp")

    output = run(["objdump", "-p", str(binary)]).stdout
    imports = re.findall(r"DLL Name:\s*(\S+)", output)
    if not imports:
        raise CheckError("no PE import DLLs found")
    allowed = []
    for name in imports:
        lowered = name.lower()
        if lowered in WINDOWS_FORBIDDEN:
            raise CheckError(f"forbidden runtime DLL import: {name}")
        if not WINDOWS_DLL_RE.fullmatch(name):
            raise CheckError(f"unexpected DLL import: {name}")
        allowed.append(name)
    required = {"kernel32.dll"}
    if not required.issubset({name.lower() for name in allowed}):
        raise CheckError("KERNEL32.dll import missing")

    if expected_version and run_version:
        check_version(binary, expected_version)
    print("windows: PASS")
    return allowed


def check_linux_archive(archive_path, executable_name, epoch):
    with tarfile.open(archive_path, "r:xz") as archive:
        members = archive.getmembers()
        names = [member.name for member in members]
        expected = ["LICENSE", "NOTICE", executable_name]
        if names != expected:
            raise CheckError(
                f"archive members are {names}; expected {expected}")
        for member in members:
            if not member.isfile():
                raise CheckError(f"archive member is not a file: {member.name}")
            if member.uid != 0 or member.gid != 0:
                raise CheckError(f"non-zero uid/gid: {member.name}")
            if member.uname or member.gname:
                raise CheckError(f"non-empty uname/gname: {member.name}")
            if member.mtime != epoch:
                raise CheckError(
                    f"unexpected mtime for {member.name}: {member.mtime}")
            wanted = 0o755 if member.name == executable_name else 0o644
            if member.mode != wanted:
                raise CheckError(
                    f"unexpected mode for {member.name}: "
                    f"{member.mode:o}, expected {wanted:o}")


def check_windows_archive(archive_path, executable_name, epoch):
    with zipfile.ZipFile(archive_path) as archive:
        names = archive.namelist()
        expected = ["LICENSE", "NOTICE", executable_name]
        if names != expected:
            raise CheckError(
                f"archive members are {names}; expected {expected}")
        expected_time = list(time.gmtime(epoch)[:6])
        expected_time[5] &= ~1
        expected_time = tuple(expected_time)
        for info in archive.infolist():
            if info.date_time != expected_time:
                raise CheckError(
                    f"unexpected ZIP timestamp for {info.filename}: "
                    f"{info.date_time}")
            wanted = 0o755 if info.filename == executable_name else 0o644
            mode = (info.external_attr >> 16) & 0xFFFF
            if mode != wanted:
                raise CheckError(
                    f"unexpected mode for {info.filename}: "
                    f"{mode:o}, expected {wanted:o}")


def build_parser():
    parser = argparse.ArgumentParser(
        description="Validate XFOIL.next portable runtime artifacts")
    parser.add_argument("--platform", required=True,
                        choices=("linux", "windows"))
    parser.add_argument("--binary", required=True)
    parser.add_argument("--archive")
    parser.add_argument("--expected-version")
    parser.add_argument("--expected-sha256")
    parser.add_argument("--run-version", action="store_true")
    return parser


def main():
    args = build_parser().parse_args()
    binary = pathlib.Path(args.binary)
    if not binary.is_file():
        print(f"check_portable_binary.py: error: binary not found: {binary}",
              file=sys.stderr)
        return 1
    if args.expected_sha256:
        actual = sha256(binary)
        if actual != args.expected_sha256:
            print("check_portable_binary.py: error: SHA-256 mismatch: "
                  f"{actual} != {args.expected_sha256}", file=sys.stderr)
            return 1

    try:
        if args.platform == "linux":
            maximum, maximum_cxx = check_linux(
                binary, args.expected_version, args.run_version)
            print(f"maximum GLIBC: {maximum[0]}.{maximum[1]}")
            print("maximum GLIBCXX: " +
                  ("none (static libstdc++)" if maximum_cxx is None
                   else ".".join(str(part) for part in maximum_cxx)))
            if args.archive:
                epoch = int(os.environ.get("SOURCE_DATE_EPOCH", "0"))
                if not epoch:
                    raise CheckError(
                        "SOURCE_DATE_EPOCH is required to validate an archive")
                check_linux_archive(
                    pathlib.Path(args.archive), binary.name, epoch)
                print("linux archive: PASS")
        else:
            imports = check_windows(binary, args.expected_version, args.run_version)
            print("PE imports:")
            for name in imports:
                print(f"  {name}")
            if args.archive:
                epoch = int(os.environ.get("SOURCE_DATE_EPOCH", "0"))
                if not epoch:
                    raise CheckError(
                        "SOURCE_DATE_EPOCH is required to validate an archive")
                check_windows_archive(
                    pathlib.Path(args.archive), binary.name, epoch)
                print("windows archive: PASS")
    except CheckError as error:
        print(f"check_portable_binary.py: error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
