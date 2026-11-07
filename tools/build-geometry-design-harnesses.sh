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

# file: tools/build-geometry-design-harnesses.sh
# Builds and runs the focused Fortran harnesses for the geometry-design core
# (tests/fortran-reference/harnesses/geometry-design) and produces the
# reproducible reference data below tests/data/core-geometry-design-reference/.
#
# The harnesses call exclusively the canonical routines from
# fortran-src/{xtcam,xgeom,spline,sort,naca,xutils}.f unchanged. They
# do not change any Fortran source, any raw reference or any golden file.
#
# GETCAM/GETMAX/CPCAM lie in xtcam.f together with CAMB (menu/plot logic
# with plot-library references). CAMB is never called in the harness and
# is not linked. Instead, tools/extract-fortran-routines.py extracts
# the actually invoked routines blocks byte-faithful from the
# canonical sources and writes them below build/; only these objects are
# linked. No linker options for symbol suppression are used.
#
# Linked objects:
# naca.o NACA4 (fortran-src/naca.f, unchanged)
# xgeom_routines.o XLFIND/SOPPS (byte-faithfully extracted)
# spline.o SCALC/SEGSPL/SEVAL/DEVAL/D2VAL (unchanged)
# sort.o SORTOL (fortran-src/sort.f, unchanged)
# xutils.o ATANC etc. (fortran-src/xutils.f, unchanged)
# xtcam_routines.o GETCAM/GETMAX/CPCAM (byte-faithfully extracted)
#
# Generated binaries are written only below build/.
#
# Modes (exactly one required):
# tools/build-geometry-design-harnesses.sh --write # build and run,
# # reference data below
# # tests/data/
# tools/build-geometry-design-harnesses.sh --check # build and run,
# # Byte comparison against
# # tests/data/; mutates
# # no Repo-file.
#
# Optional: FC (Fortran compiler, default gfortran).

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HARNESS_DIR="${REPO_ROOT}/tests/fortran-reference/harnesses/geometry-design"
DATA_DIR="${REPO_ROOT}/tests/data/core-geometry-design-reference"
BUILD_DIR="${REPO_ROOT}/build/core-geometry-design-harness"
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
  echo "The geometry-design harnesses require a Fortran compiler." >&2
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
  --source fortran-src/xtcam.f \
  --routines GETCAM GETMAX CPCAM THKCAM TCBUF TCSCAL TCSET HIPNT ZERCAM \
  --output "${BUILD_DIR}/xtcam_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xgdes.f \
  --routines TGAP LERAD SCLXY FLAP INSIDE GETXYF ABCOPY \
  --output "${BUILD_DIR}/xgdes_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xfoil.f \
  --routines ROTATE TECALC \
  --output "${BUILD_DIR}/xfoil_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/plutil.f \
  --routines SCALIT \
  --output "${BUILD_DIR}/plutil_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xpanel.f \
  --routines APCALC NCALC \
  --output "${BUILD_DIR}/xpanel_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xgeom.f \
  --routines XLFIND SOPPS LEFIND GEOPAR AECALC TCCALC YSYM CANG NORM AREFINE SSS LERSCL SCHECK \
  --output "${BUILD_DIR}/xgeom_routines.f" > /dev/null

# Canonical Fortran objects (unchanged). userio.o provides ASKR/ASKL
# (referenced by TCSCAL/TCSET, but never
# called in the harness path NINPUT=2).
for f in naca spline sort xutils userio; do
  "${FC}" -c "${REPO_ROOT}/fortran-src/${f}.f" -o "${BUILD_DIR}/${f}.o"
done

# Extracted routine objects (byte-faithful blocks from xtcam.f/xgeom.f).
"${FC}" -c -I "${REPO_ROOT}/fortran-src" \
  "${BUILD_DIR}/xtcam_routines.f" -o "${BUILD_DIR}/xtcam_routines.o"
"${FC}" -c -I "${REPO_ROOT}/fortran-src" \
  "${BUILD_DIR}/xgeom_routines.f" -o "${BUILD_DIR}/xgeom_routines.o"
"${FC}" -c -I "${REPO_ROOT}/fortran-src" \
  "${BUILD_DIR}/xfoil_routines.f" -o "${BUILD_DIR}/xfoil_routines.o"
"${FC}" -c -I "${REPO_ROOT}/fortran-src" \
  "${BUILD_DIR}/xgdes_routines.f" -o "${BUILD_DIR}/xgdes_routines.o"
"${FC}" -c -I "${REPO_ROOT}/fortran-src" \
  "${BUILD_DIR}/xpanel_routines.f" -o "${BUILD_DIR}/xpanel_routines.o"
"${FC}" -c -I "${REPO_ROOT}/fortran-src" \
  "${BUILD_DIR}/plutil_routines.f" -o "${BUILD_DIR}/plutil_routines.o"
# Plot stubs (numerically irrelevant PLTAIR/PLNEWP no-ops).
"${FC}" -c "${HARNESS_DIR}/plot_stubs.f" -o "${BUILD_DIR}/plot_stubs.o"

run_harness() {
  local harness="$1"
  local name="$2"
  "${FC}" -c -I "${REPO_ROOT}/fortran-src" \
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
    "${BUILD_DIR}/sort.o" \
    "${BUILD_DIR}/xutils.o" \
    "${BUILD_DIR}/userio.o" \
    "${BUILD_DIR}/xtcam_routines.o" \
    "${BUILD_DIR}/xfoil_routines.o" \
    "${BUILD_DIR}/xgdes_routines.o" \
    "${BUILD_DIR}/xpanel_routines.o" \
    "${BUILD_DIR}/plutil_routines.o" \
    "${BUILD_DIR}/plot_stubs.o" \
    -o "${BUILD_DIR}/${harness}"
  "${BUILD_DIR}/${harness}" > "${OUT_DIR}/${name}_reference.txt"
}

run_harness camber_thickness_harness camber_thickness
run_harness thkcam_harness thkcam
run_harness hipnt_harness hipnt
run_harness camb_session_harness camb_session
run_harness gdes_ops_harness gdes_ops
run_harness gdes_shape_harness gdes_shape
run_harness gofini_harness gofini

if [[ "${MODE}" == "write" ]]; then
  echo "Harnesses successfully executed."
  echo "Reference data: ${DATA_DIR}/*.txt"
else
  mismatch=0
  for name in camber_thickness thkcam hipnt camb_session gdes_ops gdes_shape gofini; do
    if ! cmp -s "${OUT_DIR}/${name}_reference.txt" \
        "${DATA_DIR}/${name}_reference.txt"; then
      echo "Deviation: ${name}_reference.txt" >&2
      mismatch=1
    fi
  done
  if [[ "${mismatch}" == "0" ]]; then
    echo "Reference data unchanged: all 7 Files identical."
  else
    echo "Error: Reference data differ from (see above)." >&2
    exit 1
  fi
fi
