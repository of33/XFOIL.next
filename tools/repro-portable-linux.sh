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

# Reproducibility probe: archive the current commit twice, build each
# clean source tree natively on Ubuntu 24.04 / GCC 13.3.0, package both,
# and compare executable and archive hashes.

set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT"

WORK=${XFOIL_LINUX_REPRO_DIR:-build/portable/repro/linux}
case "$WORK" in
    /*) ;;
    *) WORK="$ROOT/$WORK" ;;
esac
BUILDROOT=build/portable/linux-x86_64
BINROOT=${BINROOT:-$BUILDROOT}

fail() {
    printf 'repro-portable-linux.sh: error: %s\n' "$*" >&2
    exit 1
}

command -v git >/dev/null 2>&1 || fail "git is required"
command -v python3 >/dev/null 2>&1 || fail "python3 is required"

rm -rf "$WORK"
mkdir -p "$WORK/a" "$WORK/b"

python3 packaging/portable/package_portable.py metadata \
    --output "$WORK/metadata.json"

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

if [ -d "$ROOT/.git_custom" ]; then
    GIT_DIR="$ROOT/.git_custom" GIT_WORK_TREE="$ROOT" \
        git -C "$ROOT" archive --format=tar.gz --prefix=xfoil-next/ HEAD \
        > "$WORK/source.tar.gz"
else
    git -C "$ROOT" archive --format=tar.gz --prefix=xfoil-next/ HEAD \
        > "$WORK/source.tar.gz"
fi

tar -xzf "$WORK/source.tar.gz" -C "$WORK/a" --strip-components=1
tar -xzf "$WORK/source.tar.gz" -C "$WORK/b" --strip-components=1

for side in a b; do
    (
        cd "$WORK/$side"
        BUILDROOT="$BUILDROOT" BINROOT="$BINROOT" tools/build-portable-linux.sh
        python3 packaging/portable/package_portable.py package \
            --platform linux \
            --executable "$BINROOT/release/xfoil.next-cli" \
            --output-dir "$WORK/dist-$side" \
            --metadata "$WORK/metadata.json"
    )
done

EXE_A="$WORK/a/$BINROOT/release/xfoil.next-cli"
EXE_B="$WORK/b/$BINROOT/release/xfoil.next-cli"
ARCHIVE_A="$WORK/dist-a/XFOIL.next-$VERSION-linux-x86_64.tar.xz"
ARCHIVE_B="$WORK/dist-b/XFOIL.next-$VERSION-linux-x86_64.tar.xz"

cmp "$EXE_A" "$EXE_B"
cmp "$ARCHIVE_A" "$ARCHIVE_B"

sha256sum "$EXE_A" "$EXE_B" "$ARCHIVE_A" "$ARCHIVE_B"
printf 'portable Linux reproducibility: PASS\n'
printf 'work directory: %s\n' "$WORK"
