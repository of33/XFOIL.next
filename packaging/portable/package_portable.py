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

"""Deterministic packaging for the XFOIL.next portable runtime.

The script writes exactly one archive per platform:

  XFOIL.next-<version>-linux-x86_64.tar.xz
  XFOIL.next-<version>-windows-x86_64.zip

Each archive contains LICENSE, NOTICE and the runtime executable at the
archive root. It also writes a separate SHA256SUMS file. The product
version is read from src/app/ProductInfo.h; it is never duplicated here.

Source identity is read from Git when available, otherwise from
XFOIL_SOURCE_COMMIT/XFOIL_SOURCE_TREE/XFOIL_SOURCE_REF and
SOURCE_DATE_EPOCH. This supports builds from clean source archives.
"""

import argparse
import hashlib
import json
import os
import pathlib
import re
import subprocess
import sys
import tarfile
import tempfile
import time
import zipfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
PRODUCT_INFO = ROOT / "src/app/ProductInfo.h"
PACKAGING_DIR = ROOT / "packaging/portable"
LICENSES_DIR = PACKAGING_DIR / "licenses"
SOURCE_URL = "https://github.com/of33/XFOIL.next"

PLATFORMS = {
    "linux": {
        "asset": "linux-x86_64",
        "exe": "xfoil.next",
        "extra": ".tar.xz",
        "appendices": [("GCC Runtime Library Exception",
                        "GCC-RUNTIME-EXCEPTION.txt")],
    },
    "windows": {
        "asset": "windows-x86_64",
        "exe": "xfoil.next.exe",
        "extra": ".zip",
        "appendices": [
            ("GCC Runtime Library Exception", "GCC-RUNTIME-EXCEPTION.txt"),
            ("MinGW-w64 runtime notices", "MINGW-W64-RUNTIME-NOTICES.txt"),
            ("winpthreads MIT notice", "WINPTHREADS-MIT.txt"),
            ("BSD-3-Clause-Clear notice", "BSD-3-Clause-Clear.txt"),
        ],
    },
}

PLACEHOLDER_RE = re.compile(r"@[A-Z0-9_]+@")


def fail(message):
    print(f"package_portable.py: error: {message}", file=sys.stderr)
    raise SystemExit(1)


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def product_version():
    text = PRODUCT_INFO.read_text(encoding="utf-8")
    match = re.search(
        r'inline\s+constexpr\s+std::string_view\s+version\s*=\s*"([^"]+)"',
        text)
    if not match:
        fail(f"cannot read product version from {PRODUCT_INFO}")
    return match.group(1)


def git_environment():
    env = os.environ.copy()
    custom = ROOT / ".git_custom"
    if custom.is_dir():
        env["GIT_DIR"] = str(custom)
        env["GIT_WORK_TREE"] = str(ROOT)
    return env


def git_output(*args):
    result = subprocess.run(
        ["git", "-C", str(ROOT), *args],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        env=git_environment())
    if result.returncode != 0:
        return None
    return result.stdout.strip()


def load_metadata(path):
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        fail(f"cannot read metadata file {path}: {error}")
    if not isinstance(data, dict):
        fail(f"metadata file {path} is not a JSON object")
    return data


def source_identity(args):
    if args.metadata:
        data = load_metadata(pathlib.Path(args.metadata))
        return {
            "version": str(data.get("version") or product_version()),
            "commit": str(data.get("source_commit") or ""),
            "tree": str(data.get("source_tree") or ""),
            "ref": str(data.get("source_ref") or "not tagged"),
            "epoch": int(data.get("source_date_epoch") or 0),
        }

    commit = args.source_commit or os.environ.get("XFOIL_SOURCE_COMMIT")
    tree = args.source_tree or os.environ.get("XFOIL_SOURCE_TREE")
    ref = args.source_ref or os.environ.get("XFOIL_SOURCE_REF")
    epoch_text = os.environ.get("SOURCE_DATE_EPOCH")
    version = args.version or os.environ.get("XFOIL_VERSION") or product_version()

    if not commit:
        commit = git_output("rev-parse", "HEAD")
    if not tree:
        tree = git_output("rev-parse", "HEAD^{tree}")
    if not ref:
        ref = git_output("describe", "--tags", "--exact-match", "HEAD")
        if not ref:
            ref = "not tagged"
    if not epoch_text:
        epoch_text = git_output("show", "-s", "--format=%ct", "HEAD")

    if not commit or not tree:
        fail("source commit/tree unavailable; set XFOIL_SOURCE_COMMIT and "
             "XFOIL_SOURCE_TREE or pass --metadata")
    if not epoch_text:
        fail("SOURCE_DATE_EPOCH unavailable; set it explicitly")
    try:
        epoch = int(epoch_text)
    except ValueError:
        fail(f"invalid SOURCE_DATE_EPOCH: {epoch_text!r}")

    return {
        "version": version,
        "commit": commit,
        "tree": tree,
        "ref": ref,
        "epoch": epoch,
    }


def render_notice(platform, identity, archive_name):
    template_path = PACKAGING_DIR / f"NOTICE.{platform}.in"
    try:
        text = template_path.read_text(encoding="utf-8")
    except OSError as error:
        fail(f"cannot read notice template {template_path}: {error}")

    values = {
        "VERSION": identity["version"],
        "SOURCE_COMMIT": identity["commit"],
        "SOURCE_TREE": identity["tree"],
        "SOURCE_REF": identity["ref"],
        "SOURCE_DATE_EPOCH": str(identity["epoch"]),
        "PLATFORM_ASSET": PLATFORMS[platform]["asset"],
        "ARCHIVE_NAME": archive_name,
        "SOURCE_URL": SOURCE_URL,
    }
    for key, value in values.items():
        text = text.replace(f"@{key}@", value)

    leftover = sorted(set(PLACEHOLDER_RE.findall(text)))
    if leftover:
        fail(f"unresolved placeholders in {template_path}: "
             f"{', '.join(leftover)}")

    text = text.rstrip() + "\n"
    for heading, filename in PLATFORMS[platform]["appendices"]:
        appendix = LICENSES_DIR / filename
        try:
            body = appendix.read_text(encoding="utf-8").strip()
        except OSError as error:
            fail(f"cannot read license appendix {appendix}: {error}")
        text += (
            "\n\n"
            + "=" * 72 + "\n"
            + f"{heading}\n"
            + "=" * 72 + "\n\n"
            + body
            + "\n")
    return text


def archive_name(platform, version):
    return (f"XFOIL.next-{version}-{PLATFORMS[platform]['asset']}"
            f"{PLATFORMS[platform]['extra']}")


def write_linux_archive(path, files, epoch):
    with tarfile.open(path, "w:xz", format=tarfile.USTAR_FORMAT) as archive:
        for name, source, mode in files:
            info = tarfile.TarInfo(name)
            info.size = source.stat().st_size
            info.mode = mode
            info.mtime = epoch
            info.uid = 0
            info.gid = 0
            info.uname = ""
            info.gname = ""
            with open(source, "rb") as handle:
                archive.addfile(info, handle)


def write_windows_archive(path, files, epoch):
    if epoch < 315532800:
        fail("SOURCE_DATE_EPOCH predates 1980; ZIP cannot represent it")
    stamp = time.gmtime(epoch)[:6]
    with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_DEFLATED,
                         compresslevel=9) as archive:
        for name, source, mode in files:
            info = zipfile.ZipInfo(name, date_time=stamp)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.create_system = 3
            info.external_attr = (mode & 0xFFFF) << 16
            info.flag_bits = 0
            archive.writestr(info, source.read_bytes(),
                             compress_type=zipfile.ZIP_DEFLATED,
                             compresslevel=9)


def command_metadata(args):
    identity = source_identity(args)
    data = {
        "version": identity["version"],
        "source_commit": identity["commit"],
        "source_tree": identity["tree"],
        "source_ref": identity["ref"],
        "source_date_epoch": identity["epoch"],
    }
    text = json.dumps(data, indent=2, sort_keys=True) + "\n"
    if args.output:
        pathlib.Path(args.output).write_text(text, encoding="utf-8")
    else:
        sys.stdout.write(text)
    return 0


def command_package(args):
    if args.platform not in PLATFORMS:
        fail(f"unknown platform: {args.platform}")
    identity = source_identity(args)
    version = identity["version"]
    spec = PLATFORMS[args.platform]
    executable = pathlib.Path(args.executable)
    if not executable.is_file():
        fail(f"runtime executable not found: {executable}")
    if executable.name != spec["exe"]:
        fail(f"expected executable name {spec['exe']}, got {executable.name}")

    output_dir = pathlib.Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    name = archive_name(args.platform, version)
    target = output_dir / name
    if target.exists():
        target.unlink()

    license_path = ROOT / "LICENSE"
    if not license_path.is_file():
        fail(f"LICENSE not found: {license_path}")

    with tempfile.TemporaryDirectory(prefix="xfoil-notice-") as temporary:
        notice_path = pathlib.Path(temporary) / "NOTICE"
        notice_path.write_text(
            render_notice(args.platform, identity, name), encoding="utf-8")
        files = [
            ("LICENSE", license_path, 0o644),
            ("NOTICE", notice_path, 0o644),
            (spec["exe"], executable, 0o755),
        ]
        if args.platform == "linux":
            write_linux_archive(target, files, identity["epoch"])
        else:
            write_windows_archive(target, files, identity["epoch"])

    print(f"{sha256(target)}  {target}")
    return 0


def command_sums(args):
    version = args.version or product_version()
    output_dir = pathlib.Path(args.output_dir)
    if not output_dir.is_dir():
        fail(f"output directory not found: {output_dir}")
    archives = sorted(
        path for path in output_dir.iterdir()
        if path.is_file()
        and path.name.startswith(f"XFOIL.next-{version}-")
        and (path.name.endswith(".tar.xz") or path.name.endswith(".zip")))
    expected = {archive_name(platform, version) for platform in PLATFORMS}
    present = {path.name for path in archives}
    missing = sorted(expected - present)
    if missing:
        fail(f"missing archive(s): {', '.join(missing)}")
    target = output_dir / f"XFOIL.next-{version}-SHA256SUMS"
    text = "".join(f"{sha256(path)}  {path.name}\n" for path in archives)
    target.write_text(text, encoding="utf-8")
    print(f"{sha256(target)}  {target}")
    return 0


def build_parser():
    parser = argparse.ArgumentParser(
        description="Deterministic XFOIL.next portable packaging")
    subparsers = parser.add_subparsers(dest="command", required=True)

    def add_identity_options(command):
        command.add_argument("--version")
        command.add_argument("--source-commit")
        command.add_argument("--source-tree")
        command.add_argument("--source-ref")
        command.add_argument("--metadata")

    metadata = subparsers.add_parser("metadata")
    add_identity_options(metadata)
    metadata.add_argument("--output")
    metadata.set_defaults(func=command_metadata)

    package = subparsers.add_parser("package")
    add_identity_options(package)
    package.add_argument("--platform", required=True,
                         choices=sorted(PLATFORMS))
    package.add_argument("--executable", required=True)
    package.add_argument("--output-dir", required=True)
    package.set_defaults(func=command_package)

    sums = subparsers.add_parser("sums")
    sums.add_argument("--output-dir", required=True)
    sums.add_argument("--version")
    sums.set_defaults(func=command_sums)
    return parser


def main():
    parser = build_parser()
    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
