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
# gen-fortran-cases.sh - produces the scaffolding for all Fortran
# reference cases.
#
# Creates tests/fortran-reference/cases/<category>/<case-name>/ with the
# files commands.in, input/ (if needed) and metadata.txt.
#
# This file produces no reference artifacts; execution runs via
# tools/fortran-ref.sh (generate). No git commands, no C++/Fortran
# source changes.

set -eu

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
CASES_ROOT="${REPO_ROOT}/tests/fortran-reference/cases"
CLARK_Y="${REPO_ROOT}/sample/CLARK_Y.dat"

# Default preamble: disable graphics output (3 lines: PLOP / g / <empty>).
read -r -d '' PREAMBLE <<'EOF' || true
PLOP
g

EOF

# new_case <category> <case-name> <status> <purpose> <step> <parameters> <limitations>
# The commands.in be per heredoc after $3 (stdin) handed over.
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
  printf 'created: %s\n' "${category}/${name}"
}

# copy_input <category> <case-name>
copy_input() {
  local category="$1" name="$2"
  mkdir -p "${CASES_ROOT}/${category}/${name}/input"
  cp -a "${CLARK_Y}" "${CASES_ROOT}/${category}/${name}/input/CLARK_Y.dat"
}

# ---------------------------------------------------------------------------
# Category: geometry
# ---------------------------------------------------------------------------
new_case geometry naca-0012 canonical-exact \
  "NACA 0012 Geometrie + Plain-Coordinate-Save; symmetrisch, sichtbarer Rundungsnull (Max camber -0.000000)" \
  17r4 "none" "none" <<'EOF'
PLOP
g

NACA 0012
PSAV naca0012.dat
QUIT
EOF

new_case geometry naca-2412 canonical-exact \
  "NACA 2412 Geometrie + Plain-Coordinate-Save; kanonische Punktzahl 245, LE/TE-Ausgabe" \
  17r4 "none" "none" <<'EOF'
PLOP
g

NACA 2412
PSAV naca2412.dat
QUIT
EOF

new_case geometry naca-23012 canonical-exact \
  "NACA-Fuenfstellenprofil 23012 Geometrie + Plain-Coordinate-Save; ITYPE<=25099-Zweig" \
  17r4 "none" "none" <<'EOF'
PLOP
g

NACA 23012
PSAV naca23012.dat
QUIT
EOF

new_case geometry clark-y-load canonical-exact \
  "Geladenes Clark-Y-Profil: LOAD, Geometrieinfo, Standardpanelierung, Plain-Coordinate-Save" \
  17r4 "none" "none" <<'EOF'
PLOP
g

LOAD CLARK_Y.dat
PSAV clark_y_saved.dat
QUIT
EOF
copy_input geometry clark-y-load

# ---------------------------------------------------------------------------
# Category: paneling
# ---------------------------------------------------------------------------
new_case paneling pane-standard canonical-exact \
  "Kanonische Standardpanelierung (PANE, 160 Knoten) auf NACA 2412" \
  17r4 "none" "none" <<'EOF'
PLOP
g

NACA 2412
PANE
QUIT
EOF

new_case paneling ppar-nonstandard canonical-exact \
  "Nicht-standardmaessige PPAR-Konfiguration (N 200, P 1.5, T 0.2) mit Anwendung und Eckwinkel-Ausgabe" \
  17r4 "none" "none" <<'EOF'
PLOP
g

NACA 2412
PPAR
N 200
P 1.5
T 0.2


QUIT
EOF

new_case paneling ppar-limit canonical-exact \
  "Punktzahl-/Kapazitaetsgrenze: N 2000 > IQX-6 wird begrenzt (dimensionsaehngige Schleifengrenze)" \
  17r4 "none" "none" <<'EOF'
PLOP
g

NACA 2412
PPAR
N 2000


QUIT
EOF

new_case paneling pcop canonical-exact \
  "PCOP: Panel-Knoten direkt aus Buffer-Profilpunkten" \
  17r4 "none" "none" <<'EOF'
PLOP
g

NACA 2412
PCOP
QUIT
EOF

# ---------------------------------------------------------------------------
# Category: geometry-edit (non-graphical)
# ---------------------------------------------------------------------------
new_case geometry-edit gdes-cang canonical-exact \
  "GDES CANG: Listung der Panel-Eckwinkel (deterministische Textausgabe)" \
  17r4 "none" "none" <<'EOF'
PLOP
g

NACA 2412
GDES
CANG

QUIT
EOF

new_case geometry-edit gdes-bend canonical-exact \
  "GDES BEND: strukturelle Biegeparameter des Buffer-Profils (deterministische Textausgabe)" \
  17r4 "none" "none" <<'EOF'
PLOP
g

NACA 2412
GDES
BEND

QUIT
EOF

new_case geometry-edit gdes-tset canonical-exact \
  "GDES TSET: Dicke/Kerbung auf 0.15/0.02 setzen, Plain-Coordinate-Save des Ergebnisses" \
  17r4 "none" "none" <<'EOF'
PLOP
g

NACA 2412
GDES
TSET 0.15 0.02

PSAV tset.dat
QUIT
EOF

new_case geometry-edit gdes-rotate-dero canonical-exact \
  "GDES ADEG 5 (Rotation) + DERO (De-Rotation), Plain-Coordinate-Save des Ergebnisses" \
  17r4 "none" "none" <<'EOF'
PLOP
g

NACA 2412
GDES
ADEG 5
DERO

PSAV rotdero.dat
QUIT
EOF

new_case geometry-edit gdes-flap canonical-exact \
  "GDES FLAP: Scharnier x=0.9 y=0.0, Auslenkung 10 Grad, Plain-Coordinate-Save des Ergebnisses" \
  17r4 "none" "none" <<'EOF'
PLOP
g

NACA 2412
GDES
Flap 0.9 0.0 10

PSAV flap.dat
QUIT
EOF

new_case geometry-edit msave-behavior canonical-raw-only \
  "MSAV ohne vorhandene MSES-Datei: deterministisches 'Old file OPEN error' (Verhaltensreferenz, kein Datei-Write)" \
  17r4 "none" "MSAV erfordert eine vorhandene Multielement-MSES-Datei (STATUS='OLD'); ohne diese ist kein Datei-Write moeglich" <<'EOF'
PLOP
g

NACA 2412
MSAV mses_new.out
QUIT
EOF

# ---------------------------------------------------------------------------
# Category: aero-inviscid
# ---------------------------------------------------------------------------
inviscid_point() {
  local name="$1" profile="$2" alpha="$3" purpose="$4"
  local dir="${CASES_ROOT}/aero-inviscid/${name}"
  mkdir -p "${dir}"
  if [[ "${profile}" == "clark" ]]; then
    printf 'PLOP\ng\n\nLOAD CLARK_Y.dat\nOPER\nALFA %s\n\nQUIT\n' "${alpha}" > "${dir}/commands.in"
    copy_input aero-inviscid "${name}"
  else
    printf 'PLOP\ng\n\nNACA %s\nOPER\nALFA %s\n\nQUIT\n' "${profile}" "${alpha}" > "${dir}/commands.in"
  fi
  {
    printf 'case-id: aero-inviscid/%s\n' "${name}"
    printf 'category: aero-inviscid\n'
    printf 'purpose: %s\n' "${purpose}"
    printf 'step: 17r4\n'
    printf 'status: canonical-exact\n'
    printf 'determinism: pending\n'
    printf 'timeout: 120\n'
    printf 'locale: LC_ALL=C\n'
    printf 'parameters: inviscid, Mach 0, Ncrit 9 (Defaults), alpha=%s\n' "${alpha}"
    printf 'limitations: none\n'
  } > "${dir}/metadata.txt"
  printf 'created: aero-inviscid/%s\n' "${name}"
}

inviscid_point inv-0012-0   0012  0  "Inviskide Einzelpunkt NACA 0012 bei 0 Grad (symmetrisch)"
inviscid_point inv-0012-4   0012  4  "Inviskide Einzelpunkt NACA 0012 bei 4 Grad"
inviscid_point inv-2412-neg2 2412 -2 "Inviskide Einzelpunkt NACA 2412 bei -2 Grad"
inviscid_point inv-2412-0   2412  0  "Inviskide Einzelpunkt NACA 2412 bei 0 Grad"
inviscid_point inv-2412-4   2412  4  "Inviskide Einzelpunkt NACA 2412 bei 4 Grad"
inviscid_point inv-2412-8   2412  8  "Inviskide Einzelpunkt NACA 2412 bei 8 Grad"
inviscid_point inv-clark-y-0 clark 0  "Inviskide Einzelpunkt Clark Y bei 0 Grad"
inviscid_point inv-clark-y-4 clark 4  "Inviskide Einzelpunkt Clark Y bei 4 Grad"

inviscid_seq() {
  local name="$1" profile="$2" purpose="$3"
  local dir="${CASES_ROOT}/aero-inviscid/${name}"
  mkdir -p "${dir}"
  if [[ "${profile}" == "clark" ]]; then
    printf 'PLOP\ng\n\nLOAD CLARK_Y.dat\nOPER\nASEQ -4 8 2\n\nQUIT\n' > "${dir}/commands.in"
    copy_input aero-inviscid "${name}"
  else
    printf 'PLOP\ng\n\nNACA %s\nOPER\nASEQ -4 8 2\n\nQUIT\n' "${profile}" > "${dir}/commands.in"
  fi
  {
    printf 'case-id: aero-inviscid/%s\n' "${name}"
    printf 'category: aero-inviscid\n'
    printf 'purpose: %s\n' "${purpose}"
    printf 'step: 17r4\n'
    printf 'status: canonical-exact\n'
    printf 'determinism: pending\n'
    printf 'timeout: 120\n'
    printf 'locale: LC_ALL=C\n'
    printf 'parameters: inviscid, Mach 0, Ncrit 9 (Defaults), ASEQ -4 8 2\n'
    printf 'limitations: none\n'
  } > "${dir}/metadata.txt"
  printf 'created: aero-inviscid/%s\n' "${name}"
}

inviscid_seq seq-inv-0012    0012 "Inviskide Sequenz NACA 0012, ASEQ -4..8 Schritt 2"
inviscid_seq seq-inv-2412    2412 "Inviskide Sequenz NACA 2412, ASEQ -4..8 Schritt 2"
inviscid_seq seq-inv-clark-y clark "Inviskide Sequenz Clark Y, ASEQ -4..8 Schritt 2"

# ---------------------------------------------------------------------------
# Category: aero-viscous
# ---------------------------------------------------------------------------
viscous_point() {
  local name="$1" profile="$2" re="$3" alpha="$4" purpose="$5"
  local dir="${CASES_ROOT}/aero-viscous/${name}"
  mkdir -p "${dir}"
  if [[ "${profile}" == "clark" ]]; then
    printf 'PLOP\ng\n\nLOAD CLARK_Y.dat\nOPER\nVISC\n%s\nITER 100\nALFA %s\n\nQUIT\n' "${re}" "${alpha}" > "${dir}/commands.in"
    copy_input aero-viscous "${name}"
  else
    printf 'PLOP\ng\n\nNACA %s\nOPER\nVISC\n%s\nITER 100\nALFA %s\n\nQUIT\n' "${profile}" "${re}" "${alpha}" > "${dir}/commands.in"
  fi
  {
    printf 'case-id: aero-viscous/%s\n' "${name}"
    printf 'category: aero-viscous\n'
    printf 'purpose: %s\n' "${purpose}"
    printf 'step: 17r4\n'
    printf 'status: canonical-exact\n'
    printf 'determinism: pending\n'
    printf 'timeout: 120\n'
    printf 'locale: LC_ALL=C\n'
    printf 'parameters: viscous, Mach 0, Ncrit 9, natuerliche Transition, Re=%s, ITER 100, alpha=%s\n' "${re}" "${alpha}"
    printf 'limitations: none\n'
  } > "${dir}/metadata.txt"
  printf 'created: aero-viscous/%s\n' "${name}"
}

viscous_point visc-0012-re1e6   0012 1000000 4 "Viskose Einzelpunkt NACA 0012, Re=1e6, alpha=4"
viscous_point visc-2412-re1e6   2412 1000000 4 "Viskose Einzelpunkt NACA 2412, Re=1e6, alpha=4"
viscous_point visc-2412-re3e6   2412 3000000 4 "Viskose Einzelpunkt NACA 2412, Re=3e6, alpha=4"
viscous_point visc-clark-y      clark 1000000 4 "Viskose Einzelpunkt Clark Y, Re=1e6, alpha=4"

viscous_seq() {
  local name="$1" profile="$2" re="$3" aseq="$4" purpose="$5"
  local dir="${CASES_ROOT}/aero-viscous/${name}"
  mkdir -p "${dir}"
  if [[ "${profile}" == "clark" ]]; then
    printf 'PLOP\ng\n\nLOAD CLARK_Y.dat\nOPER\nVISC\n%s\nITER 100\nASEQ %s\n\nQUIT\n' "${re}" "${aseq}" > "${dir}/commands.in"
    copy_input aero-viscous "${name}"
  else
    printf 'PLOP\ng\n\nNACA %s\nOPER\nVISC\n%s\nITER 100\nASEQ %s\n\nQUIT\n' "${profile}" "${re}" "${aseq}" > "${dir}/commands.in"
  fi
  {
    printf 'case-id: aero-viscous/%s\n' "${name}"
    printf 'category: aero-viscous\n'
    printf 'purpose: %s\n' "${purpose}"
    printf 'step: 17r4\n'
    printf 'status: canonical-exact\n'
    printf 'determinism: pending\n'
    printf 'timeout: 120\n'
    printf 'locale: LC_ALL=C\n'
    printf 'parameters: viscous, Mach 0, Ncrit 9, natuerliche Transition, Re=%s, ITER 100, ASEQ %s\n' "${re}" "${aseq}"
    printf 'limitations: none\n'
  } > "${dir}/metadata.txt"
  printf 'created: aero-viscous/%s\n' "${name}"
}

viscous_seq seq-visc-0012    0012 1000000 "0 8 2"  "Viskose Sequenz NACA 0012, Re=1e6, ASEQ 0..8 Schritt 2"
viscous_seq seq-visc-2412    2412 1000000 "0 8 2"  "Viskose Sequenz NACA 2412, Re=1e6, ASEQ 0..8 Schritt 2"
viscous_seq seq-visc-clark-y clark 1000000 "0 6 2" "Viskose Sequenz Clark Y, Re=1e6, ASEQ 0..6 Schritt 2"

# The case deliberately does not converge (behaviour reference, kept
# separate from the stable goldens).
new_case aero-viscous visc-nonconverging canonical-raw-only \
  "Nicht konvergierender viskoser Fall NACA 2412, Re=1e6, alpha=25: 'VISCAL: Convergence failed' (Verhaltensreferenz)" \
  17r4 "viscous, Mach 0, Ncrit 9, Re=1e6, ITER 100, alpha=25" \
  "bewusst nicht konvergierend; nur als Verhaltensreferenz, nicht als stabiles numerisches Golden" <<'EOF'
PLOP
g

NACA 2412
OPER
VISC
1000000
ITER 100
ALFA 25

QUIT
EOF

# ---------------------------------------------------------------------------
# Category: state (State behavior)
# ---------------------------------------------------------------------------
new_case state state-ascending canonical-exact \
  "Aufsteigende inviskide Sequenz NACA 2412, ASEQ 0..8 Schritt 2" \
  17r4 "inviscid, ASEQ 0 8 2" "none" <<'EOF'
PLOP
g

NACA 2412
OPER
ASEQ 0 8 2

QUIT
EOF

new_case state state-descending canonical-exact \
  "Absteigende inviskide Sequenz NACA 2412, ASEQ 8..0 Schritt -2" \
  17r4 "inviscid, ASEQ 8 0 -2" "none" <<'EOF'
PLOP
g

NACA 2412
OPER
ASEQ 8 0 -2

QUIT
EOF

new_case state state-same-target-in-seq canonical-exact \
  "Derselbe Zielpunkt (alpha=4) innerhalb einer Sequenz (ASEQ 0 8 4) und als Einzelpunkt (ALFA 4)" \
  17r4 "inviscid, ASEQ 0 8 4 + ALFA 4" "none" <<'EOF'
PLOP
g

NACA 2412
OPER
ASEQ 0 8 4
ALFA 4

QUIT
EOF

new_case state state-inviscid-to-viscous canonical-exact \
  "Wechsel von inviskid zu viskos: ALFA 4 (inviscid) dann VISC Re=1e6 ITER 100 ALFA 4" \
  17r4 "ALFA 4 inviscid, dann VISC Re=1e6 ITER 100 ALFA 4" "none" <<'EOF'
PLOP
g

NACA 2412
OPER
ALFA 4
VISC
1000000
ITER 100
ALFA 4

QUIT
EOF

new_case state state-cl-driven canonical-exact \
  "CL-gesteuerte viskose Rechnung NACA 2412, Re=1e6: CL 0.5 und CL 1.0" \
  17r4 "viscous, Re=1e6, ITER 100, CL 0.5 / CL 1.0" "none" <<'EOF'
PLOP
g

NACA 2412
OPER
VISC
1000000
ITER 100
CL 0.5
CL 1.0

QUIT
EOF

# ---------------------------------------------------------------------------
# Category: polar
# ---------------------------------------------------------------------------
new_case polar polar-inv-2412 canonical-exact \
  "Inviskide Polar NACA 2412, ASEQ 0..8 Schritt 2, mit Polar-Datei (PACC)" \
  17r4 "inviscid, PACC, ASEQ 0 8 2, Polar-Datei inv2412.pol" "none" <<'EOF'
PLOP
g

NACA 2412
OPER
PACC
inv2412.pol

ASEQ 0 8 2

QUIT
EOF

new_case polar polar-visc-2412 canonical-exact \
  "Viskose Polar NACA 2412, Re=1e6 ITER 100, ASEQ 0..8 Schritt 2, mit Polar- und Dump-Datei (PACC)" \
  17r4 "viscous, Re=1e6, ITER 100, PACC, ASEQ 0 8 2, Polar-Datei visc2412.pol + Dump visc2412.dump" \
  "Dump-Datei enthaelt die kompletten Druckverteilungen je Punkt" <<'EOF'
PLOP
g

NACA 2412
OPER
VISC
1000000
ITER 100
PACC
visc2412.pol
visc2412.dump
ASEQ 0 8 2

QUIT
EOF

echo "All reference cases generated under: ${CASES_ROOT}"
