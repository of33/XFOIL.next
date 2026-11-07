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

# tools/build-viscous-harnesses.sh
# Builds and runs the Fortran reference harness for the viscous and
# coupled Solver: VISCAL/BLSYS/TESYS/BLSOLV/MRCHUE/MRCHDU/SETBL/UPDATE/
# UICALC/UECALC/QVFUE/GAMQV/STMOVE/UESET/DSSET/CDCALC + Correlations).
#
# tools/extract-fortran-routines.py extracts the actually called
# routine blocks byte-faithfully from the canonical sources and writes
# them below build/; only these objects are linked. No
# linker options for symbol suppression are used.
#
# Modes (exactly one required):
# tools/build-viscous-harnesses.sh --write # build and run,
# # Reference data write
# tools/build-viscous-harnesses.sh --check # build and run,
# # byte-for-byte compare
#
# Optional: FC (Fortran compiler, default gfortran).

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FC="${FC:-gfortran}"

if [[ $# -ne 1 ]]; then
  echo "Use: $0 {--write|--check}" >&2
  exit 2
fi

MODE="$1"
case "${MODE}" in
  --write|--check) ;;
  *)
    echo "Use: $0 {--write|--check}" >&2
    exit 2
    ;;
esac

BUILD_DIR="${REPO_ROOT}/build/fortran-reference-viscous"
HARNESS_DIR="${REPO_ROOT}/tests/fortran-reference/harnesses/viscous"
PANELING_DIR="${REPO_ROOT}/build/fortran-reference-paneling"
DATA_DIR="${REPO_ROOT}/tests/data/core-viscous-reference"

rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"

if [[ "${MODE}" == "--write" ]]; then
  mkdir -p "${DATA_DIR}"
  OUT_DIR="${DATA_DIR}"
elif [[ "${MODE}" == "--check" ]]; then
  OUT_DIR="$(mktemp -d)"
  trap 'rm -rf "${OUT_DIR}"' EXIT
fi

# Byte-faithful extraction of the viscous routines (below build/).
python3 tools/extract-fortran-routines.py \
  --source fortran-src/xbl.f \
  --routines SETBL IBLSYS MRCHUE MRCHDU UPDATE DSLIM \
  --output "${BUILD_DIR}/xbl_solver_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xbl.f \
  --routines XIFSET BLPINI \
  --output "${BUILD_DIR}/xbl_xifset_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xsolve.f \
  --routines GAUSS \
  --output "${BUILD_DIR}/xsolve_gauss_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xblsys.f \
  --routines TRCHEK AXSET TRCHEK2 BLSYS TESYS BLVAR BLMID TRDIF BLDIF \
             DAMPL DAMPL2 DIL DILW HSL CFL DIT HST CFT HCT HKIN BLPRV BLKIN \
  --output "${BUILD_DIR}/xblsys_solver_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xsolve.f \
  --routines BLSOLV LUDCMP BAKSUB \
  --output "${BUILD_DIR}/xsolve_bl_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xpanel.f \
  --routines UICALC UECALC QVFUE GAMQV STMOVE UESET DSSET \
  --output "${BUILD_DIR}/xpanel_coupling_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xpanel.f \
  --routines IBLPAN XICALC \
  --output "${BUILD_DIR}/xpanel_bl_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xfoil.f \
  --routines MRCL COMSET CPCALC CLCALC CDCALC \
  --output "${BUILD_DIR}/xfoil_inviscid_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xoper.f \
  --routines SPECAL SPECCL MHINGE VISCAL \
  --output "${BUILD_DIR}/xoper_viscous_routines.f" > /dev/null

# paneling/inviscid extractions produce, if not present.
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

# Remaining dependencies (byte-faithful).
python3 tools/extract-fortran-routines.py \
  --source fortran-src/xpanel.f \
  --routines PSILIN PSWLIN GGCALC QWCALC QDCALC STFIND QISET XYWAKE \
  --output "${BUILD_DIR}/xpanel_inviscid_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xgdes.f \
  --routines GETXYF \
  --output "${BUILD_DIR}/xgdes_getxyf_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/userio.f \
  --routines ASKR \
  --output "${BUILD_DIR}/userio_askr_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/userio.f \
  --routines ASKI \
  --output "${BUILD_DIR}/userio_aski_routines.f" > /dev/null

# Canonical Fortran objects (unchanged).
for f in naca spline xutils; do
  "${FC}" -c "${REPO_ROOT}/fortran-src/${f}.f" -o "${BUILD_DIR}/${f}.o"
done

# Extracted routine objects.
for f in xgeom_lefind_routines xfoil_paneling_routines \
         xgdes_abcopy_routines xpanel_kernels_routines \
         xbl_solver_routines xblsys_solver_routines xsolve_bl_routines \
         xpanel_coupling_routines xfoil_inviscid_routines \
         xpanel_inviscid_routines xoper_viscous_routines \
         xgdes_getxyf_routines userio_askr_routines; do
  local_dir="${BUILD_DIR}"
  if [[ ! -f "${local_dir}/${f}.f" ]]; then
    local_dir="${PANELING_DIR}"
  fi
  "${FC}" -c -I "${REPO_ROOT}/fortran-src" \
    "${local_dir}/${f}.f" -o "${BUILD_DIR}/${f}.o"
done

for f in xbl_xifset_routines xsolve_gauss_routines xpanel_bl_routines \
         userio_aski_routines; do
  "${FC}" -c -I "${REPO_ROOT}/fortran-src" \
    "${BUILD_DIR}/${f}.f" -o "${BUILD_DIR}/${f}.o"
done

# Link without symbol suppression: all required symbols are present beforehand.
"${FC}" -c -I "${REPO_ROOT}/fortran-src" \
  "${HARNESS_DIR}/viscous_harness.f" \
  -o "${BUILD_DIR}/viscous_harness.o"
# Negative check: osmap_DP.f is an inactive upstream alternative
# with the same OSMAP symbol as the canonical runtime implementation.
# The active and inactive OSMAP implementations must never be
# compiled or linked together (tools/check-osmap-selection.sh).
"${REPO_ROOT}/tools/check-osmap-selection.sh" "${BUILD_DIR}"
"${FC}" "${BUILD_DIR}/viscous_harness.o" \
  "${BUILD_DIR}/naca.o" \
  "${BUILD_DIR}/spline.o" \
  "${BUILD_DIR}/xutils.o" \
  "${BUILD_DIR}/xgeom_lefind_routines.o" \
  "${BUILD_DIR}/xfoil_paneling_routines.o" \
  "${BUILD_DIR}/xpanel_kernels_routines.o" \
  "${BUILD_DIR}/xbl_solver_routines.o" \
  "${BUILD_DIR}/xblsys_solver_routines.o" \
  "${BUILD_DIR}/xsolve_bl_routines.o" \
  "${BUILD_DIR}/xpanel_coupling_routines.o" \
  "${BUILD_DIR}/xfoil_inviscid_routines.o" \
  "${BUILD_DIR}/xpanel_inviscid_routines.o" \
  "${BUILD_DIR}/xoper_viscous_routines.o" \
  "${BUILD_DIR}/xgdes_getxyf_routines.o" \
  "${BUILD_DIR}/userio_askr_routines.o" \
  "${BUILD_DIR}/xbl_xifset_routines.o" \
  "${BUILD_DIR}/xsolve_gauss_routines.o" \
  "${BUILD_DIR}/xpanel_bl_routines.o" \
  "${BUILD_DIR}/userio_aski_routines.o" \
  -o "${BUILD_DIR}/viscous_harness"
# The harness starts in a scratch directory: VISCAL writes a debug
# file (*.bl) to the current working directory at the end (xoper.f).
(cd "${BUILD_DIR}" && ./viscous_harness) > "${OUT_DIR}/viscous_reference.txt"

if [[ "${MODE}" == "--write" ]]; then
  echo "Harnesses successfully executed."
  echo "Reference data: ${DATA_DIR}/*.txt"
else
  # --check: byte-for-byte comparison against the existing reference data.
  if cmp -s "${OUT_DIR}/viscous_reference.txt" \
      "${DATA_DIR}/viscous_reference.txt"; then
    echo "Reference data unchanged: all 1 Files identical."
  else
    echo "Error: Reference data differ from (see above)." >&2
    exit 1
  fi
fi
