# XFOIL 6.996 — U3 source-difference inventory report

Status: PASS

This report records the raw and classified differences between the frozen
XFOIL 6.99 and XFOIL 6.996 Fortran source baselines. Generated raw diffs
and machine-readable inventories remain in the external audit area.

> **Post-publication provenance errata (2026-11-14).** The XFOIL 6.99 source
> tree was later restored to the original upstream state and 21 source-based
> harness references were corrected for historical local `OFS:` output.
> Statements in this historical report about `fortran-src/**` or 6.99
> reference data remaining unchanged describe the pre-correction basis. See
> [the 6.99 provenance correction](36-xfoil-699-source-provenance-correction_report.md).

## Starting state

- HEAD: `a73122e19bb0f7e2f4001ccd639b27b16e25c3f2`.
- Parent: `3ab0fb5c1960f6fbf5a0388db041aefb08e11f1e`.
- `origin/main`: `58bca7d17f847e1aef6b1c8d3d4e167f40f69945`.
- Ahead/behind: `0 5` (behind 0, ahead 5).
- Working tree before U3: clean; staged 0.
- Tag: `v1.0.0` unchanged.
- `make check-fortran-sha`: 6.99 48/48 and 6.996 49/49, exit 0.
- `make check-license-headers`: exit 0; 6.99 48 files/23 notices,
  6.996 49 files/23 notices.
- Binary hashes: `fortran-bin/xfoil`
  `f5779d6954d0b8fdc152e0e0633f18732e00b7b69228d561d629bd524bba598c`;
  `fortran-bin/xfoil-6.996`
  `000b0468d9a7f4fce42d6fac24bd14147d7d326c8a9035909c8e9f98d180f50e`.

## Comparison method

For each common changed file the external audit area contains:

- raw diff (authoritative);
- line-ending-neutral copy;
- fixed-form view separating label, continuation and source columns;
- comment/whitespace-reduced view for first-pass classification only.

Files are matched primarily by relative name. Same-named files in different
locations are not automatically treated as renames. Routine boundaries are
derived from fixed-form `PROGRAM`, `BLOCK DATA`, `SUBROUTINE` and `FUNCTION`
statements; continued argument lists are joined. Duplicate routine names are
paired by same file first, then by unique remaining name.

External machine-readable inventories:

```text
file-set-comparison.tsv
file-diff-summary.tsv
metadata-files.tsv
routines-6.99.tsv
routines-6.996.tsv
routine-comparison.tsv
routine-difference-inventory.tsv
file-routine-summary.tsv
limits-dimensions-constants.tsv
include-common-summary.tsv
build-helper-comparison.tsv
command-state-changes.tsv
plot-classification.tsv
algorithm-classification.tsv
osmap-observations.tsv
manual-crosschecks.tsv
```

Raw diffs and the normalized views are in `raw-diffs/`, `normalized/`,
`fixed-form/` and `reduced/`.

## File-set statistics

| Classification | Count |
|---|---:|
| Common upstream files | 44 |
| Common and byte-identical | 19 |
| Common but changed | 25 |
| Only in 6.99 | 4 |
| Only in 6.996 | 5 |

Identical files:

```text
BLPAR.INC CIRCLE.INC PXPLOT.INC XBL.INC XDES.INC
aread.f blu.f dplot.f frplot.f getarg.f gui.f ntcalc.f
osmap.f polfit.f profil.f pxplot.f sort.f xqdes.f xutils.f
```

Changed files:

```text
PINDEX.INC PPLOT.INC XFOIL.INC
blplot.f iopol.f modify.f naca.f plutil.f pntops.f polplt.f
pplot.f spline.f userio.f xbl.f xblsys.f xfoil.f xgdes.f
xgeom.f xmdes.f xoper.f xpanel.f xplots.f xpol.f xsolve.f
xtcam.f
```

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

`SHA256SUMS` is repository-owned metadata in both directories and is excluded
from the upstream file-set counts.

## Routine statistics

| Baseline | Routines |
|---|---:|
| XFOIL 6.99 | 313 |
| XFOIL 6.996 | 318 |

| Classification | Count |
|---|---:|
| unchanged | 231 |
| changed | 76 |
| signature-changed | 5 |
| added | 6 |
| removed | 1 |

Added: `FRPLOT` (dummy), `SVGOUT`, `DXFOUT`, `SQTE`, `TECIRC`, `BLDUMP3`.
Removed: `OSMAP` from the inactive `osmap_DP.f` variant.

All 25 changed files have non-comment, non-whitespace differences. The
external `routine-difference-inventory.tsv` records per-routine signatures,
line deltas, include/COMMON/parameter changes, numeric/string changes and the
first classification.

## Important functional changes

### State, limits and ABI

- `XFOIL.INC`: `IQX` 370 -> 601, `NPX` 12 -> 20; `CLF`, `CPOLEL`, `KPOL`,
  `NKPOL`, `NTS`, `LPPORT`, `MDEF`, `PDEF`, `KDEF`, `DSSR`.
- `PINDEX.INC`: `KPTOT=4`, `KCX`, `KCY`, `KCM`, `KCDV`, element names and
  formats.
- `PPLOT.INC`: `NBX=5`, `ISX=2*NBX`, `CPOLEL`, `LSORT`, `CDLMOD(6,NPX)`.

This is a high-risk state/ABI group.

### Geometry

- `xgeom.f`: `NORM` now uses chord-vector length and returns shift/scale;
  `LEFIND` and `CANG` are more robust; new `SQTE` and `TECIRC`.
- `naca.f` and `xfoil.f`: sharp-TE `LTES` option; new `NACS` command.
- `xgdes.f`: new `SYTB`, `XCEN`, `SQTE`, `CIRC`, `NSPL`; `TGAP` gains a
  top/bottom split; `PLTAIR` gains `NTS`.

### Paneling and inviscid solver

- `xpanel.f` `QDCALC`: new sharp-TE bisector condition replacing the old
  zero-source extrapolation. This is a solver change.
- `PSILIN`, `PSWLIN` and `GGCALC` changes are comment/formatting only.

### Boundary layer

- `xblsys.f` `CFT`: derivative terms for the clamped `GRT` are corrected.
- `xbl.f`: adds `DSSR`, `MDEF`, `PDEF`, `KDEF` output fields; the solver
  result itself is unchanged.

### Numerics

- `spline.f`: explicit `REAL` declarations, `NMAX` 1000 -> 2500, and a
  functional `SEGSPLD` segment-end derivative fix.
- `xsolve.f`: `GAUSS`/`CGAUSS` arithmetic-IF rewrites are semantics-equivalent;
  `LUDCMP` workspace grows from 500 to 515.

### Defaults, commands and output

- `xfoil.f`: `NPAN` default 160 -> 240; `CTERAT` 0.15 -> 0.3; `CLF`;
  `SVG`/`DXF` exporters; minimum panel count 2.
- `xoper.f`: `BLDUMP2` density-corrected output, new `BLDUMP3`, `BLOU`
  command, extended console output.
- `xmdes.f`: `SMOO` now targets `KQSP=1`.
- `xpol.f`: append-mode writes and element-polar plumbing.

### File formats and I/O

- `iopol.f`: per-element polar columns, `CPOLEL`/`KPOL`/`NKPOL`, 256-byte
  input and 512-byte output buffers, revised header/format.
- `userio.f`: input buffers 80/130 -> 256/258 and parse limit 128 -> 256.
- `xtcam.f`: max thickness/camber output formats change.

### OSMAP

`osmap.f`, `ntcalc.f` and `frplot.f` are byte-identical. `getosfile.c` exists
only in 6.996. `osmap.dat` exists only in 6.99 and is absent from the 6.996
subset and archive. No format compatibility is proven.

### Plot-only

`blplot.f`, `polplt.f`, `pplot.f`, `plutil.f`, `xplots.f`, `pntops.f` and
`xtcam.f` changes are plot-only or plot-dominated, except the `xtcam` output
format change. These are not early product-port candidates.

## Candidate integration groups

| Group | Area | Key Fortran files/routines | Risk | Order |
|---|---|---|---|---|
| G01 | state/limits/polar metadata | `PINDEX.INC`, `XFOIL.INC`, `PPLOT.INC` | high | 1 |
| G02 | numerics robustness | `spline.f`, `xsolve.f` | medium | 2 |
| G03 | geometry normalization/robustness | `xgeom.f` NORM/LEFIND/CANG | high | 3 |
| G04 | sharp TE/NACA/GDES | `naca.f`, `xfoil.f`, `xgeom.f`, `xgdes.f` | high | 4 |
| G05 | paneling/inviscid sharp TE | `xpanel.f` QDCALC | high | 5 |
| G06 | boundary layer/output state | `xblsys.f`, `xbl.f` | medium | 6 |
| G07 | main defaults/commands | `xfoil.f`, `xoper.f` | medium-high | 7 |
| G08 | polar I/O/element data | `iopol.f`, `xpol.f` | high compatibility | 8 |
| G09 | inverse design | `xmdes.f` MDES | medium | 9 |
| G10 | input capacity | `userio.f` | low | 10 |
| G11 | plot-only | `blplot.f`, `polplt.f`, `pplot.f`, `plutil.f`, `xplots.f`, `pntops.f`, `xtcam.f` | low | defer |
| G12 | OSMAP | `osmap.f`, `ntcalc.f`, `frplot.f`, `getosfile.c` | unknown | blocked |

C++ candidate areas are indicative, not final mappings. The detailed group
table, expected behavior, required oracle-case proposals and dependencies are
in the U3 plan document and external `algorithm-classification.tsv`.

## Manual cross-checks

Five representative files were checked manually against the automatic
classification:

| File | Automatic | Manual result | Deviation |
|---|---|---|---|
| `XFOIL.INC` | changed | confirmed high-risk COMMON/state change | none |
| `xpanel.f` | changed | QDCALC definitive sharp-TE fix; PSILIN/PSWLIN/GGCALC comment-only | partial |
| `xgdes.f` | changed | mixed geometry/session/plot: new commands plus NORM/PLTAIR | none |
| `iopol.f` | changed | polar format extension with element columns | none |
| `blplot.f` | changed | plot-only m/P/K additions reading new BL arrays | none |

Additional refinements:

- `xsolve.f` GAUSS/CGAUSS are syntax-equivalent, not numerical changes.
- `userio.f` changes are input capacity only.
- `xbl.f` XIFSET is formatting-only; MRCHUE/MRCHDU add output fields.

These deviations are recorded in external `manual-crosschecks.tsv`.

## Gates

All executed gates exit 0:

- `make check-fortran-sha`: 6.99 48/48, 6.996 49/49.
- `make check-license-headers`.
- `make check-release-inventory`.
- `make check-public-docs`.
- `git diff --check`: exit 0.

No numerical test run was required because U3 changes only documentation and
reads the frozen sources. Both source manifests remain green.

## Protected data

- 6.99 sources: 48/48 unchanged.
- 6.996 upstream sources: 49/49 unchanged.
- Both binaries: unchanged.
- Goldens, references, tolerances and Windows Goldens: unchanged.
- Product version: `1.0.0`.
- No C++ product source changed.
- No Fortran source was formatted or normalized in the repository.

## Git status

- Working tree: only the two new U3 documents are untracked.
- Staged: 0.
- No commit and no push.
- HEAD: `a73122e19bb0f7e2f4001ccd639b27b16e25c3f2`.
- `origin/main`: `58bca7d17f847e1aef6b1c8d3d4e167f40f69945`.
- Ahead/behind: `0 5`.
- Tag: `v1.0.0` unchanged.
- `git diff --check`: clean.

## Publication hold

No push, force-push, fetch, tag, release or history rewrite. All local commits
remain unpushed until cumulative XFOIL.next 1.1.0 acceptance.

## Decision

`X6996-U3: PASS`

## Exact next allowed step

`X6996-U3-R1-Independent-Source-Difference-Inventory-Acceptance`.

## Independent acceptance — X6996-U3-R1

Decision: FAIL

### Findings

1. The candidate integration groups do not contain the required per-group
   oracle-case proposals. The group table in
   `03-source-difference-inventory.md` has no `Required oracle cases` field;
   only a general paragraph after the table says that existing 6.99 cases in
   the relevant categories and separately authorized 6.996 diagnostic cases
   are proposals. This is not a per-group statement and cannot be used to
   identify the required cases for G01 through G12. The U3-R1 acceptance
   requirement is therefore not met.

2. Group G07 combines defaults with command/session/output behavior:
   `NPAN=240`, `CTERAT=0.3`, `SVG`/`DXF`, `BLDUMP3` and console output are
   planned as one group. The audit requires defaults and command/state logic
   to be planned separately. G07 is not marked as too large and is not given
   an explicit later split. The same mixed grouping is repeated in the report
   summary table.

### Verified content

The following checks passed independently:

- File-set comparison: 44 common files, 19 identical, 25 changed, 4 only in
  6.99 and 5 only in 6.996. The sums are 48 and 49; `SHA256SUMS` is excluded
  as metadata. Two identical and two changed files were checked directly with
  `cmp`.
- Routine inventory: 313 routines in 6.99 and 318 in 6.996; 231 unchanged, 76
  changed, 5 signature-changed, 6 added and 1 removed. Representative
  unchanged, changed, signature-changed, new and removed routines were
  checked. The five signature changes are NACA4, NACA5, NACA, NORM and
  PLTAIR. The six added routines are FRPLOT, SVGOUT, DXFOUT, SQTE, TECIRC and
  BLDUMP3. The removed routine is OSMAP from the inactive osmap_DP.f variant.
- High-risk source findings were confirmed: IQX 370 -> 601 and NPX 12 -> 20;
  the new CLF, CPOLEL/KPOL/NKPOL, NTS, LPPORT, MDEF/PDEF/KDEF/DSSR fields;
  the geometry changes in NORM, LEFIND, CANG, SQTE and TECIRC; the QDCALC
  sharp-TE fix; the CFT derivative change; the SEGSPLD segment-end fix; the
  NPAN/CTERAT defaults; and the per-element polar columns.
- OSMAP: osmap.f, ntcalc.f and frplot.f are byte-identical; osmap.dat is
  absent from 6.996; no compatibility was claimed and no file was copied or
  generated.
- Gates: `make check-fortran-sha` (48/48 and 49/49),
  `make check-license-headers`, `make check-release-inventory`,
  `make check-public-docs` and `git diff --check` all exit 0.
- Protected data: both source baselines and both binaries are unchanged; no
  Golden, reference, tolerance or product-version change; product version is
  1.0.0.
- Git status: only the two U3 documents are untracked; staged 0; HEAD
  `a73122e19bb0f7e2f4001ccd639b27b16e25c3f2`; `origin/main`
  `58bca7d17f847e1aef6b1c8d3d4e167f40f69945`; ahead/behind `0 5`; tag
  `v1.0.0` unchanged.

### Required correction

The U3 plan and report summary need per-group oracle-case proposals and a
separation of defaults from command/session/output behavior, or G07 must be
explicitly marked as too large and given a concrete later split. No
correction was made in this audit.

### Next step

Renew `X6996-U3-R1` after the plan and report summary are corrected. No
commit and no push.

## Integration-group closure — X6996-U3-F1

Decision: PASS

This closure is append-only. The `X6996-U3-R1` `Decision: FAIL` section above
remains the historical record; the earlier `Candidate integration groups`
table is the pre-F1 summary and is not rewritten.

### Findings closed

1. Every current group has a `Required oracle cases` field. Each case names a
   profile or geometry, the operation, the operating condition or parameters,
   the results to compare and the purpose. No group relies only on a global
   oracle paragraph.
2. The former mixed G07 was split into defaults, file formats/exports and
   command/session behavior. The defaults group contains only
   `NPAN`/`CTERAT` initialization, display and persistence. `SVG`, `DXF` and
   `BLDUMP3` are only in the file/export group. Commands, menus, invalidation,
   warm starts, sequence/convergence and console output are only in the
   command/session group.

### Resulting groups

| Group | Scope | Dependencies | Risk | Order |
|---|---|---|---|---|
| G01 | state, limits and polar metadata | none | high | 1 |
| G02 | numerics robustness (`SEGSPLD`, solver limits) | G01 | medium | 2 |
| G03 | geometry normalization and robustness | G01 | high | 3 |
| G04 | sharp TE, NACA and GDES geometry | G03 | high | 4 |
| G05 | paneling and `QDCALC` | G01, G03 | high | 5 |
| G06 | boundary layer and derivatives | G01 | medium | 6 |
| G07 | defaults (`NPAN`, `CTERAT`) | G01 | medium-high | 7 |
| G08 | file formats and exports (polar, BL dump, SVG/DXF) | G01, G06 | high compatibility | 8 |
| G09 | command and session behavior | G01, G04, G08 | medium | 9 |
| G10 | inverse design | G01 | medium | 10 |
| G11 | input capacity | none | low | 11 |
| G12 | plot-only observations | G06 (read-only BL fields) | low | defer |
| G13 | OSMAP | data compatibility | unknown | blocked |

The former G08 polar I/O was folded into G08; former G09-G12 are now G10-G13.
Dependency references were updated; new cross-group links include G08 on G06,
G09 on G04/G08 for geometry commands and `BLOU` output, and G12 read-only on
G06.

### Oracle-case coverage

The per-group proposals cover state/limits, defaults, geometry and sharp TE,
paneling/`QDCALC`, boundary layer/`CFT`, polar and BL file formats,
command/session state, SVG/DXF export, `SEGSPLD`, OSMAP data provenance and
plot-only exclusions. G13 remains the explicit diagnostic case pending
data-file provenance and format compatibility. All proposals remain
non-canonical until separately authorized.

### Verified invariants

- File-set comparison is unchanged: 44 common files, 19 identical, 25
  changed, 4 only in 6.99 and 5 only in 6.996.
- Routine inventory is unchanged: 313 routines in 6.99 and 318 in 6.996; 231
  unchanged, 76 changed, 5 signature-changed, 6 added and 1 removed.
- Signature changes remain `NACA4`, `NACA5`, `NACA`, `NORM`, `PLTAIR`. Added
  routines remain `FRPLOT`, `SVGOUT`, `DXFOUT`, `SQTE`, `TECIRC` and
  `BLDUMP3`; removed routine remains `OSMAP` from inactive `osmap_DP.f`.
- High-risk facts and the OSMAP finding are unchanged.
- No Golden, reference, tolerance, product-version, Fortran, C++ or test
  change. Product version remains `1.0.0`.
- Protected source baselines and both binaries are unchanged.
- Gates exit 0: `make check-fortran-sha` (48/48 and 49/49),
  `make check-license-headers`, `make check-release-inventory`,
  `make check-public-docs` and `git diff --check`. The two untracked documents
  also pass an explicit whitespace/conflict-marker check via
  `git diff --no-index --check`.
- Git status after F1: only the two U3 documents are untracked; staged 0;
  HEAD `a73122e19bb0f7e2f4001ccd639b27b16e25c3f2`; `origin/main`
  `58bca7d17f847e1aef6b1c8d3d4e167f40f69945`; ahead/behind `0 5`; tag
  `v1.0.0` unchanged.

### Next step

`X6996-U3-R2-Focused-Source-Difference-Inventory-Acceptance`. No commit and
no push in F1.

## Focused independent acceptance — X6996-U3-R2

Decision: PASS

### Focused checks

- G01-G13 are complete and uniquely defined, with no missing or duplicate ID.
  Every group contains scope, concrete Fortran files or routines, expected
  change, risk, dependencies, concrete `Required oracle cases` and a
  recommended order.
- Required oracle cases exist for every group and cover limits/state layout,
  geometry and sharp TE, paneling/`QDCALC`, boundary layer, defaults,
  polar/file formats and exports, command/session state, `SEGSPLD`, plot-only
  observations and the OSMAP diagnostic. No generic oracle phrase is used.
  G13 remains the explicitly pending diagnostic case; G12 uses the plot-only
  exception only where shared state or a numerical producer is excluded.
- G07 contains only `NPAN`/`CTERAT` defaults, default initialization/display
  and the affected `WDEF`/`RDEF` path. It contains no SVG, DXF, BLDUMP3,
  polar format, command/session state, warm start or sequence logic.
- G08 contains the merged former polar I/O group and the relevant polar
  header/element columns, read/write/append, BLDUMP2/BLDUMP3 and density-
  corrected BL output. SVG and DXF are named in G08 Scope, Expected change and
  Required oracle cases, so their inclusion is an explicit planned scope
  decision rather than an unmarked assumption.
- G09 contains commands and menus, state invalidation, warm starts, sequences,
  convergence status and console output. It contains no default values and no
  file-format specification.
- The former G08 polar I/O is folded into G08, and former G09-G12 map to
  G10-G13. All dependency IDs exist, no group depends on itself, and the
  recommended order is consistent: G01-G11 in order, G12 deferred and G13
  blocked. Old IDs remain only in the historical pre-F1 report table and the
  explanatory F1 mapping; no stale dependency reference remains.
- File and routine statistics are unchanged: 44 common files, 19 identical,
  25 changed, 4 only in 6.99 and 5 only in 6.996; 313 routines in 6.99 and 318
  in 6.996, with 231 unchanged, 76 changed, 5 signature-changed, 6 added and
  1 removed.
- Report chronology is intact: `X6996-U3-R1: FAIL` remains unchanged before
  `X6996-U3-F1: PASS`, and this R2 closure is appended after both.

### Gates and repository state

- Gates exit 0: `make check-fortran-sha` (48/48 and 49/49),
  `make check-license-headers`, `make check-release-inventory`,
  `make check-public-docs` and `git diff --check`. Both untracked documents
  pass the explicit whitespace/conflict-marker check.
- Before the R2 commit: only the two U3 documents are untracked; staged 0;
  HEAD `a73122e19bb0f7e2f4001ccd639b27b16e25c3f2`; `origin/main`
  `58bca7d17f847e1aef6b1c8d3d4e167f40f69945`; ahead/behind `0 5`; tag
  `v1.0.0` unchanged.
- Publication hold remains in force: no push before cumulative XFOIL.next
  1.1.0 acceptance.

### Next step

Allowed local, path-limited commit:

`Document XFOIL 6.99 to 6.996 source differences`

Paths:

- `docs/xfoil-6.996/03-source-difference-inventory.md`
- `docs/xfoil-6.996/03-source-difference-inventory_report.md`

After the commit, `main` must be six commits ahead of `origin/main` with a
clean worktree; no push. The next workflow step is
`X6996-U4-Oracle-Behavior-Inventory`.
