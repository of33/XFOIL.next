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

# file: tools/build-math-harnesses.sh
# Builds and runs the focused Fortran harnesses for the numerics core
# (tests/fortran-reference/harnesses/math) and produces the reproducible
# reference data below tests/data/core-math-reference/.
#
# The harnesses call exclusively the canonical routines from
# fortran-src/xfoil-6.99/{xutils,sort,spline,xsolve}.f unchanged. They change
# no Fortran source, no raw reference and no golden file.
#
# Generated binaries are written only below build/.
#
# Modes (exactly one required):
# tools/build-math-harnesses.sh --write # build and run, reference data
# # below tests/data/
# # (mutates the data folder)
# tools/build-math-harnesses.sh --check # build and run, output in a
# # temporary directory
# # written and compared byte-for-byte
# # against tests/data/.
# # No repository file is modified.
#
# Without a mode (or with an unknown argument) the script prints a usage
# error and exits with exit code 2.
#
# Optional: FC (Fortran compiler, default gfortran).

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HARNESS_DIR="${REPO_ROOT}/tests/fortran-reference/harnesses/math"
DATA_DIR="${REPO_ROOT}/tests/data/core-math-reference"
BUILD_DIR="${REPO_ROOT}/build/core-math-harness"
FC="${FC:-gfortran}"

MODE=""
case "${1:-}" in
  --write) MODE="write" ;;
  --check) MODE="check" ;;
  *)
    echo "Error: exactly a Mode is required (--write or --check)." >&2
    echo "Use: $0 {--write|--check}" >&2
    exit 2
    ;;
esac

if [[ ! -x "$(command -v "${FC}")" ]]; then
  echo "Error: Fortran-Compiler '${FC}' not found." >&2
  echo "The numerics harnesses require a Fortran compiler." >&2
  exit 1
fi

mkdir -p "${BUILD_DIR}"

if [[ "${MODE}" == "write" ]]; then
  mkdir -p "${DATA_DIR}"
  OUT_DIR="${DATA_DIR}"
elif [[ "${MODE}" == "check" ]]; then
  OUT_DIR="$(mktemp -d)"
  trap 'rm -rf "${OUT_DIR}"' EXIT
fi

# Canonical Fortran-objects (unchanged).
for f in xutils sort spline xsolve; do
  "${FC}" -c "${REPO_ROOT}/fortran-src/xfoil-6.99/${f}.f" -o "${BUILD_DIR}/${f}.o"
done

run_harness() {
  local harness="$1"
  local name="$2"
  "${FC}" -c "${HARNESS_DIR}/${harness}.f" -o "${BUILD_DIR}/${harness}.o"
  # Negative check: osmap_DP.f is an inactive upstream alternative
  # with the same OSMAP symbol as the canonical runtime implementation.
  # The active and inactive OSMAP implementations must never be
  # compiled or linked together (tools/check-osmap-selection.sh).
  "${REPO_ROOT}/tools/check-osmap-selection.sh" "${BUILD_DIR}"
  "${FC}" "${BUILD_DIR}/${harness}.o" \
    $(for f in xutils sort spline xsolve; do echo "${BUILD_DIR}/${f}.o"; done) \
    -o "${BUILD_DIR}/${harness}"
  "${BUILD_DIR}/${harness}" > "${OUT_DIR}/${name}_reference.txt"
}

run_harness utils_harness utils
run_harness sort_harness sort
run_harness spline_harness spline
run_harness linalg_harness linalg

if [[ "${MODE}" == "write" ]]; then
  echo "Harnesses successfully executed."
  echo "Reference data: ${DATA_DIR}/*.txt"
else
  # --check: byte-for-byte comparison against the existing reference data.
  mismatch=0
  for name in utils sort spline linalg; do
    if ! cmp -s "${OUT_DIR}/${name}_reference.txt" \
        "${DATA_DIR}/${name}_reference.txt"; then
      echo "Deviation: ${name}_reference.txt" >&2
      mismatch=1
    fi
  done
  if [[ "${mismatch}" == "0" ]]; then
    echo "Reference data unchanged: all 4 Files identical."
  else
    echo "Error: Reference data differ from (see above)." >&2
    exit 1
  fi
fi
