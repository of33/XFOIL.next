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

# File: tests/tools/run-library-noio.sh
# Run one armed L7 proof consumer in an empty read-only directory and
# compare the recursive directory inventory before and after.

set -u

if [ "$#" -ne 3 ]; then
  echo "usage: run-library-noio.sh <consumer> <shim> <tag>" >&2
  exit 2
fi

CONSUMER=$(realpath "$1")
SHIM=$(realpath "$2")
TAG=$3
WORK=$(mktemp -d "${TMPDIR:-/tmp}/xfoil-noio-${TAG}.XXXXXX")
LOG=$(mktemp "${TMPDIR:-/tmp}/xfoil-noio-${TAG}.log.XXXXXX")
RC=1

cleanup() {
  chmod 0755 "$WORK" 2>/dev/null || true
  rm -rf "$WORK" "$LOG"
}
trap cleanup EXIT

inventory() {
  (cd "$WORK" && find . -mindepth 1 -printf '%P|%y|%s\n' | sort)
}

before=$(inventory)
chmod 0555 "$WORK"

(
  cd "$WORK" || exit 1
  LD_PRELOAD="$SHIM" "$CONSUMER"
) >"$LOG" 2>&1
consumer_rc=$?

chmod 0755 "$WORK"
after=$(inventory)

if [ "$consumer_rc" -ne 0 ]; then
  echo "LibraryNoIoProof FAIL ($TAG): consumer exit $consumer_rc" >&2
  cat "$LOG" >&2
  exit 1
fi
if [ "$before" != "$after" ]; then
  echo "LibraryNoIoProof FAIL ($TAG): working directory changed" >&2
  diff -u <(printf '%s\n' "$before") <(printf '%s\n' "$after") >&2 || true
  cat "$LOG" >&2
  exit 1
fi
if ! grep -qx 'LibraryNoIoProof PASS' "$LOG"; then
  echo "LibraryNoIoProof FAIL ($TAG): unexpected output" >&2
  cat "$LOG" >&2
  exit 1
fi

echo "LibraryNoIoProof PASS ($TAG)"
RC=0
exit "$RC"

# End: tests/tools/run-library-noio.sh
