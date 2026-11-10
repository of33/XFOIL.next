# S05 - GDES geometry kernels

Status: PASS - `X6996-S05` accepted.

## Slice contract from U6

- Slice ID: `S05`.
- U3 group: G04 sharp TE, NACA and GDES geometry.
- Scope: `SQTE`, `TECIRC`/`CIRC`, `SYTB`, `TGAP` split geometry; kernels only.
- Fortran routines: `xgeom.f`, `xgdes.f`.
- C++ area: Core Geometry Design.
- Dependency: `S03`; `S04` supplies the sharp-NACA test input.
- Risk: high.
- Oracle: `a24`, `a25`, `a26`, `H-S05-TGAP`.
- 6.99 regressions: `a09`-`a11`.
- Expected files: GDES geometry C++ and harness; no command registration.
- Non-goals: command/menu/help/prompt/redo/bang logic, CLI goldens,
  QDCALC/paneling, defaults, file/polar formats, version/banner, global
  tolerance changes.
- Commit boundary: one or two bounded local commits for S05a/S05b.

## Pre-analysis

Diffs:
`.audit-xfoil-next/xfoil-6.996/s05/xgeom.diff`,
`.audit-xfoil-next/xfoil-6.996/s05/xgdes.diff`.

| Kernel | Signature | Inputs | Outputs | Mutation | Capacity | Error contract |
|---|---|---|---|---|---|---|
| `SQTE` | `SQTE(X,XS,Y,YS,S,N,XNEW,YNEW)` | spline contour + arc length | new contour | caller copies `XNEW/YNEW`; kernel itself local | N unchanged | singular/degenerate TE system must not mutate |
| `TECIRC` | `TECIRC(X,XS,Y,YS,S,N,XNEW,YNEW,NHCIRC)` | SQTE contour + `NHCIRC` | squared TE plus TE circle | caller copies; kernel adds `2*NHCIRC` points | `N + 2*NHCIRC <= IBX=2404` | negative/capacity/degenerate circle rejected atomically |
| `CIRC` | GDES dispatcher | command | calls `TECIRC(...,15)` | command only | inherited | no kernel error contract |
| `SYTB` | inline GDES block | buffer, `YTFAC`, `YBFAC` | new Y values | X unchanged; respline/GEOPAR | N unchanged | degenerate chord invalid |
| `TGAP` | `TGAP(RINPUT,NINPUT)` | gap, DOC, FTB | split TE-gap geometry | X/Y change; respline/GEOPAR | N unchanged | degenerate chord/non-finite result invalid |

### Kernel classification

- `SQTE`: new geometry kernel in `xgeom.f`.
- `TECIRC`: new geometry kernel in `xgeom.f`; it calls `SQTE` first.
- `CIRC`: command/dispatcher only; no separate geometry formula beyond
  `TECIRC`.
- `SYTB`: new non-graphical geometry block inline in the GDES dispatcher.
- `TGAP`: existing kernel changed by the third argument `FTB`:
  upper side factor `0.5*(1+FTB)`, lower side `0.5*(1-FTB)`.
  `FTB=0` is algebraically the 6.99 contract.

### S03/S04 dependencies

- S03 `LEFIND`, `NORM`, `CANG`, `GEOPAR` are used by GDES respline/metrics.
- S04 `NACA4(...,LTES)` supplies a sharp-TE input profile for SQTE tests.
- S01 capacity is `IBX=2404`; TECIRC must check before extending.

## Internal stages

### S05a - sharp-TE and symmetry kernels

- `SQTE`
- `TECIRC` (and the `CIRC` kernel capability; no command registration)
- `SYTB`

### S05b - TGAP split geometry

- `TGAP` with `FTB` top/bottom split
- no command/prompt registration
- no default change

The Fortran diff supports this split: SQTE/TECIRC are new xgeom kernels and
SYTB is a new inline xgdes geometry block; TGAP is an independent existing
xgdes routine changed only by the split parameter.

## C++ mapping

| Fortran | C++ routine | Callers/tests | S05 change |
|---|---|---|---|
| `SQTE` | `geometry_design::squareTrailingEdge` | new focused tests | new buffer kernel |
| `TECIRC` | `geometry_design::addTrailingEdgeCircle` | new focused tests | new buffer kernel |
| `CIRC` | none | future command slice | not registered |
| `SYTB` | `geometry_design::scaleTopBottomY` | new focused tests | new buffer kernel |
| `TGAP` | `geometry_design::setTrailingEdgeGap` | existing/new tests | `ftb` argument, default `0.0` |

No CLI dependency is introduced. Results are built locally and committed
only after validation; capacity and degenerate cases leave the buffer
unchanged.

## Harness H-S05-GDES

External source/output:
`.audit-xfoil-next/xfoil-6.996/s05/harness/`.

Cases:

- SQTE: NACA 2412 open, NACA 0012 open, NACA 2412 already sharp.
- TECIRC: NACA 2412 with `NHCIRC=15` and `NHCIRC=0`.
- SYTB: NACA 0012 and 2412 with top/bottom factors `1.2/0.8`.
- TGAP: 2412 default split, `FTB=+0.5`, `FTB=-0.5`, zero gap, `DOC=0`.

The TECIRC harness filters the upstream debug `tbis hang` line; the C++
kernel emits no debug text.

## Acceptance

- Focused kernel/atomicity tests and H-S05 values pass.
- Existing GDES 6.99 regressions `a09`-`a11` remain green.
- Linux full/compiler/sanitizer/Valgrind/reference/harness/gate matrix green.
- One focused Windows S05 process.
- R1 acceptance before the local commit.
