# S10 - Polar element metadata CPOLEL/KPOL/NKPOL

Status: PASS - Linux F1 acceptance, Windows focused R1 acceptance and S10
acceptance passed.

## Slice contract

`X6996-S10-Polar-Element-Metadata` carries only the XFOIL 6.996 polar
element storage contract:

- `CPOLEL` per-point element quantities;
- `KPOL` selected element columns;
- `NKPOL` selected element-column count;
- initialization/default state;
- sizing against the integrated `NAX=800`, `NPX=20`, `KPTOT=4` and
  `NBX=5` capacities;
- slot mapping, copy, delete/compaction, sort, point removal, reset and
  session isolation;
- focused C++ tests and external H-S10-POLAR-ELEM evidence.

Explicit non-goals:

- `NTS` and `LPPORT` (`NIMP-01`);
- plot functionality;
- polar text or dump format;
- new output columns;
- CLI/command registration;
- golden migration;
- product version or banner.

No numerical result is expected to change because S10 activates no output
consumer.

## Fortran pre-analysis

Canonical sources inspected:

- `fortran-src/xfoil-6.99/PINDEX.INC`
- `fortran-src/xfoil-6.996/PINDEX.INC`
- `fortran-src/xfoil-6.99/PPLOT.INC`
- `fortran-src/xfoil-6.996/PPLOT.INC`
- `fortran-src/xfoil-6.99/XFOIL.INC`
- `fortran-src/xfoil-6.996/XFOIL.INC`
- `fortran-src/xfoil-6.996/iopol.f`
- `fortran-src/xfoil-6.996/xoper.f`
- `fortran-src/xfoil-6.996/xpol.f`
- `fortran-src/xfoil-6.996/xfoil.f`

6.99 has none of `CPOLEL`, `KPOL`, `NKPOL`, `KPTOT`, `CPOLNNAME` or
`CPOLNFORM`; S10 is an additive 6.996 state contract.

| Field | Fortran form | Initialization | Producer | Mutation | Consumer |
|---|---|---|---|---|---|
| `CPOLEL` | `REAL CPOLEL(NAX,1,KPTOT,NPX)` in `XFOIL.INC`; `CPOLEL(NAX,NBX,KPTOT,NPX)` in `PPLOT.INC` | COMMON zeroes in the main solver | none in the main XFOIL producer path; `POLREAD` can transfer file columns, but the current element-label parser leaves `KPOL=0` | no main-solver mutation; `PLRCOP`, `PLRSRT` and `PREM` omit it | `POLWRIT` only when `NKPOL>0`; currently unreachable from the main menu; plot-only in `pplot.f` |
| `KPOL` | `INTEGER KPOL(KPTOT)` in `COMMON/CI03` | COMMON zeros | none in the main solver; local arrays in `POLREAD`/`POLWRIT` are separate | none in the main solver | `POLWRIT` element-column loop |
| `NKPOL` | `INTEGER NKPOL` in `COMMON/CI03` | `NKPOL = 0` in `xfoil.f:698` | none | none | `POLWRIT` element-column loop and all `xoper.f` calls |

`PINDEX.INC` defines `KCX=1`, `KCY=2`, `KCM=3`, `KCDV=4`, `KPTOT=4`,
`CPOLNNAME` and `CPOLNFORM`. `PPLOT.INC` defines `NBX=5`, `ISX=10`; the
solver include has one element dimension, while the polar file/plot format
supports up to five elements.

### Upstream lifecycle omissions

The 6.996 main solver adds the storage but does not maintain it as a
parallel field:

- `PLRCOP` copies `CPOL`, `CPOLSD` and `CPOLXY`, but not `CPOLEL`;
- `PLRSRT` applies the sort permutation to `CPOL`/`CPOLSD`, but not to
  `CPOLEL`;
- the `PREM` loop in `xoper.f` shifts `CPOL`/`CPOLSD`, but not `CPOLEL`;
- no main-solver producer sets nonzero `KPOL`/`NKPOL`.

These omissions have no visible product effect today because no main XFOIL
output selects element columns. S10 nevertheless implements the coherent
parallel lifecycle required by the slice contract, so later S11/S12
consumers cannot observe stale element data. This is a documented C++
correction of an upstream dead-state omission, not a format or numeric
change.

## C++ mapping

Existing polar architecture is extended; no parallel polar manager is added.

| Fortran | C++ | Contract |
|---|---|---|
| `CPOLEL(IA,N,KP,IP)` | `core::polar::PolarDataset::elementData` (`PolarElementRow` per point) | parallel to `points`/`sideData`; each row has `maxPolarElements=5` and four `PolarElementData` fields |
| `KCX..KCDV` | `core::polar::PolarElementQuantity` | values 1..4; domain enum instead of magic codes |
| `KPOL(KPTOT)` | `core::polar::PolarElementSelection::columns` | `std::optional` column entries; `nullopt` is the Fortran zero slot |
| `NKPOL` | `core::polar::PolarElementSelection::count` | selected-column count; default 0 |
| session COMMON/CI03 | `session::PolarSessionState::elementSelection` | instance-local; no sharing between sessions |

`PolarElementData` fields map `Cx`, `Cy`, `Cm`, `CDv`. `PolarElementRow`
uses the `PPLOT.INC`/file capacity `NBX=5`, while the current solver still
has one element; this keeps the future polar format contract in the same
state type without changing any current producer.

## Implementation invariants

- New `PolarDataset` element storage is empty until a point is added.
- `addPoint` appends one zero-initialized element row with the point.
- Legacy datasets with points but no element rows are repaired on the next
  `addPoint` by adding zero rows for the existing points.
- `sortPoints` validates element-row count before mutation and applies the
  same `INDX` permutation to `points`, `sideData` and `elementData`.
- `copySlot`/`PLRCOP` copies element rows with the rest of the slot.
- Slot deletion and compaction copy all parallel fields through the existing
  `copySlot` path, then pop the last slot.
- `removePolarPoints` erases the point's element row in parallel.
- `deleteAllPolarSlots` clears only polar slot/file state; it does not clear
  `elementSelection` or unrelated session data.
- Slot 20 is valid; slot 21 is rejected atomically without changing the
  existing slot or selection state.
- Two sessions have independent element rows and selections.
- No text/dump writer reads or writes the new state in S10.

## H-S10-POLAR-ELEM

External harness: `external audit area: xfoil-6.996/s10/harness/`.

The harness compiles the canonical 6.996 `xpol.f`/`sort.f` against
`XFOIL.INC` with `-ffunction-sections`/`--gc-sections`, calls the actual
`PLRCOP` and `PLRSRT`, and records the state in `state.tsv`. A second driver
compiles against `PPLOT.INC` to capture `NBX=5`, `ISX=10` and the plot
element dimensions.

Captured cases include:

- XFOIL solver dimensions and cold-start `KPOL=0`, `NKPOL=0`;
- first/multiple slots through `PLRCOP`;
- element mapping through the actual copy/sort behavior;
- observed upstream omissions: `CPOLEL` is not copied by `PLRCOP` and not
  sorted by `PLRSRT`;
- `PPLOT.INC` element capacity and dimensions;
- source-level slot 20/21 limit and delete-all reset scope;
- independent sessions are C++-only because the Fortran COMMON is global.

The C++ tests assert the intended coherent lifecycle rather than the
upstream dead-state omission. The harness records both facts explicitly in
`differences.txt`.

## Focused tests

All names share the `unit.PolarElementMetadata` filter:

- types, capacities, zero initialization and default selection;
- add-point zero rows and legacy-row repair;
- point-capacity atomicity;
- sort mapping and sort-mismatch atomicity;
- deep copy/self-copy;
- session slot deletion/compaction;
- point removal parallel fields;
- slot 20 / slot 21;
- reset preserving unrelated session data and selection;
- independent sessions;
- `KPOL`/`NKPOL` selection mapping.

No test writes a file or activates a polar text/dump path.

## Acceptance

`X6996-S10-F1`: PASS on Linux.

- focused `unit.PolarElementMetadata`: 13/13;
- core 450/450, unit 1716/1716, integration 246/246, regression 5/5,
  batch 47/47;
- full debug 2017/2017;
- GCC 13 release 2017/2017, Clang 18 debug 2017/2017, Clang 18 release
  2017/2017;
- ASan/UBSan 2017/2017;
- focused Valgrind 13/13, no definite leaks or errors;
- reference 236/236, determinism 236/236, harness families 16/16;
- all permanent gates pass.

`X6996-S10-R1`: PASS on 2026-11-10. The focused Windows process
`./build/debug/xfoil_tests unit.PolarElementMetadata` reported all 13 tests
OK and `0 failure(s) in 13 test(s)`. `X6996-S10` is PASS; the S10 commit
follows this documentation update.

Commit boundary: one local commit,
`Integrate XFOIL 6.996 polar element metadata`; no push, fetch, tag or
release before cumulative XFOIL.next 1.1.0 acceptance.
