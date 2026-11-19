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

# Build the portable Linux x86_64 runtime in the pinned Ubuntu 22.04 /
# GCC 13.4.0 / glibc 2.35 builder image. The script does not package;
# use `make portable-linux` for build and package.
#
# Builds from a clean source archive need:
#   XFOIL_SOURCE_COMMIT, XFOIL_SOURCE_TREE, SOURCE_DATE_EPOCH
# XFOIL_SOURCE_REF is optional ("not tagged" by default).

set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT"

BUILDROOT=${BUILDROOT:-build/portable/linux-x86_64}
IMAGE=${XFOIL_LINUX_IMAGE:-xfoil-portable-linux:22.04-gcc13.4}
DOCKERFILE=${XFOIL_LINUX_DOCKERFILE:-tools/portable-linux.Dockerfile}
JOBS=${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || printf '1')}
CXX_FLAGS='g++-13 -static-libstdc++ -static-libgcc'

fail() {
    printf 'build-portable-linux.sh: error: %s\n' "$*" >&2
    exit 1
}

command -v docker >/dev/null 2>&1 || fail "docker is required"
command -v python3 >/dev/null 2>&1 || fail "python3 is required"
command -v readelf >/dev/null 2>&1 || fail "readelf is required"
command -v git >/dev/null 2>&1 || true

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
    rm -rf "$BUILDROOT/release" "$BUILDROOT/source-metadata.json"
fi

mkdir -p "$BUILDROOT"
python3 packaging/portable/package_portable.py metadata \
    --output "$BUILDROOT/source-metadata.json"

if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
    printf 'Building portable Linux builder image %s\n' "$IMAGE"
    docker build -f "$DOCKERFILE" -t "$IMAGE" tools
fi

docker run --rm \
    --user "$(id -u):$(id -g)" \
    -v "$ROOT:/src" \
    -w /src \
    -e SOURCE_DATE_EPOCH="$SOURCE_DATE_EPOCH" \
    -e XFOIL_SOURCE_COMMIT="$XFOIL_SOURCE_COMMIT" \
    -e XFOIL_SOURCE_TREE="$XFOIL_SOURCE_TREE" \
    -e XFOIL_SOURCE_REF="$XFOIL_SOURCE_REF" \
    "$IMAGE" \
    bash -c 'set -e; make -j"$1" release BUILDROOT="$2" CXX="$3"' \
    _ "$JOBS" "$BUILDROOT" "$CXX_FLAGS"

VERSION=$(python3 -c \
    'import json,sys; print(json.load(open(sys.argv[1]))["version"])' \
    "$BUILDROOT/source-metadata.json")

python3 packaging/portable/check_portable_binary.py \
    --platform linux \
    --binary "$BUILDROOT/release/xfoil.next" \
    --expected-version "$VERSION" \
    --run-version

printf 'source commit: %s\n' "$XFOIL_SOURCE_COMMIT"
printf 'source tree:   %s\n' "$XFOIL_SOURCE_TREE"
printf 'source ref:    %s\n' "$XFOIL_SOURCE_REF"
printf 'SOURCE_DATE_EPOCH: %s\n' "$SOURCE_DATE_EPOCH"
printf 'portable binary: %s\n' "$BUILDROOT/release/xfoil.next"
