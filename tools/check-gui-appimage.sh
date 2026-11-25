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

# Exercise the packaged GUI AppImage in a clean environment. The direct run
# uses FUSE; the second run uses APPIMAGE_EXTRACT_AND_RUN=1.

set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT"

APPIMAGE=${GUI_APPIMAGE:-}
EXPECTED_VERSION=${XFOIL_PRODUCT_VERSION:-}
WORK=${GUI_APPIMAGE_CHECK_DIR:-build/portable/gui/check-linux}
case "$WORK" in
    /*) ;;
    *) WORK="$ROOT/$WORK" ;;
esac
SKIP_DISPLAY=0

fail() {
    printf 'check-gui-appimage.sh: error: %s\n' "$*" >&2
    exit 1
}

while [ $# -gt 0 ]; do
    case "$1" in
        --appimage) APPIMAGE=$2; shift 2 ;;
        --expected-version) EXPECTED_VERSION=$2; shift 2 ;;
        --work-dir) WORK=$2; shift 2 ;;
        --skip-display) SKIP_DISPLAY=1; shift ;;
        --help|-h) echo "Usage: tools/check-gui-appimage.sh --appimage FILE --expected-version VER"; exit 0 ;;
        *) fail "unknown argument: $1" ;;
    esac
done

[ -n "$APPIMAGE" ] || fail "--appimage is required"
[ -f "$APPIMAGE" ] || fail "AppImage not found: $APPIMAGE"
case "$APPIMAGE" in
    /*) ;;
    *) APPIMAGE="$ROOT/$APPIMAGE" ;;
esac
[ -n "$EXPECTED_VERSION" ] || fail "--expected-version is required"
command -v python3 >/dev/null 2>&1 || fail "python3 is required"

rm -rf "$WORK"
mkdir -p "$WORK/home" "$WORK/runtime"
chmod 700 "$WORK/home" "$WORK/runtime"

probe() {
    local label=$1
    shift
    local run_home="$WORK/home-$label"
    local log="$WORK/$label.log"
    local pid
    rm -rf "$run_home"
    mkdir -p "$run_home/config" "$run_home/cache"
    chmod 700 "$run_home" "$run_home/config" "$run_home/cache"
    setsid env -i \
        HOME="$run_home" \
        USER="${USER:-agent}" \
        PATH=/usr/local/bin:/usr/bin:/bin \
        LANG=C.UTF-8 \
        DISPLAY="${DISPLAY:-:0}" \
        XAUTHORITY="${XAUTHORITY:-$HOME/.Xauthority}" \
        XDG_CONFIG_HOME="$run_home/config" \
        XDG_CACHE_HOME="$run_home/cache" \
        XDG_RUNTIME_DIR="$WORK/runtime" \
        "$@" >"$log" 2>&1 &
    pid=$!
    window_seen=0
    for _ in $(seq 1 15); do
        sleep 1
        if ! kill -0 "$pid" 2>/dev/null; then
            cat "$log" >&2 || true
            fail "$label exited during startup"
        fi
        if grep -Eq 'Could not load the Qt platform plugin|cannot open shared object file|This application failed to start' "$log"; then
            kill -TERM -- "-$pid" 2>/dev/null || true
            cat "$log" >&2 || true
            fail "$label reported a missing or unloadable runtime component"
        fi
        if [ "$label" = xcb ] && command -v xwininfo >/dev/null 2>&1; then
            xwininfo -root -tree >"$WORK/$label.xwininfo" 2>/dev/null || true
            if grep -Eiq 'XFOIL\.next( GUI| [0-9])' "$WORK/$label.xwininfo"; then
                window_seen=1
                break
            fi
        elif [ "$label" != xcb ]; then
            sleep 2
            break
        fi
    done
    if [ "$label" = xcb ] && [ "$window_seen" -ne 1 ]; then
        kill -TERM -- "-$pid" 2>/dev/null || true
        tail -40 "$WORK/$label.xwininfo" >&2 || true
        fail "xcb run did not expose the XFOIL.next GUI window"
    fi
    kill -TERM -- "-$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
    printf '%s: PASS\n' "$label"
}

if [ "$SKIP_DISPLAY" -eq 0 ]; then
    probe xcb "$APPIMAGE"
    probe extract-run env APPIMAGE_EXTRACT_AND_RUN=1 "$APPIMAGE"
fi
probe offscreen env QT_QPA_PLATFORM=offscreen "$APPIMAGE"

rm -rf "$WORK/extract"
mkdir -p "$WORK/extract"
(
    cd "$WORK/extract"
    APPIMAGE_EXTRACT_AND_RUN=1 "$APPIMAGE" --appimage-extract >/dev/null
)
python3 packaging/gui/linux/check_appimage.py \
    --appdir "$WORK/extract/squashfs-root" \
    --expected-version "$EXPECTED_VERSION" \
    --inventory "$WORK/extracted-inventory.json" \
    --expected-icon resources/gui/xfoil-next-icon.png

printf 'GUI AppImage checks: PASS\n'
