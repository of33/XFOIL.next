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

# file: tools/build-fmom-vels-harnesses.sh
# Builds and runs the focused Fortran harnesses for the
# canonical FMOM-/MHINGE and VELS-/PSILIN paths and
# produces the reproducible reference data below
# tests/data/core-fmom-vels-reference/.
#
# The harnesses call the canonical routines from
# fortran-src/xfoil-6.99/ unchanged. For this, all
# fortran-src/xfoil-6.99 source files except the main program routine in
# xfoil.f are compiled; the non-main routines of xfoil.f are
# extracted byte-faithfully with
# tools/extract-fortran-routines.py. No
# Fortran source, no raw reference and no golden file
# is changed.
#
# precision contract: canonical build with -fdefault-real-8
# -fdefault-double-8 (as fortran-bin/xfoil); unqualified REAL
# is REAL(8), directly bit-comparable with C++ double.
#
# Modes (exactly one required):
# tools/build-fmom-vels-harnesses.sh --write # build and run,
# # reference data below
# # tests/data/
# tools/build-fmom-vels-harnesses.sh --check # build and run, output in
# # a temporary directory
# # written and compared byte-for-byte
# # against tests/data/.
#
# Optional: FC (Fortran compiler, default gfortran).

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HARNESS_FMOM_DIR="${REPO_ROOT}/tests/fortran-reference/harnesses/fmom"
HARNESS_VELS_DIR="${REPO_ROOT}/tests/fortran-reference/harnesses/vels"
DATA_DIR="${REPO_ROOT}/tests/data/core-fmom-vels-reference"
BUILD_DIR="${REPO_ROOT}/build/core-fmom-vels-harness"
FC="${FC:-gfortran}"
R8FLAGS=(-fdefault-real-8 -fdefault-double-8 -fallow-argument-mismatch -I"${REPO_ROOT}/fortran-src/xfoil-6.99")

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

# Byte-faithful extraction of all non-main routines from xfoil.f.
ROUTINES=()
while IFS= read -r name; do
  ROUTINES+=("${name}")
done < <(grep -E '^      SUBROUTINE [A-Z][A-Z0-9]*' \
  "${REPO_ROOT}/fortran-src/xfoil-6.99/xfoil.f" | sed -E 's/^      SUBROUTINE ([A-Z][A-Z0-9]*).*/\1/' | sort -u)
python3 "${REPO_ROOT}/tools/extract-fortran-routines.py" \
  --source "${REPO_ROOT}/fortran-src/xfoil-6.99/xfoil.f" \
  --routines "${ROUTINES[@]}" \
  --output "${BUILD_DIR}/xfoil_all_subroutines.f" > /dev/null

# Compile all canonical source files except xfoil.f directly.
OBJS=()
for src in "${REPO_ROOT}"/fortran-src/xfoil-6.99/*.f; do
  base="$(basename "${src}")"
  if [[ "${base}" == "xfoil.f" ]]; then
    continue
  fi
  # Plot/polarfit routines outside the MHINGE-/PSILIN data paths are
  # not compiled (the canonical sources remain unchanged; the
  # modern gfortran strictness rules for these original Fortran sources are
  # not part of the harness requirement).
  #
  # osmap_DP.f is an inactive upstream alternative and exports the same
  # OSMAP symbol as the canonical runtime implementation. The active
  # OSMAP path is exclusively osmap.f (replaced here by the
  # harness stub); osmap_DP.f and osmapDP.dat remain
  # unrelated original artifacts and are never
  # automatically compiled or linked.
  case "${base}" in
    polfit.f|polplt.f|pplot.f|pxplot.f|blplot.f|frplot.f|plutil.f|gui.f|osmap.f|osmap_DP.f|getarg.f|xtcam.f|xplots.f|blu.f)
      continue ;;
  esac
  obj="${BUILD_DIR}/${base%.f}.o"
  "${FC}" -w "${R8FLAGS[@]}" -c "${src}" -o "${obj}"
  OBJS+=("${obj}")
done
"${FC}" -w "${R8FLAGS[@]}" -c "${BUILD_DIR}/xfoil_all_subroutines.f" \
  -o "${BUILD_DIR}/xfoil_all_subroutines.o"
OBJS+=("${BUILD_DIR}/xfoil_all_subroutines.o")

# No-op stubs for purely graphical outsider symbols,
# whose files are not compiled in the harness (no numerical
# effect on the MHINGE-/PSILIN data path).
cat > "${BUILD_DIR}/plot_stubs.f" <<'STUBEOF'
      SUBROUTINE PLINITIALIZE
      RETURN
      END
      SUBROUTINE COLORSPECTRUMHUES
      RETURN
      END
      SUBROUTINE REPLOT
      RETURN
      END
      SUBROUTINE CLRZOOM
      RETURN
      END
      SUBROUTINE ERASE
      RETURN
      END
      SUBROUTINE SETCOL(ICTY)
      INTEGER ICTY
      RETURN
      END
      SUBROUTINE DISPLA(IFLAG)
      INTEGER IFLAG
      RETURN
      END
      SUBROUTINE FINAGL
      RETURN
      END
      SUBROUTINE SETLIN(ICTY,IFLG)
      INTEGER ICTY,IFLG
      RETURN
      END
      SUBROUTINE SETLINI(IZOOM)
      INTEGER IZOOM
      RETURN
      END
      SUBROUTINE USETZOOM
      RETURN
      END
      SUBROUTINE PLEND
      RETURN
      END
      SUBROUTINE PLGRID(XMIN,YMIN,DX,NX,DY,NY,LMASK)
      REAL XMIN,YMIN,DX,DY
      INTEGER NX,NY,LMASK
      RETURN
      END
      SUBROUTINE PLOT(A,B,ICODE)
      REAL A,B
      INTEGER ICODE
      RETURN
      END
      SUBROUTINE PLOTTO(X,Y)
      REAL X,Y
      RETURN
      END
      SUBROUTINE PLOTFR(X,Y)
      REAL X,Y
      RETURN
      END
      SUBROUTINE SYMBL(X,Y,CH)
      REAL X,Y
      CHARACTER CH
      RETURN
      END
      SUBROUTINE NUMBER(X,Y,CH)
      REAL X,Y
      CHARACTER CH
      RETURN
      END
      SUBROUTINE PLOTS(NHILITE,NBRITE)
      INTEGER NHILITE,NBRITE
      RETURN
      END
      SUBROUTINE CONTOUR
      RETURN
      END
      SUBROUTINE CROSS(XC,YC,R,ANG)
      REAL XC,YC,R,ANG
      RETURN
      END
      SUBROUTINE ERASR(X,Y,R,ANG)
      REAL X,Y,R,ANG
      RETURN
      END
      SUBROUTINE LABEL(X,Y,A,DX,DY,CH)
      REAL X,Y,DX,DY
      CHARACTER*(*) A
      CHARACTER CH
      RETURN
      END
      SUBROUTINE FULLCH
      RETURN
      END
      SUBROUTINE AXISADJ(XMIN1,XMAX1,YMIN1,YMAX1,
     &                   XMIN2,XMAX2,YMIN2,YMAX2,
     &                   XMIN3,XMAX3,YMIN3,YMAX3,
     &                   XMIN4,XMAX4,YMIN4,YMAX4)
      REAL XMIN1,XMAX1,YMIN1,YMAX1,XMIN2,XMAX2,YMIN2,YMAX2
      REAL XMIN3,XMAX3,YMIN3,YMAX3,XMIN4,XMAX4,YMIN4,YMAX4
      RETURN
      END
      SUBROUTINE ANNOT(ITYPE,X,Y,CH)
      INTEGER ITYPE
      REAL X,Y
      CHARACTER*(*) CH
      RETURN
      END
      SUBROUTINE FACTOR(XMIN,XMAX,YMIN,YMAX,X0,Y0,FAC)
      REAL XMIN,XMAX,YMIN,YMAX,X0,Y0,FAC
      RETURN
      END
      SUBROUTINE PENMAP(IPEN)
      INTEGER IPEN
      RETURN
      END
      SUBROUTINE PENMOV(X,Y)
      REAL X,Y
      RETURN
      END
      SUBROUTINE PENDRW(X,Y)
      REAL X,Y
      RETURN
      END
      SUBROUTINE VECSYM(X,Y,DX,DY,CH)
      REAL X,Y,DX,DY
      CHARACTER CH
      RETURN
      END
      SUBROUTINE PANPLT
      RETURN
      END
      SUBROUTINE BLWPLT
      RETURN
      END
      SUBROUTINE BLGPLT
      RETURN
      END
      SUBROUTINE BLTPLT
      RETURN
      END
      SUBROUTINE RESPLT
      RETURN
      END
      SUBROUTINE XYPLOT
      RETURN
      END
      SUBROUTINE FNDOT(ISHOW)
      INTEGER ISHOW
      RETURN
      END
      SUBROUTINE DOTSIZ(DSIZE)
      REAL DSIZE
      RETURN
      END
      SUBROUTINE HARDCP
      RETURN
      END
      SUBROUTINE HARDBL
      RETURN
      END
      SUBROUTINE VIEWTR
      RETURN
      END
      SUBROUTINE VIEWIT
      RETURN
      END
      SUBROUTINE MARGIN(FRAC,XMIN,XMAX,YMIN,YMAX)
      REAL FRAC,XMIN,XMAX,YMIN,YMAX
      RETURN
      END
      SUBROUTINE NEXTF(ILUN)
      INTEGER ILUN
      RETURN
      END
      SUBROUTINE WRITF(ILUN)
      INTEGER ILUN
      RETURN
      END
      SUBROUTINE OPENI(IZOOM)
      INTEGER IZOOM
      RETURN
      END
      SUBROUTINE OPENQ(IZOOM)
      INTEGER IZOOM
      RETURN
      END
      SUBROUTINE OFFGET(IREF,XREF,YREF,X,Y,IRET)
      INTEGER IREF,IRET
      REAL XREF,YREF,X,Y
      RETURN
      END
      SUBROUTINE GETCURSORXY(XC,YC,IRET)
      REAL XC,YC
      INTEGER IRET
      RETURN
      END
      SUBROUTINE CURSOP(ITYPE,XC,YC)
      INTEGER ITYPE
      REAL XC,YC
      RETURN
      END
      SUBROUTINE GETCURSORKEY(CH)
      CHARACTER CH
      RETURN
      END
      SUBROUTINE SETCURSOR(ITYPE,XC,YC)
      INTEGER ITYPE
      REAL XC,YC
      RETURN
      END
      SUBROUTINE SETCURSOP(ITYPE,XC,YC)
      INTEGER ITYPE
      REAL XC,YC
      RETURN
      END
      SUBROUTINE NEWCOLOR(ICOL)
      INTEGER ICOL
      RETURN
      END
      SUBROUTINE PLFLUSH
      RETURN
      END
      SUBROUTINE NEWPEN(IPEN)
      INTEGER IPEN
      RETURN
      END
      SUBROUTINE SETCOLOR(ICOL)
      INTEGER ICOL
      RETURN
      END
      SUBROUTINE SETPEN(IPEN)
      INTEGER IPEN
      RETURN
      END
      SUBROUTINE SETLINE(ITYPE)
      INTEGER ITYPE
      RETURN
      END
      SUBROUTINE SETLINEI(ITYPE)
      INTEGER ITYPE
      RETURN
      END
      INTEGER FUNCTION GETCOLOR()
      GETCOLOR = 0
      RETURN
      END
      SUBROUTINE NEWCOLORNAME(NAME)
      CHARACTER*(*) NAME
      RETURN
      END
      SUBROUTINE SETCOLORNAME(NAME)
      CHARACTER*(*) NAME
      RETURN
      END
      SUBROUTINE PLNUMB(XP,YP,CH,NC)
      REAL XP,YP
      CHARACTER*(*) CH
      INTEGER NC
      RETURN
      END
      SUBROUTINE PLNUMB2(XP,YP,CH)
      REAL XP,YP
      CHARACTER*(*) CH
      RETURN
      END
      SUBROUTINE PLCHAR
      RETURN
      END
      SUBROUTINE PLCHARF
      RETURN
      END
      SUBROUTINE PLMATH
      RETURN
      END
      SUBROUTINE PLCON1
      RETURN
      END
      SUBROUTINE PLCON2
      RETURN
      END
      SUBROUTINE PLLINE
      RETURN
      END
      SUBROUTINE PLDOT
      RETURN
      END
      SUBROUTINE PLDOTC
      RETURN
      END
      SUBROUTINE PLVEC
      RETURN
      END
      SUBROUTINE PLVEC2
      RETURN
      END
      SUBROUTINE PLBOX
      RETURN
      END
      SUBROUTINE PLBOXC
      RETURN
      END
      SUBROUTINE PLTICK
      RETURN
      END
      SUBROUTINE PLGRID2
      RETURN
      END
      SUBROUTINE PLERASE
      RETURN
      END
      SUBROUTINE PLFRAME
      RETURN
      END
      SUBROUTINE PLSIZE
      RETURN
      END
      SUBROUTINE PLORIGIN
      RETURN
      END
      SUBROUTINE PLROT
      RETURN
      END
      SUBROUTINE PLUNDO
      RETURN
      END
      SUBROUTINE DASH
      RETURN
      END
      SUBROUTINE PLDOTL
      RETURN
      END
      SUBROUTINE PLSOLID
      RETURN
      END
      SUBROUTINE PLHATCH
      RETURN
      END
      SUBROUTINE PLHATCHC
      RETURN
      END
      SUBROUTINE PLHATCHF
      RETURN
      END
      SUBROUTINE PLTINI
      RETURN
      END
      SUBROUTINE PLFINI
      RETURN
      END
      SUBROUTINE PLSYMB
      RETURN
      END
      SUBROUTINE PLSYMC
      RETURN
      END
      SUBROUTINE BLPLOT
      RETURN
      END
      SUBROUTINE CPX
      RETURN
      END
      SUBROUTINE CPMIN
      RETURN
      END
      SUBROUTINE CPMAX
      RETURN
      END
      SUBROUTINE UEPLOT
      RETURN
      END
      SUBROUTINE GEOPLOT
      RETURN
      END
      SUBROUTINE FCPLOT
      RETURN
      END
      SUBROUTINE BLPLOT2
      RETURN
      END
      SUBROUTINE CPVEC
      RETURN
      END
      SUBROUTINE CPVECI
      RETURN
      END
      SUBROUTINE CPVEC2
      RETURN
      END
      SUBROUTINE PPAPLT
      RETURN
      END
      SUBROUTINE UEX
      RETURN
      END
      SUBROUTINE PPLO
      RETURN
      END
      SUBROUTINE VPLO
      RETURN
      END
      SUBROUTINE BLWT
      RETURN
      END
      SUBROUTINE BLC
      RETURN
      END
      SUBROUTINE BLPLT2
      RETURN
      END
      SUBROUTINE GRIDP
      RETURN
      END
      SUBROUTINE FREF2
      RETURN
      END
      SUBROUTINE NEWFACTOR
      RETURN
      END
      SUBROUTINE POLPLT
      RETURN
      END
      SUBROUTINE POLPLOT
      RETURN
      END
      SUBROUTINE POLREF2
      RETURN
      END
      SUBROUTINE XYLINE
      RETURN
      END
      SUBROUTINE XYLINE2
      RETURN
      END
      SUBROUTINE XYLINE3
      RETURN
      END
      SUBROUTINE SEQLAB
      RETURN
      END
      SUBROUTINE SEQPLT
      RETURN
      END
      SUBROUTINE SEQSET
      RETURN
      END
      SUBROUTINE SEQSRT
      RETURN
      END
      SUBROUTINE SEQADD
      RETURN
      END
      SUBROUTINE SEQDEL
      RETURN
      END
      SUBROUTINE SEQSUM
      RETURN
      END
      SUBROUTINE AIRLIM
      RETURN
      END
      SUBROUTINE CPAXES
      RETURN
      END
      SUBROUTINE CPAXES2
      RETURN
      END
      SUBROUTINE CPAXES3
      RETURN
      END
      SUBROUTINE COEFPL
      RETURN
      END
      SUBROUTINE COEFPL2
      RETURN
      END
      SUBROUTINE XAXIS
      RETURN
      END
      SUBROUTINE YAXIS
      RETURN
      END
      SUBROUTINE XAXIS2
      RETURN
      END
      SUBROUTINE YAXIS2
      RETURN
      END
      SUBROUTINE CAMB
      RETURN
      END
      SUBROUTINE ZERCAM
      RETURN
      END
      SUBROUTINE CAMBER
      RETURN
      END
      SUBROUTINE THICKD
      RETURN
      END
      SUBROUTINE TINKER
      RETURN
      END
      SUBROUTINE XTCAM2
      RETURN
      END
      SUBROUTINE NEWPLOTC
      RETURN
      END
      SUBROUTINE PLOTC
      RETURN
      END
      SUBROUTINE TCBUF
      RETURN
      END
      SUBROUTINE TCSCAL
      RETURN
      END
      SUBROUTINE TCSET
      RETURN
      END
      SUBROUTINE THKCAM
      RETURN
      END
      SUBROUTINE HIPNT
      RETURN
      END
      SUBROUTINE GETCAM
      RETURN
      END
      SUBROUTINE GETMAX
      RETURN
      END
      SUBROUTINE CPCAM
      RETURN
      END
      SUBROUTINE PLSUBS
      RETURN
      END
      SUBROUTINE PLSUBC
      RETURN
      END
      SUBROUTINE SCALIT
      RETURN
      END
      SUBROUTINE SCALIT2
      RETURN
      END
      SUBROUTINE PGUI
      RETURN
      END
      SUBROUTINE LGUI
      RETURN
      END
      SUBROUTINE GUIINIT
      RETURN
      END
      SUBROUTINE OSMAP
      RETURN
      END
      SUBROUTINE OSMAP2
      RETURN
      END
      SUBROUTINE GUIBOX
      RETURN
      END
      SUBROUTINE GETCURSORXYC
      RETURN
      END
      SUBROUTINE GETCURSORXYC2
      RETURN
      END
      SUBROUTINE GETPEN
      RETURN
      END
      SUBROUTINE SETPEN2
      RETURN
      END
STUBEOF
sed -i "s/COLORSPECTRUMHUES/COLORSPC/" /dev/null 2>/dev/null || true
"${FC}" -w "${R8FLAGS[@]}" -c "${BUILD_DIR}/plot_stubs.f" \
  -o "${BUILD_DIR}/plot_stubs.o"
OBJS+=("${BUILD_DIR}/plot_stubs.o")

BASE_OBJS=("${OBJS[@]}")
"${FC}" -w "${R8FLAGS[@]}" -c "${HARNESS_FMOM_DIR}/fmom_harness.f" \
  -o "${BUILD_DIR}/fmom_harness.o"
"${FC}" -w "${R8FLAGS[@]}" -c "${HARNESS_VELS_DIR}/vels_harness.f" \
  -o "${BUILD_DIR}/vels_harness.o"

# Negative check: osmap_DP.f is an inactive upstream alternative
# with the same OSMAP symbol as the canonical runtime implementation.
# The active and inactive OSMAP implementations must never be
# compiled or linked together (tools/check-osmap-selection.sh).
"${REPO_ROOT}/tools/check-osmap-selection.sh" "${BUILD_DIR}"
"${FC}" "${R8FLAGS[@]}" "${BASE_OBJS[@]}" "${BUILD_DIR}/fmom_harness.o" \
  -o "${BUILD_DIR}/fmom_harness"
"${FC}" "${R8FLAGS[@]}" "${BASE_OBJS[@]}" "${BUILD_DIR}/vels_harness.o" \
  -o "${BUILD_DIR}/vels_harness"

( cd "${BUILD_DIR}" && LC_ALL=C ./fmom_harness ) > "${OUT_DIR}/fmom_reference.txt" 2>&1
( cd "${BUILD_DIR}" && LC_ALL=C ./vels_harness ) > "${OUT_DIR}/vels_reference.txt" 2>&1

if [[ "${MODE}" == "write" ]]; then
  echo "Reference written: ${DATA_DIR}/fmom_reference.txt"
  echo "Reference written: ${DATA_DIR}/vels_reference.txt"
else
  ok=1
  for ref in fmom_reference.txt vels_reference.txt; do
    if ! diff -q "${DATA_DIR}/${ref}" "${OUT_DIR}/${ref}" > /dev/null; then
      ok=0
    fi
  done
  if [[ "${ok}" == "1" ]]; then
    echo "Reference data unchanged: all 2 Files identical."
  else
    echo "ERROR: Reference data differ itself." >&2
    exit 1
  fi
fi
