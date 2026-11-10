# S06 - QDCALC TE-bisector and influence row

Status: PASS - `X6996-S06` accepted.

## Slice contract from U6

- Slice ID: `S06`.
- U3 group: G05 paneling and QDCALC.
- Scope: `QDCALC` TE-bisector control point and influence matrix; no BL
  coupling.
- Fortran routines: `xpanel.f`.
- C++ area: Core paneling/inviscid.
- Dependencies: `S01`, `S03`, `S04`.
- Risk: high.
- Oracle: `a02`, `a03`, `a19`, `a22`-`a24`, `H-S06-QDCALC`.
- 6.99 regressions: `a02`, `a03`, `a19`.
- Expected files: paneling C++ and harness; no Fortran/Golden.
- Non-goals: BL/viscous mapping, commands/session behavior, defaults,
  NACA/GDES kernel changes, file/polar formats, version/banner changes,
  global tolerance changes.
- Commit boundary: one bounded local commit for QDCALC and focused tests.

## Pre-analysis

Diff:
`.audit-xfoil-next/xfoil-6.996/s06/xpanel.diff`.

The 6.99 -> 6.996 `xpanel.f` difference is limited to the `QDCALC` sharp-TE
row and `END !` comments on `PSILIN`, `PSWLIN` and `GGCALC`. No other
`xpanel.f` routine changes numerically.

| Case | XFOIL 6.99 | XFOIL 6.996 |
|---|---|---|
| Open TE | wake-source columns of row `N` come from `PSWLIN` | unchanged |
| Sharp TE | `BIJ(N,J) = 0` for `J=N+1..N+NW` | zero velocity along TE bisector at an internal control point |

6.996 sharp-TE row:

```fortran
BWT = 0.1
AG1 = ATAN2(-YP(1),-XP(1))
AG2 = ATANC(YP(N),XP(N),AG1)
ABIS = 0.5*(AG1+AG2)
CBIS = COS(ABIS)
SBIS = SIN(ABIS)
DS1 = SQRT((X(1)-X(2))**2 + (Y(1)-Y(2))**2)
DS2 = SQRT((X(N)-X(N-1))**2 + (Y(N)-Y(N-1))**2)
DSMIN = MIN(DS1,DS2)
XBIS = XTE - BWT*DSMIN*CBIS
YBIS = YTE - BWT*DSMIN*SBIS
CALL PSWLIN(0,XBIS,YBIS,-SBIS,CBIS,PSI,QBIS)
DO J=N+1,N+NW
  BIJ(N,J) = -DQDM(J)
ENDDO
```

`BWT=0.1` must remain identical to `GGCALC`; both use the same control point
distance ahead of the sharp TE. The Kutta row `N+1` remains
`BIJ(N+1,J)=0` for all wake-source columns.

## C++ mapping

| Fortran | C++ | Change |
|---|---|---|
| `QDCALC` row `N` sharp branch | `inviscid::qdCalc` in `Influence.cpp` | replace the old zero row with the TE-bisector `PSWLIN` row |
| `GGCALC` control-point constant | shared anonymous-namespace `kBwt = 0.1` | keep both kernels on one constant |
| `ATANC` | `math::atanC` | same continuation-angle rule as `GGCALC` |
| `XTE/YTE`, `X/Y/XP/YP` | `PanelGeometry::xte/yte`, `x/y/xp/yp` | no new state |
| `PSWLIN(0,...)` | `pswLin` with `nodeWake=false` | source influence at the bisector point |

The row is computed before `BAKSUB`; after the linear solve, the usual
`DIJ` writeback and wake-velocity blocks are unchanged. Open-TE behavior is
unchanged. No CLI, session, BL or default path is touched.

## Harness H-S06-QDCALC

External source/output:
`.audit-xfoil-next/xfoil-6.996/s06/harness/`.

The harness links the canonical 6.996 routines with the double-precision
Fortran flags used by the oracle:

```text
-fdefault-real-8 -fdefault-double-8
```

Cases:

- NACA 0012 open TE.
- NACA 0012 sharp TE.
- NACA 2412 open TE.
- NACA 2412 sharp TE.
- S05 SQTE 2412 profile.

For each case the harness records the panel count, wake count, `SHARP`,
`XTE/YTE`, the TE-bisector control point, `DIJ` row `N` at selected
profile/wake columns, row `N-1` as a control, and the final inviscid
`CL`/`CM`.

## Acceptance

- Focused C++ tests and H-S06 values pass.
- Existing QDCALC 6.99 regression remains green.
- Linux full/compiler/sanitizer/Valgrind/reference/harness/gate matrix green.
- One focused Windows S06 process.
- R1 acceptance before the local commit.
