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

# Build a portable XFOIL.next GUI AppImage on the current Linux host. The
# AppImage tool set is pinned and verified locally; nothing is downloaded.

set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT"

TOOLS_DIR=${APPIMAGE_TOOLS_DIR:-}
QT_ROOT=${QT_ROOT:-}
BUILD_ROOT=${GUI_PORTABLE_BUILDROOT:-build/portable/gui/linux-x86_64}
DIST=${GUI_PORTABLE_DIST:-build/portable/gui/dist}
PACKAGE_VERSION=${GUI_PACKAGE_VERSION:-}
OUTPUT=${GUI_APPIMAGE:-}
NO_BUILD=0
SOURCE_DATE_EPOCH=${SOURCE_DATE_EPOCH:-}

fail() {
    printf 'build-gui-appimage.sh: error: %s\n' "$*" >&2
    exit 1
}

usage() {
    cat <<'EOF'
Usage: tools/build-gui-appimage.sh [options]

Options:
  --no-build             reuse an existing GUI release executable
  --tools-dir DIR        pinned AppImage tool directory (or APPIMAGE_TOOLS_DIR)
  --qt-root DIR          Qt 6.12.0 kit prefix (or QT_ROOT)
  --build-root DIR       build work directory
  --dist-dir DIR         output directory
  --package-version VER  artifact package version
  --output FILE          explicit AppImage path
  --help                 show this help
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        --no-build) NO_BUILD=1; shift ;;
        --tools-dir) TOOLS_DIR=$2; shift 2 ;;
        --qt-root) QT_ROOT=$2; shift 2 ;;
        --build-root) BUILD_ROOT=$2; shift 2 ;;
        --dist-dir) DIST=$2; shift 2 ;;
        --package-version) PACKAGE_VERSION=$2; shift 2 ;;
        --output) OUTPUT=$2; shift 2 ;;
        --help|-h) usage; exit 0 ;;
        *) fail "unknown argument: $1" ;;
    esac
done

[ -n "$TOOLS_DIR" ] || fail "APPIMAGE_TOOLS_DIR/--tools-dir is required"
[ -n "$QT_ROOT" ] || fail "QT_ROOT/--qt-root is required"
[ -d "$QT_ROOT" ] || fail "Qt root not found: $QT_ROOT"
[ -x "$QT_ROOT/bin/qmake6" ] || fail "Qt qmake6 not found below $QT_ROOT"
command -v python3 >/dev/null 2>&1 || fail "python3 is required"
command -v make >/dev/null 2>&1 || fail "make is required"
command -v readelf >/dev/null 2>&1 || fail "readelf is required"
command -v desktop-file-validate >/dev/null 2>&1 || \
    fail "desktop-file-validate is required"
command -v mksquashfs >/dev/null 2>&1 || fail "mksquashfs is required"

python3 packaging/gui/check_appimage_tools.py --tools-dir "$TOOLS_DIR"

PRODUCT_VERSION=$(sed -n 's/.*inline constexpr std::string_view version = "\([^"]*\)".*/\1/p' src/app/ProductInfo.h)
[ -n "$PRODUCT_VERSION" ] || fail "cannot read ProductInfo.h version"
if [ -z "$PACKAGE_VERSION" ]; then
    PACKAGE_VERSION="${PRODUCT_VERSION}-local-g8"
fi
if [ -z "$OUTPUT" ]; then
    OUTPUT="$DIST/XFOIL.next-GUI-${PACKAGE_VERSION}-linux-x86_64.AppImage"
fi
case "$OUTPUT" in
    /*) ;;
    *) OUTPUT="$ROOT/$OUTPUT" ;;
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
    XFOIL_SOURCE_REF=$(git_value describe --tags --exact-match HEAD 2>/dev/null || true)
    XFOIL_SOURCE_REF=${XFOIL_SOURCE_REF:-not tagged}
fi
if [ -z "$SOURCE_DATE_EPOCH" ]; then
    SOURCE_DATE_EPOCH=$(git_value show -s --format=%ct HEAD 2>/dev/null || true)
fi
[ -n "$SOURCE_DATE_EPOCH" ] || fail "SOURCE_DATE_EPOCH is required"
[ -n "${XFOIL_SOURCE_COMMIT:-}" ] || fail "XFOIL_SOURCE_COMMIT is required"
[ -n "${XFOIL_SOURCE_TREE:-}" ] || fail "XFOIL_SOURCE_TREE is required"
export SOURCE_DATE_EPOCH

GCC_VERSION=$(g++ --version 2>/dev/null | sed -n '1p' || true)
GLIBC_VERSION=$(ldd --version 2>/dev/null | sed -n '1p' || true)
OS_VERSION=$(sed -n 's/^PRETTY_NAME="\(.*\)"/\1/p' /etc/os-release 2>/dev/null || true)

mkdir -p "$BUILD_ROOT" "$DIST"
APPDIR="$BUILD_ROOT/appdir"
WORK="$BUILD_ROOT/work"
EXTRACT="$BUILD_ROOT/extract"
rm -rf "$APPDIR" "$WORK" "$EXTRACT"
mkdir -p "$WORK"

if [ "$NO_BUILD" -eq 0 ]; then
    make gui-app-release QT_ROOT="$QT_ROOT" \
        BUILDROOT="$BUILD_ROOT/build" BINROOT="$BUILD_ROOT/bin" \
        CORE_BUILDROOT="$BUILD_ROOT/core"
fi
GUI_EXE="$BUILD_ROOT/bin/release/xfoil.next"
[ -x "$GUI_EXE" ] || fail "GUI release executable not found: $GUI_EXE"

export APPIMAGE_EXTRACT_AND_RUN=1
{
    printf 'linuxdeploy: '
    "$TOOLS_DIR/linuxdeploy-x86_64.AppImage" --appimage-extract-and-run --version 2>&1 | sed -n '1p'
    printf 'appimagetool: '
    "$TOOLS_DIR/appimagetool-x86_64.AppImage" --appimage-extract-and-run --version 2>&1 | sed -n '1p'
} > "$BUILD_ROOT/tool-versions.txt"

cp resources/gui/xfoil-next-icon.png "$WORK/xfoil-next.png"

PATH="$TOOLS_DIR:$PATH" \
QMAKE="$QT_ROOT/bin/qmake6" \
QT_PLUGIN_PATH="$QT_ROOT/plugins" \
LD_LIBRARY_PATH="$QT_ROOT/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
EXTRA_PLATFORM_PLUGINS="libqoffscreen.so" \
EXTRA_QT_MODULES="svg;" \
    "$TOOLS_DIR/linuxdeploy-x86_64.AppImage" --appimage-extract-and-run \
        --appdir "$APPDIR" \
        --executable "$GUI_EXE" \
        --desktop-file packaging/gui/linux/xfoil.next.desktop \
        --icon-file "$WORK/xfoil-next.png" \
        --plugin qt

[ -f "$APPDIR/AppRun" ] || fail "linuxdeploy did not create AppRun"
[ -f "$APPDIR/usr/bin/xfoil.next" ] || \
    fail "linuxdeploy did not install the GUI executable"
ln -sfn xfoil-next.png "$APPDIR/.DirIcon"

QT_VERSION=$("$QT_ROOT/bin/qmake6" -query QT_VERSION 2>/dev/null || true)
mkdir -p "$APPDIR/licenses"
cp LICENSE "$APPDIR/LICENSE"
cp packaging/gui/NOTICE.gui "$APPDIR/NOTICE"
cp packaging/gui/licenses/Qt-LGPLv3-GPLv3.txt "$APPDIR/licenses/"
cp packaging/gui/licenses/AppImage-type2-runtime-NOTICE.txt "$APPDIR/licenses/"
cp packaging/portable/licenses/GCC-RUNTIME-EXCEPTION.txt "$APPDIR/licenses/"
cp src/gui/3rdparty/qcustomplot-2.1.1/LICENSE \
    "$APPDIR/licenses/QCustomPlot-GPL-3.0.txt"

cat > "$APPDIR/SOURCE-INFO.txt" <<EOF
XFOIL.next GUI package
======================

Package version:      ${PACKAGE_VERSION}
Product version:      ${PRODUCT_VERSION}
Source URL:           https://github.com/of33/XFOIL.next
Source commit:        ${XFOIL_SOURCE_COMMIT}
Source tree:          ${XFOIL_SOURCE_TREE}
Source reference:     ${XFOIL_SOURCE_REF}
Source date epoch:    ${SOURCE_DATE_EPOCH}
Target:               Linux x86-64 AppImage
Builder baseline:     ${OS_VERSION:-Ubuntu 24.04}
Compiler:             ${GCC_VERSION:-unknown}
Runtime:              ${GLIBC_VERSION:-unknown}
Qt:                   Qt ${QT_VERSION:-unknown} (Linux gcc_64 kit)
AppImage tool set:    pinned G8 2025-11 set (SHA-256 verified)

The corresponding source is the complete source tree at the commit and tree
above. The AppImage is a binary distribution and does not contain the full
source tree.
EOF

find "$APPDIR" -exec touch -h -d "@$SOURCE_DATE_EPOCH" {} +

python3 packaging/gui/linux/check_appimage.py \
    --appdir "$APPDIR" \
    --expected-version "$PRODUCT_VERSION" \
    --inventory "$BUILD_ROOT/appdir-inventory.json" \
    --expected-icon resources/gui/xfoil-next-icon.png

mkdir -p "$(dirname "$OUTPUT")"
rm -f "$OUTPUT"
APPIMAGE_EXTRACT_AND_RUN=1 ARCH=x86_64 SOURCE_DATE_EPOCH="$SOURCE_DATE_EPOCH" \
    "$TOOLS_DIR/appimagetool-x86_64.AppImage" --appimage-extract-and-run \
        --no-appstream \
        --runtime-file "$TOOLS_DIR/runtime-x86_64" \
        --comp zstd \
        "$APPDIR" "$OUTPUT"

mkdir -p "$EXTRACT"
(
    cd "$EXTRACT"
    APPIMAGE_EXTRACT_AND_RUN=1 "$OUTPUT" --appimage-extract >/dev/null
)
[ -d "$EXTRACT/squashfs-root" ] || fail "AppImage extraction failed"

python3 packaging/gui/linux/check_appimage.py \
    --appdir "$EXTRACT/squashfs-root" \
    --expected-version "$PRODUCT_VERSION" \
    --inventory "$BUILD_ROOT/extracted-inventory.json" \
    --compare-appdir "$APPDIR" \
    --expected-icon resources/gui/xfoil-next-icon.png

sha256sum "$OUTPUT" | tee "$OUTPUT.sha256"
printf 'AppImage: %s\n' "$OUTPUT"
