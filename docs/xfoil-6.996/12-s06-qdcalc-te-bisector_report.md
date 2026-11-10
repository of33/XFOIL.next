# XFOIL 6.996 — S06 QDCALC TE-bisector and influence-row report

Status: PASS - `X6996-S06` and `X6996-S06-R1` accepted. Decision:
`X6996-S06: PASS`.

## Starting state

- HEAD before S06: `dcd93d99d6f23f36f7a30b9853b5a81ead958696`.
- Ahead/behind before S06: `0 17`.
- Tag `v1.0.0` unchanged.
- Working tree before S06: clean, 0 staged.
- Audit data: external only,
  `.audit-xfoil-next/xfoil-6.996/s06/`.

## Fortran diff

Diff:
`.audit-xfoil-next/xfoil-6.996/s06/xpanel.diff`.

| Case | XFOIL 6.99 | XFOIL 6.996 | Classification |
|---|---|---|---|
| Open TE | row `N` wake columns from `PSWLIN` | unchanged | no change |
| Sharp TE | `BIJ(N,J) = 0` for `J=N+1..N+NW` | TE-bisector control point, `BIJ(N,J) = -DQDM(J)` | changed QDCALC row |

The remaining `xpanel.f` diff is limited to `END !` comments on `PSILIN`,
`PSWLIN` and `GGCALC`. No other routine changes numerically.

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

`BWT=0.1` is shared with `GGCALC`. The Kutta row `N+1` remains
`BIJ(N+1,J)=0` for all wake-source columns; only row `N` changes.

## C++ mapping

| Fortran | C++ | Change |
|---|---|---|
| `QDCALC` sharp row | `inviscid::qdCalc` in `src/core/inviscid_core/Influence.cpp` | TE-bisector `PSWLIN` row replaces old zero row |
| `GGCALC`/`QDCALC` `BWT` | anonymous-namespace `kBwt = 0.1` | one shared constant |
| `ATANC` | `math::atanC` | same TE continuation-angle rule as `GGCALC` |
| `PSWLIN(0,...)` | `pswLin` with `nodeWake=false` | `-dqdm` copied into wake columns of `BIJ` row `N` |
| `XTE/YTE` | `PanelGeometry::xte/yte` | no new state |

The new row is built before the wake-column `BAKSUB` loop; the existing
`DIJ` writeback and wake-velocity blocks are unchanged. Open-TE QDCALC values
remain unchanged. No BL, session, command, default, format or version path is
touched.

## Harness H-S06-QDCALC

External source/output:
`.audit-xfoil-next/xfoil-6.996/s06/harness/`.

The harness links canonical 6.996 routines with the oracle flags:

```text
-fdefault-real-8 -fdefault-double-8
```

Inputs: NACA 0012/2412 from `NACA4` with `LTES=false/true`, S05 `SQTE` 2412,
PANGEN, GGCALC, SPECAL at alpha = 4 degrees, XYWAKE and QDCALC. All cases use
`MATYP=1/RETYP=1`, `MINF1=0`, `REINF1=3e6`.

| Case | N/NW | SHARP | DIJ(N,1) | DIJ(N,N) | DIJ(N,N+1) | DIJ(N,N+NW) | DIJ(N-1,N+1) | CL | CM |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| NACA 0012 open | 160/23 | 0 | 4.9511802837259552e1 | -4.9511802837234498e1 | -5.6733871816692449e1 | 1.3517726396010582e-1 | -6.1697254551623866e0 | 4.8285065614687539e-1 | -5.5744477661467482e-3 |
| NACA 0012 sharp | 160/23 | 1 | 4.3844547379763831e1 | -4.3844547379772379e1 | -4.5323097397452877e1 | 1.3385304304669707e-1 | -1.1951145857019286e1 | 4.8231955315951469e-1 | -5.2707106554811273e-3 |
| NACA 2412 open | 160/23 | 0 | 5.0433303005528842e1 | -4.8828268896632316e1 | -5.6863558887090569e1 | 1.3517922326693399e-1 | -5.8828113825062811e0 | 7.3757352558247768e-1 | -6.1626823037021876e-2 |
| NACA 2412 sharp | 160/23 | 1 | 4.3885551712752530e1 | -4.4038131848513153e1 | -4.5424339104302106e1 | 1.3380918528357205e-1 | -1.1893395117425696e1 | 7.3638631518672759e-1 | -6.1103475019161156e-2 |
| SQTE 2412 | 160/23 | 0 | 4.9558473600283278e1 | -4.9708015380633093e1 | -5.6868002138398936e1 | 1.3513136354654826e-1 | -6.1288783928333039e0 | 7.3845992358355028e-1 | -6.1833250844121758e-2 |

The sharp-TE row is non-zero and the old extrapolation row is removed. The
open-TE and SQTE cases keep the 6.99 row behavior. C++ `qdCalc` values match
the double-precision harness at the recorded precision.

## Tests

New:

- `unit.QdcalcSharpTeRowNaca0012`: sharp flag, QDCALC row `N` at profile and
  wake columns, row `N-1` control, final CL/CM.
- `unit.QdcalcSharpTeRowNaca2412`: same for cambered sharp TE.
- `unit.QdcalcOpenTeRowNaca2412Unchanged`: open-TE 6.99 row regression.
- `unit.QdcalcSqte2412RowUnchanged`: S05 SQTE profile/open-TE row regression.

The test source is `tests/core/QdcalcSharpTeTests.cpp`; it uses the existing
portable `kDijMatrixValue` comparison for Windows portability. No tolerance
was increased and no golden/reference file was changed.

## Linux matrix

Toolchain: GCC 13.3.0, Clang 18.1.3, Valgrind 3.26.0.

| Check | Result |
|---|---|
| GCC 13 Debug `make test` | 1989/1989 pass |
| GCC 13 Release | 1989/1989 pass |
| Clang 18 Debug | 1989/1989 pass |
| Clang 18 Release | 1989/1989 pass |
| `make test-core` | 429/429 pass |
| `make test-unit` | 1689/1689 pass |
| `make test-integration` | 245/245 pass |
| `make test-regression` | 5/5 pass |
| `make test-batch` | 47/47 pass |
| `make test-sanitize` (ASan/UBSan) | 1989/1989 pass |
| `make test-valgrind` | pass, no definite leaks |
| focused S06 Valgrind (5 tests) | pass, no definite leaks |
| `make test-reference` | 236/236 pass |
| `make test-reference-determinism` | 236/236 pass |
| `make test-harnesses` | 16 families, 0 failures |
| Gates | dependencies, Fortran SHA, release inventory, license, public docs, core/session link checks pass |

No `src/app/cli/**` change. Existing QDCALC 6.99 regression remains green.

## Windows focused run

Source: user-provided Windows 11 / MSYS2 UCRT64 console output for the single
focused S06 process:

```bash
make -j4 build/debug/xfoil_tests
./build/debug/xfoil_tests unit.Qdcalc
```

```text
[ RUN      ] unit.QdcalcSharpTeRowNaca0012
[       OK ] unit.QdcalcSharpTeRowNaca0012
[ RUN      ] unit.QdcalcSharpTeRowNaca2412
[       OK ] unit.QdcalcSharpTeRowNaca2412
[ RUN      ] unit.QdcalcOpenTeRowNaca2412Unchanged
[       OK ] unit.QdcalcOpenTeRowNaca2412Unchanged
[ RUN      ] unit.QdcalcSqte2412RowUnchanged
[       OK ] unit.QdcalcSqte2412RowUnchanged
[ RUN      ] unit.QdcalcWakeDijClarkY
[       OK ] unit.QdcalcWakeDijClarkY
[ RUN      ] unit.QdcalcWakeDijNaca4412
[       OK ] unit.QdcalcWakeDijNaca4412
0 failure(s) in 6 test(s)
```

The process covers both existing QDCALC regressions and all four new S06
tests. No full Windows matrix was run for S06.

## Changed paths

- `src/core/inviscid_core/Influence.cpp`
- `tests/core/QdcalcSharpTeTests.cpp`
- `Makefile` (test source registration)
- `docs/xfoil-6.996/12-s06-qdcalc-te-bisector.md`
- `docs/xfoil-6.996/12-s06-qdcalc-te-bisector_report.md`
- external audit harness:
  `.audit-xfoil-next/xfoil-6.996/s06/harness/`

No Fortran, golden, canonical reference, default, command or version file was
changed.

## Acceptance checklist

- [x] Fortran diff limited to QDCALC sharp-TE row.
- [x] C++ row uses shared `BWT`, ATANC, TE bisector and `PSWLIN`.
- [x] Open-TE/6.99 row behavior retained.
- [x] Sharp NACA and SQTE tests pass.
- [x] Linux full/compiler/sanitizer/Valgrind/reference/harness/gate matrix green.
- [x] Focused Windows S06 process green.
- [x] `X6996-S06-R1-Focused-QDCALC-TE-Bisector-Acceptance`.
- [x] Create the single local S06 commit.

## Focused acceptance - X6996-S06-R1-Focused-QDCALC-TE-Bisector-Acceptance

Decision: **PASS**

The focused review confirms:

- the 6.996 `xpanel.f` difference is limited to the `QDCALC` sharp-TE row
  and non-numerical `END !` comments;
- `qdCalc` uses the shared `BWT=0.1`, `math::atanC`, the TE-bisector control
  point and `pswLin` before the wake-column `BAKSUB`;
- open-TE and S05 SQTE rows retain their 6.99 behavior;
- sharp NACA 0012/2412 row values match the double-precision
  H-S06-QDCALC harness;
- focused C++ tests, Linux Debug/Release GCC/Clang, ASan/UBSan, Valgrind,
  reference, determinism, harnesses and gates are green;
- the single focused Windows process passed `6/6`;
- no Fortran source/binary, patch, canonical reference, Golden, CLI/command,
  default, format, product version or banner changed.

This report is committed with the single local S06 commit. No push, new tag
or release is permitted.

## Commit

```text
Integrate XFOIL 6.996 QDCALC trailing-edge row
```
