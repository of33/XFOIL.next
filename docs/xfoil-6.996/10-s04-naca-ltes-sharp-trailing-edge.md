# S04 - NACA LTES and sharp-trailing-edge kernel

Status: PASS - `X6996-S04` accepted.

## Slice contract from U6

- Slice ID: `S04`.
- U3 group: G04 sharp TE, NACA and GDES geometry.
- Scope: `NACA4`/`NACA5` `LTES`, `NACA` signature, sharp-TE NACA
  generation; `NACS` kernel only.
- Fortran routines: `naca.f`, `xfoil.f` `NACA`.
- C++ area: core airfoil generation.
- Dependency: `S03`.
- Risk: high.
- Oracle: `H-S04-NACA-LTES`; `a22`, `a23` only after S15.
- 6.99 regressions: `a02`, `a03`.
- Expected files: NACA C++ and harness; no command registration yet.
- Non-goals: `NACS` CLI/menu registration, `SQTE`/`TECIRC`, GDES changes,
  QDCALC, defaults, file/polar formats, version/banner, global tolerances.
- Commit boundary: one local commit for the NACA kernel and focused tests.

## Scope decision: NACA versus NACS

The 6.996 sources use one `NACA(IDES1,LTES)` wrapper:

- existing `NACA` command: `CALL NACA(IINPUT(1),.FALSE.)`;
- new `NACS` command: `CALL NACA(IINPUT(1),.TRUE.)`.

`LTES` only changes the x argument used by the thickness distribution:

```fortran
IF(LTES) THEN
  X = XX(I) * XTES
ELSE
  X = XX(I)
ENDIF
```

`XX`, camber `YC`, buffer ordering and normal (`LTES=.FALSE.`) output are
unchanged. The existing NACA command therefore keeps its 6.99 open-TE
contract.

The new `NACS` command dispatch is not registered in S04. The `NACA`
subroutine signature and `LTES=.TRUE.` kernel path are prepared, but no menu,
help, prompt, redo/bang or CLI golden is touched.

## Fortran pre-analysis

Compared files:

- `fortran-src/xfoil-6.99/naca.f`
- `fortran-src/xfoil-6.996/naca.f`
- `fortran-src/xfoil-6.99/xfoil.f`
- `fortran-src/xfoil-6.996/xfoil.f`

Diffs:
`.audit-xfoil-next/xfoil-6.996/s04/naca.diff`,
`.audit-xfoil-next/xfoil-6.996/s04/xfoil.diff`.

| Routine/contract | XFOIL 6.99 | XFOIL 6.996 | Effect |
|---|---|---|---|
| `NACA4` signature | `(IDES,XX,YT,YC,NSIDE,XB,YB,NB,NAME)` | `(...,NAME,LTES)` | explicit sharp-TE flag |
| `NACA5` signature | `(IDES,XX,YT,YC,NSIDE,XB,YB,NB,NAME)` | `(...,NAME,LTES)` | explicit sharp-TE flag |
| sharp-TE constant | none | `XTES = 1.00893041136514270931482193883054` | thickness argument at TE |
| thickness argument | `YT(XX)` | `YT(XX*XTES)` iff `LTES` | TE gap closes when true |
| camber/buffer | `YC(XX)`, buffer from `XX/YC/YT` | unchanged | no camber/order change |
| `NACA` wrapper | `NACA(IDES1)` | `NACA(IDES1,LTES)` | caller selects TE contract |
| existing `NACA` command | calls wrapper without flag | passes `.FALSE.` | open-TE behavior retained |
| new `NACS` command | absent | passes `.TRUE.` | kernel capability only in S04 |
| `NSIDE` | `IQX/3` | `IQX/3` | formula unchanged |
| NACA buffer | `IQX=370` -> `123/245` | `IQX=601` -> `200/399` | capacity side effect, not NACA formula |
| point distribution | TE-bunched `XX`; `NB=2*NSIDE-1` | unchanged | no per-NSIDE point-count change |
| symmetric/cambered | same section formulas | unchanged | only thickness x argument changes |
| 4/5-digit validity | `ITYPE`, `N543=210..250` | unchanged | same error ranges |
| local workspaces | `XX/YT/YC(NSIDE)`, `XB/YB(2*NSIDE)` | unchanged | caller capacity must fit |

### 399-versus-245 decision

`G-NACA-BUFFER` is explicitly `T-PENDING` in U6; the 399-point standard NACA
buffer is a capacity/default integration decision, not part of the NACA
formula diff. In S04:

- the kernel accepts caller-selected `nside`, including `nside=200` -> `399`;
- the existing `NACA` command retains `nside=123` -> `245`;
- no CLI/default/command activation changes the existing NACA point count;
- the 399-point product activation remains deferred to `G-NACA-BUFFER` and the
  NACS/command integration (S15 or a separate explicit decision).

## C++ mapping

| Fortran | C++ routine | Callers | Tests | S04 change |
|---|---|---|---|---|
| `NACA4` | `core::geometry::naca4` | `nacaAirfoil`, tests | `GeometryNaca4*` | `LTES` bool and `XTES` thickness branch |
| `NACA5` | `core::geometry::naca5` | `nacaAirfoil`, tests | `GeometryNaca5*` | same |
| `NACA(IDES1,LTES)` | `core::geometry::nacaAirfoil` | `handleNacaCore`, tests | `GeometryNacaAirfoil*` | optional bool, existing callers keep `false` |
| `NACA` command context | `handleNacaCore` | CLI | CLI/regression | unchanged, still open TE |

No second NACA implementation is introduced. The numeric core has no CLI
dependency. The parameter is named `sharpTrailingEdge` and maps directly to
Fortran `LTES`.

## Minimal product contract

- Local result construction; validation before returning.
- Invalid 4/5-digit designation and `nside < 2` remain controlled errors.
- No partial mutation because the API returns a fresh result.
- No new magic capacity; `nside` remains caller-selected and existing
  capacities are used.
- `-Werror` remains active.
- Existing NACA command behavior, session invalidation and paneling are
  unchanged.

## Harness H-S04-NACA-LTES

External harness:
`.audit-xfoil-next/xfoil-6.996/s04/harness/h_s04_naca_ltes.f`.

Cases at `NSIDE=123`:

- NACA4 0012 normal / sharp
- NACA4 2412 normal / sharp
- NACA5 23012 normal / sharp

Additional 399-point observation:

- NACA4 0012 normal / sharp at `NSIDE=200`

6.99 has no `LTES`; its wrapper calls the old signature and provides the
normal-TE control contract. Sharp target values come from 6.996.

Compared values: `N`, first/last and TE coordinates, TE gap, LE, maximum
thickness and x, maximum camber and x, selected interior points, signed-area
orientation, endpoint duplication and finiteness.

## Focused C++ tests

Planned:

- `unit.NacaLtesContracts` covers all six normal/sharp cases, point counts,
  TE coordinates/gap, selected interior points, thickness/camber maxima,
  orientation, finite values, and the deferred 399-point kernel observation.
- existing `unit.GeometryNaca4*`, `unit.GeometryNaca5*`,
  `unit.GeometryNacaAirfoil*` keep the normal-path contract green.
- invalid 4/5-digit and too-small-capacity tests are retained, with sharp
  variants added.

No tolerance increase is planned. All target values come from the external
6.996 harness.

## Acceptance

- H-S04-NACA-LTES and focused C++ tests pass.
- Existing NACA normal path and 6.99 regressions `a02`, `a03` stay green.
- No `src/app/cli/**` NACS registration or text change.
- Linux: full GCC/Clang Debug/Release, ASan/UBSan, Valgrind focused NACA
  cases, reference/determinism `236/236`, harnesses `16/16`, gates.
- Windows: exactly one focused process, expected `unit.NacaLtesContracts`
  `1/1`.
- R1 acceptance before the local commit.

## Commit boundary

One local commit:

```text
Integrate XFOIL 6.996 NACA sharp-trailing-edge kernel
```
