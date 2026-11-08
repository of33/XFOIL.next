# U3 — Raw XFOIL 6.99 to 6.996 source-difference inventory

## Purpose

Inventory and classify every difference between the frozen XFOIL 6.99 and
XFOIL 6.996 Fortran source baselines. U3 is analysis and documentation only.
It does not port anything into the C++ product.

## Scope

- Compare the 48 XFOIL 6.99 source files with the 49 XFOIL 6.996 source files.
- Classify file-set, byte, routine, include/COMMON, dimension, algorithm,
  command, format, plot and build-helper differences.
- Record the differences in a form that can drive later, separately accepted
  C++ integration groups.
- Keep raw diffs, machine-readable inventories and normalized analysis copies
  in the external audit area.

## Explicit non-goals

- No change to either Fortran baseline.
- No Fortran formatting or normalization in the repository.
- No C++ product-code change.
- No test, Golden, reference or tolerance change.
- No canonical 6.996 oracle declaration.
- No new 6.996 reference corpus.
- No compiler or build-flag change.
- No replacement for the missing `osmap.dat`.
- No plot-functionality port.

## Protected source baselines

- `fortran-src/xfoil-6.99/`: 48 protected upstream files plus repository-owned
  `SHA256SUMS`; 48/48 hashes and modes unchanged.
- `fortran-src/xfoil-6.996/`: 49 protected upstream files plus repository-owned
  `SHA256SUMS`; 49/49 hashes and modes unchanged.
- `fortran-bin/xfoil` and `fortran-bin/xfoil-6.996` are unchanged.
- The 6.996 binary remains diagnostic and is not a canonical oracle.

The repository bytes remain authoritative. Normalized copies are diagnostic
only and never define hash or provenance contracts.

## Comparison methodology

For every common changed file four external views were generated:

- **A — raw diff**: byte- and line-faithful diff; the authoritative view.
- **B — line-ending-neutral view**: analysis only.
- **C — fixed-form view**: separates label, continuation marker and source
  columns for inspection.
- **D — comment/whitespace-reduced view**: first-pass classification only.
  It never asserts semantic equality.

The primary comparison key is the relative file name. A same-named file in
another location is not automatically treated as a rename. Routine inventory
is derived from fixed-form `PROGRAM`, `BLOCK DATA`, `SUBROUTINE` and `FUNCTION`
statements, including continued argument lists.

## File-set comparison

Common upstream files: **44**.

| Classification | Count |
|---|---:|
| Common and byte-identical | 19 |
| Common but changed | 25 |
| Only in 6.99 | 4 |
| Only in 6.996 | 5 |

Repository-owned `SHA256SUMS` exists in both directories and is metadata, not
an upstream source file.

## Byte-identical files

```text
BLPAR.INC   CIRCLE.INC  PXPLOT.INC  XBL.INC     XDES.INC
aread.f     blu.f       dplot.f     frplot.f    getarg.f
gui.f       ntcalc.f    osmap.f     polfit.f    profil.f
pxplot.f    sort.f      xqdes.f     xutils.f
```

Notable consequences: `osmap.f` and `ntcalc.f` are unchanged; the 6.99 OSMAP
reader and transition producer therefore have no source difference in U3.

## Changed files

```text
PINDEX.INC  PPLOT.INC  XFOIL.INC
blplot.f    iopol.f    modify.f    naca.f      plutil.f
pntops.f    polplt.f   pplot.f     spline.f    userio.f
xbl.f       xblsys.f   xfoil.f     xgdes.f     xgeom.f
xmdes.f     xoper.f    xpanel.f    xplots.f    xpol.f
xsolve.f    xtcam.f
```

Line-level statistics are in the external `file-diff-summary.tsv`. All 25
changed files have non-comment, non-whitespace differences; no changed file is
formatting-only as a whole.

## Files present only in one baseline

Only in 6.99:

```text
README.md
osmap.dat
osmapDP.dat
osmap_DP.f
```

Only in 6.996:

```text
Makefile_gfortran
frplot0.f
getosfile.c
p.ftnchek
x.ftnchek
```

`README.md`, `osmap.dat` and the inactive `osmap_DP.f`/`osmapDP.dat` are not
part of the 6.996 reduced subset. `frplot0.f` is a dummy FRPLOT stub;
`getosfile.c` is the OSMAP environment-variable helper; `Makefile_gfortran`,
`p.ftnchek` and `x.ftnchek` are upstream build/analysis descriptions.

## Routine-level inventory

| Baseline | Programs, block data, subroutines, functions |
|---|---:|
| XFOIL 6.99 | 313 |
| XFOIL 6.996 | 318 |

Routine comparison:

| Classification | Count |
|---|---:|
| unchanged | 231 |
| changed | 76 |
| signature-changed | 5 |
| added | 6 |
| removed | 1 |

Added routines: `FRPLOT` (dummy in `frplot0.f`), `SVGOUT`, `DXFOUT`, `SQTE`,
`TECIRC`, `BLDUMP3`.

Removed routine: `OSMAP` from the inactive `osmap_DP.f` variant.

Changed routine names by file:

| File | Changed / signature-changed / added / removed |
|---|---|
| `blplot.f` | BLPLOT |
| `frplot0.f` | +FRPLOT |
| `iopol.f` | POLREAD, POLWRIT |
| `modify.f` | MODIFY, MODIXY |
| `naca.f` | signature: NACA4, NACA5 |
| `osmap_DP.f` | -OSMAP |
| `plutil.f` | OPLSET |
| `pntops.f` | ADDP, MOVP, DELP |
| `polplt.f` | POLPLT, POLLAB, GETVAR, VEPPLT |
| `pplot.f` | PPLOT, GETDEF, GETSET |
| `spline.f` | all 14 routines (type declarations; SEGSPLD also functional) |
| `userio.f` | READI, READR, GETINT, GETFLT |
| `xbl.f` | MRCHUE, MRCHDU, XIFSET, UPDATE |
| `xblsys.f` | CFT |
| `xfoil.f` | XFOIL, INIT, CDCALC, LOAD, GETPAN; signature: NACA; +SVGOUT, +DXFOUT |
| `xgdes.f` | GDES, TGAP, LERAD, FLAP, PLOTG, PLTCAM, OVER; signature: PLTAIR |
| `xgeom.f` | LEFIND, XLFIND, GEOPAR, CANG; signature: NORM; +SQTE, +TECIRC |
| `xmdes.f` | MDES |
| `xoper.f` | OPER, BLDUMP2, VPAR, VISCAL; +BLDUMP3 |
| `xpanel.f` | PSILIN, PSWLIN, GGCALC, QDCALC |
| `xplots.f` | PLTINI, PPAPLT |
| `xpol.f` | PLRSET, PLRINI, PLRADD, PLXADD |
| `xsolve.f` | GAUSS, CGAUSS, LUDCMP |
| `xtcam.f` | ZERCAM, TCBUF, TCSCAL |

Machine-readable inventories: `routines-6.99.tsv`, `routines-6.996.tsv`,
`routine-comparison.tsv`, `routine-difference-inventory.tsv` and
`file-routine-summary.tsv`.

## Include and common-state changes

The eight include files split as follows:

- Identical: `BLPAR.INC`, `CIRCLE.INC`, `PXPLOT.INC`, `XBL.INC`,
  `XDES.INC`.
- Changed: `PINDEX.INC`, `PPLOT.INC`, `XFOIL.INC`.

`PINDEX.INC` adds per-element polar metadata: `KPTOT=4`, `KCX`, `KCY`, `KCM`,
`KCDV`, `CPOLNNAME` and `CPOLNFORM`.

`PPLOT.INC` adds `NBX=5`, `ISX=2*NBX`, `CPOLEL`, `LSORT` and changes
`CDLMOD` from 5 to 6 rows. The changed COMMON blocks are `PPCOM_L` and
`PPCOM_R`.

`XFOIL.INC` changes several COMMON layouts and dimensions:

- `IQX` 370 -> 601 and `NPX` 12 -> 20;
- derived `IWX`, `IBX`, `IZX`, `IVX` therefore change;
- `CR09` gains `CLF`;
- `CR10` gains `CPOLEL`, `KPOL`, `NKPOL`;
- `CI04` gains `NTS`;
- `CR12` gains `LPPORT`;
- `CR15` gains `MDEF`, `PDEF`, `KDEF`, `DSSR`.

These are high-risk ABI/state changes. Any later C++ port must update the
corresponding state structures and array limits as one coordinated group.

## Limits, dimensions and constants

| Symbol | File | 6.99 | 6.996 | Risk / effect |
|---|---|---:|---:|---|
| `IQX` | `XFOIL.INC` | 370 | 601 | high: panel/workspace and derived sizes |
| `NPX` | `XFOIL.INC` | 12 | 20 | high: stored polar capacity/state |
| `NPAN` default | `xfoil.f` | 160 | 240 | high: default panel distribution |
| `CTERAT` | `xfoil.f` | 0.15 | 0.3 | high: default TE clustering |
| `ISX` | `PPLOT.INC` | 10 | `2*NBX` (=10) | medium: semantic change |
| `NBX` | `PPLOT.INC` | — | 5 | medium: new element dimension |
| `NMAX` | `spline.f` | 1000 | 2500 | medium: larger spline workspace |
| `NVX` | `xsolve.f` | 500 | 515 | medium: larger LUDCMP workspace |
| `KPTOT` | `PINDEX.INC` | — | 4 | medium: per-element polar data |
| `NTS` | `xfoil.f` | — | 20 | low: plot subdivision |
| input line | `userio.f` | 80/130 | 256/258 | low: capacity only |
| parsed input | `userio.f` | 128 | 256 | low: capacity only |
| polar line | `iopol.f` | 128 | 256/512 | medium: format compatibility |
| `BWT` | `xpanel.f` | — | 0.1 | medium: sharp-TE control point |
| `XTES` | `naca.f` | — | 1.00893… | high: sharp-TE NACA geometry |
| `NHCIRC` | `xgdes.f` | — | 15 | low: TE-circle resolution |

The external `limits-dimensions-constants.tsv` contains the complete table.

## Numerical algorithm changes

High-impact numerical candidates:

- `xpanel.f` `QDCALC`: replaces the old sharp-TE zero-source extrapolation
  with a TE-bisector control-point condition. This is a solver change and
  needs dedicated inviscid oracle cases.
- `xgeom.f` `NORM`: now normalizes by chord-vector length and returns shift
  and scale; `LEFIND` and `CANG` are made more robust. This affects geometry
  used by many later routines.
- `naca.f` `NACA4`/`NACA5` and `xfoil.f` `NACA`: add the `LTES` sharp-TE
  option and the new `NACS` command.
- `xgeom.f` `SQTE`/`TECIRC` and `xgdes.f` `SQTE`/`CIRC`: new square-TE and
  TE-circle geometry algorithms.
- `spline.f` `SEGSPLD`: segment-end derivative handling is corrected; the
  local workspace limit rises from 1000 to 2500.
- `xblsys.f` `CFT`: derivative terms for the clamped `GRT` are corrected;
  this is a convergence-path-sensitive boundary-layer change.
- `xmdes.f` `MDES`: `SMOO` now smooths `KQSP=1` instead of `KQTARG`.
- `xfoil.f` defaults: `NPAN=240` and `CTERAT=0.3` change default paneling.

Lower-impact or non-solver changes:

- `xbl.f` adds `DSSR`, `MDEF`, `PDEF`, `KDEF` output fields; existing solver
  results are not changed by the assignments.
- `xsolve.f` `GAUSS`/`CGAUSS` rewrite an arithmetic IF into an equivalent
  `.GT.` test; `LUDCMP` raises its workspace limit.
- `xfoil.f` computes `CLF` (friction lift) and adds new output commands.
- `xoper.f` extends BL dump output and adds `BLDUMP3`; the VISCAL output path
  is extended, not re-derived here.
- `userio.f` only increases input capacity.
- Plot files do not change the product solver.

External `algorithm-classification.tsv` and
`routine-difference-inventory.tsv` record the per-routine classification and
expected effect.

## Command and state-machine changes

Main non-graphical command additions/changes:

- `xfoil.f`: `SVG`, `DXF`, `NACS`; `NACA` now takes an `LTES` argument;
  panel-count warning; minimum panel count 2.
- `xoper.f`: `BLOU` (BLDUMP3); changed polar element plumbing; revised
  `BLDUMP2`/`BLDUMP3` output; output format extended with `CLF`, `CDP`.
- `xgdes.f`: `SYTB`, `XCEN`, `SQTE`, `CIRC`, `NSPL`; `TGAP` gains a
  top/bottom split parameter; `Adeg` alias; `NORM` signature/output.
- `xmdes.f`: `SMOO` target fix.
- `xqdes.f`: unchanged.
- `xpol.f`: append-mode polar writes and element-polar arguments.

`xgdes.f` and `xfoil.f` mix non-graphical geometry/session behavior with plot
updates. `blplot.f` is plot-only.

## File-format and output changes

- `iopol.f`: polar reader/writer gain per-element columns (`CPOLEL`, `KPOL`,
  `NKPOL`, `KPTOT`), longer line buffers, and revised header/column layout.
  This is a potential polar-file compatibility boundary.
- `xpol.f`: changed `POLREAD`/`POLWRIT` calls, element arguments, and append
  mode (replacing the old bottom-seek pattern).
- `userio.f`: input buffers and parse limit grow from 80/128 to 256.
- `xfoil.f`: new SVG and DXF airfoil exporters.
- `xoper.f`: `BLDUMP2` density-corrected quantities and new `BLDUMP3`;
  console output now includes `CLF` and `CDP`.
- `xtcam.f`: max thickness/camber output format widths change.
- `xgdes.f`: `XCEN` prints point coordinates; `TGAP` accepts a third
  split parameter.

No new Golden or reference format is declared in U3. Compatibility must be
tested before any C++ writer/reader changes.

## Plot-only changes

The following are classified as plot-only or plot-dominated:

- `blplot.f`: new m/P/K plots using `MDEF`/`PDEF`/`KDEF`; plot axis defaults.
- `polplt.f`: per-element polar plotting, sorting and label changes.
- `pplot.f`: sorting toggle, Mach exponent, six polar modifiers.
- `plutil.f`: orientation/individual-file options and signature; plot-only.
- `xplots.f`: `PLTINI` formatting and `PLTAIR` call updates.
- `pntops.f` and `xtcam.f`: `PLTAIR` `NTS` argument; `xtcam` also has output
  format changes.
- `dplot.f`, `frplot.f`, `frplot0.f`, `gui.f`, `pxplot.f`: unchanged or
  stub-only.

Plot-only changes are not candidates for an early C++ product port. Only
shared-state reads or numerical producers are candidates.

## Build-description and helper changes

- `Makefile_gfortran` exists only in 6.996. It is an upstream description, not
  the repository build; it references `../src`, `../osrc`, `../plotlib` and
  X11, and includes OSMAP objects (`frplot.o`, `ntcalc.o`, `osmap.o`,
  `getosfile.o`).
- `getosfile.c` exists only in 6.996 and reads the `OSMAP` environment
  variable.
- `p.ftnchek` and `x.ftnchek` exist only in 6.996; they reference the external
  plot library and `osrc`.
- `getarg.f` is byte-identical.
- The repository build keeps its own adapter flags; U3 does not copy
  `-fallow-argument-mismatch` or any other adapter flag into an upstream file.

## OSMAP observations

- `osmap.f`: byte-identical.
- `ntcalc.f`: byte-identical.
- `frplot.f`: byte-identical; plot-only.
- `getosfile.c`: present only in 6.996; environment-variable lookup helper.
- `osmap_DP.f`: inactive variant present only in 6.99.
- `osmap.dat`: present only in 6.99, absent from the 6.996 subset and absent
  from the 6.996 upstream archive.

There is no source-level evidence in U3 that the 6.99 `osmap.dat` format is
compatible with the 6.996 reader. Controlled diagnostic use may be plausible
later, but compatibility is not proven and no file is copied.

## Candidate functional integration groups

These are planning groups, not final C++ file mappings. The former G07 was
split for F1 into G07 defaults, G08 file formats/exports and G09
command/session behavior. The former G08 polar I/O was folded into G08;
former G09-G12 became G10-G13.

### G01 — State, limits and polar metadata

- **Scope:** dimensions, COMMON layout and per-element polar state.
- **Fortran files/routines:** `PINDEX.INC`, `XFOIL.INC`, `PPLOT.INC`.
- **C++ candidate area:** core state, polar and boundary-layer types; exact
  files to be mapped.
- **Expected change:** `IQX=601`, `NPX=20`, `KPTOT=4`, `CLF`,
  `CPOLEL`/`KPOL`/`NKPOL`, `NTS`, `LPPORT`, `MDEF`/`PDEF`/`KDEF`/`DSSR`,
  `NBX=5`, `ISX=2*NBX`.
- **Numerical risk:** high.
- **Dependencies:** none.
- **Required oracle cases:**
  - NACA 0012 at panel counts 580, 600 and 601 via `PANE`; compare
    acceptance/rejection at the `IQX` boundary, resulting panel count, CL, CM
    and panel-coordinate arrays. Purpose: verify old and new panel limits.
  - Store 12 and 20 polars for NACA 0012 and Clark Y; write, read and select
    them; verify 20 are accepted, a 21st is rejected, and all values of the
    12-polar set remain byte-identical. Purpose: verify `NPX`.
  - One-element and five-element polar records; verify the new per-element
    slots are present, zero when unused, and round-trip when populated.
    Purpose: verify `KPTOT`/`NBX` state.
  - Existing 6.99 NACA 0012 and Clark Y inviscid/viscous cases; compare all
    standard output when no new field is requested. Purpose: regression.
- **Non-goals:** no change to C++ limits before G01 is accepted.
- **Recommended order:** 1.

### G02 — Numerics robustness

- **Scope:** segmented spline and linear-solver workspace.
- **Fortran files/routines:** `spline.f` `SEGSPLD`, `SPLINE`, `SPLIND`;
  `xsolve.f` `LUDCMP`.
- **C++ candidate area:** core math/numerics; exact files to be mapped.
- **Expected change:** `SEGSPLD` end-condition bug fix, `NMAX` 2500,
  `NVX` 515.
- **Numerical risk:** medium.
- **Dependencies:** G01 (workspace limits).
- **Required oracle cases:**
  - Airfoil with a doubled segment joint (for example a flap-hinge or
    duplicated `S` value) that reaches the corrected `XS1T`/`XS2T` branch;
    compare spline values and first derivatives at the joint and at an
    interior point against 6.99 and 6.996. Purpose: prove the bug fix.
  - 160-, 240- and 600-panel cases that exercise `NMAX` and `NVX`; compare
    solver results and rejection above the new limits. Purpose: capacity.
  - Existing 6.99 standard NACA 0012 spline/inverse case; compare unchanged
    values. Purpose: regression.
- **Non-goals:** no blanket recalculation of all spline results.
- **Recommended order:** 2.

### G03 — Geometry normalization and robustness

- **Scope:** `NORM`, `LEFIND` and `CANG`.
- **Fortran files/routines:** `xgeom.f` `NORM`, `LEFIND`, `XLFIND`, `CANG`,
  `GEOPAR`.
- **C++ candidate area:** core geometry; exact files to be mapped.
- **Expected change:** chord-vector normalization with returned shift/scale;
  robust leading-edge search; full-range bend angle via `ATAN2`.
- **Numerical risk:** high.
- **Dependencies:** G01.
- **Required oracle cases:**
  - NACA 0012 (symmetric), NACA 2412 (cambered) and Clark Y; rotate,
    translate and scale, then run `NORM`; compare chord, LE, TE, arc length
    and panel coordinates. Purpose: normalization invariance.
  - Blunt-TE and sharp-TE variants; compare `LEFIND` index, leading-edge
    coordinates and geometry parameters. Purpose: LE robustness.
  - `CANG` on symmetric, cambered and high-curvature contours; compare the
    reported bend angle and the corresponding 6.99 case. Purpose: angle
    formula change.
- **Non-goals:** no new geometry defaults.
- **Recommended order:** 3.

### G04 — Sharp trailing edge, NACA and GDES geometry

- **Scope:** sharp-TE NACA, `NACS`, `SQTE`, `TECIRC`, `SYTB` and `TGAP`.
- **Fortran files/routines:** `naca.f` `NACA4`/`NACA5`; `xfoil.f` `NACA`;
  `xgeom.f` `SQTE`/`TECIRC`; `xgdes.f` `SQTE`/`CIRC`/`SYTB`/`TGAP`.
- **C++ candidate area:** core geometry, geometry design and CLI; exact files
  to be mapped.
- **Expected change:** `LTES` sharp-TE option, `NACS`, square-TE and TE-circle
  geometry, independent top/bottom scaling, three-parameter `TGAP`.
- **Numerical risk:** high.
- **Dependencies:** G03.
- **Required oracle cases:**
  - NACA 0012 and NACA 2412 generated with standard `NACA` and with `NACS`;
    run `PANE` and inviscid alpha 0 and 5; compare TE gap, panel count,
    geometry arrays, CL and CM. Purpose: sharp-TE NACA behavior.
  - NACA 2412 in GDES with `SYTB` top/bottom factors; compare thickness,
    camber, TE and panel coordinates. Purpose: independent scaling.
  - Blunt-TE airfoil with `SQTE` and then `CIRC`; compare TE-base
    orthogonality, TE-circle points and geometry parameters. Purpose: new TE
    algorithms.
  - `TGAP` with split values -1, 0 and +1; compare gap, top/bottom
    displacement and geometry. Purpose: third-parameter behavior.
  - `PLTAIR` `NTS` 1, 20 and 50 is plot-only; no non-graphical oracle is
    required unless shared state changes.
- **Non-goals:** no plot-functionality port.
- **Recommended order:** 4.

### G05 — Paneling and QDCALC

- **Scope:** sharp-TE inviscid influence matrix.
- **Fortran files/routines:** `xpanel.f` `QDCALC` (other changed routines are
  comment-only).
- **C++ candidate area:** core paneling/inviscid; exact files to be mapped.
- **Expected change:** TE-bisector control-point condition replaces zero
  source influence at the sharp-TE row.
- **Numerical risk:** high.
- **Dependencies:** G01, G03.
- **Required oracle cases:**
  - Sharp-TE NACA 0012 at alpha 0 and 5 with 160 and 240 panels; compare the
    changed `BIJ`/`DQDM` TE-row values, CL, CM, GAMMA and convergence.
    Purpose: prove the QDCALC change.
  - Blunt-TE airfoil and a case with `SHARP` false; compare against 6.99.
    Purpose: branch regression.
  - Selected surface-Cp stations on NACA 0012 and Clark Y; compare final
    inviscid Cp. Purpose: output-level verification.
- **Non-goals:** no viscous coupling change in this group.
- **Recommended order:** 5.

### G06 — Boundary layer and derivatives

- **Scope:** `CFT` derivatives and new boundary-layer output fields.
- **Fortran files/routines:** `xblsys.f` `CFT`; `xbl.f` `MRCHUE`, `MRCHDU`,
  `UPDATE`.
- **C++ candidate area:** core boundary layer and export; exact files to be
  mapped.
- **Expected change:** corrected `GRT` derivative terms and `DSSR`, `MDEF`,
  `PDEF`, `KDEF` outputs.
- **Numerical risk:** medium.
- **Dependencies:** G01.
- **Required oracle cases:**
  - Laminar NACA 0012 at alpha 0, Re 1e6; compare `CFT`, `CF_RT`, `CF_MSQ`,
    transition location and station values. Purpose: derivative fix.
  - Turbulent NACA 0012 at alpha 5; compare `CFT`, CL and CD. Purpose:
    turbulent branch.
  - Wake case; compare `CFT` and the new `DSSR`/`MDEF`/`PDEF`/`KDEF` fields
    along the wake. Purpose: wake branch.
  - Converged and controlled non-converged high-alpha case; compare
    iterations, residuals and final state. Purpose: convergence behavior.
  - `BLDUMP` output for the same viscous case; compare new fields against the
    6.99 dump. Purpose: output-state verification.
- **Non-goals:** no new BL model.
- **Recommended order:** 6.

### G07 — Defaults

- **Scope:** only `NPAN` and `CTERAT` default initialization, display and
  persistence.
- **Fortran files/routines:** `xfoil.f` initialization/`VPAR` display;
  `WDEF`/`RDEF` defaults path if it stores these values.
- **C++ candidate area:** app CLI/defaults; exact files to be mapped.
- **Expected change:** `NPAN=240`, `CTERAT=0.3`.
- **Numerical risk:** medium-high.
- **Dependencies:** G01.
- **Required oracle cases:**
  - Cold start with no saved defaults on NACA 0012; show `VPAR`; compare
    `NPAN=240`, `CTERAT=0.3`, panel count and generated panel coordinates.
    Purpose: default initialization/display.
  - `LOAD` Clark Y then `PANE`; compare default panel count and geometry.
    Purpose: default geometry path.
  - `WDEF` then `RDEF` round-trip, if these defaults are persisted; read back
    and display the values. Purpose: default persistence.
  - Explicit `NPAN`/`CTERAT` override case; compare with the 6.99 result.
    Purpose: non-default regression.
- **Non-goals:** command/session state, console output and file/export
  formats are not part of this group.
- **Recommended order:** 7.

### G08 — File formats and exports

- **Scope:** polar files, BL dump output, SVG and DXF.
- **Fortran files/routines:** `iopol.f` `POLREAD`/`POLWRIT`; `xpol.f`
  `PLRSET`/`PLRINI`/`PLRADD`/`PLXADD`; `xfoil.f` `SVGOUT`/`DXFOUT`;
  `xoper.f` `BLDUMP2` and `BLDUMP3`.
- **C++ candidate area:** core polar, IO and export; exact files to be mapped.
- **Expected change:** per-element polar columns, append-mode writes, BLDUMP3,
  density-corrected BL dump output, SVG/DXF files.
- **Numerical risk:** high compatibility risk.
- **Dependencies:** G01, G06.
- **Required oracle cases:**
  - Inviscid NACA 0012 polar from alpha 0 to 5; compare header, column count,
    record bytes, zero/unused element columns and a full write/read round-trip.
    Purpose: polar format.
  - Viscous polar for one and, where supported, multiple elements; compare
    `Cx`, `Cy`, `Cm`, `CDv` element columns and the header. Purpose: new
    element data.
  - Append two polar runs and read them back; verify no overwrite and stable
    order. Purpose: append semantics.
  - 6.99 polar read by 6.996 and 6.996 polar read by 6.99; record exact
    compatibility or failure. Purpose: migration boundary.
  - NACA 0012 viscous `BLDUMP2` and `BLDUMP3`; compare header, 20 columns, new
    m/P/K/delta* fields, density factors and record bytes. Purpose: BL output
    format, separate from defaults.
  - Controlled NACA 0012 `SVG` and `DXF` export; compare file name, header,
    structure and coordinate order; test an unwritable target. Purpose:
    export format and error handling.
- **Non-goals:** no Golden replacement and no canonical 6.996 reference in U3.
- **Recommended order:** 8.

### G09 — Command and session behavior

- **Scope:** commands, menu/session state, invalidation, warm starts,
  sequence/convergence behavior and console output.
- **Fortran files/routines:** `xfoil.f` command dispatcher/`INIT`; `xoper.f`
  `OPER`/`VPAR`; `xgdes.f` `GDES` and command handlers.
- **C++ candidate area:** app CLI/session; exact files to be mapped.
- **Expected change:** `NACS`, `BLOU`, `SYTB`, `XCEN`, `SQTE`, `CIRC`,
  `NSPL`, `Adeg` alias, menu text, state invalidation and output lines.
- **Numerical risk:** medium.
- **Dependencies:** G01, G04, G08.
- **Required oracle cases:**
  - Start XFOIL, enter and leave OPER and GDES; invoke `NACS`, `BLOU`,
    `SYTB`, `SQTE`, `CIRC`, `XCEN` and `NSPL`; compare acceptance, prompts,
    state invalidation and menu return. Purpose: command/session behavior.
  - NACA 0012 inviscid run followed by viscous warm start; compare sequence
    continuation, convergence status and invalidation after a geometry edit.
    Purpose: warm start/sequence.
  - Geometry change after a converged run; verify the correct state is
    invalidated and the next operation recomputes it. Purpose: invalidation.
  - Compare console status/error lines for the changed commands. Purpose:
    output behavior.
  - `SVG`/`DXF` command registration only; file content is verified in G08.
    `NSPL` is plot-only; no non-graphical oracle required for its effect.
- **Non-goals:** no default values (G07) and no file format bytes (G08).
- **Recommended order:** 9.

### G10 — Inverse design

- **Scope:** `SMOO` target selection in `MDES`.
- **Fortran files/routines:** `xmdes.f` `MDES`.
- **C++ candidate area:** core inverse design; exact files to be mapped.
- **Expected change:** `KQSP=1` instead of `KQTARG`.
- **Numerical risk:** medium.
- **Dependencies:** G01.
- **Required oracle cases:**
  - Qspec inverse case with `KQTARG` different from 1; run `SMOO`; compare
    the smoothed Qspec distribution, target segment, converged geometry, CL
    and CM. Purpose: prove the corrected target.
  - Existing 6.99 case with `KQTARG=1`; compare unchanged. Purpose:
    regression.
  - Edge/non-converged target; compare convergence status and error output.
    Purpose: failure path.
- **Non-goals:** no general inverse-design refactor.
- **Recommended order:** 10.

### G11 — Input capacity

- **Scope:** console input buffer and parse limit.
- **Fortran files/routines:** `userio.f` `READI`, `READR`, `GETINT`,
  `GETFLT`.
- **C++ candidate area:** app CLI input parsing; exact files to be mapped.
- **Expected change:** 256-character input, 256-character parse limit.
- **Numerical risk:** low.
- **Dependencies:** none.
- **Required oracle cases:**
  - A 200-character command line containing several numbers; verify all are
    parsed by 6.996 and compare with the 6.99 truncation. Purpose: capacity.
  - 256-character boundary and one character beyond it; verify the documented
    accepted/rejected behavior. Purpose: boundary.
  - Malformed tab/spacing input and a short existing command; verify unchanged
    parsing. Purpose: regression.
- **Non-goals:** no grammar change.
- **Recommended order:** 11.

### G12 — Plot-only observations

- **Scope:** plot appearance and plot-only commands.
- **Fortran files/routines:** `blplot.f`, `polplt.f`, `pplot.f`, `plutil.f`,
  `xplots.f`, `pntops.f`, `xtcam.f`, `NSPL`.
- **C++ candidate area:** no product port planned.
- **Expected change:** plotting only, except read-only use of new BL/polar
  arrays.
- **Numerical risk:** low.
- **Dependencies:** G06 (read-only BL fields for BL plots).
- **Required oracle cases:**
  - For pure plot changes: no non-graphical oracle case required unless shared
    state, file output or a numerical producer is affected.
  - BL m/P/K plot after a viscous NACA 0012 solution; verify the plotted
    `MDEF`/`PDEF`/`KDEF` equal the `BLDUMP3` values and CL/CD are unchanged.
    Purpose: mixed plot/shared-state check.
  - Polar plot sorting/modifiers; verify the saved polar bytes are unchanged.
    Purpose: ensure plot-only does not mutate stored data.
- **Non-goals:** no plot-functionality port.
- **Recommended order:** defer.

### G13 — OSMAP

- **Scope:** OSMAP reader and transition producer.
- **Fortran files/routines:** `osmap.f`, `ntcalc.f`, `frplot.f`,
  `getosfile.c`.
- **C++ candidate area:** Orr-Sommerfeld map and transition; exact files to be
  mapped.
- **Expected change:** none in source; `osmap.dat` data contract is missing.
- **Numerical risk:** unknown.
- **Dependencies:** data-file provenance and format compatibility.
- **Required oracle cases:**
  - `diagnostic case pending data-file provenance and format compatibility`;
    later verify the 6.99 `osmap.dat` provenance, whether the 6.996 reader
    opens it, and compare OSMAP interpolation on a controlled `(rsp, wsp)`
    grid. No canonical case is created in U3.
- **Non-goals:** no `osmap.dat` copy or generation.
- **Recommended order:** blocked.

All oracle cases are proposals until separately authorized. Run 6.99 and
6.996 in separate directories with `LC_ALL=C`; do not overwrite existing
Goldens or references; Linux remains the canonical reference platform; no
global tolerance is introduced. U3 creates no new references.

## Open questions

- Is the 6.996 reader compatible with the 6.99 `osmap.dat` format? Not proven.
- Which 6.996 polar-file changes require a migration or dual reader?
- Should the new `NPAN=240` and `CTERAT=0.3` defaults be adopted as product
  defaults, or only as 6.996 diagnostic settings?
- How should the larger state/COMMON layout be represented in the C++ types?
- Are the new GDES commands (`SYTB`, `SQTE`, `CIRC`, `XCEN`, `NSPL`) all
  product-relevant, or partly plot/design-tool behavior?
- Do the QDCALC, NORM and SEGSPLD changes need dedicated 6.996 diagnostic
  reference cases before porting?
- Which plot-only changes must be explicitly excluded from the product port?
- Are there debug prints or commented diagnostics in 6.996 that should not be
  carried into C++?

## Acceptance criteria

- File-set, routine, include/COMMON, limits and algorithm inventories are
  complete enough to plan integration groups.
- All 48/49 protected source hashes and both binaries remain unchanged.
- No C++ product code, Golden, reference, tolerance or product-version change.
- Gates and `make test-core` remain green.
- Public documentation contains a compact classified overview, not raw diffs.
- External audit area contains the machine-readable inventories and raw diffs.

## Git and publication policy

U3 creates no commit before independent acceptance. No push, force-push,
fetch, tag, release or history rewrite. All local commits remain unpushed
until cumulative XFOIL.next 1.1.0 acceptance.

## Exact next allowed step

`X6996-U3-R1-Independent-Source-Difference-Inventory-Acceptance`.
