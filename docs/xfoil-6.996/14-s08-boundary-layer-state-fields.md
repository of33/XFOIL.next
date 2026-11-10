# S08 - Boundary-layer state fields CLF, DSSR, MDEF, PDEF, KDEF

Status: PASS - `X6996-S08` and `X6996-S08-R1` accepted on Linux and Windows;
local commit prepared.

## Slice contract

`X6996-S08-Boundary-Layer-State-Fields` carries only the XFOIL 6.996
boundary-layer state fields:

- `CLF` — scalar friction-lift coefficient;
- `DSSR` — station density-thickness quantity;
- `MDEF` — station mass-defect quantity;
- `PDEF` — station momentum-defect quantity;
- `KDEF` — station kinetic-energy-defect quantity.

The slice covers state definition, initialization, dimensioning, producers,
active BL-station assignment, side/wake assignment, iteration lifetime,
session isolation and focused tests.

Explicit non-goals: CFT formulas/derivatives, BLDUMP3 or other output formats,
polar columns, CLI/command changes, defaults, file formats, global tolerances,
product version or banner.

## Fortran pre-analysis

Canonical sources:

- `fortran-src/xfoil-6.99/XFOIL.INC`
- `fortran-src/xfoil-6.996/XFOIL.INC`
- `fortran-src/xfoil-6.99/xbl.f`
- `fortran-src/xfoil-6.996/xbl.f`

Only these files contain the S08 field declarations/producers in the
6.99 -> 6.996 diff:

| Field | Declaration | Producer | Consumer |
|---|---|---|---|
| `CLF` | `COMMON/CR09` scalar `REAL` | `CDCALC` after the `DO 20 IS` / `DO 205 IBL` friction loop | 6.996 text output `CLf`; deferred to S18 |
| `DSSR` | `COMMON/CR15 DSSR(IVX,ISX)` | `MRCHUE` and `MRCHDU` after the station store: `DSSR(IBL,IS)=HC2*T2`; `UPDATE` wake equating | `BLDUMP3` (`DC=DSSR`); deferred to S13 |
| `MDEF` | `COMMON/CR15 MDEF(IVX,ISX)` | `MRCHUE`/`MRCHDU`: `MDEF(IBL,IS)=R2*U2*D2`; `UPDATE` wake equating | plot-only `blplot.f`; `NIMP-01` |
| `PDEF` | `COMMON/CR15 PDEF(IVX,ISX)` | `MRCHUE`/`MRCHDU`: `PDEF(IBL,IS)=R2*U2*U2*T2`; `UPDATE` wake equating | plot-only `blplot.f`; `NIMP-01` |
| `KDEF` | `COMMON/CR15 KDEF(IVX,ISX)` | `MRCHUE`/`MRCHDU`: `KDEF(IBL,IS)=R2*U2*U2*U2*T2*HS2*0.5`; `UPDATE` wake equating | plot-only `blplot.f`; `NIMP-01` |

The station fields are per-side, wake-aware `REAL` arrays:
Laminar and turbulent surface stations are written by the marching producer;
wake stations are written on the lower-side array and copied to the upper
wake pointers by `UPDATE`. `HC2` is zero for `M=0` in `HCT`, so `DSSR`
is zero for incompressible cases and nonzero only with Mach > 0. The fields
are written after the primary variables at each active station, not at a
separate post-convergence pass. They persist as COMMON state between
iterations and are overwritten at active stations on the next march.

`CLF` is a scalar session quantity. It is set to zero and accumulated in
`CDCALC` with `DY = -(X(I)-X(IM))*SA + (Y(I)-Y(IM))*CA`. It is not a station
array and does not belong to a side or wake.

## C++ mapping

Prefer the existing active BL state architecture:

- `core::boundary_layer::ViscalState` for persistent session state;
- `BoundaryLayerStationField` with `top`/`bottom` vectors for per-side
  station values;
- existing `MarchInput`/`UpdateInput` handover paths.

| Fortran field | C++ structure/field | Producer | Handover | Test |
|---|---|---|---|---|
| `DSSR` | `ViscalState::dssr` (`BoundaryLayerStationField`) | `MRCHUE`/`MRCHDU` | `MarchInput::dssr`; `UpdateInput::dssr` wake copy | focused S08 station tests |
| `MDEF` | `ViscalState::mdef` | `MRCHUE`/`MRCHDU` | `MarchInput::mdef`; `UpdateInput::mdef` | focused S08 station tests |
| `PDEF` | `ViscalState::pdef` | `MRCHUE`/`MRCHDU` | `MarchInput::pdef`; `UpdateInput::pdef` | focused S08 station tests |
| `KDEF` | `ViscalState::kdef` | `MRCHUE`/`MRCHDU` | `MarchInput::kdef`; `UpdateInput::kdef` | focused S08 station tests |
| `CLF` | `ViscalState::clf`; `ViscalResult::clf`; `CdCalcResult::clf` | `CDCALC` / `cdcalc` | `ViscalState` result; session CR09 preservation | focused S08 CLF tests |

No parallel station-state structure is introduced. No raw-pointer API and no
global state are added.

## Implementation contract

- Default and reset station fields are empty, and `clf` starts at zero.
- Fields use the existing active BL length (`0..NBL` per side plus existing
  wake-pointer extent), with no artificial extra padding.
- New station fields are sized before their first producer write.
- Laminar, turbulent and wake stations receive defined values.
- `MRCHUE` and `MRCHDU` both write the same contract.
- `UPDATE` equates upper-wake fields to lower-wake fields.
- `STMOVE` preserves the existing capacity and does not overwrite values.
- `ViscalState` remains instance-local; two sessions do not share fields.
- Empty/reset states contain no stale field values.
- No output or numeric consumer is activated; CLF is available for a later
  output slice, DSSR for S13, and MDEF/PDEF/KDEF remain plot-only evidence.

## Focused tests

Add `unit.BoundaryLayerNewStateFields*` tests in the existing Clark Y core
path:

- default/empty state and session isolation;
- converged incompressible point: laminar and turbulent surface stations on
  both sides, wake station, upper/lower wake equivalence;
- incompressible `DSSR = 0` (HCT consequence) and compressible nonzero
  `DSSR`;
- non-converged point with one controlled iteration;
- active length/upper wake extent for longer and shorter wake lengths;
- unchanged existing numerical coefficients from the established tests.

Expected values come from the external instrumented 6.996 Fortran harness
`H-S08-BL-FIELDS`; exploratory outputs stay outside the repository.

## Regression and acceptance

No product goldens or Fortran references are expected to change. The
existing 6.99 cases `a04`, `a05`, `a08` stay canonical. CLF/DSSR are not
wired into any output, so the existing visible results remain unchanged.

Linux acceptance: the standard permanent gates, full test suite, core/unit/
integration/regression/batch suites, GCC/Clang Debug+Release, ASan/UBSan,
focused Valgrind, reference/determinism and harness family checks.

Windows acceptance: one focused `unit.BoundaryLayerNewStateFields` process
after Linux is green. No full Windows matrix.

Commit boundary: one local commit, `Integrate XFOIL 6.996 boundary-layer
state fields`; no push, tag or release before cumulative 1.1.0 acceptance.
