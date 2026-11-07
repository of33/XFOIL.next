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

# file: tools/build-polar-io-harnesses.sh
# Builds and runs the focused Fortran harnesses for the polar core
# (tests/fortran-reference/harnesses/polar-io) and produces the
# reproducible reference data below tests/data/core-polar-io-reference/.
#
# The harnesses call exclusively the canonical routines from
# fortran-src/{xpol,iopol,xoper,xfoil,xmdes,xgdes,xpanel,xgeom,xqdes,
# xblsys,spline,sort,userio}.f unchanged. They change no
# Fortran source, no raw reference and no golden file.
#
# Interactive inputs (ASKS/ASKI/ASKL) are fed deterministically via stdin
# assignment; the read values are harness input data
# below tests/fortran-reference/harnesses/polar-io/data/.
#
# Modes (exactly one required):
# tools/build-polar-io-harnesses.sh --write # build and run,
# # reference data below
# # tests/data/
# tools/build-polar-io-harnesses.sh --check # build and run,
# # Byte comparison against
# # tests/data/; mutates
# # no Repo-file.
#
# Optional: FC (Fortran compiler, default gfortran).

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HARNESS_DIR="${REPO_ROOT}/tests/fortran-reference/harnesses/polar-io"
DATA_DIR="${REPO_ROOT}/tests/data/core-polar-io-reference"
BUILD_DIR="${REPO_ROOT}/build/core-polar-io-harness"
FC="${FC:-gfortran}"
# -std=legacy: suppresses the newer libgfortran EOF-marker check,
# so that the canonical PLXINI/PLXADD sequence runs as in the reference binary.
# -fdefault-real-8/-fdefault-double-8: reproduces the double-precision
# unformatted dump records of the canonical reference binaries.
FFLAGS=(-std=legacy -fdefault-real-8 -fdefault-double-8)

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

# run directory with harness input data and raw polar references
# (is only read, never changed).
RUN_DIR="$(mktemp -d)"
if [[ "${MODE}" == "check" ]]; then
  trap 'rm -rf "${RUN_DIR}" "${OUT_DIR}"' EXIT
else
  trap 'rm -rf "${RUN_DIR}"' EXIT
fi
cp "${HARNESS_DIR}"/data/* "${RUN_DIR}/"
for f in \
  polar-inv-0012/generated/polar-inv-0012.pol \
  polar-inv-2412/generated/inv2412.pol \
  polar-inv-clark-y/generated/polar-inv-clark-y.pol \
  polar-nonconverging/generated/polar-nonconverging.pol \
  polar-state-ascending/generated/polar-state-ascending.pol \
  polar-state-cl-driven/generated/polar-state-cl-driven.pol \
  polar-state-descending/generated/polar-state-descending.pol \
  polar-visc-0012/generated/polar-visc-0012.pol \
  polar-visc-2412/generated/visc2412.pol \
  polar-visc-2412-re3e6/generated/polar-visc-2412-re3e6.pol \
  polar-visc-clark-y/generated/polar-visc-clark-y.pol ; do
  cp "${REPO_ROOT}/tests/fortran-reference/cases/polar/${f}" "${RUN_DIR}/"
done
cp "${REPO_ROOT}/tests/fortran-reference/cases/polar/polar-visc-0012/generated/polar-visc-0012.pol" \
   "${RUN_DIR}/plrini_case.pol"
cp "${REPO_ROOT}/tests/fortran-reference/cases/polar/polar-visc-2412/generated/visc2412.dump" \
   "${RUN_DIR}/plxini_fixture.dump"

# Byte-faithful extraction of the called canonical routines.
python3 tools/extract-fortran-routines.py \
  --source fortran-src/xpol.f \
  --routines PLRSET APCOPY PLRINI PLXINI PLRADD PLXADD PLRSRT PLRCOP PRFCOP PLRSUM PRFSUM BOTTOM BOTTOMX \
  --output "${BUILD_DIR}/xpol_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/iopol.f \
  --routines POLREAD POLWRIT POLREF \
  --output "${BUILD_DIR}/iopol_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xoper.f \
  --routines NAMMOD MHINGE BLDUMP BLDUMP2 CPDUMP \
  --output "${BUILD_DIR}/xoper_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xfoil.f \
  --routines TECALC COMSET MRCL GETDEF WRTDEF \
  --output "${BUILD_DIR}/xfoil_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xmdes.f \
  --routines CNDUMP \
  --output "${BUILD_DIR}/xmdes_cndump.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xmdes.f \
  --routines GETVOV \
  --output "${BUILD_DIR}/xmdes_getvov.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xgdes.f \
  --routines GETXYF \
  --output "${BUILD_DIR}/xgdes_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xpanel.f \
  --routines NCALC APCALC \
  --output "${BUILD_DIR}/xpanel_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xgeom.f \
  --routines LEFIND \
  --output "${BUILD_DIR}/xgeom_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xqdes.f \
  --routines QINCOM SPLQSP \
  --output "${BUILD_DIR}/xqdes_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/xblsys.f \
  --routines HKIN \
  --output "${BUILD_DIR}/xblsys_routines.f" > /dev/null

python3 tools/extract-fortran-routines.py \
  --source fortran-src/spline.f \
  --routines SCALC SEGSPL SEVAL SPLIND SINVRT DEVAL D2VAL TRISOL \
  --output "${BUILD_DIR}/spline_routines.f" > /dev/null

# Canonical unmodified source files.
"${FC}" -c "${FFLAGS[@]}" "${REPO_ROOT}/fortran-src/userio.f" -o "${BUILD_DIR}/userio.o"
"${FC}" -c "${FFLAGS[@]}" "${REPO_ROOT}/fortran-src/sort.f" -o "${BUILD_DIR}/sort.o"

# Extracted routine objects.
for f in xpol_routines iopol_routines xoper_routines xfoil_routines \
         xmdes_cndump xmdes_getvov xgdes_routines xpanel_routines xgeom_routines \
         xqdes_routines xblsys_routines spline_routines; do
  "${FC}" -c "${FFLAGS[@]}" -I "${REPO_ROOT}/fortran-src" \
    "${BUILD_DIR}/${f}.f" -o "${BUILD_DIR}/${f}.o"
done

# The shared object set is used by nearly all harnesses.
BASE_OBJS=(
  "${BUILD_DIR}/xpol_routines.o"
  "${BUILD_DIR}/iopol_routines.o"
  "${BUILD_DIR}/xoper_routines.o"
  "${BUILD_DIR}/xfoil_routines.o"
  "${BUILD_DIR}/xgdes_routines.o"
  "${BUILD_DIR}/xpanel_routines.o"
  "${BUILD_DIR}/xgeom_routines.o"
  "${BUILD_DIR}/xqdes_routines.o"
  "${BUILD_DIR}/xblsys_routines.o"
  "${BUILD_DIR}/spline_routines.o"
  "${BUILD_DIR}/userio.o"
  "${BUILD_DIR}/sort.o"
)

# run_harness <harness.f without extension> <reference name> [stdin file] [further objects...]
run_harness() {
  local harness="$1"
  local name="$2"
  shift 2
  local stdin_file=""
  if [[ "${1:-}" == *stdin.txt ]]; then
    stdin_file="$1"
    shift
  fi
  "${FC}" -c "${FFLAGS[@]}" -I "${REPO_ROOT}/fortran-src" \
    "${HARNESS_DIR}/${harness}.f" -o "${BUILD_DIR}/${harness}.o"
  # Negative check: osmap_DP.f is an inactive upstream alternative
  # with the same OSMAP symbol as the canonical runtime implementation.
  # The active and inactive OSMAP implementations must never be
  # compiled or linked together (tools/check-osmap-selection.sh).
  "${REPO_ROOT}/tools/check-osmap-selection.sh" "${BUILD_DIR}"
  "${FC}" "${FFLAGS[@]}" "${BUILD_DIR}/${harness}.o" "$@" \
    -o "${BUILD_DIR}/${harness}"
  if [[ -n "${stdin_file}" ]]; then
    ( cd "${RUN_DIR}" && "${BUILD_DIR}/${harness}" \
        < "${HARNESS_DIR}/data/${stdin_file}" \
        > "${OUT_DIR}/${name}_reference.txt" )
  else
    ( cd "${RUN_DIR}" && "${BUILD_DIR}/${harness}" \
        > "${OUT_DIR}/${name}_reference.txt" )
  fi
}

# compare_generated <file in the RUN_DIR> <reference name>
compare_generated() {
  local gen="$1"
  local name="$2"
  if [[ "${MODE}" == "write" ]]; then
    cp "${RUN_DIR}/${gen}" "${OUT_DIR}/${name}"
  else
    if ! cmp -s "${RUN_DIR}/${gen}" "${DATA_DIR}/${name}"; then
      echo "Deviation: ${name}" >&2
      GENERATED_MISMATCH=1
    fi
  fi
}
GENERATED_MISMATCH=0

run_harness polread_harness polread \
  "${BASE_OBJS[@]}"

run_harness polwrit_harness polwrit \
  "${BASE_OBJS[@]}"

run_harness polref_harness polref \
  "${BASE_OBJS[@]}"

run_harness plradd_harness plradd \
  "${BASE_OBJS[@]}"

run_harness plrsrt_harness plrsrt \
  "${BASE_OBJS[@]}"

run_harness plrcop_harness plrcop \
  "${BASE_OBJS[@]}"

run_harness plrset_harness plrset \
  "${BASE_OBJS[@]}"

run_harness apcopy_harness apcopy \
  "${BASE_OBJS[@]}"

run_harness plrsum_harness plrsum \
  "${BASE_OBJS[@]}"

run_harness nammod_harness nammod nammod_stdin.txt \
  "${BASE_OBJS[@]}"

run_harness plrini_harness plrini plrini_stdin.txt \
  "${BASE_OBJS[@]}"
compare_generated plrini_new.pol plrini_new_pol_reference.txt

run_harness plxini_harness plxini plxini_stdin.txt \
  "${BASE_OBJS[@]}"
compare_generated plxini_case.dump plxini_case_dump_reference.bin

run_harness plxadd_harness plxadd plxadd_stdin.txt \
  "${BASE_OBJS[@]}"
compare_generated plxadd_case.dump plxadd_case_dump_reference.bin

run_harness bldump_harness bldump \
  "${BASE_OBJS[@]}"
compare_generated bldump_case.bl bldump_case_reference.bl
compare_generated bldump_case_csv.bl bldump_case_csv_reference.bl
compare_generated bldump_case_tab.bl bldump_case_tab_reference.bl
compare_generated bldump_case_bad.bl bldump_case_bad_reference.bl
compare_generated bldump_case_inviscid.bl bldump_case_inviscid_reference.bl

run_harness bldump2_harness bldump2 \
  "${BASE_OBJS[@]}"
compare_generated bldump2_case.bl bldump2_case_reference.bl
compare_generated bldump2_case_inviscid.bl bldump2_case_inviscid_reference.bl

run_harness cpdump_harness cpdump \
  "${BASE_OBJS[@]}"
compare_generated cpdump_case.cp cpdump_case_reference.cp
compare_generated cpdump_case_csv.cp cpdump_case_csv_reference.cp
compare_generated cpdump_case_tab.cp cpdump_case_tab_reference.cp

run_harness cndump_harness cndump \
  "${BUILD_DIR}/xmdes_cndump.o"

run_harness getdef_harness getdef \
  "${BUILD_DIR}/xfoil_routines.o" \
  "${BUILD_DIR}/userio.o"

run_harness getvov_harness getvov getvov_stdin.txt \
  "${BUILD_DIR}/xmdes_getvov.o" \
  "${BUILD_DIR}/xqdes_routines.o" \
  "${BUILD_DIR}/xfoil_routines.o" \
  "${BUILD_DIR}/spline_routines.o" \
  "${BUILD_DIR}/userio.o" \
  "${BUILD_DIR}/sort.o"

if [[ "${MODE}" == "write" ]]; then
  echo "Harnesses successfully executed."
  echo "Reference data: ${DATA_DIR}/"
else
  mismatch=0
  for name in polread polwrit polref plradd plrsrt plrcop plrset apcopy \
              plrsum nammod plrini plxini plxadd bldump bldump2 cpdump \
              cndump getdef getvov; do
    if ! cmp -s "${OUT_DIR}/${name}_reference.txt" \
        "${DATA_DIR}/${name}_reference.txt"; then
      echo "Deviation: ${name}_reference.txt" >&2
      mismatch=1
    fi
  done
  if [[ "${mismatch}" == "0" && "${GENERATED_MISMATCH}" == "0" ]]; then
    echo "Reference data unchanged: all Files identical."
  else
    echo "Error: Reference data differ from (see above)." >&2
    exit 1
  fi
fi
