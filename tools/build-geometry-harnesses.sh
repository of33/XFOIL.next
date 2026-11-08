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

# file: tools/build-geometry-harnesses.sh
# Builds and runs the focused Fortran harnesses for the geometry core
# (tests/fortran-reference/harnesses/geometry) and produces the
# reproducible reference data below tests/data/core-geometry-reference/.
#
# The harnesses call exclusively the canonical routines from
# fortran-src/xfoil-6.99/{naca,xgeom,spline,xutils,xfoil}.f unchanged. They
# do not change any Fortran source, any raw reference or any golden file.
#
# ROTATE/TECALC lie in xfoil.f together with PROGRAM XFOIL; xgeom.f
# contains PLTCRV with references to a plot library not contained in
# fortran-src/xfoil-6.99. Neither is ever called in the harness and neither is
# linked. Instead, tools/extract-fortran-routines.py extracts the
# actually invoked routines blocks byte-faithful from the canonical
# sources and writes them below build/; only these objects are linked.
# No linker options for symbol suppression are used: the
# harness links only, if all actually required symbols are present.
#
# Linked objects:
# naca.o NACA4/NACA5 (fortran-src/xfoil-6.99/naca.f, unchanged)
# xgeom_routines.o 19 needed xgeom.f routines (byte-faithful
# extracted, without PLTCRV)
# spline.o spline routines (fortran-src/xfoil-6.99/spline.f,
# unchanged)
# xutils.o ATANC etc. (fortran-src/xfoil-6.99/xutils.f, unchanged)
# xfoil_geom_routines.o ROTATE/TECALC (byte-faithfully extracted, without
# PROGRAM XFOIL)
#
# Generated binaries are written only below build/.
#
# Modes (exactly one required):
# tools/build-geometry-harnesses.sh --write # build and run,
# # reference data below
# # tests/data/
# # (mutates the data folder)
# tools/build-geometry-harnesses.sh --check # build and run, output in
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
HARNESS_DIR="${REPO_ROOT}/tests/fortran-reference/harnesses/geometry"
DATA_DIR="${REPO_ROOT}/tests/data/core-geometry-reference"
BUILD_DIR="${REPO_ROOT}/build/core-geometry-harness"
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
  echo "The geometry harnesses require a Fortran compiler." >&2
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
  --source fortran-src/xfoil-6.99/xgeom.f \
  --routines LEFIND XLFIND NSFIND SOPPS NORM GEOPAR AECALC TCCALC \
             YSYM LERSCL SSS CLIS CANG INTER INTERX IJSECT AREFINE \
             SCHECK HALF \
  --output "${BUILD_DIR}/xgeom_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xfoil-6.99/xfoil.f \
  --routines ROTATE TECALC \
  --output "${BUILD_DIR}/xfoil_geom_routines.f" > /dev/null

# Canonical Fortran-objects (unchanged; without xgeom.o/xfoil.o).
for f in naca spline xutils; do
  "${FC}" -c "${REPO_ROOT}/fortran-src/xfoil-6.99/${f}.f" -o "${BUILD_DIR}/${f}.o"
done

# Extracted routine objects (byte-faithful blocks from xgeom.f/xfoil.f).
"${FC}" -c -I "${REPO_ROOT}/fortran-src/xfoil-6.99" \
  "${BUILD_DIR}/xgeom_routines.f" -o "${BUILD_DIR}/xgeom_routines.o"
"${FC}" -c -I "${REPO_ROOT}/fortran-src/xfoil-6.99" \
  "${BUILD_DIR}/xfoil_geom_routines.f" -o "${BUILD_DIR}/xfoil_geom_routines.o"

# link without symbol suppression: all required symbols are present beforehand.
run_harness() {
  local harness="$1"
  local name="$2"
  "${FC}" -c -I "${REPO_ROOT}/fortran-src/xfoil-6.99" \
    "${HARNESS_DIR}/${harness}.f" -o "${BUILD_DIR}/${harness}.o"
  # Negative check: osmap_DP.f is an inactive upstream alternative
  # with the same OSMAP symbol as the canonical runtime implementation.
  # The active and inactive OSMAP implementations must never be
  # compiled or linked together (tools/check-osmap-selection.sh).
  "${REPO_ROOT}/tools/check-osmap-selection.sh" "${BUILD_DIR}"
  # shellcheck disable=SC2086
  "${FC}" "${BUILD_DIR}/${harness}.o" \
    "${BUILD_DIR}/naca.o" \
    "${BUILD_DIR}/xgeom_routines.o" \
    "${BUILD_DIR}/spline.o" \
    "${BUILD_DIR}/xutils.o" \
    "${BUILD_DIR}/xfoil_geom_routines.o" \
    -o "${BUILD_DIR}/${harness}"
  "${BUILD_DIR}/${harness}" > "${OUT_DIR}/${name}_reference.txt"
}

run_harness naca_harness naca
run_harness le_pipeline_harness le_pipeline
run_harness geopar_harness geopar
run_harness transform_harness transform
run_harness interp_harness interp
run_harness xfoil_geom_harness xfoil_geom

if [[ "${MODE}" == "write" ]]; then
  echo "Harnesses successfully executed."
  echo "Reference data: ${DATA_DIR}/*.txt"
else
  # --check: byte-for-byte comparison against the existing reference data.
  mismatch=0
  for name in naca le_pipeline geopar transform interp xfoil_geom; do
    if ! cmp -s "${OUT_DIR}/${name}_reference.txt" \
        "${DATA_DIR}/${name}_reference.txt"; then
      echo "Deviation: ${name}_reference.txt" >&2
      mismatch=1
    fi
  done
  if [[ "${mismatch}" == "0" ]]; then
    echo "Reference data unchanged: all 6 Files identical."
  else
    echo "Error: Reference data differ from (see above)." >&2
    exit 1
  fi
fi
