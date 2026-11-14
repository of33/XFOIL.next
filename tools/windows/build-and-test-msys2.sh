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
# Reproducible MSYS2 UCRT64 evidence run for XFOIL.next (Windows).
#
# Entry point: tools/windows/build-and-test-msys2.bat (Windows-side
# argument validation and MSYS2 invocation). This script performs the
# build/test/evidence sequence and writes timestamped logs into a fresh
# run subdirectory of the given log root, which must lie OUTSIDE the
# repository. It performs no network access, no package installation,
# no git write operation and no repository cleanup. It never rebuilds
# between the core/regression repetitions (make finds the build
# up to date; no compiler invocations follow).
#
# Expected suite totals are verified as final result lines, not only
# via exit codes:
#   core 414, unit 1669, integration 246, regression 5, batch 47,
#   full suite 1970, PACC diagnostic 1.
#
# Usage (normally invoked by the .bat entry point):
#   build-and-test-msys2.sh <log-dir-outside-repo> <repo-dir>

set -u

usage() {
  echo "usage: build-and-test-msys2.sh <log-dir> <repo-dir>" >&2
  exit 2
}

[ "$#" -ge 2 ] || usage
LOGROOT_ARG=$1
REPO_ARG=$2

# Authoritative path normalization. The .bat entry passes raw paths:
# they may be Windows-style, relative, contain ".." segments or a
# trailing backslash. cygpath maps Windows paths; realpath -m
# absolutizes and resolves ".." without requiring the path to exist.
normalize() {
  local p=$1
  if command -v cygpath >/dev/null 2>&1; then
    p=$(cygpath -u "$p" 2>/dev/null) || p=$1
  fi
  if command -v realpath >/dev/null 2>&1; then
    p=$(realpath -m -- "$p" 2>/dev/null) || p=$1
  fi
  printf '%s' "$p"
}

LOGROOT=$(normalize "$LOGROOT_ARG")
REPO=$(normalize "$REPO_ARG")

case $REPO in
  /*) ;;
  *) echo "error: repository path is not absolute: $REPO" >&2; exit 2 ;;
esac
[ -f "$REPO/Makefile" ] || { echo "error: not a repository: $REPO" >&2; exit 2; }

lower() { printf '%s' "$1" | tr '[:upper:]' '[:lower:]'; }
LOGROOT_LC=$(lower "$LOGROOT")
REPO_LC=$(lower "$REPO")
case $LOGROOT_LC in
  "$REPO_LC"|"$REPO_LC"/*)
    echo "error: log directory must be outside the repository: $LOGROOT" >&2
    exit 3 ;;
esac

RUNID=run-$(date +%Y%m%d-%H%M%S)-$$
LOGDIR=$LOGROOT/$RUNID
mkdir -p "$LOGROOT" || exit 1
[ -e "$LOGDIR" ] && { echo "error: run directory exists: $LOGDIR" >&2; exit 1; }
mkdir "$LOGDIR" || exit 1

cd "$REPO" || exit 1

CORE_TOTAL=414
UNIT_TOTAL=1669
INTEGRATION_TOTAL=246
REGRESSION_TOTAL=5
BATCH_TOTAL=47
FULL_TOTAL=1970
PACC_TOTAL=1

SUMMARY="$LOGDIR/50-summary.txt"
FAILED_STEPS=""

mark() {  # stepname status
  printf '%-22s %s\n' "$1:" "$2" >> "$SUMMARY"
  if [ "$2" != "PASS" ]; then
    FAILED_STEPS="$FAILED_STEPS $1"
  fi
}

# run_logged <logfile> -- <command...>
# Runs the command with combined output into the log, then appends the
# exit code. Never swallows failures.
run_logged() {
  local logfile=$1
  shift 2
  {
    echo "\$ $*"
    echo "-----------------------------------------------------------------"
  } > "$logfile"
  local rc=0
  "$@" >> "$logfile" 2>&1 || rc=$?
  echo "-----------------------------------------------------------------" >> "$logfile"
  echo "exit code: $rc" >> "$logfile"
  return "$rc"
}

# step <stepname> <logfile> <expected-final-line> <command...>
# PASS only if exit code is 0 and the expected completion line appears.
step() {
  local stepname=$1 logfile=$2 expected=$3
  shift 3
  if run_logged "$logfile" -- "$@" \
     && tail -n 6 "$logfile" | grep -qF "$expected"; then
    mark "$stepname" PASS
    return 0
  fi
  echo "expected final line: $expected" >> "$logfile"
  mark "$stepname" FAIL
  return 1
}

skip_many() {
  local s
  for s in "$@"; do mark "$s" SKIPPED; done
}

# exe <path-without-suffix>: appends .exe on Windows builds.
exe() {
  if [ -f "$1.exe" ]; then
    printf '%s.exe' "$1"
  else
    printf '%s' "$1"
  fi
}

echo "== XFOIL.next MSYS2 evidence run: $RUNID"
echo "   repository: $REPO"
echo "   log directory: $LOGDIR"

# ---------------------------------------------------------------------------
# 00 - environment metadata (read-only)
# ---------------------------------------------------------------------------
ENVLOG="$LOGDIR/00-environment.txt"
{
  date -Is
  echo "--- windows version (cmd ver) ---"
  cmd //c ver
  echo "--- msys identity ---"
  echo "MSYSTEM=${MSYSTEM:-}"
  echo "MSYSTEM_CARCH=${MSYSTEM_CARCH:-}"
  echo "MSYSTEM_PREFIX=${MSYSTEM_PREFIX:-}"
  echo "MINGW_PREFIX=${MINGW_PREFIX:-}"
  uname -a
  echo "--- toolchain ---"
  g++ --version
  gcc --version
  make --version
  echo "--- shell ---"
  echo "bash: $BASH_VERSION"
  echo "--- repository ---"
  echo "repo path: $REPO"
  if command -v git >/dev/null 2>&1 \
     && git -C "$REPO" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    echo "--- git (read-only) ---"
    git -C "$REPO" rev-parse HEAD
    if [ -n "$(git -C "$REPO" status --porcelain)" ]; then
      echo "WORKTREE STATE: contains uncommitted modifications; the audited tree is an uncommitted working tree on top of the recorded commit."
    else
      echo "WORKTREE STATE: clean"
    fi
  else
    echo "--- git (read-only) ---"
    echo "WORKTREE STATE: not a git repository (exported audit tree); the commit basis is documented in EXPORT-INFO.txt stored alongside the exported tree."
  fi
} > "$ENVLOG" 2>&1
if [ -s "$ENVLOG" ]; then
  mark environment PASS
else
  mark environment FAIL
fi

# ---------------------------------------------------------------------------
# 01/02 - builds via the canonical make paths (-Werror is repository
# default for both debug and release profiles)
# ---------------------------------------------------------------------------
BUILD_RC=PASS
if step "debug build" "$LOGDIR/01-debug-build.txt" "DEBUG-BUILD-DONE" \
     bash -c 'make -B debug && echo DEBUG-BUILD-DONE'; then
  :
else
  BUILD_RC=FAIL
fi

if [ "$BUILD_RC" = PASS ]; then
  # The release profile plus the release build of the test program
  # (make release alone does not build xfoil_tests).
  if step "release build" "$LOGDIR/02-release-build.txt" "RELEASE-BUILD-DONE" \
       bash -c 'make -B release build/release/xfoil_tests && echo RELEASE-BUILD-DONE'; then
    :
  else
    BUILD_RC=FAIL
  fi
else
  mark "release build" SKIPPED
fi

# ---------------------------------------------------------------------------
# Suite runs from the same untouched debug build. Repetitions re-run
# make targets whose build steps are up to date: no recompilation and
# no relinking happen between repetitions.
# ---------------------------------------------------------------------------
if [ "$BUILD_RC" = PASS ]; then
  step "core r1" "$LOGDIR/10-debug-core-r1.txt" \
    "0 failure(s) in $CORE_TOTAL test(s)" make test-core || true
  step "core r2" "$LOGDIR/11-debug-core-r2.txt" \
    "0 failure(s) in $CORE_TOTAL test(s)" make test-core || true
  step "core r3" "$LOGDIR/12-debug-core-r3.txt" \
    "0 failure(s) in $CORE_TOTAL test(s)" make test-core || true

  step unit "$LOGDIR/20-debug-unit.txt" \
    "0 failure(s) in $UNIT_TOTAL test(s)" make test-unit || true
  step integration "$LOGDIR/21-debug-integration.txt" \
    "0 failure(s) in $INTEGRATION_TOTAL test(s)" make test-integration || true
  step "regression r1" "$LOGDIR/22-debug-regression-r1.txt" \
    "0 failure(s) in $REGRESSION_TOTAL test(s)" make test-regression || true
  step "regression r2" "$LOGDIR/23-debug-regression-r2.txt" \
    "0 failure(s) in $REGRESSION_TOTAL test(s)" make test-regression || true
  step "regression r3" "$LOGDIR/24-debug-regression-r3.txt" \
    "0 failure(s) in $REGRESSION_TOTAL test(s)" make test-regression || true
  step batch "$LOGDIR/25-debug-batch.txt" \
    "0 failure(s) in $BATCH_TOTAL test(s)" make test-batch || true

  # Release full suite against the release product binary.
  CLI_RELEASE=$(exe build/release/xfoil.next)
  RUNNER_RELEASE=$(exe build/release/xfoil_tests)
  step "release full suite" "$LOGDIR/30-release-full.txt" \
    "0 failure(s) in $FULL_TOTAL test(s)" \
    env XFOIL_CLI_PATH="$CLI_RELEASE" "./$RUNNER_RELEASE" || true

  # Focused PACC dump diagnostic: XFOIL_DUMP_DIAG=1 only lifts report
  # sample caps; the test verdict must stay green.
  CLI_DEBUG=$(exe build/debug/xfoil.next)
  RUNNER_DEBUG=$(exe build/debug/xfoil_tests)
  step "PACC diagnostic" "$LOGDIR/31-pacc-diagnostic.txt" \
    "0 failure(s) in $PACC_TOTAL test(s)" \
    env XFOIL_DUMP_DIAG=1 XFOIL_CLI_PATH="$CLI_DEBUG" \
      "./$RUNNER_DEBUG" regression.PaccDumpByteParityMatchesFortranReference || true

  # Binary contract (debug and release). On Windows the product binary
  # is xfoil.next.exe; xfoil.exe must never exist.
  BINLOG="$LOGDIR/40-binary-contract.txt"
  BIN_RC=PASS
  {
    echo "binary contract: xfoil.next(.exe) present, xfoil.exe absent"
    for d in debug release; do
      bin=""
      if [ -f "build/$d/xfoil.next.exe" ]; then
        bin="build/$d/xfoil.next.exe"
      elif [ -f "build/$d/xfoil.next" ]; then
        bin="build/$d/xfoil.next"
      fi
      if [ -n "$bin" ]; then
        echo "present: $bin"
        ver_out=$("$bin" --version 2>&1)
        ver_rc=$?
        echo "version exit: $ver_rc output: [$ver_out]"
        if [ "$ver_rc" -ne 0 ] \
           || ! printf '%s' "$ver_out" | grep -q '^XFOIL\.next 1\.0\.1$'; then
          echo "VIOLATION: unexpected --version result for $bin"
          BIN_RC=FAIL
        fi
      else
        echo "VIOLATION: product binary missing in build/$d"
        BIN_RC=FAIL
      fi
      if [ -f "build/$d/xfoil.exe" ]; then
        echo "VIOLATION: forbidden old-name binary present: build/$d/xfoil.exe"
        BIN_RC=FAIL
      else
        echo "absent (correct): build/$d/xfoil.exe"
      fi
    done
  } > "$BINLOG" 2>&1
  if [ "$BIN_RC" = PASS ]; then
    mark "binary contract" PASS
  else
    mark "binary contract" FAIL
  fi
else
  skip_many "core r1" "core r2" "core r3" unit integration \
    "regression r1" "regression r2" "regression r3" batch \
    "release full suite" "PACC diagnostic" "binary contract"
fi

# ---------------------------------------------------------------------------
# Summary and checksums
# ---------------------------------------------------------------------------
if [ -z "${FAILED_STEPS// /}" ]; then
  OVERALL=PASS
  RC=0
else
  OVERALL=FAIL
  RC=1
  echo "failed or skipped steps:$FAILED_STEPS" >> "$SUMMARY"
fi
{
  printf '%-22s %s\n' "Overall:" "$OVERALL"
  echo "run: $RUNID"
  echo "repository: $REPO"
} >> "$SUMMARY"

(
  cd "$LOGDIR" || exit 1
  sha256sum ./*.txt > SHA256SUMS.txt
) || RC=1

echo "== Overall: $OVERALL (logs: $LOGDIR)"
exit "$RC"
