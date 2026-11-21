#!/usr/bin/env bash
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

# Reproducibility probe: copy the source tree twice without generated
# build output, build each clean tree in MSYS2 UCRT64, package both, and
# compare executable and archive hashes.

set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT"

WORK=${XFOIL_WINDOWS_REPRO_DIR:-build/portable/repro/windows}
case "$WORK" in
    /*) ;;
    [A-Za-z]:*) ;;
    *) WORK="$ROOT/$WORK" ;;
esac
BUILDROOT=build/portable/windows-x86_64
BINROOT=${BINROOT:-$BUILDROOT}

fail() {
    printf 'repro-portable-windows.sh: error: %s\n' "$*" >&2
    exit 1
}

command -v python3 >/dev/null 2>&1 || fail "python3 is required"
command -v tar >/dev/null 2>&1 || fail "tar is required"

rm -rf "$WORK"
mkdir -p "$WORK/a" "$WORK/b"

METADATA_SOURCE="$ROOT/build/portable/windows-x86_64/source-metadata.json"
if [ -n "${XFOIL_SOURCE_COMMIT:-}" ]; then
    python3 packaging/portable/package_portable.py metadata \
        --output "$WORK/metadata.json"
elif [ -f "$METADATA_SOURCE" ]; then
    cp "$METADATA_SOURCE" "$WORK/metadata.json"
else
    python3 packaging/portable/package_portable.py metadata \
        --output "$WORK/metadata.json"
fi

read_value() {
    python3 - "$WORK/metadata.json" "$1" <<'PYVALUE'
import json
import sys
data = json.load(open(sys.argv[1], encoding="utf-8"))
print(data[sys.argv[2]])
PYVALUE
}

VERSION=$(read_value version)
COMMIT=$(read_value source_commit)
TREE=$(read_value source_tree)
REF=$(read_value source_ref)
EPOCH=$(read_value source_date_epoch)

export XFOIL_SOURCE_COMMIT="$COMMIT"
export XFOIL_SOURCE_TREE="$TREE"
export XFOIL_SOURCE_REF="$REF"
export SOURCE_DATE_EPOCH="$EPOCH"

copy_tree() {
    destination=$1
    (
        cd "$ROOT"
        tar --exclude=./build --exclude=./bin --exclude=./.git --exclude=./.git_custom \
            --exclude=./.audit-xfoil-next -cf - .
    ) | (
        cd "$destination"
        tar -xf -
    )
}

copy_tree "$WORK/a"
copy_tree "$WORK/b"

for side in a b; do
    (
        cd "$WORK/$side"
        BUILDROOT="$BUILDROOT" BINROOT="$BINROOT" tools/build-portable-windows.sh
        python3 packaging/portable/package_portable.py package \
            --platform windows \
            --executable "$BINROOT/release/xfoil.next.exe" \
            --output-dir "$WORK/dist-$side" \
            --metadata "$WORK/metadata.json"
    )
done

EXE_A="$WORK/a/$BINROOT/release/xfoil.next.exe"
EXE_B="$WORK/b/$BINROOT/release/xfoil.next.exe"
ARCHIVE_A="$WORK/dist-a/XFOIL.next-$VERSION-windows-x86_64.zip"
ARCHIVE_B="$WORK/dist-b/XFOIL.next-$VERSION-windows-x86_64.zip"

assert_same() {
    python3 - "$1" "$2" <<'PYASSERT'
import hashlib
import sys

def digest(path):
    value = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            value.update(chunk)
    return value.hexdigest()

left = digest(sys.argv[1])
right = digest(sys.argv[2])
if left != right:
    print(f"mismatch: {sys.argv[1]} {left} != {sys.argv[2]} {right}",
          file=sys.stderr)
    raise SystemExit(1)
print(f"{left}  {sys.argv[1]}")
print(f"{right}  {sys.argv[2]}")
PYASSERT
}

assert_same "$EXE_A" "$EXE_B"
assert_same "$ARCHIVE_A" "$ARCHIVE_B"

sha256sum "$EXE_A" "$EXE_B" "$ARCHIVE_A" "$ARCHIVE_B"
printf 'portable Windows reproducibility: PASS\n'
printf 'work directory: %s\n' "$WORK"
