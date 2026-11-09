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

#
# build-xfoil-6.996-oracle.sh - external, reproducible XFOIL 6.996 build
#
# Reads the 49 frozen sources in fortran-src/xfoil-6.996/ without modifying
# them, requires an external build root and an external libPlt_gDP.a, and
# checks GNU Fortran/GCC 13.3 before building. The resulting binary is left
# in the external build root and is never copied into fortran-bin/.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
FROZEN_SRC="${REPO_ROOT}/fortran-src/xfoil-6.996"
DEFAULT_JOBS="${U5_JOBS:-1}"

usage() {
  cat <<'EOF'
Usage: build-xfoil-6.996-oracle.sh --buildroot DIR --plotlib-root DIR [--jobs N]

  --buildroot DIR     empty directory outside the repository
  --plotlib-root DIR  external directory containing libPlt_gDP.a
  --jobs N            parallel make jobs (default: $U5_JOBS or 1)

The script performs no downloads, changes no frozen source, and does not
copy the built binary into fortran-bin/.
EOF
}

die() {
  printf 'build-xfoil-6.996-oracle: %s\n' "$*" >&2
  exit 1
}

BUILDROOT=""
PLOTLIB_ROOT=""
JOBS="${DEFAULT_JOBS}"
while [[ $# -gt 0 ]]; do
  case "$1" in
    --buildroot) BUILDROOT="${2:-}"; shift 2 ;;
    --plotlib-root) PLOTLIB_ROOT="${2:-}"; shift 2 ;;
    --jobs) JOBS="${2:-}"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) die "unknown argument: $1" ;;
  esac
done

[[ -n "${BUILDROOT}" ]] || die "--buildroot is required"
[[ -n "${PLOTLIB_ROOT}" ]] || die "--plotlib-root is required"
[[ "${JOBS}" =~ ^[1-9][0-9]*$ ]] || die "--jobs must be a positive integer"

command -v gfortran >/dev/null 2>&1 || die "gfortran not found"
command -v cc >/dev/null 2>&1 || die "cc not found"
command -v make >/dev/null 2>&1 || die "make not found"
command -v ar >/dev/null 2>&1 || die "ar not found"
command -v ranlib >/dev/null 2>&1 || die "ranlib not found"

read -r GFV_MAJOR GFV_MINOR GFV_PATCH < <(gfortran -dumpfullversion 2>/dev/null | tr '.' ' ')
[[ "${GFV_MAJOR:-}" == "13" && "${GFV_MINOR:-}" == "3" ]] \
  || die "GNU Fortran 13.3 required, found $(gfortran -dumpfullversion 2>/dev/null || gfortran --version | head -n 1)"

CC_VERSION="$(cc -dumpfullversion 2>/dev/null || true)"
[[ "${CC_VERSION}" == 13.3* ]] \
  || die "GNU C compiler 13.3 required, found ${CC_VERSION:-unknown}"

[[ -d "${FROZEN_SRC}" ]] || die "frozen source directory missing: ${FROZEN_SRC}"
[[ -f "${FROZEN_SRC}/SHA256SUMS" ]] || die "frozen SHA256SUMS missing"

BUILDROOT_REAL="$(realpath -m -- "${BUILDROOT}")"
PLOTLIB_REAL="$(realpath -m -- "${PLOTLIB_ROOT}")"
REPO_REAL="$(realpath -m -- "${REPO_ROOT}")"

case "${BUILDROOT_REAL}/" in
  "${REPO_REAL}/"*) die "build root must be outside the repository: ${BUILDROOT_REAL}" ;;
esac
case "${PLOTLIB_REAL}/" in
  "${REPO_REAL}/"*) die "plotlib root must be outside the repository: ${PLOTLIB_REAL}" ;;
esac

[[ -f "${PLOTLIB_REAL}/libPlt_gDP.a" ]] \
  || die "external libPlt_gDP.a missing in ${PLOTLIB_REAL}"

if [[ -e "${BUILDROOT_REAL}" ]]; then
  [[ -d "${BUILDROOT_REAL}" ]] || die "build root is not a directory"
  [[ -z "$(find "${BUILDROOT_REAL}" -mindepth 1 -maxdepth 1 -print -quit)" ]] \
    || die "build root is not empty: ${BUILDROOT_REAL}"
else
  mkdir -p -- "${BUILDROOT_REAL}"
fi

(
  cd "${REPO_ROOT}"
  sha256sum --check "fortran-src/xfoil-6.996/SHA256SUMS" >/dev/null
) || die "frozen source manifest check failed"

mkdir -p -- "${BUILDROOT_REAL}/bin"
ln -s -- "${FROZEN_SRC}" "${BUILDROOT_REAL}/src"
ln -s -- "${FROZEN_SRC}" "${BUILDROOT_REAL}/osrc"
ln -s -- "${PLOTLIB_REAL}" "${BUILDROOT_REAL}/plotlib"

cp -- "${FROZEN_SRC}/Makefile_gfortran" "${BUILDROOT_REAL}/bin/Makefile.u5"

python3 - "${BUILDROOT_REAL}/bin/Makefile.u5" <<'PY'
import sys
p = sys.argv[1]
s = open(p).read()
s = s.replace('xgdes.o: $(SRC)/xgdes.f  $(SRC)/XFOIL.INC $(SRC)/XDES.INC\n\t$(FC) -c $(FFLAGS)  $(SRC)/xgdes.f',
              'xgdes.o: $(SRC)/xgdes.f  $(SRC)/XFOIL.INC $(SRC)/XDES.INC\n\t$(FC) -c $(FFLAGS) -fallow-argument-mismatch $(SRC)/xgdes.f')
s = s.replace('blplot.o: $(SRC)/blplot.f $(SRC)/XFOIL.INC $(SRC)/BLPAR.INC\n\t$(FC) -c $(FFLAGS)  $(SRC)/blplot.f',
              'blplot.o: $(SRC)/blplot.f $(SRC)/XFOIL.INC $(SRC)/BLPAR.INC\n\t$(FC) -c $(FFLAGS) -fallow-argument-mismatch $(SRC)/blplot.f')
open(p, 'w').write(s)
PY

{
  printf 'buildroot=%s\n' "${BUILDROOT_REAL}"
  printf 'plotlib_root=%s\n' "${PLOTLIB_REAL}"
  printf 'gfortran=%s\n' "$(command -v gfortran)"
  gfortran --version | sed -n '1,2p'
  printf 'gfortran_version=%s\n' "$(gfortran -dumpfullversion -dumpversion)"
  printf 'cc=%s\n' "$(command -v cc)"
  cc --version | sed -n '1p'
  printf 'cc_version=%s\n' "${CC_VERSION}"
  printf 'ld_version=%s\n' "$(ld --version | sed -n '1p')"
  printf 'uname=%s\n' "$(uname -a)"
  printf 'locale=LC_ALL=C\n'
  printf 'source_manifest_sha256=%s\n' "$(sha256sum "${FROZEN_SRC}/SHA256SUMS" | awk '{print $1}')"
  printf 'plotlib_lib_sha256=%s\n' "$(sha256sum "${PLOTLIB_REAL}/libPlt_gDP.a" | awk '{print $1}')"
  printf 'flags_xgdes=-fallow-argument-mismatch\n'
  printf 'flags_blplot=-fallow-argument-mismatch\n'
  printf 'flags_other=none\n'
} >"${BUILDROOT_REAL}/build-environment.txt"

(
  cd "${BUILDROOT_REAL}/bin"
  LC_ALL=C make -f Makefile.u5 BINDIR=. INSTALLCMD=true -j"${JOBS}" xfoil
) >"${BUILDROOT_REAL}/build.log" 2>&1

[[ -x "${BUILDROOT_REAL}/bin/xfoil" ]] \
  || die "build completed without an executable at ${BUILDROOT_REAL}/bin/xfoil"

{
  printf 'binary=%s\n' "${BUILDROOT_REAL}/bin/xfoil"
  printf 'binary_sha256=%s\n' "$(sha256sum "${BUILDROOT_REAL}/bin/xfoil" | awk '{print $1}')"
  printf 'binary_size=%s\n' "$(stat -c %s "${BUILDROOT_REAL}/bin/xfoil")"
  if command -v readelf >/dev/null 2>&1; then
    printf 'build_id=%s\n' "$(LC_ALL=C readelf -n "${BUILDROOT_REAL}/bin/xfoil" | sed -n 's/.*Build[ -]ID: //p' | head -n 1)"
    printf 'needed=%s\n' "$(LC_ALL=C readelf -d "${BUILDROOT_REAL}/bin/xfoil" | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p' | paste -sd, -)"
    printf 'rpath_runpath=%s\n' "$(LC_ALL=C readelf -d "${BUILDROOT_REAL}/bin/xfoil" | sed -n 's/.*(\(RPATH\|RUNPATH\)).*\[\(.*\)\]/\2/p' | paste -sd, -)"
  fi
  printf 'object_hashes:\n'
  (cd "${BUILDROOT_REAL}/bin" && sha256sum ./*.o | sort)
  printf 'plotlib_objects:\n'
  (cd "${BUILDROOT_REAL}/plotlib" && sha256sum ./*.o 2>/dev/null | sort || true)
  printf 'plotlib_lib_sha256=%s\n' "$(sha256sum "${PLOTLIB_REAL}/libPlt_gDP.a" | awk '{print $1}')"
} >"${BUILDROOT_REAL}/build-manifest.txt"

printf 'Built %s\n' "${BUILDROOT_REAL}/bin/xfoil"
printf 'SHA-256: %s\n' "$(sha256sum "${BUILDROOT_REAL}/bin/xfoil" | awk '{print $1}')"
printf 'Manifest: %s\n' "${BUILDROOT_REAL}/build-manifest.txt"
