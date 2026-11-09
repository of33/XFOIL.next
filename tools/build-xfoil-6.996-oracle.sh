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
#
# With --apply-polar-dump-patch, the frozen sources are copied to the external
# build root, the exact xpol.f input hash is verified, and a repository-owned
# patch adds REWIND(LU) before the new polar-dump header write. The patch is
# applied with --fuzz=0; the frozen repository sources are never modified.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
FROZEN_SRC="${REPO_ROOT}/fortran-src/xfoil-6.996"
PATCH_REL="patches/xfoil-6.996/0001-rewind-polar-dump-unit-before-header.patch"
PATCH_FILE="${REPO_ROOT}/${PATCH_REL}"
EXPECTED_XPOL_SHA="048c491773bd896d7325fa4e00928f1c18551283fae6ed9f45b42f979c5e3f8f"
EXPECTED_PATCHED_XPOL_SHA="f29b6b04b8a18052c8a2996fae12174931243145a3e9677ff4c22fefc84b528d"
EXPECTED_PATCH_SHA="be992f7c48cee137293e15b8fe2dc169b3e5838b5f46c95475b479048cca0572"
PATCH_SHA="not-applied"
PATCHED_XPOL_SHA="not-applied"
DEFAULT_JOBS="${U5_JOBS:-1}"

usage() {
  cat <<'EOF'
Usage: build-xfoil-6.996-oracle.sh --buildroot DIR --plotlib-root DIR [--jobs N] [--apply-polar-dump-patch]

  --buildroot DIR     empty directory outside the repository
  --plotlib-root DIR  external directory containing libPlt_gDP.a
  --jobs N            parallel make jobs (default: $U5_JOBS or 1)
  --apply-polar-dump-patch
                      copy frozen sources to the external build root and
                      apply the repository-owned polar-dump REWIND patch

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
APPLY_PATCH=0
while [[ $# -gt 0 ]]; do
  case "$1" in
    --buildroot) BUILDROOT="${2:-}"; shift 2 ;;
    --plotlib-root) PLOTLIB_ROOT="${2:-}"; shift 2 ;;
    --jobs) JOBS="${2:-}"; shift 2 ;;
    --apply-polar-dump-patch) APPLY_PATCH=1; shift ;;
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
command -v sha256sum >/dev/null 2>&1 || die "sha256sum not found"
command -v python3 >/dev/null 2>&1 || die "python3 not found"
if [[ "${APPLY_PATCH}" -eq 1 ]]; then
  command -v patch >/dev/null 2>&1 || die "patch not found"
fi

read -r GFV_MAJOR GFV_MINOR GFV_PATCH < <(gfortran -dumpfullversion 2>/dev/null | tr '.' ' ')
[[ "${GFV_MAJOR:-}" == "13" && "${GFV_MINOR:-}" == "3" ]] \
  || die "GNU Fortran 13.3 required, found $(gfortran -dumpfullversion 2>/dev/null || gfortran --version | head -n 1)"

CC_VERSION="$(cc -dumpfullversion 2>/dev/null || true)"
[[ "${CC_VERSION}" == 13.3* ]] \
  || die "GNU C compiler 13.3 required, found ${CC_VERSION:-unknown}"

[[ -d "${FROZEN_SRC}" ]] || die "frozen source directory missing: ${FROZEN_SRC}"
[[ -f "${FROZEN_SRC}/SHA256SUMS" ]] || die "frozen SHA256SUMS missing"
[[ -f "${REPO_ROOT}/tools/fortran-provenance.json" ]] \
  || die "Fortran provenance manifest missing"

MANIFEST_XPOL_SHA="$(awk '$2 == "fortran-src/xfoil-6.996/xpol.f" {print $1}' \
  "${FROZEN_SRC}/SHA256SUMS")"
PROVENANCE_XPOL_SHA="$(python3 -c '
import json, sys
p = json.load(open(sys.argv[1]))
print(next(f["sha256"] for v in p["versions"] if v["id"] == "xfoil-6.996"
           for f in v["files"] if f["path"] == "xpol.f"))
' "${REPO_ROOT}/tools/fortran-provenance.json")"
ACTUAL_XPOL_SHA="$(sha256sum "${FROZEN_SRC}/xpol.f" | awk '{print $1}')"
[[ "${MANIFEST_XPOL_SHA}" == "${EXPECTED_XPOL_SHA}" ]] \
  || die "unexpected xpol.f hash in frozen SHA256SUMS: ${MANIFEST_XPOL_SHA}"
[[ "${PROVENANCE_XPOL_SHA}" == "${EXPECTED_XPOL_SHA}" ]] \
  || die "unexpected xpol.f hash in provenance manifest: ${PROVENANCE_XPOL_SHA}"
[[ "${ACTUAL_XPOL_SHA}" == "${EXPECTED_XPOL_SHA}" ]] \
  || die "frozen xpol.f hash mismatch: ${ACTUAL_XPOL_SHA}"

if [[ "${APPLY_PATCH}" -eq 1 ]]; then
  [[ -f "${PATCH_FILE}" ]] || die "patch file missing: ${PATCH_REL}"
  PATCH_SHA="$(sha256sum "${PATCH_FILE}" | awk '{print $1}')"
  [[ "${PATCH_SHA}" == "${EXPECTED_PATCH_SHA}" ]] \
    || die "unexpected patch hash: ${PATCH_SHA}"
fi

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
if [[ "${APPLY_PATCH}" -eq 1 ]]; then
  mkdir -p -- "${BUILDROOT_REAL}/src"
  cp -a -- "${FROZEN_SRC}/." "${BUILDROOT_REAL}/src/"
  ln -s -- "${BUILDROOT_REAL}/src" "${BUILDROOT_REAL}/osrc"
  ln -s -- "${PLOTLIB_REAL}" "${BUILDROOT_REAL}/plotlib"

  PRE_PATCH_XPOL_SHA="$(sha256sum "${BUILDROOT_REAL}/src/xpol.f" | awk '{print $1}')"
  [[ "${PRE_PATCH_XPOL_SHA}" == "${EXPECTED_XPOL_SHA}" ]] \
    || die "copied xpol.f hash mismatch: ${PRE_PATCH_XPOL_SHA}"
  [[ "${PRE_PATCH_XPOL_SHA}" != "${EXPECTED_PATCHED_XPOL_SHA}" ]] \
    || die "polar-dump patch already applied"

  if ! (
    cd "${BUILDROOT_REAL}/src"
    patch --batch --fuzz=0 --forward --no-backup-if-mismatch -p1 < "${PATCH_FILE}"
  ) >"${BUILDROOT_REAL}/patch.log" 2>&1; then
    die "polar-dump patch failed without fuzz; see ${BUILDROOT_REAL}/patch.log"
  fi

  [[ ! -e "${BUILDROOT_REAL}/src/xpol.f.rej" ]] \
    || die "polar-dump patch left a reject file"
  [[ ! -e "${BUILDROOT_REAL}/src/xpol.f.orig" ]] \
    || die "polar-dump patch left a backup file"

  PATCHED_XPOL_SHA="$(sha256sum "${BUILDROOT_REAL}/src/xpol.f" | awk '{print $1}')"
  [[ "${PATCHED_XPOL_SHA}" == "${EXPECTED_PATCHED_XPOL_SHA}" ]] \
    || die "patched xpol.f hash mismatch: ${PATCHED_XPOL_SHA}"

  python3 - "${FROZEN_SRC}" "${BUILDROOT_REAL}/src" \
    "${EXPECTED_XPOL_SHA}" "${EXPECTED_PATCHED_XPOL_SHA}" <<'PY' \
    || die "patched source tree contract failed"
import hashlib
import pathlib
import sys

frozen = pathlib.Path(sys.argv[1])
tree = pathlib.Path(sys.argv[2])
input_sha = sys.argv[3]
output_sha = sys.argv[4]

def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()

frozen_files = {p.relative_to(frozen).as_posix() for p in frozen.rglob('*') if p.is_file()}
tree_files = {p.relative_to(tree).as_posix() for p in tree.rglob('*') if p.is_file()}
if frozen_files != tree_files:
    missing = sorted(frozen_files - tree_files)
    extra = sorted(tree_files - frozen_files)
    print(f'patched tree file set differs: missing={missing} extra={extra}', file=sys.stderr)
    raise SystemExit(1)
for rel in sorted(frozen_files):
    if rel == 'xpol.f':
        continue
    if sha(frozen / rel) != sha(tree / rel):
        print(f'patched tree changed unexpected file: {rel}', file=sys.stderr)
        raise SystemExit(1)
if sha(frozen / 'xpol.f') != input_sha:
    print('frozen xpol.f input hash mismatch', file=sys.stderr)
    raise SystemExit(1)
if sha(tree / 'xpol.f') != output_sha:
    print('patched xpol.f output hash mismatch', file=sys.stderr)
    raise SystemExit(1)
text = (tree / 'xpol.f').read_text()
if text.count('\n      REWIND(LU)\n') != 1:
    print('patched xpol.f does not contain exactly one added REWIND', file=sys.stderr)
    raise SystemExit(1)
if ("   60 CONTINUE\n      REWIND(LU)\n"
    "      WRITE(LU) NAME, 'XFOIL   ', VERSION\n") not in text:
    print('patched xpol.f REWIND is not at the expected header-write site', file=sys.stderr)
    raise SystemExit(1)
PY

  PATCH_STATUS=applied
  SOURCE_XPOL_SHA="${PATCHED_XPOL_SHA}"
else
  ln -s -- "${FROZEN_SRC}" "${BUILDROOT_REAL}/src"
  ln -s -- "${FROZEN_SRC}" "${BUILDROOT_REAL}/osrc"
  ln -s -- "${PLOTLIB_REAL}" "${BUILDROOT_REAL}/plotlib"
  PATCH_STATUS=not-applied
  SOURCE_XPOL_SHA="${EXPECTED_XPOL_SHA}"
fi

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
  printf 'source_baseline=XFOIL 6.996\n'
  printf 'source_manifest_sha256=%s\n' "$(sha256sum "${FROZEN_SRC}/SHA256SUMS" | awk '{print $1}')"
  printf 'frozen_xpol_f_sha256=%s\n' "${EXPECTED_XPOL_SHA}"
  printf 'build_xpol_f_sha256=%s\n' "${SOURCE_XPOL_SHA}"
  printf 'source_patch=%s\n' "${PATCH_REL}"
  printf 'patch_status=%s\n' "${PATCH_STATUS}"
  printf 'patch_file_sha256=%s\n' "${PATCH_SHA}"
  printf 'patched_xpol_f_sha256=%s\n' "${PATCHED_XPOL_SHA}"
  printf 'plotlib_lib_sha256=%s\n' "$(sha256sum "${PLOTLIB_REAL}/libPlt_gDP.a" | awk '{print $1}')"
  printf 'flags_xgdes=-fallow-argument-mismatch\n'
  printf 'flags_blplot=-fallow-argument-mismatch\n'
  printf 'flags_other=none\n'
} >"${BUILDROOT_REAL}/build-environment.txt"

{
  printf 'source_baseline=XFOIL 6.996\n'
  printf 'source_patch=%s\n' "${PATCH_REL}"
  printf 'patch_status=%s\n' "${PATCH_STATUS}"
  printf 'patch_file_sha256=%s\n' "${PATCH_SHA}"
  printf 'frozen_xpol_f_sha256=%s\n' "${EXPECTED_XPOL_SHA}"
  printf 'build_xpol_f_sha256=%s\n' "${SOURCE_XPOL_SHA}"
  printf 'patched_xpol_f_sha256=%s\n' "${PATCHED_XPOL_SHA}"
  if [[ "${APPLY_PATCH}" -eq 1 ]]; then
    printf 'patch_application_log=%s\n' "${BUILDROOT_REAL}/patch.log"
  else
    printf 'patch_application_log=not-applied\n'
  fi
} >"${BUILDROOT_REAL}/patch-manifest.txt"

(
  cd "${BUILDROOT_REAL}/bin"
  LC_ALL=C make -f Makefile.u5 BINDIR=. INSTALLCMD=true -j"${JOBS}" xfoil
) >"${BUILDROOT_REAL}/build.log" 2>&1

[[ -x "${BUILDROOT_REAL}/bin/xfoil" ]] \
  || die "build completed without an executable at ${BUILDROOT_REAL}/bin/xfoil"

{
  printf 'source_baseline=XFOIL 6.996\n'
  printf 'source_patch=%s\n' "${PATCH_REL}"
  printf 'patch_status=%s\n' "${PATCH_STATUS}"
  printf 'patch_file_sha256=%s\n' "${PATCH_SHA}"
  printf 'frozen_xpol_f_sha256=%s\n' "${EXPECTED_XPOL_SHA}"
  printf 'build_xpol_f_sha256=%s\n' "${SOURCE_XPOL_SHA}"
  printf 'patched_xpol_f_sha256=%s\n' "${PATCHED_XPOL_SHA}"
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
