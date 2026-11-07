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
# fortran-ref.sh - Reproducible Fortran golden-reference basis
#
# Runs the canonical XFOIL-6.99 Fortran process (fortran-bin/xfoil) in a
# separate, empty temporary working directory, with LC_ALL=C,
# an enforced timeout, separate capture of stdout/stderr, recursive
# collection of the generated files (null-terminated) and computation of
# SHA-256 checksums.
#
# The tool:
# - runs no Git commands,
# - changes no Fortran sources,
# - does not rebuild the binary,
# - changes no C++ goldens,
# - runs no productive C++ implementation,
# - changes nothing for `verify`/`determinism` (only `generate`
# overwrites reference artifacts, and only in the uniquely resolved
# case directory below CASES_ROOT).
#
# Robustness:
# - temporary comparison lists per call (mktemp), no fixed /tmp path,
# - recursive, null-terminated file capture (hidden files,
# subdirectories, names like `:00.bl`, changed input files),
# - the single documented name normalization `:00.bl` -> `100.bl`
# (XFOIL 6.99 VISCAL tens-digit overflow; content unchanged),
# - no symlinks followed out of the working directory; symlinks => abort,
# - case names are resolved exclusively below CASES_ROOT (no
# absolute path, no `..`, no leaving CASES_ROOT),
# - all temporary directories are cleaned up on signal/error/timeout/
# return.
#
# Modes:
# generate <case> run a case and write the reference artifacts
# verify <case> check a case (does not overwrite the reference)
# determinism <case> check a case twice in separate directories
# all-generate generate all cases
# all-verify check all cases
# all-determinism check determinism of all cases
# manifest rebuild the manifest (manifest.tsv) from the cases
# list list all cases
#
# A case lies below:
# tests/fortran-reference/cases/<category>/<case-name>/
# commands.in input lines for stdin (with PLOP/graphics deactivation)
# input/ optional input files (copied into the working directory)
# metadata.txt key: value - metadata (TIMEOUT, PURPOSE, STEP, ...)
#
# Produced reference artifacts (only in generate mode):
# stdout.raw.txt, stderr.raw.txt, generated/*, run-info.txt, artifacts.sha256

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BINARY="${REPO_ROOT}/fortran-bin/xfoil"
CASES_ROOT="${REPO_ROOT}/tests/fortran-reference/cases"
MANIFEST="${REPO_ROOT}/tests/fortran-reference/manifest.tsv"
DEFAULT_TIMEOUT=120

if [[ ! -x "${BINARY}" ]]; then
  echo "Error: binary not executable: ${BINARY}" >&2
  exit 1
fi

BIN_SHA="$(sha256sum "${BINARY}" | awk '{print $1}')"

# Normalized (symlink-resolved) CASES_ROOT for the safe path check.
CASES_ROOT_REAL="$(realpath -m -- "${CASES_ROOT}")"

log() { printf '%s\n' "$*" >&2; }

# ---------------------------------------------------------------------------
# Temporary directories: central tracking + guaranteed cleanup.
# ---------------------------------------------------------------------------
TMPDIRS=()

# Creates a private temporary directory and registers it for the
# cleanup. Sets LAST_TMPDIR (must be called without command substitution,
# since otherwise the registration is lost in a subshell).
LAST_TMPDIR=""
_make_tmp() {
  LAST_TMPDIR="$(mktemp -d "${TMPDIR:-/tmp}/xfoilref.XXXXXX")" || return 1
  TMPDIRS+=("${LAST_TMPDIR}")
}

# Deletes all temporary directories created so far.
_cleanup_all() {
  local d
  for d in "${TMPDIRS[@]:-}"; do
    if [[ -n "${d}" && -d "${d}" ]]; then rm -rf -- "${d}"; fi
  done
}

trap '_cleanup_all' EXIT
trap 'trap - EXIT; _cleanup_all; exit 130' INT
trap 'trap - EXIT; _cleanup_all; exit 143' TERM

# ---------------------------------------------------------------------------
# metadata helpers
# ---------------------------------------------------------------------------
# Reads a key from a metadata.txt/run-info.txt (first matching line
# "key: value").
meta_get() {
  local file="$1" key="$2"
  [[ -f "${file}" ]] || { printf ''; return; }
  awk -F': *' -v k="${key}" '
    BEGIN { lk = tolower(k) }
    tolower($1)==lk { sub(/^[^:]*:[ \t]*/,""); print; exit }
  ' "${file}"
}

# ---------------------------------------------------------------------------
# Safe case resolution
# ---------------------------------------------------------------------------
# Resolves a relative case name exclusively below CASES_ROOT.
# Rejected: absolute paths, `..` traversal, leaving CASES_ROOT.
# Prints the resolved absolute path on stdout.
resolve_case() {
  local arg="$1"
  if [[ -z "${arg}" ]]; then
    log "Error: case name missing"
    return 1
  fi
  if [[ "${arg}" == /* ]]; then
    log "Error: absolute case paths are not allowed: ${arg}"
    return 1
  fi
  if [[ "${arg}" == *".."* || "${arg}" == ./* || "${arg}" == */*..* ]]; then
    log "Error: the case name must not contain '..' path traversal: ${arg}"
    return 1
  fi
  local real
  real="$(realpath -m -- "${CASES_ROOT}/${arg}" 2>/dev/zero)" \
    || { log "Error: case path not resolvable: ${arg}"; return 1; }
  case "${real}" in
    "${CASES_ROOT_REAL}"/*) ;;
    *)
      log "Error: case path escapes CASES_ROOT: ${arg}"
      return 1
      ;;
  esac
  printf '%s' "${real}"
}

# ---------------------------------------------------------------------------
# Single-case execution
# ---------------------------------------------------------------------------
# Runs a case in a fresh, empty temporary working directory.
# $1 = case dir (resolved), $2 = temporary working directory, $3 = artifact directory.
#
# Return values (deliberately separate):
# 0 run completed (the Fortran process ran; the exit status is stored
# in run-info.txt as a reference value and is not a tool error).
# 2 infrastructure error (e.g. commands.in missing).
# 3 unsupported artifact (symlink in the working directory).
run_case_once() {
  local casedir="$1" workdir="$2" outdir="$3"
  local timeout="${4:-${DEFAULT_TIMEOUT}}"

  rm -rf -- "${workdir}" "${outdir}"
  mkdir -p -- "${workdir}" "${outdir}"

  # Copy the input files (avoids state adoption).
  if [[ -d "${casedir}/input" ]]; then
    cp -a "${casedir}/input/." "${workdir}/"
  fi

  local commands="${casedir}/commands.in"
  if [[ ! -f "${commands}" ]]; then
    log "Error: ${commands} missing"
    return 2
  fi

  # input file after the workdir copy, so that relative paths work.
  cp -a "${commands}" "${workdir}/commands.in"

  local start end rc
  start="$(date +%s)"
  # timeout enforces the limit; EOF/timeout => rc != 0.
  ( cd "${workdir}" && \
    LC_ALL=C timeout --signal=TERM "${timeout}" \
      "${BINARY}" < commands.in > stdout.raw.txt 2> stderr.raw.txt )
  rc=$?
  end="$(date +%s)"

  # timeout code of GNU timeout is 124 (TERM) or 143 (128+SIGTERM).
  local timedout=0
  if [[ ${rc} -eq 124 || ${rc} -eq 143 ]]; then timedout=1; fi

  # Symlink detection: symlinks are not followed. If the Fortran
  # program produces a symlink, that is an unsupported artifact => abort.
  local symlink
  symlink="$(find "${workdir}" -type l -print 2>/dev/zero | head -n 1)"
  if [[ -n "${symlink}" ]]; then
    log "Error: symlink in the working directory (unsupported artifact): ${symlink}"
    return 3
  fi

  cp -a "${workdir}/stdout.raw.txt" "${outdir}/stdout.raw.txt"
  cp -a "${workdir}/stderr.raw.txt"  "${outdir}/stderr.raw.txt"

  # Collect produced files recursively and null-terminated:
  # - normal, hidden and nested files,
  # - unusual names like `:00.bl`,
  # - input files changed during the run (same relative path).
  # Excluded: commands.in, stdout.raw.txt, stderr.raw.txt and
  # unchanged files from input/ (comparison via relative path).
  # Relative subdirectories are kept below generated/.
  mkdir -p -- "${outdir}/generated"
  local f rel
  while IFS= read -r -d '' f; do
    rel="${f#"${workdir}"/}"
    case "${rel}" in
      commands.in|stdout.raw.txt|stderr.raw.txt) continue ;;
    esac
    # Portable artifact-name contract (PUB-W1): the original Fortran
    # VISCAL (xoper.f:3061-3077) encodes xtrip/c * 100 into three
    # characters; for the untrip value 1.0 the tens digit 10 renders as
    # CHAR(10 + '0') = ':', producing ':00.bl' on POSIX. Windows rejects
    # ':' in filenames. For this one known, exact XFOIL 6.99 case only,
    # the captured name is mapped to the technically intended portable
    # name 100.bl; the file content is never changed. No other name is
    # ever rewritten.
    if [[ "${rel}" == ':00.bl' ]]; then rel='100.bl'; fi
    if [[ -e "${casedir}/input/${rel}" ]]; then
      # input file: only capture as generated if its own content changed.
      if cmp -s -- "${f}" "${casedir}/input/${rel}"; then
        continue
      fi
    fi
    mkdir -p -- "${outdir}/generated/$(dirname -- "${rel}")"
    cp -a -- "${f}" "${outdir}/generated/${rel}"
  done < <(find "${workdir}" -type f -print0)

  # Result metadata.
  {
    printf 'exit-status: %s\n' "${rc}"
    printf 'timed-out: %s\n' "${timedout}"
    printf 'duration-sec: %s\n' "$(( end - start ))"
    printf 'timeout-limit-sec: %s\n' "${timeout}"
    printf 'binary-sha256: %s\n' "${BIN_SHA}"
    printf 'locale: LC_ALL=C\n'
  } > "${outdir}/run-info.txt"
  return 0
}

# ---------------------------------------------------------------------------
# Artifact set (null-terminated) and comparison
# ---------------------------------------------------------------------------
# Lists the canonical artifact set of a folder as null-terminated
# relative paths: stdout.raw.txt, stderr.raw.txt, generated/* (recursive).
# Both relative names and contents are accounted for; they are secured
# by the callers; here only the set.
list_all_artifacts_null() {
  local d="$1"
  {
    [[ -f "${d}/stdout.raw.txt" ]] && printf 'stdout.raw.txt\0'
    [[ -f "${d}/stderr.raw.txt" ]] && printf 'stderr.raw.txt\0'
    if [[ -d "${d}/generated" ]]; then
      ( cd "${d}/generated" && find . -type f -printf 'generated/%P\0' )
    fi
  } | sort -z
}

# Compares the canonical artifact set of two folders byte-wise. 0 for identity.
# Uses its own mktemp-produced comparison lists per call in a
# private temporary directory and cleans up on all return paths.
compare_artifacts() {
  local a="$1" b="$2"
  local tmp
  _make_tmp || return 2
  tmp="${LAST_TMPDIR}"
  local al="${tmp}/a.art" bl="${tmp}/b.art"

  list_all_artifacts_null "${a}" > "${al}" || { rm -rf -- "${tmp}"; return 1; }
  list_all_artifacts_null "${b}" > "${bl}" || { rm -rf -- "${tmp}"; return 1; }

  if ! cmp -s -- "${al}" "${bl}"; then
    log "  File set differs:"
    tr '\0' '\n' < "${al}" | sed 's/^/    a: /' >&2
    tr '\0' '\n' < "${bl}" | sed 's/^/    b: /' >&2
    rm -rf -- "${tmp}"
    return 1
  fi

  local f
  while IFS= read -r -d '' f; do
    if [[ -z "${f}" ]]; then continue; fi
    if ! cmp -s -- "${a}/${f}" "${b}/${f}"; then
      log "  File differs: ${f}"
      rm -rf -- "${tmp}"
      return 1
    fi
  done < "${al}"

  rm -rf -- "${tmp}"
  return 0
}

# Compares exit status, timeout status, binary SHA and locale of two
# run-info.txt files. duration-sec is not compared. 0 for identity.
check_run_info() {
  local actual="$1" canonical="$2"
  local ok=0
  local a_exit a_tmo a_sha a_loc c_exit c_tmo c_sha c_loc
  a_exit="$(meta_get "${actual}" exit-status)"
  a_tmo="$(meta_get "${actual}" timed-out)"
  a_sha="$(meta_get "${actual}" binary-sha256)"
  a_loc="$(meta_get "${actual}" locale)"
  c_exit="$(meta_get "${canonical}" exit-status)"
  c_tmo="$(meta_get "${canonical}" timed-out)"
  c_sha="$(meta_get "${canonical}" binary-sha256)"
  c_loc="$(meta_get "${canonical}" locale)"
  if [[ "${a_exit}" != "${c_exit}" ]]; then
    log "  Exit status differs: actual=${a_exit} canonical=${c_exit}"
    ok=1
  fi
  if [[ "${a_tmo}" != "${c_tmo}" ]]; then
    log "  Timeout status differs: actual=${a_tmo} canonical=${c_tmo}"
    ok=1
  fi
  if [[ "${a_sha}" != "${c_sha}" ]]; then
    log "  Binary SHA differs: actual=${a_sha} canonical=${c_sha}"
    ok=1
  fi
  if [[ "${a_loc}" != "LC_ALL=C" || "${c_loc}" != "LC_ALL=C" ]]; then
    log "  Locale differs: actual=${a_loc} canonical=${c_loc} (expected LC_ALL=C)"
    ok=1
  fi
  return ${ok}
}

# Writes artifacts.sha256 for the canonical runtime artifact set
# (stdout.raw.txt, stderr.raw.txt, generated/*). Recursive, null-terminated
# paths; relative names and contents feed into the checksum.
write_artifact_hashes() {
  local outdir="$1"
  ( cd "${outdir}" && \
    { \
      [[ -f stdout.raw.txt ]] && printf 'stdout.raw.txt\0'
      [[ -f stderr.raw.txt ]] && printf 'stderr.raw.txt\0'
      if [[ -d generated ]]; then
        ( cd generated && find . -type f -printf 'generated/%P\0' )
      fi
    } | sort -z | xargs -0 -r sha256sum ) > "${outdir}/artifacts.sha256"
}

# ---------------------------------------------------------------------------
# Commands
# ---------------------------------------------------------------------------
cmd_generate() {
  local casedir="$1"
  [[ -d "${casedir}" ]] || { log "Error: case directory missing: ${casedir}"; return 1; }
  local timeout; timeout="$(meta_get "${casedir}/metadata.txt" TIMEOUT)"
  [[ -n "${timeout}" ]] || timeout="${DEFAULT_TIMEOUT}"

  local tmp
  _make_tmp || return 2
  tmp="${LAST_TMPDIR}"
  run_case_once "${casedir}" "${tmp}/work" "${tmp}/out" "${timeout}"
  local rc=$?
  if [[ ${rc} -ne 0 ]]; then return ${rc}; fi

  # Replaces only artifacts in the unique resolved case directory.
  cp -a "${tmp}/out/stdout.raw.txt" "${casedir}/stdout.raw.txt"
  cp -a "${tmp}/out/stderr.raw.txt"  "${casedir}/stderr.raw.txt"
  cp -a "${tmp}/out/run-info.txt"    "${casedir}/run-info.txt"
  rm -rf -- "${casedir}/generated"; mkdir -p -- "${casedir}/generated"
  if [[ -d "${tmp}/out/generated" ]]; then
    cp -a "${tmp}/out/generated/." "${casedir}/generated/"
  fi
  write_artifact_hashes "${casedir}"
  log "OK generate: ${casedir#*cases/}"
  return 0
}

cmd_verify() {
  local casedir="$1"
  [[ -d "${casedir}" ]] || { log "Error: case directory missing: ${casedir}"; return 1; }
  [[ -f "${casedir}/stdout.raw.txt" ]] || { log "Error: no reference in ${casedir}"; return 1; }
  [[ -f "${casedir}/run-info.txt" ]] || { log "Error: run-info.txt missing in ${casedir}"; return 1; }
  local timeout; timeout="$(meta_get "${casedir}/metadata.txt" TIMEOUT)"
  [[ -n "${timeout}" ]] || timeout="${DEFAULT_TIMEOUT}"

  local tmp
  _make_tmp || return 2
  tmp="${LAST_TMPDIR}"
  run_case_once "${casedir}" "${tmp}/work" "${tmp}/out" "${timeout}"
  local rc=$?
  if [[ ${rc} -ne 0 ]]; then
    log "ERROR verify (infrastructure error, rc=${rc}): ${casedir#*cases/}"
    return ${rc}
  fi

  local ok=0
  compare_artifacts "${tmp}/out" "${casedir}" || ok=1
  check_run_info "${tmp}/out/run-info.txt" "${casedir}/run-info.txt" || ok=1
  # binary SHA must match the current binary (the binary itself may not have changed).
  if [[ "$(meta_get "${tmp}/out/run-info.txt" binary-sha256)" != "${BIN_SHA}" ]]; then
    log "  binary SHA differs from the current binary"
    ok=1
  fi
  if [[ ${ok} -eq 0 ]]; then
    log "OK verify: ${casedir#*cases/}"
  else
    log "ERROR verify (deviation): ${casedir#*cases/}"
  fi
  return ${ok}
}

cmd_determinism() {
  local casedir="$1"
  [[ -d "${casedir}" ]] || { log "Error: case directory missing: ${casedir}"; return 1; }
  local timeout; timeout="$(meta_get "${casedir}/metadata.txt" TIMEOUT)"
  [[ -n "${timeout}" ]] || timeout="${DEFAULT_TIMEOUT}"

  local t1 t2
  _make_tmp || return 2
  t1="${LAST_TMPDIR}"
  _make_tmp || return 2
  t2="${LAST_TMPDIR}"
  run_case_once "${casedir}" "${t1}/work" "${t1}/out" "${timeout}" || return 1
  run_case_once "${casedir}" "${t2}/work" "${t2}/out" "${timeout}" || return 1

  local ok=0
  compare_artifacts "${t1}/out" "${t2}/out" || ok=1
  # Both runs must have the same exit status and the same timeout status.
  local e1 e2 o1 o2
  e1="$(meta_get "${t1}/out/run-info.txt" exit-status)"
  e2="$(meta_get "${t2}/out/run-info.txt" exit-status)"
  o1="$(meta_get "${t1}/out/run-info.txt" timed-out)"
  o2="$(meta_get "${t2}/out/run-info.txt" timed-out)"
  if [[ "${e1}" != "${e2}" ]]; then
    log "  Exit status differs: run1=${e1} run2=${e2}"
    ok=1
  fi
  if [[ "${o1}" != "${o2}" ]]; then
    log "  Timeout status differs: run1=${o1} run2=${o2}"
    ok=1
  fi
  if [[ ${ok} -eq 0 ]]; then
    log "OK determinism: ${casedir#*cases/}"
  else
    log "ERROR determinism (not deterministic): ${casedir#*cases/}"
  fi
  return ${ok}
}

list_cases() {
  find "${CASES_ROOT}" -name 'commands.in' 2>/dev/null \
    | sed "s|${CASES_ROOT}/||; s|/commands.in$||" | sort
}

cmd_all() {
  local mode="$1" fn="$2"
  local total=0 fail=0 c rel
  while IFS= read -r rel; do
    [[ -z "${rel}" ]] && continue
    c="${CASES_ROOT}/${rel}"
    total=$((total+1))
    "${fn}" "${c}" || fail=$((fail+1))
  done < <(list_cases)
  log "Overall: ${total}, failures: ${fail}"
  [[ ${fail} -eq 0 ]]
}

cmd_manifest() {
  {
    printf 'case-id\tcategory\tpurpose\tstep\tstatus\trole\texit-status\ttimed-out\tdeterminism\ttimeout-sec\tbinary-sha256\tlocale\tcommands-sha256\tstdout-sha256\tstderr-sha256\tgenerated-sha256\tparameters\tlimitations\n'
    local rel
    while IFS= read -r rel; do
      [[ -z "${rel}" ]] && continue
      local c="${CASES_ROOT}/${rel}"
      local category purpose step status role exitst timedout timeout params lim
      category="$(meta_get "${c}/metadata.txt" CATEGORY)"
      purpose="$(meta_get "${c}/metadata.txt" PURPOSE)"
      step="$(meta_get "${c}/metadata.txt" STEP)"
      status="$(meta_get "${c}/metadata.txt" STATUS)"
      role="$(meta_get "${c}/metadata.txt" ROLE)"
      [[ -n "${role}" ]] || role="-"
      params="$(meta_get "${c}/metadata.txt" PARAMETERS)"
      lim="$(meta_get "${c}/metadata.txt" LIMITATIONS)"
      [[ -n "${status}" ]] || status="needs-review"
      timeout="$(meta_get "${c}/metadata.txt" TIMEOUT)"
      [[ -n "${timeout}" ]] || timeout="${DEFAULT_TIMEOUT}"
      exitst="-"; timedout="-"; determinism="-"
      if [[ -f "${c}/run-info.txt" ]]; then
        exitst="$(meta_get "${c}/run-info.txt" exit-status)"
        timedout="$(meta_get "${c}/run-info.txt" timed-out)"
        determinism="$(meta_get "${c}/metadata.txt" DETERMINISM)"
      fi
      local cmdsha stdsha errsha gensha
      cmdsha="$(sha256sum "${c}/commands.in" | awk '{print $1}')"
      stdsha="-"; errsha="-"
      [[ -f "${c}/stdout.raw.txt" ]] && stdsha="$(sha256sum "${c}/stdout.raw.txt" | awk '{print $1}')"
      [[ -f "${c}/stderr.raw.txt" ]]  && errsha="$(sha256sum "${c}/stderr.raw.txt"  | awk '{print $1}')"
      gensha="-"
      if [[ -d "${c}/generated" ]]; then
        gensha="$( (cd "${c}/generated" && find . -type f -print0 | sort -z | xargs -0 -r sha256sum) | sha256sum | awk '{print $1}' )"
      fi
      printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
        "${rel}" "${category}" "${purpose}" "${step}" "${status}" "${role}" \
        "${exitst}" "${timedout}" "${determinism}" "${timeout}" "${BIN_SHA}" "LC_ALL=C" \
        "${cmdsha}" "${stdsha}" "${errsha}" "${gensha}" "${params}" "${lim}"
    done < <(list_cases)
  } > "${MANIFEST}"
  log "manifest written: ${MANIFEST}"
}

main() {
  local cmd="${1:-}"
  case "${cmd}" in
    generate|verify|determinism)
      local casedir
      casedir="$(resolve_case "${2:-}")" || exit 1
      case "${cmd}" in
        generate)    cmd_generate "${casedir}" ;;
        verify)      cmd_verify "${casedir}" ;;
        determinism) cmd_determinism "${casedir}" ;;
      esac
      ;;
    all-generate)    cmd_all all-generate cmd_generate ;;
    all-verify)      cmd_all all-verify cmd_verify ;;
    all-determinism) cmd_all all-determinism cmd_determinism ;;
    manifest)        cmd_manifest ;;
    list)            list_cases ;;
    *)
      echo "Use: $0 {generate|verify|determinism|all-generate|all-verify|all-determinism|manifest|list} [case]" >&2
      exit 2
      ;;
  esac
}

main "$@"
