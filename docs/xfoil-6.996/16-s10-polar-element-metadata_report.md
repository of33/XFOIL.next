# XFOIL 6.996 - S10 polar element metadata report

Status: PASS - Linux F1 acceptance, Windows focused R1 acceptance and S10
acceptance passed.

## Starting state

- HEAD: `b83a1031d50bd61124b08669ea1765583b67220d`
  (`Remove local audit paths from S09 documentation`).
- Parent: `8ee4706e4cf667bbb4b23532add0ea70fb3ff63c` (S09).
- Worktree clean before S10; staged 0; ahead 22; tag `v1.0.0`.
- No fetch, push, tag or release.

## Scope and non-goals

Carried state: `CPOLEL`, `KPOL`, `NKPOL`.

Covered: definition, initialization, dimensions, polar-slot mapping,
copy/move/delete/compaction, sorting, point removal, reset, session
isolation and focused tests.

Not covered: `NTS`/`LPPORT`, plots, polar text/dump formats, new output
columns, CLI/commands, golden migration, product version or banner.

## Fortran pre-analysis

The complete field table and source references are in
`16-s10-polar-element-metadata.md`. Key points:

- 6.99 has no `CPOLEL`/`KPOL`/`NKPOL`; the contract is additive in 6.996.
- `PINDEX.INC`: `KPTOT=4`, `KCX=1`, `KCY=2`, `KCM=3`, `KCDV=4`.
- `XFOIL.INC`: `CPOLEL(NAX,1,KPTOT,NPX)`, `KPOL(KPTOT)`, `NKPOL`,
  `NAX=800`, `NPX=20`.
- `PPLOT.INC`: `CPOLEL(NAX,NBX,KPTOT,NPX)`, `NBX=5`, `ISX=10`.
- `xfoil.f:698`: `NKPOL=0`; `KPOL` is a COMMON zero.
- No main-solver producer selects nonzero `KPOL`/`NKPOL`.
- `PLRCOP`, `PLRSRT` and `PREM` do not carry `CPOLEL` in upstream 6.996.
  S10 implements the coherent parallel lifecycle required by its contract;
  the upstream omission is documented and has no current visible effect.

## C++ implementation

Changed state/product files:

- `include/xfoil/core/polar/PolarTypes.h`
  - `PolarElementQuantity`, `PolarElementData`;
  - `PolarElementRow` with `maxPolarElements=5`;
  - `PolarElementSelection` (`columns`/`count`), default empty;
  - `PolarDataset::elementData`, parallel to points/side data.
- `src/core/polar/PolarOps.cpp`
  - `addPoint` appends zero element rows, repairs legacy parallel state and
    rejects mismatches atomically;
  - `sortPoints` validates and applies the same permutation to element rows.
- `include/xfoil/session/SessionState.h`
  - `PolarSessionState::elementSelection` for `KPOL`/`NKPOL`;
  - corrected stale `NPX=12` comment to `NPX=20`.
- `src/session/SessionOps.cpp`
  - `removePolarPoints` erases the parallel element row.

Existing `copySlot`/session slot compaction now carries `elementData`
automatically by value semantics. `deleteAllPolarSlots` leaves
`elementSelection` and unrelated session state intact.

No CLI, text/dump writer, format, tolerance, version or banner code changed.

## H-S10-POLAR-ELEM

External harness: `external audit area: xfoil-6.996/s10/harness/`.

`state.tsv` records:

- XFOIL dimensions: `NAX=800`, `NPX=20`, `KPTOT=4`,
  `CPOLEL(800,1,4,20)`; `KPOL=0,0,0,0`, `NKPOL=0`;
- actual `PLRCOP(1,2)`: `CPOL` copied, `CPOLEL` slot 2 remains its prior
  value 99.0 instead of 11.0/22.0;
- actual `PLRSRT(1,IAL)`: `CPOL` sorted, `CPOLEL` remains storage order
  30.0/10.0/20.0;
- `PPLOT.INC` dimensions: `NAX=500`, `NPX=20`, `NBX=5`, `ISX=10`,
  `CPOLEL(500,5,4,20)`.

`differences.txt` also records the source-level slot 20/21 capacity check,
the PDEL reset scope and the fact that Fortran COMMON cannot express two
independent sessions; session isolation is therefore tested on the C++ value
state. The two missing upstream lifecycle operations are classified
explicitly. S10 intentionally maintains coherent parallel state; this
activates no output and changes no numerical value.

## Focused tests

Thirteen tests share the `unit.PolarElementMetadata` filter:

- types/capacities/default initialization;
- zero element rows on add;
- legacy-row repair;
- point-capacity atomicity;
- sort mapping and sort-mismatch atomicity;
- deep copy/self-copy;
- slot delete/compaction;
- point removal;
- slot 20/21;
- reset preserving unrelated data;
- independent sessions;
- selection definition.

Final focused run: `13/13` green. Full debug suite: `2017/2017`;
core `450/450`, unit `1716/1716`, integration `246/246`,
regression `5/5`, batch `47/47`.

## Linux acceptance

`X6996-S10-F1`: PASS.

| Check | Result |
|---|---|
| GCC 13 Debug full suite | 2017/2017 |
| GCC 13 Release full suite | 2017/2017 |
| Clang 18 Debug full suite | 2017/2017 |
| Clang 18 Release full suite | 2017/2017 |
| `make test-core` | 450/450 |
| `make test-unit` | 1716/1716 |
| `make test-integration` | 246/246 |
| `make test-regression` | 5/5 |
| `make test-batch` | 47/47 |
| ASan/UBSan | 2017/2017 |
| Valgrind `unit.PolarElementMetadata` | 13/13, no definite leaks |
| Reference | 236/236 |
| Reference determinism | 236/236 |
| Harness families | 16/16 |
| Permanent gates | all pass |

## Windows acceptance

`X6996-S10-R1`: PASS on 2026-11-10.

The focused Windows process

```text
./build/debug/xfoil_tests unit.PolarElementMetadata
```

reported all 13 tests OK and `0 failure(s) in 13 test(s)`. `X6996-S10` is
therefore PASS; the S10 commit follows this report update.

## Protected data and Git status

- `fortran-src/**`, `fortran-bin/**`, `patches/**`: unchanged.
- `tests/golden/**`, `tests/fortran-reference/**`, old `tests/data/**`:
  unchanged.
- CLI, formats, tolerances, product version and banner: unchanged.
- No push, fetch, tag or release.
- S10 commit follows this documentation update.
