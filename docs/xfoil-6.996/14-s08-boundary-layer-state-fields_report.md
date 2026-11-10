# XFOIL 6.996 — S08 boundary-layer state-fields report

Status: PASS - `X6996-S08` and `X6996-S08-R1` accepted on Linux and Windows;
local commit prepared.

Decision: `X6996-S08: PASS`

## Starting state

- HEAD/start commit: `304a0172d93a0ee6866d1f04e566cdca3f4600c6`.
- Parent: `2cd8b7260e3997228a5424b1642af55bb79f9391`.
- Worktree at start: clean; staged 0; ahead 19; behind 0; tag `v1.0.0`.
- No fetch, push, tag or release was performed.

No product output, golden or Fortran-reference file is changed by S08.

## Scope and non-goals

Carried fields: `CLF`, `DSSR`, `MDEF`, `PDEF`, `KDEF`.

Covered: definitions, initialization, dimensioning, producers, active-station
assignment, side/wake assignment, iteration lifetime, session isolation and
focused tests.

Not covered: CFT formulas/derivatives, BLDUMP3/output formats, polar columns,
CLI/commands, defaults, file formats, global tolerances, product version,
banner, plot implementation.

## Fortran diff

`XFOIL.INC` adds the fields without changing any existing declaration:

```fortran
REAL NX, NY, MASS, MDEF, KDEF, MINF1, MINF, MINF_CL, MVISC, MACHP1
COMMON/CR09/ ...,CLF,...
COMMON/CR15/ ...,MDEF(IVX,ISX),PDEF(IVX,ISX),KDEF(IVX,ISX),...
     &       ...,DSSR(IVX,ISX),...
```

`xbl.f` adds the same producer block in both `MRCHUE` and `MRCHDU`:

```fortran
DSSR(IBL,IS) = HC2*T2
MDEF(IBL,IS) = R2*U2*D2
PDEF(IBL,IS) = R2*U2*U2*T2
KDEF(IBL,IS) = R2*U2*U2*U2*T2*HS2*0.5
```

and the `UPDATE` wake equating:

```fortran
DSSR(IBLTE(1)+KBL,1) = DSSR(IBLTE(2)+KBL,2)
MDEF(IBLTE(1)+KBL,1) = MDEF(IBLTE(2)+KBL,2)
PDEF(IBLTE(1)+KBL,1) = PDEF(IBLTE(2)+KBL,2)
KDEF(IBLTE(1)+KBL,1) = KDEF(IBLTE(2)+KBL,2)
```

`xfoil.f` adds the `CLF` accumulator in `CDCALC`:

```fortran
CLF = 0.0
...
DY = -(X(I)-X(IM))*SA + (Y(I)-Y(IM))*CA
CLF = CLF + 0.5*(TAU(IBL,IS)+TAU(IBL-1,IS))*DY * 2.0/QINF**2
```

## Field meaning and data flow

| Field | Form | Side/wake | Producer | Lifetime/consumer |
|---|---|---|---|---|
| `CLF` | scalar | session | `CDCALC` | recomputed with each force evaluation; text consumer deferred to S18 |
| `DSSR` | station `(IVX,ISX)` | both sides + wake | `MRCHUE`, `MRCHDU`, `UPDATE` wake copy | overwritten per active station; `BLDUMP3` consumer deferred to S13 |
| `MDEF` | station `(IVX,ISX)` | both sides + wake | `MRCHUE`, `MRCHDU`, `UPDATE` wake copy | plot-only consumer (`NIMP-01`) |
| `PDEF` | station `(IVX,ISX)` | both sides + wake | `MRCHUE`, `MRCHDU`, `UPDATE` wake copy | plot-only consumer (`NIMP-01`) |
| `KDEF` | station `(IVX,ISX)` | both sides + wake | `MRCHUE`, `MRCHDU`, `UPDATE` wake copy | plot-only consumer (`NIMP-01`) |

- Surface laminar/turbulent stations are written on their own side.
- Wake stations are written on side 2 and copied to the upper wake pointers by
  `UPDATE`; this is the canonical side/wake assignment.
- Values are produced per BL marching call, after the primary station store,
  and are not a post-convergence-only product.
- `DSSR` is zero at `M=0` because `HCT` returns `HC2=MSQ*(...)`; the
  compressible harness case demonstrates its nonzero producer.
- No field is persisted to a file; all are rebuilt from the BL session state.

## C++ mapping

| Fortran | C++ | Producer | Handover |
|---|---|---|---|
| `DSSR` | `ViscalState::dssr` | `MRCHUE`/`MRCHDU` | `MarchInput::dssr`, `UpdateInput::dssr` |
| `MDEF` | `ViscalState::mdef` | `MRCHUE`/`MRCHDU` | `MarchInput::mdef`, `UpdateInput::mdef` |
| `PDEF` | `ViscalState::pdef` | `MRCHUE`/`MRCHDU` | `MarchInput::pdef`, `UpdateInput::pdef` |
| `KDEF` | `ViscalState::kdef` | `MRCHUE`/`MRCHDU` | `MarchInput::kdef`, `UpdateInput::kdef` |
| `CLF` | `ViscalState::clf`, `ViscalResult::clf`, `CdCalcResult::clf` | `cdcalc`/`CDCALC` | result and session CR09 state |

Changed product files:

- `include/xfoil/core/boundary_layer/Viscal.h`
- `include/xfoil/core/boundary_layer/Marching.h`
- `include/xfoil/core/boundary_layer/Update.h`
- `include/xfoil/core/boundary_layer/Coupling.h`
- `src/core/boundary_layer_solver/Marching.cpp`
- `src/core/boundary_layer_solver/Update.cpp`
- `src/core/boundary_layer_solver/Viscal.cpp`
- `src/core/boundary_layer_solver/Coupling.cpp`
- `src/session/SessionOps.cpp`

New station fields are grown before the first producer write in the marching
entry points and in the VISCAL state path. Existing capacity is retained on
wake shrink; active length is governed by the mapping. `UPDATE` copies the
four upper-wake fields from the lower side. `STMOVE` preserves the existing
station capacity and does not create a second BL state. The CLF scalar is
preserved with the existing CR09 session scalars when the VISCAL state is
reset.

## H-S08-BL-FIELDS external harness

Location outside the repository:
`/local/Olaf/Entwicklung/.audit-xfoil-next/xfoil-6.996/s08/harness/`.

A private copy of the frozen 6.996 source was instrumented (only
`WRITE` statements, no numerical changes) and built with the canonical
GNU Fortran flags. Canonical `fortran-src/**` stayed unchanged. The harness
records side, Fortran IBL, IBLTE, ITRAN, IPAN, XSSI, THET, DSTR, UEDG, TAU,
DSSR, MDEF, PDEF, KDEF, plus `CLF`/CDF/CD from `CDCALC`.

Executed cases:

- focused Clark Y, Re=1e5, alpha=4, converged;
- focused Clark Y, Re=1e5, alpha=4, `ITER 1` (controlled non-converged);
- focused Clark Y, Re=1e5, Mach 0.3, alpha=4;
- U6 `a04` Clark Y, Re=1e6, alpha=4;
- U6 `a05` NACA 0012, Re=1e6, ASEQ 0/8/2;
- U6 `a08` Clark Y, Re=3e6, DUMP path;
- U6 `a28` Clark Y, Re=1e6, BLOU/BLDUMP3 path.

Station rows are in `harness/stations.tsv`; scalar force values are in
`harness/clf.tsv`. Fortran IBL is 1-based; the C++ station index is
`IBL - 1`.

Selected converged incompressible values (Clark Y, Re=1e5, alpha=4):

| Side | Fortran IBL | Status | IPAN | DSSR | MDEF | PDEF | KDEF |
|---:|---:|---|---:|---:|---:|---:|---:|
| 1 | 6 | laminar | 78 | 0 | 1.5795743640368106e-04 | 5.3637449771010964e-05 | 3.3233521853002677e-05 |
| 1 | 64 | turbulent | 20 | 0 | 1.1744237590045935e-02 | 4.582886175658467e-03 | 4.124847151642602e-03 |
| 2 | 10 | laminar | 91 | 0 | 5.801336786136245e-04 | 1.8284403766452106e-04 | 1.1194026183377396e-04 |
| 2 | 82 | wake | 163 | 0 | 1.3525247108848863e-02 | 9.136927521034012e-03 | 7.725507266501801e-03 |

`CLF = 1.3718900545702014e-03`, `CDF = 8.1030259647362300e-03`,
`CD = 1.7288688307967353e-02`.

Selected compressible values (Clark Y, Re=1e5, Mach 0.3, alpha=4):

| Side | Fortran IBL | Status | DSSR | MDEF |
|---:|---:|---|---:|---:|
| 1 | 6 | laminar | 2.0380151677881378e-06 | 1.9594903087619486e-04 |
| 1 | 64 | turbulent | 1.4532063706575274e-04 | 9.575453062111189e-03 |
| 2 | 82 | wake | 2.9588384934974986e-04 | 1.588660557218645e-02 |

`CLF = 1.3342545946136348e-03`, `CDF = 8.2577882006462430e-03`,
`CD = 1.8139222217264349e-02`.

Controlled `ITER 1` values used for the focused test:
`CLF = 1.3090420460097126e-03`, side-1 IBL64
`MDEF=1.0323947751800054e-02`, `PDEF=7.943952465266108e-03`,
`KDEF=8.173186602589434e-03`; side-2 IBL82
`MDEF=1.0499120294512904e-02`.

## Tests

New focused tests in `tests/core/ClarkYViscousCoreTests.cpp`:

- `unit.BoundaryLayerNewStateFieldsDefaultAndSessionIsolation`
- `unit.BoundaryLayerNewStateFieldsConvergedStationsBothSidesAndWake`
- `unit.BoundaryLayerNewStateFieldsCompressibleDssr`
- `unit.BoundaryLayerNewStateFieldsNonConverged`
- `unit.BoundaryLayerNewStateFieldsWakeActiveLengthGrowthAndShrink`

`tests/core/BoundaryLayerCorrelationTests.cpp` was extended so the UPDATE
wake-equivalence tests initialize and assert the four new station fields.
No golden/reference file changed; existing Clark Y coefficient tests remain
green.

Current counts: full `1995/1995`, core `434/434`, unit `1694/1694`,
integration `246/246`, regression `5/5`, batch `47/47`.

## Linux matrix

Toolchain: GCC 13.3, Clang 18.1.3, Valgrind 3.26.0.

| Check | Result |
|---|---|
| GCC 13 Debug `make test` | 1995/1995 pass |
| GCC 13 Release | 1995/1995 pass |
| Clang 18 Debug | 1995/1995 pass |
| Clang 18 Release | 1995/1995 pass |
| `make test-core` | 434/434 pass |
| `make test-unit` | 1694/1694 pass |
| `make test-integration` | 246/246 pass |
| `make test-regression` | 5/5 pass |
| `make test-batch` | 47/47 pass |
| ASan/UBSan `make test-sanitize` | 1995/1995 pass |
| Valgrind focused S08 prefix | 5/5 pass, no definite leaks |
| Valgrind UPDATE wake tests | 2/2 pass, no definite leaks |
| `make test-reference` | 236/236 pass |
| `make test-reference-determinism` | 236/236 pass |
| `make test-harnesses` | 16 families, 0 failures |
| `check-dependencies` | PASS |
| `check-fortran-sha` | PASS |
| `check-release-inventory` | PASS |
| `check-license-headers` | PASS (421 sources) |
| `check-public-docs` | PASS (17 public documents) |
| `core-link-check` | PASS |
| `session-link-check` | PASS |

New tests: 5 focused S08 tests; full count 1990 -> 1995, unit
1689 -> 1694, core 429 -> 434. Integration/regression/batch counts are
unchanged.

## Windows focused run

PASS. Single focused process:

```text
[ RUN      ] unit.BoundaryLayerNewStateFieldsDefaultAndSessionIsolation
[       OK ] unit.BoundaryLayerNewStateFieldsDefaultAndSessionIsolation
[ RUN      ] unit.BoundaryLayerNewStateFieldsConvergedStationsBothSidesAndWake
[       OK ] unit.BoundaryLayerNewStateFieldsConvergedStationsBothSidesAndWake
[ RUN      ] unit.BoundaryLayerNewStateFieldsCompressibleDssr
[       OK ] unit.BoundaryLayerNewStateFieldsCompressibleDssr
[ RUN      ] unit.BoundaryLayerNewStateFieldsNonConverged
[       OK ] unit.BoundaryLayerNewStateFieldsNonConverged
[ RUN      ] unit.BoundaryLayerNewStateFieldsWakeActiveLengthGrowthAndShrink
[       OK ] unit.BoundaryLayerNewStateFieldsWakeActiveLengthGrowthAndShrink
0 failure(s) in 5 test(s)
```

No full Windows suite was run.

## Protected data and Git status

- `fortran-src/**`, `fortran-bin/**`, `patches/**`: unchanged.
- `tests/golden/**`: unchanged.
- canonical `tests/fortran-reference/**`, `tests/data/**`: unchanged.
- CLI, commands, formats, CFT formulas, tolerances, version and banner:
  unchanged.
- The S08 change set is prepared for the local commit
  `Integrate XFOIL 6.996 boundary-layer state fields`.
- No push, fetch, new tag or release.

## Acceptance checklist

- [x] Fields defined, initialized, dimensioned and produced on both sides/wake.
- [x] C++ mapping uses the existing station-state architecture.
- [x] Focused Fortran harness and expected values available externally.
- [x] Focused tests for default/reset, laminar, turbulent, wake, both sides,
  mapping, converged, non-converged, session isolation and wake extent.
- [x] No golden/reference output change.
- [x] Full Linux matrix.
- [x] Focused Windows process: 5/5 pass.
- [x] `X6996-S08`/`X6996-S08-R1` acceptance.
- [x] Local commit `Integrate XFOIL 6.996 boundary-layer state fields`.
