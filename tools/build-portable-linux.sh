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

# Build the portable Linux x86_64 runtime natively on the supported
# Ubuntu 24.04 / GCC 13.3.0 baseline. The script does not package;
# use `make portable-linux` for build and package.
#
# Builds from a clean source archive need:
#   XFOIL_SOURCE_COMMIT, XFOIL_SOURCE_TREE, SOURCE_DATE_EPOCH
# XFOIL_SOURCE_REF is optional ("not tagged" by default).

set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT"

BUILDROOT=${BUILDROOT:-build/portable/linux-x86_64}
BINROOT=${BINROOT:-$BUILDROOT}
JOBS=${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || printf '1')}
CXX_FLAGS='g++ -static-libstdc++ -static-libgcc'

fail() {
    printf 'build-portable-linux.sh: error: %s\n' "$*" >&2
    exit 1
}

[ "$(uname -s)" = "Linux" ] || fail "a Linux host is required"
[ "$(uname -m)" = "x86_64" ] || fail "an x86_64 Linux host is required"
command -v g++ >/dev/null 2>&1 || fail "g++ is required"
command -v python3 >/dev/null 2>&1 || fail "python3 is required"
command -v readelf >/dev/null 2>&1 || fail "readelf is required"
command -v git >/dev/null 2>&1 || true

VERSION_ID=$(sed -n 's/^VERSION_ID="\([^"]*\)"/\1/p' /etc/os-release 2>/dev/null || true)
[ "$VERSION_ID" = "24.04" ] || \
    fail "Ubuntu 24.04 is required (found VERSION_ID=${VERSION_ID:-unknown})"
GCC_VERSION=$(g++ -dumpfullversion 2>/dev/null || true)
case "$GCC_VERSION" in
    13.3*) ;;
    *) fail "GCC 13.3.x is required (found ${GCC_VERSION:-unknown})" ;;
esac

git_value() {
    if [ -d "$ROOT/.git_custom" ]; then
        GIT_DIR="$ROOT/.git_custom" GIT_WORK_TREE="$ROOT" \
            git -C "$ROOT" "$@"
    elif [ -d "$ROOT/.git" ]; then
        git -C "$ROOT" "$@"
    else
        return 1
    fi
}

if [ -z "${XFOIL_SOURCE_COMMIT:-}" ]; then
    XFOIL_SOURCE_COMMIT=$(git_value rev-parse HEAD 2>/dev/null || true)
fi
if [ -z "${XFOIL_SOURCE_TREE:-}" ]; then
    XFOIL_SOURCE_TREE=$(git_value rev-parse 'HEAD^{tree}' 2>/dev/null || true)
fi
if [ -z "${XFOIL_SOURCE_REF:-}" ]; then
    XFOIL_SOURCE_REF=$(git_value describe --tags --exact-match HEAD \
        2>/dev/null || true)
    XFOIL_SOURCE_REF=${XFOIL_SOURCE_REF:-not tagged}
fi
if [ -z "${SOURCE_DATE_EPOCH:-}" ]; then
    SOURCE_DATE_EPOCH=$(git_value show -s --format=%ct HEAD 2>/dev/null \
        || true)
fi

[ -n "$XFOIL_SOURCE_COMMIT" ] || \
    fail "XFOIL_SOURCE_COMMIT is required outside a Git checkout"
[ -n "$XFOIL_SOURCE_TREE" ] || \
    fail "XFOIL_SOURCE_TREE is required outside a Git checkout"
[ -n "$SOURCE_DATE_EPOCH" ] || \
    fail "SOURCE_DATE_EPOCH is required outside a Git checkout"
case "$SOURCE_DATE_EPOCH" in
    ''|*[!0-9]*) fail "SOURCE_DATE_EPOCH must be an integer" ;;
esac

export XFOIL_SOURCE_COMMIT XFOIL_SOURCE_TREE XFOIL_SOURCE_REF
export SOURCE_DATE_EPOCH

if [ "${XFOIL_PORTABLE_CLEAN:-1}" = "1" ]; then
    rm -rf "$BUILDROOT/release" "$BINROOT/release" "$BUILDROOT/source-metadata.json"
fi

mkdir -p "$BUILDROOT"
python3 packaging/portable/package_portable.py metadata \
    --output "$BUILDROOT/source-metadata.json"

make -j"$JOBS" release \
    BUILDROOT="$BUILDROOT" BINROOT="$BINROOT" CXX="$CXX_FLAGS"

VERSION=$(python3 -c \
    'import json,sys; print(json.load(open(sys.argv[1]))["version"])' \
    "$BUILDROOT/source-metadata.json")

python3 packaging/portable/check_portable_binary.py \
    --platform linux \
    --binary "$BINROOT/release/xfoil.next-cli" \
    --expected-version "$VERSION" \
    --run-version

printf 'compiler: %s\n' "$(g++ --version | sed -n '1p')"
printf 'runtime:  %s\n' "$(ldd --version | sed -n '1p')"
printf 'source commit: %s\n' "$XFOIL_SOURCE_COMMIT"
printf 'source tree:   %s\n' "$XFOIL_SOURCE_TREE"
printf 'source ref:    %s\n' "$XFOIL_SOURCE_REF"
printf 'SOURCE_DATE_EPOCH: %s\n' "$SOURCE_DATE_EPOCH"
printf 'portable binary: %s\n' "$BINROOT/release/xfoil.next-cli"
