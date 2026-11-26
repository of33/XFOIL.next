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

# Prepare the current Linux or MSYS2 shell for the optional GUI build.
# Usage:
#   source scripts/setup-gui-build.sh /path/to/Qt/6.12.0/gcc_64
#   source scripts/setup-gui-build.sh --qt-root '/path/with spaces/Qt/6.12.0/kit'
#   scripts/setup-gui-build.sh /path/to/Qt/6.12.0/gcc_64

_script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
_repo_root=$(cd "$_script_dir/.." && pwd)
_is_sourced=0
if [ "${BASH_SOURCE[0]}" != "$0" ]; then
    _is_sourced=1
fi

_setup_gui_usage() {
    cat <<'EOF'
Usage:
  source scripts/setup-gui-build.sh [--qt-root] <Qt 6.12.0 kit path>

The script validates the kit and prepares QT_ROOT, PATH, QT_PLUGIN_PATH and
(on Linux) LD_LIBRARY_PATH for the current shell. It never installs Qt,
downloads anything, changes system settings, or edits shell profiles.

If no path is supplied, QT_ROOT is used. A unique Qt 6.12.0 kit below the
documented standard locations ($HOME/Qt, /opt/Qt, $USERPROFILE/Qt or C:/Qt)
may be selected. Multiple valid kits are rejected.

After sourcing, build from the repository root:
  make gui-check-qt
  make gui-app
  make test-gui

Run the script without 'source' to validate the kit and print the exact
export commands without changing the parent shell.
EOF
}

_setup_gui_fail() {
    printf 'setup-gui-build.sh: error: %s\n' "$*" >&2
    return 1
}

_setup_gui_find_from_standard_locations() {
    local base kit
    local -a candidates=()
    for base in "$HOME/Qt" "/opt/Qt" "${USERPROFILE:-}/Qt" "/c/Qt" "C:/Qt"; do
        [ -d "$base/6.12.0" ] || continue
        for kit in "$base"/6.12.0/*; do
            [ -d "$kit" ] && candidates+=("$kit")
        done
    done
    if [ "${#candidates[@]}" -eq 0 ]; then
        return 1
    fi

    local -a valid=()
    local candidate
    for candidate in "${candidates[@]}"; do
        if python3 "$_repo_root/tools/check-qt.py" \
            --qt-root "$candidate" --platform auto --cxx "${CXX:-g++}" \
            >/dev/null 2>&1; then
            valid+=("$candidate")
        fi
    done
    if [ "${#valid[@]}" -eq 1 ]; then
        printf '%s\n' "${valid[0]}"
        return 0
    fi
    if [ "${#valid[@]}" -eq 0 ]; then
        _setup_gui_fail "no valid Qt 6.12.0 kit found under standard locations"
        return 1
    fi
    printf 'setup-gui-build.sh: error: multiple valid Qt 6.12.0 kits found:\n' >&2
    printf '  %s\n' "${valid[@]}" >&2
    _setup_gui_fail "specify one kit explicitly with --qt-root"
    return 1
}

_setup_gui_add_path() {
    local variable="$1"
    local directory="$2"
    local current="${!variable:-}"
    local part result=""
    local -a parts=()
    IFS=: read -r -a parts <<< "$current"
    for part in "${parts[@]}"; do
        [ -n "$part" ] || continue
        [ "$part" = "$directory" ] && continue
        if [ -z "$result" ]; then result="$part"; else result="$result:$part"; fi
    done
    if [ -z "$result" ]; then result="$directory"; else result="$directory:$result"; fi
    printf -v "$variable" '%s' "$result"
    export "$variable"
}

_setup_gui_main() {
_setup_gui_qt_root=""
while [ "$#" -gt 0 ]; do
    case "$1" in
        -h|--help)
            _setup_gui_usage
            return 0 2>/dev/null || exit 0
            ;;
        --qt-root)
            [ "$#" -ge 2 ] || { _setup_gui_fail "--qt-root needs a path"; return 1; }
            _setup_gui_qt_root="$2"
            shift 2
            ;;
        --)
            shift
            break
            ;;
        -*)
            _setup_gui_fail "unknown option: $1"
            return 1
            ;;
        *)
            [ -z "$_setup_gui_qt_root" ] || {
                _setup_gui_fail "only one Qt root may be supplied"
                return 1
            }
            _setup_gui_qt_root="$1"
            shift
            ;;
    esac
done

if [ -z "$_setup_gui_qt_root" ]; then
    _setup_gui_qt_root="${QT_ROOT:-}"
fi
if [ -z "$_setup_gui_qt_root" ]; then
    _setup_gui_qt_root=$(_setup_gui_find_from_standard_locations) || {
        return 1
    }
fi

if [ ! -d "$_setup_gui_qt_root" ]; then
    _setup_gui_fail "Qt root does not exist: $_setup_gui_qt_root"
    return 1
fi
_setup_gui_qt_root=$(cd "$_setup_gui_qt_root" && pwd)

if ! python3 "$_repo_root/tools/check-qt.py" \
    --qt-root "$_setup_gui_qt_root" --platform auto --cxx "${CXX:-g++}"; then
    _setup_gui_fail "Qt kit validation failed"
    return 1
fi

if [ "$_is_sourced" -eq 1 ]; then
    export QT_ROOT="$_setup_gui_qt_root"
    _setup_gui_add_path PATH "$QT_ROOT/bin"
    export QT_PLUGIN_PATH="$QT_ROOT/plugins"
    case "$(uname -s 2>/dev/null || true)" in
        MINGW*|MSYS*|CYGWIN*) ;;
        *) _setup_gui_add_path LD_LIBRARY_PATH "$QT_ROOT/lib" ;;
    esac
    printf '\nGUI build environment configured for this shell.\n'
    printf 'Recommended commands from %s:\n' "$_repo_root"
    printf '  make gui-check-qt\n'
    printf '  make gui-app\n'
    printf '  make test-gui\n'
    printf '  bin/release/xfoil.next\n'
else
    printf '\nThe script was executed, not sourced; the parent shell was not changed.\n'
    printf 'To configure the current shell, run:\n'
    printf '  source scripts/setup-gui-build.sh %q\n\n' "$_setup_gui_qt_root"
    printf 'Equivalent exports:\n'
    printf '  export QT_ROOT=%q\n' "$_setup_gui_qt_root"
    printf '  export PATH=%q:$PATH\n' "$_setup_gui_qt_root/bin"
    printf '  export QT_PLUGIN_PATH=%q\n' "$_setup_gui_qt_root/plugins"
    case "$(uname -s 2>/dev/null || true)" in
        MINGW*|MSYS*|CYGWIN*) ;;
        *) printf '  export LD_LIBRARY_PATH=%q:$LD_LIBRARY_PATH\n' "$_setup_gui_qt_root/lib" ;;
    esac
    printf '\nThen run: make gui-check-qt && make gui-app && make test-gui\n'
fi

}

if [ "$_is_sourced" -eq 1 ]; then
    _setup_gui_main "$@"
    _setup_gui_status=$?
    unset _setup_gui_main _setup_gui_fail _setup_gui_usage
    unset _setup_gui_find_from_standard_locations _setup_gui_add_path
    unset _script_dir _repo_root _is_sourced _setup_gui_qt_root
    return "$_setup_gui_status"
else
    _setup_gui_main "$@"
    _setup_gui_status=$?
    unset _setup_gui_main _setup_gui_fail _setup_gui_usage
    unset _setup_gui_find_from_standard_locations _setup_gui_add_path
    unset _script_dir _repo_root _is_sourced _setup_gui_qt_root
    exit "$_setup_gui_status"
fi
