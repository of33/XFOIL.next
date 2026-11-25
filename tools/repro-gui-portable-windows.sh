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

# Build the Windows GUI ZIP once, then repeat the packaging run and compare.

set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT"

WORK=${GUI_WINDOWS_REPRO_DIR:-build/portable/gui/repro/windows}
QT_ROOT=${QT_ROOT:?QT_ROOT is required}
PACKAGE_VERSION=${GUI_PACKAGE_VERSION:-}
if [ -z "$PACKAGE_VERSION" ]; then
    PRODUCT_VERSION=$(sed -n 's/.*inline constexpr std::string_view version = "\([^"]*\)".*/\1/p' src/app/ProductInfo.h)
    PACKAGE_VERSION="${PRODUCT_VERSION}-local-g8"
fi

rm -rf "$WORK"
mkdir -p "$WORK/a" "$WORK/b"

A="$WORK/a/XFOIL.next-GUI-${PACKAGE_VERSION}-windows-x86_64.zip"
B="$WORK/b/XFOIL.next-GUI-${PACKAGE_VERSION}-windows-x86_64.zip"

tools/build-gui-portable-windows.sh --qt-root "$QT_ROOT" \
    --package-version "$PACKAGE_VERSION" --output "$A"
tools/build-gui-portable-windows.sh --package-only --qt-root "$QT_ROOT" \
    --package-version "$PACKAGE_VERSION" --output "$B"

A_SHA=$(sha256sum "$A" | cut -d' ' -f1)
B_SHA=$(sha256sum "$B" | cut -d' ' -f1)
if [ "$A_SHA" = "$B_SHA" ]; then
    printf 'Windows GUI ZIP reproducibility: PASS\n%s  %s\n' \
        "$A_SHA" "$(basename "$A")"
    exit 0
fi

printf 'Windows GUI ZIP reproducibility: DIFFERENT\n' >&2
printf '  A %s\n  B %s\n' "$A_SHA" "$B_SHA" >&2
python3 - "$A" "$B" > "$WORK/differences.txt" <<'PYDIFF'
import hashlib
import sys
import zipfile


def entries(path):
    result = {}
    with zipfile.ZipFile(path) as archive:
        for info in archive.infolist():
            result[info.filename] = (
                info.date_time, info.external_attr,
                hashlib.sha256(archive.read(info.filename)).hexdigest())
    return result

left = entries(sys.argv[1])
right = entries(sys.argv[2])
for name in sorted(set(left) | set(right)):
    if left.get(name) != right.get(name):
        print(name, left.get(name), right.get(name))
PYDIFF
cat "$WORK/differences.txt" >&2
exit 1
