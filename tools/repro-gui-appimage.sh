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

# Run the AppImage packaging twice from the same release build and compare.

set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT"

WORK=${GUI_APPIMAGE_REPRO_DIR:-build/portable/gui/repro/linux}
APPIMAGE_TOOLS_DIR=${APPIMAGE_TOOLS_DIR:?APPIMAGE_TOOLS_DIR is required}
QT_ROOT=${QT_ROOT:?QT_ROOT is required}
PACKAGE_VERSION=${GUI_PACKAGE_VERSION:-}
if [ -z "$PACKAGE_VERSION" ]; then
    PRODUCT_VERSION=$(sed -n 's/.*inline constexpr std::string_view version = "\([^"]*\)".*/\1/p' src/app/ProductInfo.h)
    PACKAGE_VERSION="${PRODUCT_VERSION}-local-g8"
fi

rm -rf "$WORK"
mkdir -p "$WORK/a" "$WORK/b"

A="$WORK/a/XFOIL.next-GUI-${PACKAGE_VERSION}-linux-x86_64.AppImage"
B="$WORK/b/XFOIL.next-GUI-${PACKAGE_VERSION}-linux-x86_64.AppImage"

tools/build-gui-appimage.sh \
    --tools-dir "$APPIMAGE_TOOLS_DIR" --qt-root "$QT_ROOT" \
    --package-version "$PACKAGE_VERSION" --output "$A"

tools/build-gui-appimage.sh --no-build \
    --tools-dir "$APPIMAGE_TOOLS_DIR" --qt-root "$QT_ROOT" \
    --package-version "$PACKAGE_VERSION" --output "$B"

A_SHA=$(sha256sum "$A" | cut -d' ' -f1)
B_SHA=$(sha256sum "$B" | cut -d' ' -f1)
if [ "$A_SHA" = "$B_SHA" ]; then
    printf 'AppImage reproducibility: PASS\n%s  %s\n' "$A_SHA" "$(basename "$A")"
    exit 0
fi

printf 'AppImage reproducibility: DIFFERENT\n' >&2
printf '  A %s\n  B %s\n' "$A_SHA" "$B_SHA" >&2
for image in "$A" "$B"; do
    dest="$WORK/$(basename "$(dirname "$image")")-extract"
    mkdir -p "$dest"
    (
        cd "$dest"
        APPIMAGE_EXTRACT_AND_RUN=1 "$image" --appimage-extract >/dev/null
    )
done
python3 - "$WORK/a-extract/squashfs-root" "$WORK/b-extract/squashfs-root" \
    > "$WORK/differences.txt" <<'PYDIFF'
import hashlib
import pathlib
import sys


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def tree(root):
    root = pathlib.Path(root)
    result = {}
    for path in root.rglob("*"):
        if path.is_file():
            result[path.relative_to(root).as_posix()] = digest(path)
        elif path.is_symlink():
            result[path.relative_to(root).as_posix()] = ("symlink", path.readlink())
    return result

left = tree(sys.argv[1])
right = tree(sys.argv[2])
for name in sorted(set(left) | set(right)):
    if left.get(name) != right.get(name):
        print(name)
PYDIFF
printf 'Content differences (empty means container metadata only):\n' >&2
cat "$WORK/differences.txt" >&2
exit 1
