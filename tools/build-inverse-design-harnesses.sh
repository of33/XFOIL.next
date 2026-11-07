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

# file: tools/build-inverse-design-harnesses.sh
# Builds and runs the focused Fortran harnesses for the inverse-design core
# (tests/fortran-reference/harnesses/inverse-design) and produces the
# reproducible reference data below tests/data/core-inverse-design-reference/.
#
# The harnesses call exclusively the canonical routines from
# fortran-src/{xmdes,xqdes,ntcalc,airfoil,xpanel,xsolve,spline,sort,xutils,osmap}.f
# unchanged. They change no Fortran source, no raw reference and
# no golden file.
#
# routines with menu/plot/prompt/file logic (MDES, QDES, PERT, GETVOV,
# IQSGET, CRPLOT, CNPLOT, ...) are never called in the harness and are not
# linked. Instead, tools/extract-fortran-routines.py extracts the
# actually called numerical routine blocks byte-faithfully from the
# canonical sources and writes them below build/; only these objects are
# linked. No linker options for symbol suppression are used.
#
# Generated binaries are written only below build/.
#
# Modes (exactly one required):
# tools/build-inverse-design-harnesses.sh --write # build and run,
# # reference data below
# # tests/data/
# tools/build-inverse-design-harnesses.sh --check # build and run,
# # Byte comparison against
# # tests/data/; mutates
# # no Repo-file.
#
# Optional: FC (Fortran compiler, default gfortran).

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HARNESS_DIR="${REPO_ROOT}/tests/fortran-reference/harnesses/inverse-design"
DATA_DIR="${REPO_ROOT}/tests/data/core-inverse-design-reference"
BUILD_DIR="${REPO_ROOT}/build/core-inverse-design-harness"
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
  echo "The inverse-design harnesses require a Fortran compiler." >&2
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
  --source fortran-src/xmdes.f \
  --routines EIWSET PIQSUM FTP CNFILT CNSYMM \
  --output "${BUILD_DIR}/xmdes_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xmdes.f \
  --routines SCINIT CNCALC ZCCALC ZCNORM ZLEFIND \
  --output "${BUILD_DIR}/xmdes_mapping.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xmdes.f \
  --routines QCCALC QSPINT MAPGEN \
  --output "${BUILD_DIR}/xmdes_speed.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xgeom.f \
  --routines LEFIND \
  --output "${BUILD_DIR}/xgeom_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/profil.f \
  --routines FS B3SOLV PRWALL UWALL \
  --output "${BUILD_DIR}/xprofil.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xblsys.f \
  --routines CFT \
  --output "${BUILD_DIR}/xblsys_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xqdes.f \
  --routines SPLQSP SMOOQ GAMQSP SYMQSP QINCOM \
  --output "${BUILD_DIR}/xqdes_kernels.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xmdes.f \
  --routines DZTSET AGTSET \
  --output "${BUILD_DIR}/xmdes_setters.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xmdes.f \
  --routines MAPGAM QSPCIR \
  --output "${BUILD_DIR}/xmdes_mapgam.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xpanel.f \
  --routines PSILIN APCALC NCALC \
  --output "${BUILD_DIR}/xpanel_geolin.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xfoil.f \
  --routines PANGEN TECALC CLCALC \
  --output "${BUILD_DIR}/xfoil_pangen.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xqdes.f \
  --routines MIXED GAMLIN \
  --output "${BUILD_DIR}/xqdes_mixed.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xgdes.f \
  --routines ABCOPY \
  --output "${BUILD_DIR}/xgdes_abcopy.f" > /dev/null

# Canonical Fortran-objects (unchanged), if of a harness needed.
for f in naca spline xsolve osmap ntcalc xutils; do
  "${FC}" -c "${REPO_ROOT}/fortran-src/${f}.f" -o "${BUILD_DIR}/${f}.o"
done

# harness-glue for OSMAP (GETOSFILE missing in fortran-src/).
"${FC}" -c "${HARNESS_DIR}/getosfile.f" -o "${BUILD_DIR}/getosfile.o"

# harness glue: link-only stub for ASKR (the non-interactive branch is never reached).
"${FC}" -c "${HARNESS_DIR}/askr.f" -o "${BUILD_DIR}/askr.o"

# harness-glue: XFOIL.INC units for MAPGAM/QSPCIR (PI conflict with CIRCLE.INC).
"${FC}" -c -I "${REPO_ROOT}/fortran-src" \
  "${HARNESS_DIR}/mdes_panel_glue.f" -o "${BUILD_DIR}/mdes_panel_glue.o"

# Extracted routine objects (byte-faithful blocks from xmdes.f/xgeom.f).
"${FC}" -c -I "${REPO_ROOT}/fortran-src" \
  "${BUILD_DIR}/xmdes_routines.f" -o "${BUILD_DIR}/xmdes_routines.o"
"${FC}" -c -I "${REPO_ROOT}/fortran-src" \
  "${BUILD_DIR}/xmdes_mapping.f" -o "${BUILD_DIR}/xmdes_mapping.o"
"${FC}" -c -I "${REPO_ROOT}/fortran-src" \
  "${BUILD_DIR}/xmdes_speed.f" -o "${BUILD_DIR}/xmdes_speed.o"
"${FC}" -c -I "${REPO_ROOT}/fortran-src" \
  "${BUILD_DIR}/xgeom_routines.f" -o "${BUILD_DIR}/xgeom_routines.o"
"${FC}" -c -I "${REPO_ROOT}/fortran-src" \
  "${BUILD_DIR}/xprofil.f" -o "${BUILD_DIR}/xprofil.o"
"${FC}" -c -I "${REPO_ROOT}/fortran-src" \
  "${BUILD_DIR}/xblsys_routines.f" -o "${BUILD_DIR}/xblsys_routines.o"
"${FC}" -c -I "${REPO_ROOT}/fortran-src" \
  "${BUILD_DIR}/xqdes_kernels.f" -o "${BUILD_DIR}/xqdes_kernels.o"
"${FC}" -c -I "${REPO_ROOT}/fortran-src" \
  "${BUILD_DIR}/xmdes_setters.f" -o "${BUILD_DIR}/xmdes_setters.o"
"${FC}" -c -I "${REPO_ROOT}/fortran-src" \
  "${BUILD_DIR}/xmdes_mapgam.f" -o "${BUILD_DIR}/xmdes_mapgam.o"
"${FC}" -c -I "${REPO_ROOT}/fortran-src" \
  "${BUILD_DIR}/xpanel_geolin.f" -o "${BUILD_DIR}/xpanel_geolin.o"
"${FC}" -c -I "${REPO_ROOT}/fortran-src" \
  "${BUILD_DIR}/xfoil_pangen.f" -o "${BUILD_DIR}/xfoil_pangen.o"
"${FC}" -c -I "${REPO_ROOT}/fortran-src" \
  "${BUILD_DIR}/xqdes_mixed.f" -o "${BUILD_DIR}/xqdes_mixed.o"
"${FC}" -c -I "${REPO_ROOT}/fortran-src" \
  "${BUILD_DIR}/xgdes_abcopy.f" -o "${BUILD_DIR}/xgdes_abcopy.o"

# harness glue: CIRCLE-side units for the MDES-INIT matrix harness.
"${FC}" -c -I "${REPO_ROOT}/fortran-src" \
  "${HARNESS_DIR}/mdes_init_matrix_glue.f" \
  -o "${BUILD_DIR}/mdes_init_matrix_glue.o"

# Canonical precision for the MDES-INIT matrix-harness: the
# canonical reference build (fortran-bin/xfoil) uses
# -fdefault-real-8 -fdefault-double-8; unqualified REAL/COMPLEX is
# there effectively REAL(8)/COMPLEX(16). The active declarations in the
# PANGEN/MDES-INIT path (xfoil.f, xpanel.f, xmdes.f, xgeom.f, xgdes.f,
# xqdes.f, spline.f, naca.f, xsolve.f, XFOIL.INC, XDES.INC, CIRCLE.INC)
# contain more than explicit REAL*4/COMPLEX*8 (precision audit).
# The r8 objects are built in a separate directory
# so that the 11 older inverse references (the REAL(4) build of this
# script, existing state) remain bit-exact unchanged.
R8DIR="${BUILD_DIR}/r8"
R8FLAGS=(-fdefault-real-8 -fdefault-double-8)
mkdir -p "${R8DIR}"
for f in naca spline xsolve; do
  "${FC}" "${R8FLAGS[@]}" -c "${REPO_ROOT}/fortran-src/${f}.f" \
    -o "${R8DIR}/${f}.o"
done
for u in xmdes_routines xmdes_mapping xmdes_speed xgeom_routines \
         xqdes_kernels xpanel_geolin xfoil_pangen xmdes_mapgam \
         xgdes_abcopy; do
  "${FC}" "${R8FLAGS[@]}" -c -I "${REPO_ROOT}/fortran-src" \
    "${BUILD_DIR}/${u}.f" -o "${R8DIR}/${u}.o"
done
"${FC}" "${R8FLAGS[@]}" -c -I "${REPO_ROOT}/fortran-src" \
  "${HARNESS_DIR}/mdes_init_matrix_glue.f" \
  -o "${R8DIR}/mdes_init_matrix_glue.o"

run_harness() {
  local harness="$1"
  local name="$2"
  shift 2
  "${FC}" -c -I "${REPO_ROOT}/fortran-src" \
    "${HARNESS_DIR}/${harness}.f" -o "${BUILD_DIR}/${harness}.o"
  # Negative check: osmap_DP.f is an inactive upstream alternative
  # with the same OSMAP symbol as the canonical runtime implementation.
  # The active and inactive OSMAP implementations must never be
  # compiled or linked together (tools/check-osmap-selection.sh).
  # (tools/check-osmap-selection.sh).
  "${REPO_ROOT}/tools/check-osmap-selection.sh" "${BUILD_DIR}"
  # shellcheck disable=SC2086
  "${FC}" "${BUILD_DIR}/${harness}.o" "$@" \
    -o "${BUILD_DIR}/${harness}"
  "${BUILD_DIR}/${harness}" > "${OUT_DIR}/${name}_reference.txt"
}

run_harness circle_kernels_harness circle_kernels \
  "${BUILD_DIR}/xmdes_routines.o"

run_harness scinit_mapping_harness scinit_mapping \
  "${BUILD_DIR}/xmdes_routines.o" \
  "${BUILD_DIR}/xmdes_mapping.o" \
  "${BUILD_DIR}/xgeom_routines.o" \
  "${BUILD_DIR}/naca.o" \
  "${BUILD_DIR}/spline.o"

run_harness qcalc_mapgen_harness qcalc_mapgen \
  "${BUILD_DIR}/xmdes_routines.o" \
  "${BUILD_DIR}/xmdes_mapping.o" \
  "${BUILD_DIR}/xmdes_speed.o" \
  "${BUILD_DIR}/xgeom_routines.o" \
  "${BUILD_DIR}/naca.o" \
  "${BUILD_DIR}/spline.o" \
  "${BUILD_DIR}/xsolve.o"

# OSMAP/NTCALC harness: find the database via the OSMAP environment variable.
"${FC}" -c -I "${REPO_ROOT}/fortran-src" \
  "${HARNESS_DIR}/osmap_nt_harness.f" -o "${BUILD_DIR}/osmap_nt_harness.o"
# Negative check: osmap_DP.f is an inactive upstream alternative
# with the same OSMAP symbol as the canonical runtime implementation.
# The active and inactive OSMAP implementations must never be
# compiled or linked together (tools/check-osmap-selection.sh).
"${REPO_ROOT}/tools/check-osmap-selection.sh" "${BUILD_DIR}"
"${FC}" "${BUILD_DIR}/osmap_nt_harness.o" \
  "${BUILD_DIR}/osmap.o" \
  "${BUILD_DIR}/ntcalc.o" \
  "${BUILD_DIR}/getosfile.o" \
  -o "${BUILD_DIR}/osmap_nt_harness"
OSMAP="${REPO_ROOT}/fortran-src/osmap.dat" \
  "${BUILD_DIR}/osmap_nt_harness" > "${OUT_DIR}/osmap_nt_reference.txt"

run_harness wall_profile_harness wall_profile \
  "${BUILD_DIR}/xprofil.o" \
  "${BUILD_DIR}/xblsys_routines.o"

run_harness qdes_kernels_harness qdes_kernels \
  "${BUILD_DIR}/xqdes_kernels.o" \
  "${BUILD_DIR}/spline.o" \
  "${BUILD_DIR}/xsolve.o"

run_harness mdes_setters_harness mdes_setters \
  "${BUILD_DIR}/xmdes_setters.o" \
  "${BUILD_DIR}/askr.o"

run_harness mapgam_qspcir_harness mapgam_qspcir \
  "${BUILD_DIR}/mdes_panel_glue.o" \
  "${BUILD_DIR}/xmdes_mapgam.o" \
  "${BUILD_DIR}/xmdes_routines.o" \
  "${BUILD_DIR}/xmdes_mapping.o" \
  "${BUILD_DIR}/xmdes_speed.o" \
  "${BUILD_DIR}/xgeom_routines.o" \
  "${BUILD_DIR}/xqdes_kernels.o" \
  "${BUILD_DIR}/naca.o" \
  "${BUILD_DIR}/spline.o" \
  "${BUILD_DIR}/xsolve.o"

run_harness mdes_init_harness mdes_init \
  "${BUILD_DIR}/mdes_panel_glue.o" \
  "${BUILD_DIR}/xmdes_mapgam.o" \
  "${BUILD_DIR}/xmdes_routines.o" \
  "${BUILD_DIR}/xmdes_mapping.o" \
  "${BUILD_DIR}/xmdes_speed.o" \
  "${BUILD_DIR}/xgeom_routines.o" \
  "${BUILD_DIR}/xqdes_kernels.o" \
  "${BUILD_DIR}/naca.o" \
  "${BUILD_DIR}/spline.o" \
  "${BUILD_DIR}/xsolve.o"

# MDES-INIT matrix harness: guard orchestration (xmdes.f 63-107) with
# ABCOPY (xgdes.f) and PANGEN (xfoil.f) as panel sources; CLARK-Y input
# via environment variable CLARKY_DAT (existing test input file).
CLARKY_DAT="${REPO_ROOT}/tests/fortran-reference/cases/file-export/dmp-visc-clark-y/input/CLARK_Y.dat"
export CLARKY_DAT
"${FC}" "${R8FLAGS[@]}" -c -I "${REPO_ROOT}/fortran-src" \
  "${HARNESS_DIR}/mdes_init_matrix_harness.f" \
  -o "${R8DIR}/mdes_init_matrix_harness.o"
"${FC}" "${R8DIR}/mdes_init_matrix_harness.o" \
  "${R8DIR}/mdes_init_matrix_glue.o" \
  "${R8DIR}/xgdes_abcopy.o" \
  "${R8DIR}/xmdes_mapgam.o" \
  "${R8DIR}/xmdes_routines.o" \
  "${R8DIR}/xmdes_mapping.o" \
  "${R8DIR}/xmdes_speed.o" \
  "${R8DIR}/xgeom_routines.o" \
  "${R8DIR}/xqdes_kernels.o" \
  "${R8DIR}/xpanel_geolin.o" \
  "${R8DIR}/xfoil_pangen.o" \
  "${R8DIR}/naca.o" \
  "${R8DIR}/spline.o" \
  "${R8DIR}/xsolve.o" \
  -o "${BUILD_DIR}/mdes_init_matrix_harness"
"${BUILD_DIR}/mdes_init_matrix_harness" \
  > "${OUT_DIR}/mdes_init_matrix_reference.txt"
unset CLARKY_DAT

run_harness psilin_geolin_harness psilin_geolin \
  "${BUILD_DIR}/xpanel_geolin.o" \
  "${BUILD_DIR}/xfoil_pangen.o" \
  "${BUILD_DIR}/xgeom_routines.o" \
  "${BUILD_DIR}/naca.o" \
  "${BUILD_DIR}/spline.o"

run_harness mixed_harness mixed \
  "${BUILD_DIR}/xqdes_mixed.o" \
  "${BUILD_DIR}/xpanel_geolin.o" \
  "${BUILD_DIR}/xfoil_pangen.o" \
  "${BUILD_DIR}/xgeom_routines.o" \
  "${BUILD_DIR}/naca.o" \
  "${BUILD_DIR}/spline.o" \
  "${BUILD_DIR}/xsolve.o" \
  "${BUILD_DIR}/xutils.o"

if [[ "${MODE}" == "write" ]]; then
  echo "Harnesses successfully executed."
  echo "Reference data: ${DATA_DIR}/*.txt"
else
  mismatch=0
  for name in circle_kernels scinit_mapping qcalc_mapgen osmap_nt wall_profile qdes_kernels mdes_setters mapgam_qspcir mdes_init mdes_init_matrix psilin_geolin mixed; do
    if ! cmp -s "${OUT_DIR}/${name}_reference.txt" \
        "${DATA_DIR}/${name}_reference.txt"; then
      echo "Deviation: ${name}_reference.txt" >&2
      mismatch=1
    fi
  done
  if [[ "${mismatch}" == "0" ]]; then
    echo "Reference data unchanged: all 12 Files identical."
  else
    echo "Error: Reference data differ from (see above)." >&2
    exit 1
  fi
fi
