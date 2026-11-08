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

# file: tools/build-intx-half-harnesses.sh
# Builds and runs the focused Fortran harness for the canonical
# INTERX/HALF routines and produces the reproducible
# reference data below tests/data/core-intx-half-reference/.
#
# The harness calls exclusively the canonical routines from
# fortran-src/xfoil-6.99/{naca,xgeom,spline,xutils}.f unchanged. It changes
# no Fortran source, no raw reference and no golden file.
#
# Precision contract: the
# canonical Fortran build uses -fdefault-real-8
# -fdefault-double-8 (as fortran-bin/xfoil); unqualified REAL is
# REAL(8), directly bit-comparable with C++ double.
#
# Modes (exactly one required):
# tools/build-intx-half-harnesses.sh --write # build and run,
# # reference data below
# # tests/data/
# tools/build-intx-half-harnesses.sh --check # build and run, output
# # in a temporary
# # directory write and
# # byte-for-byte comparison against
# # tests/data/ compare.
#
# Optional: FC (Fortran compiler, default gfortran).

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HARNESS_DIR="${REPO_ROOT}/tests/fortran-reference/harnesses/intx-half"
DATA_DIR="${REPO_ROOT}/tests/data/core-intx-half-reference"
BUILD_DIR="${REPO_ROOT}/build/core-intx-half-harness"
FC="${FC:-gfortran}"
R8FLAGS=(-fdefault-real-8 -fdefault-double-8)

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

# Byte-faithful extraction of the actually called canonical
# routines (below build/; no Fortran source is changed).
python3 tools/extract-fortran-routines.py \
  --source fortran-src/xfoil-6.99/xgeom.f \
  --routines LEFIND XLFIND NSFIND SOPPS INTERX HALF \
  --output "${BUILD_DIR}/xgeom_intx_routines.f" > /dev/null

# Canonical Fortran-objects (unchanged; canonical R8-flags).
for f in naca spline xutils; do
  "${FC}" "${R8FLAGS[@]}" -c "${REPO_ROOT}/fortran-src/xfoil-6.99/${f}.f" \
    -o "${BUILD_DIR}/${f}.o"
done

"${FC}" "${R8FLAGS[@]}" -I "${REPO_ROOT}/fortran-src/xfoil-6.99" \
  -c "${BUILD_DIR}/xgeom_intx_routines.f" \
  -o "${BUILD_DIR}/xgeom_intx_routines.o"

# link without symbol suppression: all required symbols are present beforehand.
"${FC}" "${R8FLAGS[@]}" -I "${REPO_ROOT}/fortran-src/xfoil-6.99" \
  -c "${HARNESS_DIR}/intx_half_harness.f" \
  -o "${BUILD_DIR}/intx_half_harness.o"
# shellcheck disable=SC2086
# Negative check: osmap_DP.f is an inactive upstream alternative
# with the same OSMAP symbol as the canonical runtime implementation.
# The active and inactive OSMAP implementations must never be
# compiled or linked together (tools/check-osmap-selection.sh).
"${REPO_ROOT}/tools/check-osmap-selection.sh" "${BUILD_DIR}"
"${FC}" "${R8FLAGS[@]}" "${BUILD_DIR}/intx_half_harness.o" \
  "${BUILD_DIR}/naca.o" \
  "${BUILD_DIR}/xgeom_intx_routines.o" \
  "${BUILD_DIR}/spline.o" \
  "${BUILD_DIR}/xutils.o" \
  -o "${BUILD_DIR}/intx_half_harness"
"${BUILD_DIR}/intx_half_harness" > "${OUT_DIR}/intx_half_reference.txt"

if [[ "${MODE}" == "write" ]]; then
  echo "Harness successfully executed."
  echo "Reference data: ${DATA_DIR}/*.txt"
else
  if cmp -s "${OUT_DIR}/intx_half_reference.txt" \
      "${DATA_DIR}/intx_half_reference.txt"; then
    echo "Reference data unchanged: all 1 Files identical."
  else
    echo "Error: Reference data differ from (see above)." >&2
    exit 1
  fi
fi
