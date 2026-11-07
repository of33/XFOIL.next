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

# file: tools/build-cpi-harnesses.sh
# Builds and runs the focused Fortran harness for the canonical
# CPI producer chain and produces the reproducible
# reference data below tests/data/core-cpi-reference/.
#
# The harness calls exclusively the canonical routines from
# fortran-src/{naca,xgeom,spline,xutils,xfoil,xpanel,xoper,xsolve}.f
# unchanged. It changes no Fortran source, no raw reference
# and no golden file.
#
# Precision contract: the
# canonical Fortran build uses -fdefault-real-8
# -fdefault-double-8 (as fortran-bin/xfoil); unqualified REAL is
# REAL(8), directly bit-comparable with C++ double.
#
# Modes (exactly one required):
# tools/build-cpi-harnesses.sh --write # build and run,
# # reference data below
# # tests/data/
# tools/build-cpi-harnesses.sh --check # build and run, output in
# # a temporary directory
# # written and compared byte-for-byte
# # against tests/data/.
#
# Optional: FC (Fortran compiler, default gfortran).

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HARNESS_DIR="${REPO_ROOT}/tests/fortran-reference/harnesses/cpi"
DATA_DIR="${REPO_ROOT}/tests/data/core-cpi-reference"
BUILD_DIR="${REPO_ROOT}/build/core-cpi-harness"
PANELING_DIR="${REPO_ROOT}/build/core-paneling-harness"
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
# Shared paneling-extraction directory: fills itself on demand
# (byte-faithful extraction); --check must start from clean.
mkdir -p "${PANELING_DIR}"

if [[ "${MODE}" == "write" ]]; then
  mkdir -p "${DATA_DIR}"
  OUT_DIR="${DATA_DIR}"
elif [[ "${MODE}" == "check" ]]; then
  OUT_DIR="$(mktemp -d)"
  trap 'rm -rf "${OUT_DIR}"' EXIT
fi

# Byte-faithful extraction of the required routines (below build/).
python3 tools/extract-fortran-routines.py \
  --source fortran-src/xfoil.f \
  --routines MRCL COMSET CPCALC CLCALC \
  --output "${BUILD_DIR}/xfoil_inviscid_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xpanel.f \
  --routines GGCALC QISET PSILIN \
  --output "${BUILD_DIR}/xpanel_cpi_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xoper.f \
  --routines SPECAL MHINGE \
  --output "${BUILD_DIR}/xoper_cpi_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xgdes.f \
  --routines GETXYF \
  --output "${BUILD_DIR}/xgdes_getxyf_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/userio.f \
  --routines ASKR \
  --output "${BUILD_DIR}/userio_askr_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xsolve.f \
  --routines LUDCMP BAKSUB \
  --output "${BUILD_DIR}/xsolve_lu_routines.f" > /dev/null

# Paneling extractions (PANGEN/TECALC, APCALC/NCALC, LEFIND chain)
# produce, if not present (same byte-faithful extraction).
if [[ ! -f "${PANELING_DIR}/xfoil_paneling_routines.f" ]]; then
  python3 tools/extract-fortran-routines.py \
    --source fortran-src/xfoil.f \
    --routines PANGEN TECALC \
    --output "${PANELING_DIR}/xfoil_paneling_routines.f" > /dev/null
  python3 tools/extract-fortran-routines.py \
    --source fortran-src/xpanel.f \
    --routines APCALC NCALC \
    --output "${PANELING_DIR}/xpanel_kernels_routines.f" > /dev/null
  python3 tools/extract-fortran-routines.py \
    --source fortran-src/xgeom.f \
    --routines LEFIND XLFIND NSFIND SOPPS \
    --output "${PANELING_DIR}/xgeom_lefind_routines.f" > /dev/null
fi

# Canonical Fortran-objects (unchanged, canonical R8-flags).
for f in naca spline xutils; do
  "${FC}" "${R8FLAGS[@]}" -c "${REPO_ROOT}/fortran-src/${f}.f" \
    -o "${BUILD_DIR}/${f}.o"
done

# Extracted routine objects.
for f in xfoil_inviscid_routines xpanel_cpi_routines xoper_cpi_routines \
         xsolve_lu_routines xgdes_getxyf_routines userio_askr_routines; do
  "${FC}" "${R8FLAGS[@]}" -I "${REPO_ROOT}/fortran-src" \
    -c "${BUILD_DIR}/${f}.f" -o "${BUILD_DIR}/${f}.o"
done
for f in xgeom_lefind_routines xfoil_paneling_routines \
         xpanel_kernels_routines; do
  "${FC}" "${R8FLAGS[@]}" -I "${REPO_ROOT}/fortran-src" \
    -c "${PANELING_DIR}/${f}.f" -o "${BUILD_DIR}/${f}.o"
done

# link without symbol suppression: all required symbols are present beforehand.
"${FC}" "${R8FLAGS[@]}" -I "${REPO_ROOT}/fortran-src" \
  -c "${HARNESS_DIR}/cpi_harness.f" -o "${BUILD_DIR}/cpi_harness.o"
# shellcheck disable=SC2086
# Negative check: osmap_DP.f is an inactive upstream alternative
# with the same OSMAP symbol as the canonical runtime implementation.
# The active and inactive OSMAP implementations must never be
# compiled or linked together (tools/check-osmap-selection.sh).
"${REPO_ROOT}/tools/check-osmap-selection.sh" "${BUILD_DIR}"
"${FC}" "${R8FLAGS[@]}" "${BUILD_DIR}/cpi_harness.o" \
  "${BUILD_DIR}/naca.o" \
  "${BUILD_DIR}/spline.o" \
  "${BUILD_DIR}/xutils.o" \
  "${BUILD_DIR}/xgeom_lefind_routines.o" \
  "${BUILD_DIR}/xfoil_paneling_routines.o" \
  "${BUILD_DIR}/xpanel_kernels_routines.o" \
  "${BUILD_DIR}/xfoil_inviscid_routines.o" \
  "${BUILD_DIR}/xpanel_cpi_routines.o" \
  "${BUILD_DIR}/xoper_cpi_routines.o" \
  "${BUILD_DIR}/xgdes_getxyf_routines.o" \
  "${BUILD_DIR}/userio_askr_routines.o" \
  "${BUILD_DIR}/xsolve_lu_routines.o" \
  -o "${BUILD_DIR}/cpi_harness"
"${BUILD_DIR}/cpi_harness" > "${OUT_DIR}/cpi_reference.txt"

if [[ "${MODE}" == "write" ]]; then
  echo "Harness successfully executed."
  echo "Reference data: ${DATA_DIR}/*.txt"
else
  # --check: byte-for-byte comparison against the existing reference data.
  if cmp -s "${OUT_DIR}/cpi_reference.txt" \
      "${DATA_DIR}/cpi_reference.txt"; then
    echo "Reference data unchanged: all 1 Files identical."
  else
    echo "Error: Reference data differ from (see above)." >&2
    exit 1
  fi
fi
