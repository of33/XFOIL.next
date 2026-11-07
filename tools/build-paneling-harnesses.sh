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

# file: tools/build-paneling-harnesses.sh
# Builds and runs the focused Fortran harness for the paneling core
# (tests/fortran-reference/harnesses/paneling) and produces the
# reproducible reference data below tests/data/core-paneling-reference/.
#
# The harness calls the canonical routines from
# fortran-src/{naca,xgeom,spline,xutils,xfoil,xgdes,xpanel}.f unchanged.
# It changes no Fortran source, no raw reference and no
# golden file.
#
# PANGEN/TECALC lie in xfoil.f, ABCOPY in xgdes.f and NCALC/APCALC in
# xpanel.f together with unneeded routines (PROGRAM XFOIL,
# solver and plot dependencies). Neither is ever called in the
# harness and is not linked. Instead
# tools/extract-fortran-routines.py extracts the actually called
# routine blocks byte-faithfully from the canonical sources and writes
# them below build/; only these objects are linked. No
# linker options for symbol suppression are used: the harness links
# only, if all actually required symbols are present.
#
# Linked objects:
# naca.o NACA4 (fortran-src/naca.f, unchanged)
# spline.o SCALC/SEGSPL/SEVAL/DEVAL/CURV/TRISOL
# (fortran-src/spline.f, unchanged)
# xutils.o ATANC etc. (fortran-src/xutils.f,
# unchanged)
# xgeom_lefind_routines.o LEFIND/XLFIND/NSFIND/SOPPS (byte-faithful
# extracted from xgeom.f)
# xfoil_paneling_routines.o PANGEN/TECALC (byte-faithfully extracted from
# xfoil.f, without PROGRAM XFOIL)
# xgdes_abcopy_routines.o ABCOPY (byte-faithfully extracted from xgdes.f)
# xpanel_kernels_routines.o APCALC/NCALC (byte-faithfully extracted from
# xpanel.f)
#
# Generated binaries are written only below build/.
#
# Modes (exactly one required):
# tools/build-paneling-harnesses.sh --write # build and run,
# # reference data below
# # tests/data/
# # (mutates the data folder)
# tools/build-paneling-harnesses.sh --check # build and run, output in
# # a temporary directory
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
HARNESS_DIR="${REPO_ROOT}/tests/fortran-reference/harnesses/paneling"
DATA_DIR="${REPO_ROOT}/tests/data/core-paneling-reference"
BUILD_DIR="${REPO_ROOT}/build/core-paneling-harness"
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
  echo "The paneling harnesses require a Fortran compiler." >&2
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

# Byte-faithful extraction of the actually called canonical routines
# (below build/; no Fortran source is changed).
python3 tools/extract-fortran-routines.py \
  --source fortran-src/xgeom.f \
  --routines LEFIND XLFIND NSFIND SOPPS \
  --output "${BUILD_DIR}/xgeom_lefind_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xfoil.f \
  --routines PANGEN TECALC \
  --output "${BUILD_DIR}/xfoil_paneling_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xgdes.f \
  --routines ABCOPY \
  --output "${BUILD_DIR}/xgdes_abcopy_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xpanel.f \
  --routines APCALC NCALC \
  --output "${BUILD_DIR}/xpanel_kernels_routines.f" > /dev/null

# Canonical Fortran-objects (unchanged; without xgeom.o/xfoil.o/xgdes.o/
# xpanel.o).
for f in naca spline xutils; do
  "${FC}" -c "${REPO_ROOT}/fortran-src/${f}.f" -o "${BUILD_DIR}/${f}.o"
done

# Extracted routine objects (byte-faithful blocks from xgeom.f/xfoil.f/
# xgdes.f/xpanel.f).
for f in xgeom_lefind_routines xfoil_paneling_routines \
         xgdes_abcopy_routines xpanel_kernels_routines; do
  "${FC}" -c -I "${REPO_ROOT}/fortran-src" \
    "${BUILD_DIR}/${f}.f" -o "${BUILD_DIR}/${f}.o"
done

# link without symbol suppression: all required symbols are present beforehand.
"${FC}" -c -I "${REPO_ROOT}/fortran-src" \
  "${HARNESS_DIR}/paneling_harness.f" -o "${BUILD_DIR}/paneling_harness.o"
# shellcheck disable=SC2086
# Negative check: osmap_DP.f is an inactive upstream alternative
# with the same OSMAP symbol as the canonical runtime implementation.
# The active and inactive OSMAP implementations must never be
# compiled or linked together (tools/check-osmap-selection.sh).
"${REPO_ROOT}/tools/check-osmap-selection.sh" "${BUILD_DIR}"
"${FC}" "${BUILD_DIR}/paneling_harness.o" \
  "${BUILD_DIR}/naca.o" \
  "${BUILD_DIR}/spline.o" \
  "${BUILD_DIR}/xutils.o" \
  "${BUILD_DIR}/xgeom_lefind_routines.o" \
  "${BUILD_DIR}/xfoil_paneling_routines.o" \
  "${BUILD_DIR}/xgdes_abcopy_routines.o" \
  "${BUILD_DIR}/xpanel_kernels_routines.o" \
  -o "${BUILD_DIR}/paneling_harness"
"${BUILD_DIR}/paneling_harness" > "${OUT_DIR}/paneling_reference.txt"

# Guard-case harness: expects the Fortran STOP message
# 'Panel: Too many panels. Increase IQX in XFOIL.INC'. The Fortran STOP
# ends in gfortran with exit code 0, so the message is checked in the
# reference file (not the exit code).
"${FC}" -c -I "${REPO_ROOT}/fortran-src" \
  "${HARNESS_DIR}/pangen_limit_harness.f" \
  -o "${BUILD_DIR}/pangen_limit_harness.o"
# shellcheck disable=SC2086
"${FC}" "${BUILD_DIR}/pangen_limit_harness.o" \
  "${BUILD_DIR}/naca.o" \
  "${BUILD_DIR}/spline.o" \
  "${BUILD_DIR}/xutils.o" \
  "${BUILD_DIR}/xgeom_lefind_routines.o" \
  "${BUILD_DIR}/xfoil_paneling_routines.o" \
  "${BUILD_DIR}/xgdes_abcopy_routines.o" \
  "${BUILD_DIR}/xpanel_kernels_routines.o" \
  -o "${BUILD_DIR}/pangen_limit_harness"
"${BUILD_DIR}/pangen_limit_harness" \
  > "${OUT_DIR}/pangen_limit_reference.txt" 2>&1
if ! grep -q "STOP PANEL: Too many panels" \
    "${OUT_DIR}/pangen_limit_reference.txt"; then
  echo "Error: guard-case harness triggered no STOP." >&2
  exit 1
fi

if [[ "${MODE}" == "write" ]]; then
  echo "Harnesses successfully executed."
  echo "Reference data: ${DATA_DIR}/*.txt"
else
  # --check: byte-for-byte comparison against the existing reference data.
  mismatch=0
  for name in paneling pangen_limit; do
    if ! cmp -s "${OUT_DIR}/${name}_reference.txt" \
        "${DATA_DIR}/${name}_reference.txt"; then
      echo "Deviation: ${name}_reference.txt" >&2
      mismatch=1
    fi
  done
  if [[ "${mismatch}" == "0" ]]; then
    echo "Reference data unchanged: all 2 Files identical."
  else
    echo "Error: Reference data differ from (see above)." >&2
    exit 1
  fi
fi
