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
# check-osmap-selection.sh - permanent negative check of the OSMAP selection
#
# Binding contract:
#
# fortran-src/osmap.f active XFOIL 6.99 runtime path
# fortran-src/osmap.dat active REAL*4 database
# fortran-src/osmap_DP.f inactive original artifact; never
# compiled or linked automatically
# fortran-src/osmapDP.dat inactive DP database; never used as the active
# OSMAP database
#
# The check examines only the active build selection. The bare
# existence of fortran-src/osmap_DP.f or fortran-src/osmapDP.dat is
# explicitly allowed and is not flagged.
#
# Call:
# tools/check-osmap-selection.sh <build directory> [further object files...]
#
# The check examines the object files present in the build directory
# (plus explicitly passed object files) and fails if:
#
# 1. an object file from the inactive alternative osmap_DP.f
#    appears (file name osmap_DP.o), or
# 2. several object files define the same OSMAP runtime symbol
#    (osmap_/osmap2_), or the active and inactive OSMAP
#    implementations are linked together.
#
# Exit: 0 = selection in order, 1 = violation of the OSMAP contract.

set -u

if [[ $# -lt 1 ]]; then
  echo "Call: $0 <build directory> [object files...]" >&2
  exit 2
fi

build_dir="$1"
shift

if [[ ! -d "${build_dir}" ]]; then
  echo "ERROR: build directory not found: ${build_dir}" >&2
  exit 2
fi

objects=()
while IFS= read -r obj; do
  objects+=("${obj}")
done < <(find "${build_dir}" -maxdepth 1 -type f -name '*.o' -print | sort)

for extra in "$@"; do
  if [[ -f "${extra}" ]]; then
    objects+=("${extra}")
  fi
done

status=0

# 1. No object of the inactive OSMAP alternative in an active link.
for obj in "${objects[@]}"; do
  base="$(basename "${obj}")"
  if [[ "${base}" == "osmap_DP.o" ]]; then
    echo "ERROR: inactive OSMAP alternative in the active link: ${obj}" >&2
    echo "        osmap_DP.f is an inactive original artifact and must" >&2
    echo "        not be compiled or linked." >&2
    status=1
  fi
done

# 2. At most one object file defines the active OSMAP runtime symbol.
providers=()
for obj in "${objects[@]}"; do
  if nm --defined-only --extern-only "${obj}" 2>/dev/null \
       | grep -qE '^[0-9a-fA-F]+ T (osmap_|osmap2_)$'; then
    providers+=("$(basename "${obj}")")
  fi
done

if [[ ${#providers[@]} -gt 1 ]]; then
  echo "ERROR: several OSMAP definitions in the same active link:" >&2
  for provider in "${providers[@]}"; do
    echo "        ${provider}" >&2
  done
  echo "        active runtime path (osmap.f) and inactive alternative" >&2
  echo "        (osmap_DP.f) or a harness stub must not be linked" >&2
  echo "        together." >&2
  status=1
fi

exit "${status}"
