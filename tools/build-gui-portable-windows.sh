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

# Build and package the XFOIL.next Windows GUI ZIP in MSYS2 UCRT64.

set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT"

QT_ROOT=${QT_ROOT:-}
BUILD_ROOT=${GUI_PORTABLE_BUILDROOT:-build/portable/gui/windows-x86_64}
DIST=${GUI_PORTABLE_DIST:-build/portable/gui/dist}
PACKAGE_VERSION=${GUI_PACKAGE_VERSION:-}
OUTPUT=${GUI_WINDOWS_ARCHIVE:-}
NO_BUILD=0
PACKAGE_ONLY=0
SOURCE_DATE_EPOCH=${SOURCE_DATE_EPOCH:-}
PYTHON=${PYTHON:-python3}

fail() {
    printf 'build-gui-portable-windows.sh: error: %s\n' "$*" >&2
    exit 1
}

while [ $# -gt 0 ]; do
    case "$1" in
        --no-build) NO_BUILD=1; shift ;;
        --package-only) PACKAGE_ONLY=1; shift ;;
        --qt-root) QT_ROOT=$2; shift 2 ;;
        --build-root) BUILD_ROOT=$2; shift 2 ;;
        --dist-dir) DIST=$2; shift 2 ;;
        --package-version) PACKAGE_VERSION=$2; shift 2 ;;
        --output) OUTPUT=$2; shift 2 ;;
        --help|-h) echo "Usage: tools/build-gui-portable-windows.sh [--no-build|--package-only]"; exit 0 ;;
        *) fail "unknown argument: $1" ;;
    esac
done

[ -n "$QT_ROOT" ] || fail "QT_ROOT is required"
[ -d "$QT_ROOT" ] || fail "Qt root not found: $QT_ROOT"
WINDEPLOYQT="$QT_ROOT/bin/windeployqt.exe"
[ -x "$WINDEPLOYQT" ] || fail "windeployqt not found: $WINDEPLOYQT"
command -v "$PYTHON" >/dev/null 2>&1 || fail "$PYTHON is required"
command -v objdump >/dev/null 2>&1 || fail "objdump is required"
command -v make >/dev/null 2>&1 || fail "make is required"

PRODUCT_VERSION=$(sed -n 's/.*inline constexpr std::string_view version = "\([^"]*\)".*/\1/p' src/app/ProductInfo.h)
[ -n "$PRODUCT_VERSION" ] || fail "cannot read ProductInfo.h version"
if [ -z "$PACKAGE_VERSION" ]; then
    PACKAGE_VERSION="${PRODUCT_VERSION}-local-g8"
fi
TOP_DIR="XFOIL.next-GUI-${PACKAGE_VERSION}-windows-x86_64"
if [ -z "$OUTPUT" ]; then
    OUTPUT="$DIST/${TOP_DIR}.zip"
fi

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
[ -n "${XFOIL_SOURCE_COMMIT:-}" ] || fail "XFOIL_SOURCE_COMMIT is required"
[ -n "${XFOIL_SOURCE_TREE:-}" ] || fail "XFOIL_SOURCE_TREE is required"
[ -n "$SOURCE_DATE_EPOCH" ] || fail "SOURCE_DATE_EPOCH is required"
export SOURCE_DATE_EPOCH

STAGE="$BUILD_ROOT/stage/$TOP_DIR"
GUI_EXE="$BUILD_ROOT/bin/release/xfoil.next-gui.exe"

if [ "$PACKAGE_ONLY" -eq 0 ]; then
    if [ "$NO_BUILD" -eq 0 ]; then
        make gui-app-release QT_ROOT="$QT_ROOT" \
            BUILDROOT="$BUILD_ROOT/build" BINROOT="$BUILD_ROOT/bin" \
            CORE_BUILDROOT="$BUILD_ROOT/core"
    fi
    [ -f "$GUI_EXE" ] || fail "GUI release executable not found: $GUI_EXE"
    objdump -h "$GUI_EXE" | grep -q '\.rsrc' || \
        fail "Windows GUI executable has no .rsrc icon section"
    "$PYTHON" packaging/gui/windows/check_windows_subsystems.py \
        --gui "$GUI_EXE"

    rm -rf "$BUILD_ROOT/stage"
    mkdir -p "$STAGE/licenses"
    cp "$GUI_EXE" "$STAGE/xfoil.next-gui.exe"
    cp "packaging/gui/windows/Install XFOIL.next GUI.cmd" "$STAGE/"
    cp packaging/gui/windows/Install-XFOIL.next-GUI.ps1 "$STAGE/"
    cp "packaging/gui/windows/Uninstall XFOIL.next GUI.cmd" "$STAGE/"
    cp packaging/gui/windows/Uninstall-XFOIL.next-GUI.ps1 "$STAGE/"
    cp packaging/gui/windows/README-Windows.txt "$STAGE/README-Windows.txt"

    PATH="$QT_ROOT/bin:$PATH" \
        "$WINDEPLOYQT" --release --compiler-runtime --no-translations \
            --no-system-d3d-compiler --no-opengl-sw \
            --dir "$STAGE" "$STAGE/xfoil.next-gui.exe"

    for runtime_dll in libstdc++-6.dll libgcc_s_seh-1.dll libwinpthread-1.dll; do
        if [ ! -f "$STAGE/$runtime_dll" ]; then
            [ -f "$QT_ROOT/bin/$runtime_dll" ] || \
                fail "MinGW runtime DLL not found: $QT_ROOT/bin/$runtime_dll"
            cp "$QT_ROOT/bin/$runtime_dll" "$STAGE/$runtime_dll"
        fi
    done

    declare -A queued=()
    queue=()
    while IFS= read -r pe; do
        queue+=("$pe")
        queued["$(basename "$pe")"]=1
    done < <(find "$STAGE" -type f \( -iname '*.exe' -o -iname '*.dll' \) -print)
    index=0
    while [ "$index" -lt "${#queue[@]}" ]; do
        pe=${queue[$index]}
        index=$((index + 1))
        while IFS= read -r dependency; do
            name=$(basename "$dependency")
            if [ -n "${queued[$name]:-}" ]; then
                continue
            fi
            cp "$dependency" "$STAGE/$name"
            queued["$name"]=1
            queue+=("$STAGE/$name")
        done < <(ldd "$pe" 2>/dev/null | sed -n 's/.*=> \/ucrt64\/bin\/\([^ ]*\).*/\/ucrt64\/bin\/\1/p' || true)
    done

    cp LICENSE "$STAGE/LICENSE"
    cp packaging/gui/NOTICE.gui "$STAGE/NOTICE"
    cp packaging/gui/licenses/Qt-LGPLv3-GPLv3.txt "$STAGE/licenses/"
    cp packaging/portable/licenses/GCC-RUNTIME-EXCEPTION.txt "$STAGE/licenses/"
    cp packaging/portable/licenses/MINGW-W64-RUNTIME-NOTICES.txt "$STAGE/licenses/"
    cp packaging/portable/licenses/WINPTHREADS-MIT.txt "$STAGE/licenses/"
    cp packaging/portable/licenses/BSD-3-Clause-Clear.txt "$STAGE/licenses/"
    cp packaging/portable/licenses/MINGW-W64-ZPL-2.1.txt "$STAGE/licenses/"
    cp src/gui/3rdparty/qcustomplot-2.1.1/LICENSE \
        "$STAGE/licenses/QCustomPlot-GPL-3.0.txt"

    if [ -d "$QT_ROOT/share/licenses" ]; then
        for package in qt6-base qt6-svg pcre2 zlib zstd brotli freetype \
                harfbuzz md4c libpng libjpeg-turbo double-conversion libb2 \
                libgcc libstdc++ libwinpthread winpthreads; do
            [ -d "$QT_ROOT/share/licenses/$package" ] || continue
            mkdir -p "$STAGE/licenses/$package"
            cp -RL "$QT_ROOT/share/licenses/$package/." \
                "$STAGE/licenses/$package/"
        done
        for icu_license in "$QT_ROOT"/share/icu/*/LICENSE; do
            [ -f "$icu_license" ] || continue
            cp "$icu_license" "$STAGE/licenses/ICU-LICENSE.txt"
            break
        done
    fi

    GCC_VERSION=$(g++ --version 2>/dev/null | sed -n '1p' || true)
    QT_VERSION=$("$QT_ROOT/bin/qmake6" -query QT_VERSION 2>/dev/null || true)
    cat > "$STAGE/SOURCE-INFO.txt" <<EOF
XFOIL.next GUI package
======================

Package version:      ${PACKAGE_VERSION}
Product version:      ${PRODUCT_VERSION}
Source URL:           https://github.com/of33/XFOIL.next
Source commit:        ${XFOIL_SOURCE_COMMIT}
Source tree:          ${XFOIL_SOURCE_TREE}
Source reference:     ${XFOIL_SOURCE_REF}
Source date epoch:    ${SOURCE_DATE_EPOCH}
Target:               Windows x86-64 ZIP
Builder baseline:     Windows 11, MSYS2 UCRT64
Compiler:             ${GCC_VERSION:-unknown}
Qt:                   Qt ${QT_VERSION:-unknown} (MSYS2 UCRT64 kit)

The corresponding source is the complete source tree at the commit and tree
above. The ZIP is a binary distribution and does not contain the full source
tree.
EOF

    find "$STAGE" -exec touch -d "@$SOURCE_DATE_EPOCH" {} +
    printf '%s\n' "dxcompiler/dxil files in staging:" > "$BUILD_ROOT/dx-runtime.txt"
    find "$STAGE" -maxdepth 1 -type f -iname 'dx*.dll' \
        -printf '%f\n' >> "$BUILD_ROOT/dx-runtime.txt"
fi

[ -d "$STAGE" ] || fail "staging directory not found: $STAGE"
mkdir -p "$(dirname "$OUTPUT")"
"$PYTHON" packaging/gui/windows/package_gui_windows.py \
    --staging-dir "$STAGE" \
    --output "$OUTPUT" \
    --top-dir "$TOP_DIR" \
    --epoch "$SOURCE_DATE_EPOCH" \
    --expected-version "$PRODUCT_VERSION" \
    --inventory "$BUILD_ROOT/windows-inventory.json"

sha256sum "$OUTPUT" | tee "$OUTPUT.sha256"
printf 'Windows GUI ZIP: %s\n' "$OUTPUT"
