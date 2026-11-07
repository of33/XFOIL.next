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

# File: tools/build-spec-failure-harnesses.sh
# Builds and runs the focused Fortran harness for the
# SPECAL-/SPECCL convergence aborts (tests/fortran-reference/harnesses/
# specfailure) and produces the reproducible reference data below
# tests/data/core-spec-failure-reference/.
#
# The harness calls exclusively the canonical routines from
# fortran-src/{naca,spline,xutils,xgeom,xfoil,xgdes,xpanel,xoper,xsolve}.f
# unchanged (byte-faithful extraction as in the inviscid harness).
# It changes no Fortran source, no existing raw reference and no
# golden file is changed; the reference data are exclusively new.
#
# Modes (exactly one required):
# tools/build-spec-failure-harnesses.sh --write # build and run,
# # reference data below
# # tests/data/
# tools/build-spec-failure-harnesses.sh --check # build and run, Output
# # temporary and Byte-for-
# # Byte against tests/data/
#
# Optional: FC (Fortran compiler, default gfortran).

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HARNESS_DIR="${REPO_ROOT}/tests/fortran-reference/harnesses/specfailure"
DATA_DIR="${REPO_ROOT}/tests/data/core-spec-failure-reference"
BUILD_DIR="${REPO_ROOT}/build/core-spec-failure-harness"
PANELING_DIR="${REPO_ROOT}/build/core-paneling-harness"
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
  --routines PSILIN PSWLIN GGCALC QWCALC QDCALC STFIND QISET XYWAKE \
  --output "${BUILD_DIR}/xpanel_inviscid_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xoper.f \
  --routines SPECAL SPECCL MHINGE \
  --output "${BUILD_DIR}/xoper_inviscid_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xgdes.f \
  --routines GETXYF \
  --output "${BUILD_DIR}/xgdes_getxyf_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xsolve.f \
  --routines LUDCMP BAKSUB \
  --output "${BUILD_DIR}/xsolve_lu_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/userio.f \
  --routines ASKR \
  --output "${BUILD_DIR}/userio_askr_routines.f" > /dev/null

# Paneling extractions (PANGEN/TECALC, APCALC/NCALC, LEFIND chain)
# produce, if not present (same byte-faithful extraction).
if [[ ! -f "${PANELING_DIR}/xfoil_paneling_routines.f" ]]; then
  python3 tools/extract-fortran-routines.py \
    --source fortran-src/xfoil.f \
    --routines PANGEN TECALC \
    --output "${PANELING_DIR}/xfoil_paneling_routines.f" > /dev/null
  python3 tools/extract-fortran-routines.py \
    --source fortran-src/xgdes.f \
    --routines ABCOPY \
    --output "${PANELING_DIR}/xgdes_abcopy_routines.f" > /dev/null
  python3 tools/extract-fortran-routines.py \
    --source fortran-src/xpanel.f \
    --routines APCALC NCALC \
    --output "${PANELING_DIR}/xpanel_kernels_routines.f" > /dev/null
  python3 tools/extract-fortran-routines.py \
    --source fortran-src/xgeom.f \
    --routines LEFIND XLFIND NSFIND SOPPS \
    --output "${PANELING_DIR}/xgeom_lefind_routines.f" > /dev/null
fi

# Canonical Fortran objects (unchanged).
for f in naca spline xutils; do
  "${FC}" -c "${REPO_ROOT}/fortran-src/${f}.f" -o "${BUILD_DIR}/${f}.o"
done

# Extracted routine objects.
for f in xgeom_lefind_routines xfoil_paneling_routines \
         xgdes_abcopy_routines xpanel_kernels_routines \
         xfoil_inviscid_routines xpanel_inviscid_routines \
         xoper_inviscid_routines xgdes_getxyf_routines \
         xsolve_lu_routines userio_askr_routines; do
  local_dir="${BUILD_DIR}"
  if [[ ! -f "${local_dir}/${f}.f" ]]; then
    local_dir="${PANELING_DIR}"
  fi
  "${FC}" -c -I "${REPO_ROOT}/fortran-src" \
    "${local_dir}/${f}.f" -o "${BUILD_DIR}/${f}.o"
done

# Link without symbol suppression: all required symbols are present beforehand.
"${FC}" -c -I "${REPO_ROOT}/fortran-src" \
  "${HARNESS_DIR}/specfailure_harness.f" -o "${BUILD_DIR}/specfailure_harness.o"
# shellcheck disable=SC2086
# Negative check: osmap_DP.f is an inactive upstream alternative
# with the same OSMAP symbol as the canonical runtime implementation.
# The active and inactive OSMAP implementations must never be
# compiled or linked together (tools/check-osmap-selection.sh).
"${REPO_ROOT}/tools/check-osmap-selection.sh" "${BUILD_DIR}"
"${FC}" "${BUILD_DIR}/specfailure_harness.o" \
  "${BUILD_DIR}/naca.o" \
  "${BUILD_DIR}/spline.o" \
  "${BUILD_DIR}/xutils.o" \
  "${BUILD_DIR}/xgeom_lefind_routines.o" \
  "${BUILD_DIR}/xfoil_paneling_routines.o" \
  "${BUILD_DIR}/xpanel_kernels_routines.o" \
  "${BUILD_DIR}/xfoil_inviscid_routines.o" \
  "${BUILD_DIR}/xpanel_inviscid_routines.o" \
  "${BUILD_DIR}/xoper_inviscid_routines.o" \
  "${BUILD_DIR}/xgdes_getxyf_routines.o" \
  "${BUILD_DIR}/xsolve_lu_routines.o" \
  "${BUILD_DIR}/userio_askr_routines.o" \
  -o "${BUILD_DIR}/specfailure_harness"
"${BUILD_DIR}/specfailure_harness" > "${OUT_DIR}/specfailure_reference.txt"

if [[ "${MODE}" == "write" ]]; then
  echo "Harnesses successfully executed."
  echo "Reference data: ${DATA_DIR}/*.txt"
else
  # --check: byte-for-byte comparison against the existing reference data.
  if cmp -s "${OUT_DIR}/specfailure_reference.txt" \
      "${DATA_DIR}/specfailure_reference.txt"; then
    echo "Reference data unchanged: all 1 Files identical."
  else
    echo "Error: Reference data differ from (see above)." >&2
    exit 1
  fi
fi
