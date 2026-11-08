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

# tools/build-c11a-state-harnesses.sh
# Builds and runs the session-state harness group:
# (1) c11a_state_harness (Clark Y, Re=1e5, Mach=0, alpha=4, no-PANE):
# canonical VISCAL initialization (xoper.f 2904-2964) and first
# Newton iteration (xoper.f 2970-3003) with snapshots S0-S5 at the
# state boundaries (It24.16, canonical order) plus DIJ lines for
# the first wake BL panel position (QDCALC regression) for Clark Y
# and NACA 4412.
# (2) c11a_twopoint_harness (Clark Y, Re=1e5, Mach=0, no-PANE):
# canonical VISCAL sequence semantics according to xoper.f for the
# batch sequence alpha=0.0 (unconverged) -> alpha=0.5 (converged),
# per-point flags according to the batch contract, full COMMON persistence,
# snapshots P0B/P0C/P1A/P1B/P1C/P1D/P1E/P1F (incl. DIJ wake block,
# XSSI, UINV, SETBL common scalars). reference:
# c11a_twopoint_reference.txt.
#
# The harness is built with the canonical build flags:
# -fdefault-real-8 -fdefault-double-8
# (the canonical xfoil binary computes with unqualified REAL as
# REAL(8); the interpolation osmap.dat remains separately REAL*4).
#
# Modes (exactly one required):
# tools/build-c11a-state-harnesses.sh --write # build and run,
# # reference write
# tools/build-c11a-state-harnesses.sh --check # build and run,
# # byte-for-byte
# # compare
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

BUILD_DIR="${REPO_ROOT}/build/c11a-state-harness-dbl"
HARNESS_DIR="${REPO_ROOT}/tests/fortran-reference/harnesses/c11a"
PANELING_DIR="${REPO_ROOT}/build/fortran-reference-paneling"
DATA_DIR="${REPO_ROOT}/tests/data/core-c11a-state-reference"
DBL="-fdefault-real-8 -fdefault-double-8"

rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"

if [[ "${MODE}" == "--write" ]]; then
  mkdir -p "${DATA_DIR}"
  OUT_DIR="${DATA_DIR}"
elif [[ "${MODE}" == "--check" ]]; then
  OUT_DIR="$(mktemp -d)"
  trap 'rm -rf "${OUT_DIR}"' EXIT
fi

# Byte-faithful extraction of the BL routines (below build/).
python3 tools/extract-fortran-routines.py \
  --source fortran-src/xfoil-6.99/xbl.f \
  --routines SETBL IBLSYS MRCHUE MRCHDU UPDATE DSLIM \
  --output "${BUILD_DIR}/xbl_solver_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xfoil-6.99/xbl.f \
  --routines XIFSET BLPINI \
  --output "${BUILD_DIR}/xbl_xifset_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xfoil-6.99/xsolve.f \
  --routines GAUSS \
  --output "${BUILD_DIR}/xsolve_gauss_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xfoil-6.99/xblsys.f \
  --routines TRCHEK AXSET TRCHEK2 BLSYS TESYS BLVAR BLMID TRDIF BLDIF \
             DAMPL DAMPL2 DIL DILW HSL CFL DIT HST CFT HCT HKIN BLPRV BLKIN \
  --output "${BUILD_DIR}/xblsys_solver_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xfoil-6.99/xsolve.f \
  --routines BLSOLV LUDCMP BAKSUB \
  --output "${BUILD_DIR}/xsolve_bl_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xfoil-6.99/xpanel.f \
  --routines UICALC UECALC QVFUE GAMQV STMOVE UESET DSSET \
  --output "${BUILD_DIR}/xpanel_coupling_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xfoil-6.99/xpanel.f \
  --routines IBLPAN XICALC \
  --output "${BUILD_DIR}/xpanel_bl_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xfoil-6.99/xfoil.f \
  --routines MRCL COMSET CPCALC CLCALC CDCALC NACA \
  --output "${BUILD_DIR}/xfoil_inviscid_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xfoil-6.99/xgeom.f \
  --routines GEOPAR AECALC TCCALC \
  --output "${BUILD_DIR}/xgeom_geopar_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xfoil-6.99/xoper.f \
  --routines SPECAL SPECCL MHINGE VISCAL FCPMIN \
  --output "${BUILD_DIR}/xoper_viscous_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xfoil-6.99/xpanel.f \
  --routines PSILIN PSWLIN GGCALC QWCALC QDCALC STFIND QISET XYWAKE \
  --output "${BUILD_DIR}/xpanel_inviscid_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xfoil-6.99/xgdes.f \
  --routines GETXYF \
  --output "${BUILD_DIR}/xgdes_getxyf_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xfoil-6.99/userio.f \
  --routines ASKR STRIP \
  --output "${BUILD_DIR}/userio_askr_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xfoil-6.99/userio.f \
  --routines ASKI \
  --output "${BUILD_DIR}/userio_aski_routines.f" > /dev/null

# paneling/inviscid extractions produce, if not present.
if [[ ! -f "${PANELING_DIR}/xfoil_paneling_routines.f" ]]; then
  python3 tools/extract-fortran-routines.py \
    --source fortran-src/xfoil-6.99/xfoil.f \
    --routines PANGEN TECALC \
    --output "${PANELING_DIR}/xfoil_paneling_routines.f" > /dev/null
  python3 tools/extract-fortran-routines.py \
    --source fortran-src/xfoil-6.99/xgdes.f \
    --routines ABCOPY \
    --output "${PANELING_DIR}/xgdes_abcopy_routines.f" > /dev/null
  python3 tools/extract-fortran-routines.py \
    --source fortran-src/xfoil-6.99/xpanel.f \
    --routines APCALC NCALC \
    --output "${PANELING_DIR}/xpanel_kernels_routines.f" > /dev/null
  python3 tools/extract-fortran-routines.py \
    --source fortran-src/xfoil-6.99/xgeom.f \
    --routines LEFIND XLFIND NSFIND SOPPS \
    --output "${PANELING_DIR}/xgeom_lefind_routines.f" > /dev/null
fi

# Canonical Fortran-objects (unchanged, DBL-flags).
for f in naca spline xutils; do
  "${FC}" -c ${DBL} "${REPO_ROOT}/fortran-src/xfoil-6.99/${f}.f" \
    -o "${BUILD_DIR}/${f}.o"
done

# Extracted routine objects (DBL flags).
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
  "${FC}" -c ${DBL} -I "${REPO_ROOT}/fortran-src/xfoil-6.99" \
    "${local_dir}/${f}.f" -o "${BUILD_DIR}/${f}.o"
done

for f in xbl_xifset_routines xsolve_gauss_routines xpanel_bl_routines \
         userio_aski_routines xgeom_geopar_routines; do
  "${FC}" -c ${DBL} -I "${REPO_ROOT}/fortran-src/xfoil-6.99" \
    "${BUILD_DIR}/${f}.f" -o "${BUILD_DIR}/${f}.o"
done

# harness (DBL-flags).
"${FC}" -c ${DBL} -I "${REPO_ROOT}/fortran-src/xfoil-6.99" \
  "${HARNESS_DIR}/c11a_state_harness.f" \
  -o "${BUILD_DIR}/c11a_state_harness.o"
# Negative check: osmap_DP.f is an inactive upstream alternative
# with the same OSMAP symbol as the canonical runtime implementation.
# The active and inactive OSMAP implementations must never be
# compiled or linked together (tools/check-osmap-selection.sh).
"${REPO_ROOT}/tools/check-osmap-selection.sh" "${BUILD_DIR}"
"${FC}" "${BUILD_DIR}/c11a_state_harness.o" \
  "${BUILD_DIR}/naca.o" \
  "${BUILD_DIR}/spline.o" \
  "${BUILD_DIR}/xutils.o" \
  "${BUILD_DIR}/xgeom_lefind_routines.o" \
  "${BUILD_DIR}/xfoil_paneling_routines.o" \
  "${BUILD_DIR}/xpanel_kernels_routines.o" \
  "${BUILD_DIR}/xgdes_abcopy_routines.o" \
  "${BUILD_DIR}/xgeom_geopar_routines.o" \
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
  -o "${BUILD_DIR}/c11a_state_harness"

# harness start: reads CLARK_Y.dat from the current working directory.
cp "${REPO_ROOT}/sample/CLARK_Y.dat" "${BUILD_DIR}/CLARK_Y.dat"

(cd "${BUILD_DIR}" && ./c11a_state_harness) > "${OUT_DIR}/c11a_state_reference.txt"

# Two-point harness (batch sequence alpha=0.0 -> 0.5, canonical
# VISCAL sequence semantics, per-point flags according to the batch contract).
"${FC}" -c ${DBL} -I "${REPO_ROOT}/fortran-src/xfoil-6.99" \
  "${HARNESS_DIR}/c11a_twopoint_harness.f" \
  -o "${BUILD_DIR}/c11a_twopoint_harness.o"
"${FC}" "${BUILD_DIR}/c11a_twopoint_harness.o" \
  "${BUILD_DIR}/naca.o" \
  "${BUILD_DIR}/spline.o" \
  "${BUILD_DIR}/xutils.o" \
  "${BUILD_DIR}/xgeom_lefind_routines.o" \
  "${BUILD_DIR}/xfoil_paneling_routines.o" \
  "${BUILD_DIR}/xpanel_kernels_routines.o" \
  "${BUILD_DIR}/xgdes_abcopy_routines.o" \
  "${BUILD_DIR}/xgeom_geopar_routines.o" \
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
  -o "${BUILD_DIR}/c11a_twopoint_harness"
(cd "${BUILD_DIR}" && ./c11a_twopoint_harness) > "${OUT_DIR}/c11a_twopoint_reference.txt"

# Shared object list for additional harnesses (without harness object).
SHARED_OBJS=(
  "${BUILD_DIR}/naca.o" "${BUILD_DIR}/spline.o" "${BUILD_DIR}/xutils.o"
  "${BUILD_DIR}/xgeom_lefind_routines.o" "${BUILD_DIR}/xfoil_paneling_routines.o"
  "${BUILD_DIR}/xpanel_kernels_routines.o" "${BUILD_DIR}/xgdes_abcopy_routines.o"
  "${BUILD_DIR}/xgeom_geopar_routines.o" "${BUILD_DIR}/xbl_solver_routines.o"
  "${BUILD_DIR}/xblsys_solver_routines.o" "${BUILD_DIR}/xsolve_bl_routines.o"
  "${BUILD_DIR}/xpanel_coupling_routines.o" "${BUILD_DIR}/xfoil_inviscid_routines.o"
  "${BUILD_DIR}/xpanel_inviscid_routines.o" "${BUILD_DIR}/xoper_viscous_routines.o"
  "${BUILD_DIR}/xgdes_getxyf_routines.o" "${BUILD_DIR}/userio_askr_routines.o"
  "${BUILD_DIR}/xbl_xifset_routines.o" "${BUILD_DIR}/xsolve_gauss_routines.o"
  "${BUILD_DIR}/xpanel_bl_routines.o" "${BUILD_DIR}/userio_aski_routines.o"
)

# CSEQ harnesses: (a) normal path ascending CL 0.4..0.8/0.2,
# (b) NSEQEX halt Cl 0.4..1.2/0.2 with ITMAX=1. Both use ABCOPY contour
# and canonical SPECCL/SPECAL/VISCAL routines (It24.16).
for h in c11a_cseq_harness c11a_cseq_nseqex_harness; do
  "${FC}" -c ${DBL} -I "${REPO_ROOT}/fortran-src/xfoil-6.99" \
    "${HARNESS_DIR}/${h}.f" -o "${BUILD_DIR}/${h}.o"
  "${FC}" "${BUILD_DIR}/${h}.o" "${SHARED_OBJS[@]}" -o "${BUILD_DIR}/${h}"
  (cd "${BUILD_DIR}" && ./${h}) > "${OUT_DIR}/${h}_reference.txt"
done

# Batch-contract stall-region harness (Clark-Y stall-region after
# sequence pass): exactly the product contract of the Clark-Y standard
# run (Re=1e5, Mach=0, ITER=100, cold path per point via INIT toggle
# xoper.f 394-400, LALFA=true with SPECAL per point, VISCAL(ITMAX)
# without ITMAX+5, sequence 0.0->14.5/0.5, ABCOPY no-PANE) with the complete
# VISCAL iteration history (It24.16) and point output per point. Additive:
# existing references remain unchanged; --write produces only the new file.
"${FC}" -c ${DBL} -I "${REPO_ROOT}/fortran-src/xfoil-6.99" \
  "${HARNESS_DIR}/c11a_batch_stall_harness.f" \
  -o "${BUILD_DIR}/c11a_batch_stall_harness.o"
"${FC}" "${BUILD_DIR}/c11a_batch_stall_harness.o" "${SHARED_OBJS[@]}" \
  -o "${BUILD_DIR}/c11a_batch_stall_harness"
(cd "${BUILD_DIR}" && ./c11a_batch_stall_harness) \
  > "${OUT_DIR}/c11a_batch_stall_harness_reference.txt"

# MRCHUE fallback proof harness (Alpha-13 first-term pass):
# identical stall-region contract, but with an instrumented
# xbl copy (F341*/U*/H0 WRITEs, LDUMPM for ISET=341). Only the
# xbl routines were replaced; all remaining routines stay canonical. Additive:
# existing references remain unchanged.
"${FC}" -c ${DBL} -I "${REPO_ROOT}/fortran-src/xfoil-6.99" \
  "${HARNESS_DIR}/c11a_mrchue341_probe.f" \
  -o "${BUILD_DIR}/c11a_mrchue341_probe.o"
MRCHUE_OBJS=()
for o in "${SHARED_OBJS[@]}"; do
  case "${o}" in
    *xbl_solver_routines.o) ;;
    *) MRCHUE_OBJS+=("${o}") ;;
  esac
done
"${FC}" "${BUILD_DIR}/c11a_mrchue341_probe.o" "${MRCHUE_OBJS[@]}" \
  "${BUILD_DIR}/c11a_batch_stall_harness.o" \
  -o "${BUILD_DIR}/c11a_mrchue341_probe"
(cd "${BUILD_DIR}" && ./c11a_mrchue341_probe) \
  > "${OUT_DIR}/c11a_mrchue341_probe_reference.txt"

# OPER single-point harness: canonical OPER single-point semantics (xoper.f
# 416-542) for Alfa/CLI/Cl with conditional 1e-5 invalidation after the
# inviscid solution, retention of LIPAN/LBLINI, VISCAL(ITMAX) without NSEQEX.
# Scenarios A (Alfa 2 cold/warm), B (CLI inviscid/viscous), C (Cl viscous),
# D (CLI vs. Cl same target Cl), E (ITMAX=1 no convergence).
"${FC}" -c ${DBL} -I "${REPO_ROOT}/fortran-src/xfoil-6.99" \
  "${HARNESS_DIR}/c11a_oper_singlepoint_harness.f" \
  -o "${BUILD_DIR}/c11a_oper_singlepoint_harness.o"
"${FC}" "${BUILD_DIR}/c11a_oper_singlepoint_harness.o" "${SHARED_OBJS[@]}" \
  -o "${BUILD_DIR}/c11a_oper_singlepoint_harness"
(cd "${BUILD_DIR}" && ./c11a_oper_singlepoint_harness) \
  > "${OUT_DIR}/c11a_oper_singlepoint_harness_reference.txt"

# OPER Alfa cold-point diagnostics harness: complete VISCAL iteration
# history (It24.16) of the first cold Alfa-2 point
# (NACA 2412, PANGEN, Re=1e6) for the iteration-path comparison
# Core vs Fortran (RMSBL/RMXBL/VMXBL/IMXBL/ISMXBL/RLX/ALFA/CL/CM/CD/
# CDF/CDP/ITRAN/NBL/IBLTE/flags per iteration).
"${FC}" -c ${DBL} -I "${REPO_ROOT}/fortran-src/xfoil-6.99" \
  "${HARNESS_DIR}/c11a_alfa_diag_harness.f" \
  -o "${BUILD_DIR}/c11a_alfa_diag_harness.o"
"${FC}" "${BUILD_DIR}/c11a_alfa_diag_harness.o" "${SHARED_OBJS[@]}" \
  -o "${BUILD_DIR}/c11a_alfa_diag_harness"
(cd "${BUILD_DIR}" && ./c11a_alfa_diag_harness) \
  > "${OUT_DIR}/c11a_alfa_diag_harness_reference.txt"

# LRECALC harness: canonical '!' / LRECALC continuation semantics (xoper.f
# 85-92/416-542). SPECAL once, then VISCAL(ITMAX=1) repeatedly without a new
# SPECAL (continuation on the retained state), until convergence or the limit.
"${FC}" -c ${DBL} -I "${REPO_ROOT}/fortran-src/xfoil-6.99" \
  "${HARNESS_DIR}/c11a_lrecalc_harness.f" \
  -o "${BUILD_DIR}/c11a_lrecalc_harness.o"
"${FC}" "${BUILD_DIR}/c11a_lrecalc_harness.o" "${SHARED_OBJS[@]}" \
  -o "${BUILD_DIR}/c11a_lrecalc_harness"
(cd "${BUILD_DIR}" && ./c11a_lrecalc_harness) \
  > "${OUT_DIR}/c11a_lrecalc_harness_reference.txt"

# OPER INIT harness: canonical OPER INIT toggle/flag contract (xoper.f
# 394-401, content-identical to VPAR INIT 2649-2653): toggle LBLINI, on
# switching off LIPAN=false; LVCONV/LWAKE/LWDIJ/LADIJ, AWAKE/AVISC/MVISC,
# station fields and mapping content (IPAN/Is/SST/NBL/ISYS/NSYS) remain
# preserved; the next VISCAL run (COMOLD='Alfa', LRECALC=true) sets
# LIPAN via IBLPAN again and runs with LBLINI=false via SETBL/MRCHUE.
"${FC}" -c ${DBL} -I "${REPO_ROOT}/fortran-src/xfoil-6.99" \
  "${HARNESS_DIR}/c11a_init_harness.f" \
  -o "${BUILD_DIR}/c11a_init_harness.o"
"${FC}" "${BUILD_DIR}/c11a_init_harness.o" "${SHARED_OBJS[@]}" \
  -o "${BUILD_DIR}/c11a_init_harness"
(cd "${BUILD_DIR}" && ./c11a_init_harness) \
  > "${OUT_DIR}/c11a_init_harness_reference.txt"

# Mach post-processing harness: isolates the immediate canonical
# Mach side effects (MRCL/COMSET/CPCALC/CLCALC/CDCALC, xoper.f 322-333) on
# a present point without a new solver run. Scenarios C (viscous),
# B (inviscid), A (no point), depending on COMMON fresh initialization (It24.16).
"${FC}" -c ${DBL} -I "${REPO_ROOT}/fortran-src/xfoil-6.99" \
  "${HARNESS_DIR}/c11a_mach_postproc_harness.f" \
  -o "${BUILD_DIR}/c11a_mach_postproc_harness.o"
"${FC}" "${BUILD_DIR}/c11a_mach_postproc_harness.o" "${SHARED_OBJS[@]}" \
  -o "${BUILD_DIR}/c11a_mach_postproc_harness"
(cd "${BUILD_DIR}" && ./c11a_mach_postproc_harness) \
  > "${OUT_DIR}/c11a_mach_postproc_harness_reference.txt"

# Mach Cp observability harness: pins the canonical CPCALC
# side effect (xoper.f 328-329) and the Cp-minimum selection FCPMIN
# (xoper.f 1768-1801) after Mach on a present point. Scenarios
# C (viscous), B (inviscid), A (no point), depending on COMMON fresh
# initialization (It24.16). Validates for the first time the on-demand
# C++ derivation cpCalc(QINV/QVIS)+fcpmin against Fortran.
"${FC}" -c ${DBL} -I "${REPO_ROOT}/fortran-src/xfoil-6.99" \
  "${HARNESS_DIR}/c11a_mach_cp_harness.f" \
  -o "${BUILD_DIR}/c11a_mach_cp_harness.o"
"${FC}" "${BUILD_DIR}/c11a_mach_cp_harness.o" "${SHARED_OBJS[@]}" \
  -o "${BUILD_DIR}/c11a_mach_cp_harness"
(cd "${BUILD_DIR}" && ./c11a_mach_cp_harness) \
  > "${OUT_DIR}/c11a_mach_cp_harness_reference.txt"

if [[ "${MODE}" == "--write" ]]; then
  echo "State harness group successfully executed."
  echo "Reference data: ${DATA_DIR}/c11a_state_reference.txt"
  echo "Reference data: ${DATA_DIR}/c11a_twopoint_reference.txt"
  echo "Reference data: ${DATA_DIR}/c11a_cseq_harness_reference.txt"
  echo "Reference data: ${DATA_DIR}/c11a_cseq_nseqex_harness_reference.txt"
  echo "Reference data: ${DATA_DIR}/c11a_lrecalc_harness_reference.txt"
  echo "Reference data: ${DATA_DIR}/c11a_init_harness_reference.txt"
  echo "Reference data: ${DATA_DIR}/c11a_mach_postproc_harness_reference.txt"
  echo "Reference data: ${DATA_DIR}/c11a_mach_cp_harness_reference.txt"
  echo "Reference data: ${DATA_DIR}/c11a_oper_singlepoint_harness_reference.txt"
  echo "Reference data: ${DATA_DIR}/c11a_alfa_diag_harness_reference.txt"
  echo "Reference data: ${DATA_DIR}/c11a_mrchue341_probe_reference.txt"
else
  # --check: byte-for-byte comparison against the existing reference data.
  nbad=0
  nref=0
  for ref in c11a_state_reference.txt c11a_twopoint_reference.txt \
             c11a_cseq_harness_reference.txt \
             c11a_cseq_nseqex_harness_reference.txt \
             c11a_batch_stall_harness_reference.txt \
             c11a_mrchue341_probe_reference.txt \
             c11a_lrecalc_harness_reference.txt \
             c11a_init_harness_reference.txt \
             c11a_mach_postproc_harness_reference.txt \
             c11a_mach_cp_harness_reference.txt \
             c11a_oper_singlepoint_harness_reference.txt \
             c11a_alfa_diag_harness_reference.txt; do
    if cmp -s "${OUT_DIR}/${ref}" "${DATA_DIR}/${ref}"; then
      echo "Reference unchanged: ${ref}"
      nref=$((nref+1))
    else
      echo "Error: Reference differs from: ${ref}" >&2
      nbad=$((nbad+1))
    fi
  done
  if [[ "${nbad}" -ne 0 ]]; then
    echo "Error: ${nbad} reference file(s) differ (see above)." >&2
    exit 1
  fi
  echo "Reference data unchanged: all ${nref} Files identical."
fi
