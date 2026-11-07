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
# gen-fortran-cases-audit.sh - extension cases of the audit.
#
# Adds new reference cases additionally without overwriting existing
# raw references. Covers:
# - S3: panel coordinates (PANE/PPAR/PCOP + PSAV)
# - S4: FCPP009 boundaries (clamp 364 = IQX-6, below/on/above)
# - S2: corrected GDES cases with EXEC + INTX/INTERX
# - S5/S6: polar and dump artifacts (inviscid/viscous/state/non-converging)
#
# No git commands, no C++/Fortran source or golden changes.

set -eu

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
CASES_ROOT="${REPO_ROOT}/tests/fortran-reference/cases"
CLARK_Y="${REPO_ROOT}/sample/CLARK_Y.dat"

new_case() {
  local category="$1" name="$2" status="$3" purpose="$4" step="$5" params="$6" lim="$7"
  local dir="${CASES_ROOT}/${category}/${name}"
  mkdir -p "${dir}"
  cat > "${dir}/commands.in"
  {
    printf 'case-id: %s/%s\n' "${category}" "${name}"
    printf 'category: %s\n' "${category}"
    printf 'purpose: %s\n' "${purpose}"
    printf 'step: %s\n' "${step}"
    printf 'status: %s\n' "${status}"
    printf 'determinism: pending\n'
    printf 'timeout: 120\n'
    printf 'locale: LC_ALL=C\n'
    printf 'parameters: %s\n' "${params}"
    printf 'limitations: %s\n' "${lim}"
  } > "${dir}/metadata.txt"
  printf 'created: %s/%s\n' "${category}" "${name}"
}

copy_input() {
  local category="$1" name="$2"
  mkdir -p "${CASES_ROOT}/${category}/${name}/input"
  cp -a "${CLARK_Y}" "${CASES_ROOT}/${category}/${name}/input/CLARK_Y.dat"
}

# ===========================================================================
# S3/S4: panel coordinates + FCPP009 boundaries
# ===========================================================================
new_case panel-coord pane-standard-psav canonical-exact \
  "Panelkoordinaten nach Standard-PANE: PANE + PSAV schreibt die aktuelle (panelierte) Kontur X(1..N), 160 Punkte" \
  17r4 "NACA 2412, PANE (NPAN=160), PSAV" "PSAV schreibt X(1..N) (aktuelle/Panel-Kontur), nicht den Buffer" <<'EOF'
PLOP
g

NACA 2412
PANE
PSAV pane.dat
QUIT
EOF

new_case panel-coord ppar-n200-psav canonical-exact \
  "Panelkoordinaten nach nicht-standardmaessiger PPAR-Konfiguration (N 200) + PSAV, 200 Punkte" \
  17r4 "NACA 2412, PPAR N 200 (angewendet), PSAV" "PSAV schreibt X(1..N) (aktuelle/Panel-Kontur)" <<'EOF'
PLOP
g

NACA 2412
PPAR
N 200


PSAV ppar200.dat
QUIT
EOF

new_case panel-coord pcop-psav canonical-exact \
  "PCOP + PSAV: Buffer-Kontur als aktuelle Kontur (N=NB=245), PSAV schreibt X(1..N)=245 Punkte" \
  17r4 "NACA 2412 (Buffer 245), PCOP (ABCOPY), PSAV" "PCOP setzt N=NB; PSAV schreibt die Buffer-abgeleitete Kontur" <<'EOF'
PLOP
g

NACA 2412
PCOP
PSAV pcop.dat
QUIT
EOF

new_case panel-coord ppar-n364-at-clamp canonical-exact \
  "FCPP009-Grenze AUF der Clamp-Grenze: PPAR N 364 (= IQX-6) wird ohne Reduktion akzeptiert, PSAV 364 Punkte" \
  17r4 "NACA 2412, PPAR N 364 (IQX-6), PSAV" "364 ist nicht > IQX-6, daher keine Reduktion" <<'EOF'
PLOP
g

NACA 2412
PPAR
N 364


PSAV p364.dat
QUIT
EOF

new_case panel-coord ppar-n365-above-clamp canonical-exact \
  "FCPP009-Grenze UEBER der Clamp-Grenze: PPAR N 365 (> IQX-6) wird auf 364 reduziert (Meldung), PSAV 364 Punkte" \
  17r4 "NACA 2412, PPAR N 365 (IQX-6+1), PSAV" "365 > IQX-6=364, daher Reduktion auf 364 mit Meldung" <<'EOF'
PLOP
g

NACA 2412
PPAR
N 365


PSAV p365.dat
QUIT
EOF

# ===========================================================================
# S2: corrected GDES cases (with eXec) + INTX/INTERX
# ===========================================================================
new_case geometry-edit gdes-tset-exec canonical-exact \
  "Korrigiert: GDES TSET 0.15/0.02 + eXec + PSAV; eXec uebertraegt den geaenderten Buffer (245) in die aktuelle Kontur" \
  17r4 "NACA 2412, GDES TSET 0.15 0.02, eXec, PSAV" "ohne eXec wuerde PSAV die ungeaenderte aktuelle Kontur schreiben (siehe gdes-tset)" <<'EOF'
PLOP
g

NACA 2412
GDES
TSET 0.15 0.02
eXec

PSAV tset_exec.dat
QUIT
EOF

new_case geometry-edit gdes-flap-exec canonical-exact \
  "Korrigiert: GDES FLAP 0.9/0.0/10 + eXec + PSAV; eXec uebertraegt den geflappten Buffer (245) in die aktuelle Kontur" \
  17r4 "NACA 2412, GDES Flap 0.9 0.0 10, eXec, PSAV" "ohne eXec wuerde PSAV die ungeaenderte aktuelle Kontur schreiben (siehe gdes-flap)" <<'EOF'
PLOP
g

NACA 2412
GDES
Flap 0.9 0.0 10
eXec

PSAV flap_exec.dat
QUIT
EOF

new_case geometry-edit gdes-rotate-exec canonical-exact \
  "Korrigiert: GDES ADEG 5 (ROTATE) + eXec + PSAV; eXec uebertraegt den rotierten Buffer (245) in die aktuelle Kontur" \
  17r4 "NACA 2412, GDES ADEG 5, eXec, PSAV" "ADEG ruft ROTATE(XB,YB,NB,ARAD) auf (xgdes.f:234); ohne eXec bleibt die aktuelle Kontur unveraendert" <<'EOF'
PLOP
g

NACA 2412
GDES
ADEG 5
eXec

PSAV rotate_exec.dat
QUIT
EOF

new_case geometry-edit gdes-dero-exec canonical-exact \
  "Korrigiert: GDES ADEG 5 + DERO (ROTATE) + eXec + PSAV; De-Rotation des Buffers, eXec uebertraegt in die aktuelle Kontur" \
  17r4 "NACA 2412, GDES ADEG 5, DERO, eXec, PSAV" "DERO ruft ROTATE(XB,YB,NB,ARAD) auf (xgdes.f:397); ohne eXec bleibt die aktuelle Kontur unveraendert" <<'EOF'
PLOP
g

NACA 2412
GDES
ADEG 5
DERO
eXec

PSAV dero_exec.dat
QUIT
EOF

new_case geometry-edit intx-interp canonical-exact \
  "INTX (CLI) + INTERX (Routine): Interpolation der aktuellen Kontur mit sich selbst (Frac 0.5), PCOP + PSAV" \
  17r4 "NACA 0012, INTX C C 0.5, PCOP, PSAV" "INTX (xfoil.f:229) ruft INTERX (xgeom.f:1226) auf; Ergebnis liegt im Buffer, PCOP uebertraegt" <<'EOF'
PLOP
g

NACA 0012
INTX
C
C
0.5
intxtest
PCOP
PSAV intx.dat
QUIT
EOF

# ===========================================================================
# S5/S6: polar and dump artifacts
# ===========================================================================
new_case polar polar-inv-0012 canonical-exact \
  "Inviskide Polar NACA 0012, ASEQ -4..8 Schritt 2, mit Polar-Datei (PACC)" \
  17r4 "inviscid, PACC, ASEQ -4 8 2, Polar-Datei polar-inv-0012.pol" "none" <<'EOF'
PLOP
g

NACA 0012
OPER
PACC
polar-inv-0012.pol

ASEQ -4 8 2

QUIT
EOF

new_case polar polar-inv-clark-y canonical-exact \
  "Inviskide Polar Clark Y, ASEQ -4..8 Schritt 2, mit Polar-Datei (PACC)" \
  17r4 "inviscid, PACC, ASEQ -4 8 2, Polar-Datei polar-inv-clark-y.pol" "none" <<'EOF'
PLOP
g

LOAD CLARK_Y.dat
OPER
PACC
polar-inv-clark-y.pol

ASEQ -4 8 2

QUIT
EOF
copy_input polar polar-inv-clark-y

new_case polar polar-visc-0012 canonical-exact \
  "Viskose Polar NACA 0012 Re=1e6 ITER 100, ASEQ 0..8 Schritt 2, mit Polar- und Dump-Datei (PACC)" \
  17r4 "viscous, Re=1e6, ITER 100, PACC, ASEQ 0 8 2, Polar polar-visc-0012.pol + Dump" \
  "Dump-Datei (binär) enthaelt die Druckverteilungen je Punkt" <<'EOF'
PLOP
g

NACA 0012
OPER
VISC
1000000
ITER 100
PACC
polar-visc-0012.pol
polar-visc-0012.dump
ASEQ 0 8 2

QUIT
EOF

new_case polar polar-visc-2412-re3e6 canonical-exact \
  "Viskose Polar NACA 2412 Re=3e6 ITER 100, ASEQ 0..8 Schritt 2, mit Polar- und Dump-Datei (PACC)" \
  17r4 "viscous, Re=3e6, ITER 100, PACC, ASEQ 0 8 2, Polar polar-visc-2412-re3e6.pol + Dump" \
  "Dump-Datei (binär) enthaelt die Druckverteilungen je Punkt" <<'EOF'
PLOP
g

NACA 2412
OPER
VISC
3000000
ITER 100
PACC
polar-visc-2412-re3e6.pol
polar-visc-2412-re3e6.dump
ASEQ 0 8 2

QUIT
EOF

new_case polar polar-visc-clark-y canonical-exact \
  "Viskose Polar Clark Y Re=1e6 ITER 100, ASEQ 0..6 Schritt 2, mit Polar- und Dump-Datei (PACC)" \
  17r4 "viscous, Re=1e6, ITER 100, PACC, ASEQ 0 6 2, Polar polar-visc-clark-y.pol + Dump" \
  "Dump-Datei (binär) enthaelt die Druckverteilungen je Punkt" <<'EOF'
PLOP
g

LOAD CLARK_Y.dat
OPER
VISC
1000000
ITER 100
PACC
polar-visc-clark-y.pol
polar-visc-clark-y.dump
ASEQ 0 6 2

QUIT
EOF
copy_input polar polar-visc-clark-y

new_case polar polar-state-ascending canonical-exact \
  "Zustand: aufsteigende inviskide Polar NACA 2412, ASEQ 0..8 Schritt 2 (PACC)" \
  17r4 "inviscid, PACC, ASEQ 0 8 2" "none" <<'EOF'
PLOP
g

NACA 2412
OPER
PACC
polar-state-ascending.pol

ASEQ 0 8 2

QUIT
EOF

new_case polar polar-state-descending canonical-exact \
  "Zustand: absteigende inviskide Polar NACA 2412, ASEQ 8..0 Schritt -2 (PACC)" \
  17r4 "inviscid, PACC, ASEQ 8 0 -2" "none" <<'EOF'
PLOP
g

NACA 2412
OPER
PACC
polar-state-descending.pol

ASEQ 8 0 -2

QUIT
EOF

new_case polar polar-state-cl-driven canonical-exact \
  "Zustand: CL-gesteuerte viskose Polar NACA 2412 Re=1e6, CSEQ 0.2..1.2 Schritt 0.2 (PACC)" \
  17r4 "viscous, Re=1e6, ITER 100, PACC, CSEQ 0.2 1.2 0.2" "none" <<'EOF'
PLOP
g

NACA 2412
OPER
VISC
1000000
ITER 100
PACC
polar-state-cl-driven.pol

CSEQ 0.2 1.2 0.2

QUIT
EOF

new_case polar polar-nonconverging canonical-raw-only \
  "Zustand: kontrolliert nicht konvergierende viskose Polar NACA 2412 Re=1e6, ALFA 25 (PACC); Polar-Datei wird mit 0 Punkten erzeugt" \
  17r4 "viscous, Re=1e6, ITER 100, PACC, ALFA 25" \
  "bewusst nicht konvergierend; nicht konvergierte viskose Punkte werden NICHT in die Polar aufgenommen (xoper.f:440 LVCONV-Bedingung); Verhaltensreferenz" <<'EOF'
PLOP
g

NACA 2412
OPER
VISC
1000000
ITER 100
PACC
polar-nonconverging.pol

ALFA 25

QUIT
EOF

echo "All audit extension cases were generated under: ${CASES_ROOT}"
