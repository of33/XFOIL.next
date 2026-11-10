# XFOIL 6.996 — S05 GDES geometry kernels report

Status: PASS - `X6996-S05` and `X6996-S05-R1` accepted. Decision:
`X6996-S05: PASS`.

## Starting state

- HEAD before S05: `e741e1eebc9dd17a5de8f1a4aa560f79179eb8c1`.
- Ahead/behind before S05: `0 16`.
- Tag `v1.0.0` unchanged.
- Working tree before S05: clean, 0 staged.
- Audit data: external only,
  `.audit-xfoil-next/xfoil-6.996/s05/`.

## Fortran diff

Diffs:

- `.audit-xfoil-next/xfoil-6.996/s05/xgeom.diff`
- `.audit-xfoil-next/xfoil-6.996/s05/xgdes.diff`

| Kernel | XFOIL 6.99 | XFOIL 6.996 | Classification |
|---|---|---|---|
| `SQTE` | absent | new `SQTE(X,XS,Y,YS,S,N,XNEW,YNEW)` | new xgeom kernel |
| `TECIRC` | absent | new `TECIRC(...,NHCIRC)`; calls `SQTE` | new xgeom kernel |
| `CIRC` | absent | GDES dispatcher calls `TECIRC(...,15)` | command only; not registered |
| `SYTB` | absent | inline GDES top/bottom y-scaling block | new non-graphical GDES kernel |
| `TGAP` | `(RINPUT,NINPUT)`, gap+DOC | same plus `FTB` third argument | changed xgdes kernel |

TGAP split:

```fortran
IF(SB(I).LE.SBLE) THEN
  XB(I) = XB(I) + 0.5*(1.0+FTB)*DGAP*XOC*TFAC*DXU
  YB(I) = YB(I) + 0.5*(1.0+FTB)*DGAP*XOC*TFAC*DYU
ELSE
  XB(I) = XB(I) - 0.5*(1.0-FTB)*DGAP*XOC*TFAC*DXU
  YB(I) = YB(I) - 0.5*(1.0-FTB)*DGAP*XOC*TFAC*DYU
ENDIF
```

`FTB=0` is algebraically the 6.99 0.5/0.5 split. `CIRC` contains no separate
geometry formula; `TECIRC` is the kernel.

## S05a/S05b boundary

- **S05a**: `SQTE`, `TECIRC` (CIRC kernel capability), `SYTB`.
- **S05b**: `TGAP` split geometry.

The split follows the source structure: SQTE/TECIRC are new xgeom routines and
SYTB is a new inline xgdes geometry block; TGAP is an independent existing
xgdes routine changed only by `FTB`.

No command, menu, prompt, help, redo/bang or CLI golden was activated.

## C++ mapping

| Fortran | C++ routine | Public effect |
|---|---|---|
| `SQTE` | `geometry_design::squareTrailingEdge` | local SQTE result, then respline/GEOPAR |
| `TECIRC` | `geometry_design::addTrailingEdgeCircle` | SQTE + TE circle; adds `2*NHCIRC` points |
| `CIRC` | none | intentionally not registered |
| `SYTB` | `geometry_design::scaleTopBottomY` | Y-only top/bottom scaling |
| `TGAP` | `geometry_design::setTrailingEdgeGap` | gap/DOC/FTB split |

All kernels are in `GeometryDesignOps`; no CLI dependency or duplicate NACA /
GDES implementation was added.

## Error and atomicity contract

- SQTE constructs local `XNEW/YNEW`; singular/degenerate systems and
  non-finite results fail before mutation. Iteration non-convergence is
  reported as `SqteResult::converged=false` while preserving the last
  canonical iterate.
- TECIRC rejects negative `NHCIRC` and capacity overflow
  (`N + 2*NHCIRC > IBX=2404`) before mutation. `NHCIRC=0` is the SQTE-only
  limit.
- SYTB rejects a degenerate chord (`XTE == XLE`) and non-finite results before
  assigning Y.
- TGAP rejects a degenerate chord squared and non-finite results before
  assigning X/Y; `FTB` is not silently clamped (matching the Fortran).
- No Teilmutation occurs in the atomicity tests.

## Harness H-S05-GDES

Source/output:
`.audit-xfoil-next/xfoil-6.996/s05/harness/`.

The direct SQTE/TECIRC routines are linked from 6.996 xgeom.f. SYTB and TGAP
reproduce the non-prompt/non-plot xgdes blocks in the external harness. The
upstream TECIRC debug `tbis hang` line is filtered; no numerical line is
removed.

| Case | N | TE gap | max thickness @ x | max camber @ x |
|---|---:|---:|---:|---:|
| SQTE 2412 open | 245 | 2.5092712052974544e-3 | 0.12003228622287992 @ 0.29757707521376275 | 0.020004123718118258 @ 0.40310795477497668 |
| SQTE 0012 open | 245 | 2.5199999999999572e-3 | 0.12003207898370065 @ 0.29749550101842870 | -5.8980598183211441e-17 @ 0.014776720928247184 |
| SQTE 2412 already sharp | 245 | -3.3081498495382879e-17 | 0.12003450290498287 @ 0.29749566856972637 | 0.019999452316196244 @ 0.40318958149393869 |
| TECIRC 2412 NHCIRC=15 | 275 | 0.0 | 0.12003250006408106 @ 0.29756839319449008 | 0.020031976534593699 @ 0.40311633144197262 |
| TECIRC 2412 NHCIRC=0 | 245 | 2.5092712052974544e-3 | 0.12003228622287992 @ 0.29757707521376275 | 0.020004123718118258 @ 0.40310795477497668 |
| SYTB 0012 top=1.2 bot=0.8 | 245 | 2.5199999999999554e-3 | 0.12003208056299047 @ 0.29749734811139866 | 0.011762043003052437 @ 0.29749734811139866 |
| SYTB 2412 top=1.2 bot=0.8 | 245 | 2.5199999999999554e-3 | 0.12758063823477089 @ 0.30759505985623964 | 0.031447933147663976 @ 0.38127898369724716 |
| TGAP 2412 default | 245 | 1.9999999999999997e-2 | 0.12107631312966896 @ 0.30759573547760188 | 0.019999449541470843 @ 0.40318957888199458 |
| TGAP 2412 FTB=+0.5 | 245 | 2.0000000000902178e-2 | 0.12106444137772734 @ 0.30705818066329105 | 0.018744095450085624 @ 0.39166758098926580 |
| TGAP 2412 FTB=-0.5 | 245 | 1.9999999999097816e-2 | 0.12108935006609581 @ 0.30705789260734939 | 0.021286747120950616 @ 0.41478642123598430 |
| TGAP 2412 zero gap | 245 | 2.1684043449710089e-19 | 0.11928239451196850 @ 0.29749566589122634 | 0.019999452773123396 @ 0.40318958249816600 |
| TGAP 2412 DOC=0 | 245 | 1.9999999999999997e-2 | 0.12003208396171303 @ 0.29749568756140099 | 0.019999451013511933 @ 0.40318957531719563 |

Selected interior points, TE coordinates, LE and orientation are checked by
`unit.GeometryDesignKernelContracts`; the exact P1/P2/P3 harness values are
preserved in the external output. SQTE on symmetric 0012 leaves the contour
unchanged; SQTE on an already sharp TE leaves the zero gap unchanged.

## Tests

New/updated:

- `unit.GeometryDesignKernelContracts` covers SQTE, TECIRC/CIRC kernel,
  SYTB and TGAP, including point counts, TE coordinates/gap, selected
  interior points, thickness/camber and the 399-adjacent NHCIRC=0 limit.
- `unit.GeometryDesignKernelAtomicity` covers negative NHCIRC, capacity
  overflow, degenerate SQTE arc length, non-finite SYTB/TGAP inputs and
  unchanged buffers.
- Existing `unit.GeometryDesignGdesShape2412Sequence` and
  `unit.GeometryDesignGdesOps2412Sequence` remain green, including the
  `FTB=0` TGAP compatibility contract and the S03/S04 normal path.

No tolerance was increased.

## Linux matrix

Toolchain: GCC 13.3.0, Clang 18.1.3, Valgrind 3.26.0.

| Check | Result |
|---|---|
| GCC 13 Debug `make test` | 1985/1985 pass |
| GCC 13 Release | 1985/1985 pass |
| Clang 18 Debug | 1985/1985 pass |
| Clang 18 Release | 1985/1985 pass |
| `make test-core` | 425/425 pass |
| `make test-unit` | 1685/1685 pass |
| `make test-integration` | 245/245 pass |
| `make test-regression` | 5/5 pass |
| `make test-batch` | 47/47 pass |
| `make test-sanitize` (ASan/UBSan) | 1985/1985 pass |
| `make test-valgrind` | pass, no definite leaks |
| focused S05 Valgrind (4 tests) | pass, no definite leaks |
| `make test-reference` | 236/236 pass |
| `make test-reference-determinism` | 236/236 pass |
| `make test-harnesses` | 16 families, 0 failures |
| Gates | dependencies, Fortran SHA, release inventory, license, public docs, core/session link checks pass |

No `src/app/cli/**` change. Existing `a09`-`a11` and S03/S04 regressions
remain green.

## Windows focused run

Source: user-provided Windows 11 / MSYS2 UCRT64 console output for the single
focused S05 process recorded in
`.audit-xfoil-next/xfoil-6.996/s05/windows-s05-commands.txt`:

```bash
make -j4 build/debug/xfoil_tests
./build/debug/xfoil_tests unit.GeometryDesignKernel
```

```text
[ RUN      ] unit.GeometryDesignKernelContracts
[       OK ] unit.GeometryDesignKernelContracts
[ RUN      ] unit.GeometryDesignKernelAtomicity
[       OK ] unit.GeometryDesignKernelAtomicity
0 failure(s) in 2 test(s)
```

The process covers SQTE, TECIRC/CIRC kernel, SYTB, TGAP split geometry,
capacity/atomicity and the NHCIRC=0 limit. No full Windows matrix was run for
S05.

## Changed paths

- `src/core/geometry_design/GeometryDesignOps.cpp`
- `include/xfoil/core/geometry_design/GeometryDesignOps.h`
- `tests/core/GeometryDesignTests.cpp`
- `docs/xfoil-6.996/11-s05-gdes-geometry-kernels.md`
- `docs/xfoil-6.996/11-s05-gdes-geometry-kernels_report.md`

No change to `fortran-src/**`, `fortran-bin/**`, `patches/**`,
`tests/golden/**`, canonical `tests/fortran-reference/**`, formats,
CLI/command registration, product version or banner.

## Git state and publication

The local S05 commit is created with this accepted scope:

```text
Integrate XFOIL 6.996 GDES geometry kernels
```

No push, new tag or release before the cumulative XFOIL.next 1.1.0 acceptance.

## S05 checklist

- [x] Fortran SQTE/TECIRC/CIRC/SYTB/TGAP diff and kernel classification.
- [x] S05a/S05b boundary fixed and documented.
- [x] C++ kernels implemented without CLI/command registration.
- [x] Atomicity/capacity/error paths implemented and tested.
- [x] External H-S05-GDES harness and selected point values.
- [x] Focused `GeometryDesignKernelContracts` and atomicity tests.
- [x] Linux full/compiler/sanitizer/Valgrind/reference/harness/gate evidence.
- [x] One focused Windows S05 process.
- [x] `X6996-S05-R1-Focused-GDES-Geometry-Kernel-Acceptance`.
- [x] Create the single local S05 commit.

## Focused acceptance - X6996-S05-R1-Focused-GDES-Geometry-Kernel-Acceptance

Decision: **PASS**

The focused review confirms:

- SQTE/TECIRC are mapped from the new 6.996 xgeom kernels; CIRC remains
  dispatcher-only and is not registered;
- SYTB is mapped from the inline GDES geometry block and leaves X unchanged;
- TGAP uses the 6.996 `FTB` split with `FTB=0` preserving the 6.99 contract;
- results are validated before buffer mutation; negative NHCIRC, capacity
  overflow, degenerate arc length/chord and non-finite inputs are rejected
  atomically;
- TECIRC `NHCIRC=15` produces 275 points and `NHCIRC=0` is the SQTE limit;
- focused values come from the external H-S05-GDES harness and no tolerance
  was increased;
- Linux Debug/Release GCC/Clang, ASan/UBSan, Valgrind, reference,
  determinism, harnesses and gates are green;
- the single focused Windows process passed `2/2`;
- no CLI/command, Fortran source/binary, patch, canonical reference, Golden,
  format, product version or banner changed.

This report is committed with the single local S05 commit. No push, new tag
or release is permitted.

## Commit

```text
Integrate XFOIL 6.996 GDES geometry kernels
```
