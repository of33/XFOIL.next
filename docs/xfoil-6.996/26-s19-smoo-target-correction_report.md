# XFOIL 6.996 — S19 SMOO target correction report

Status: PASS — `X6996-S19` and `X6996-S19-R1` accepted. Implementation,
Linux matrix, focused Windows process and R1 documentation check are green.

## Starting state

- HEAD: `2427c54241a047236b5a2dd1115dcbcd0c97def0`
  (`Integrate XFOIL 6.996 console text behavior`).
- Parent: `a0b03d72f8ab7551d607788d8b1394f5f89280ad`.
- Worktree clean, staged 0, untracked 0, ahead 36 behind 0.
- Tag `v1.0.0` unchanged at
  `dc7305b057286574cd137cb6f1af170b74650ec5`.
- No S19 commit existed at the start.
- No push, fetch, tag or release.

## U6 scope

`S19` is group G10 and owns the 6.996 `SMOO` `KQSP=1` correction in
`xmdes.f` `MDES`. It changes no other inverse-design behavior. `S01` and
`S03` are accepted dependencies. The U6 commit boundary is one local
implementation commit.

## Upstream analysis

The 6.99/6.996 `xmdes.f` diff has exactly one change in the `SMOO` branch:

```diff
-       KQSP = KQTARG
+       KQSP = 1
```

The surrounding 6.996 sequence is unchanged:

```text
SMOOQ(IQ1,IQ2,KQSP)
CNCALC(QSPEC(1,KQSP),LQSYM)
QSPCIR
QSPINT update and FORMAT 1200/1210 report
```

Consequences:

- MDES `SMOO` always targets `QSPEC(:,1)`.
- `KQTARG` no longer selects the MDES `SMOO` column.
- Without a prior QDES run, the `KQTARG=0` overlay on `QGAMM` is irrelevant
  for `SMOO`; `QGAMM` remains unchanged.
- After a prior QDES run, `KQTARG` is 1 (0-based 0), but the result follows
  from the fixed `KQSP=1`, not from the `KQTARG` value.

## Implementation

Changed product and test paths:

- `src/session/SessionOps.cpp`
- `include/xfoil/session/SessionOps.h`
- `tests/unit/SessionMdesCommandTests.cpp`
- `tests/unit/SessionCommitTests.cpp`
- `tests/integration/SmooTargetS19Tests.cpp` (new)
- `Makefile` (test registration)
- this S19 document pair

`mdesSmoothTargetSegment` now validates that a first Qspec distribution
exists, smooths `qspecs[0]` with the shared target segment, calls
`CNCALC` on that distribution and then runs the existing `QSPCIR` and
`QSPINT` passes. The obsolete `KQTARG` column selection and `QGAMM` alias
branch are removed. `KQTARG` itself remains in the session state unchanged.
No kernel, format, default or command grammar is changed.

## Tests

Focused unit coverage:

- `unit.SmooTargetS19WithoutQdesSmoothsFirstQspec`
- `unit.SmooTargetS19IgnoresKqtarg`
- `unit.SmooTargetS19MdesWithoutQdesKeepsQgamm`
- `unit.SmooTargetS19AfterQdesKeepsQgamm`

Focused integration coverage:

- `integration.SmooTargetS19MdesWithoutQdesUsesFirstColumn`
- `integration.SmooTargetS19AfterQdesUsesFirstColumn`

Focused result: unit `4/4`, integration `2/2`, 0 failures.

The unit tests assert the fixed first-column selection, `KQTARG`
independence and the unchanged `QGAMM` value. The integration tests compare
exact `DUMP` bytes and FORMAT 1200/1210 records against the stock 6.996
oracle.

## Oracle evidence

External audit area: `xfoil-6.996/s19/`.

Four stock 6.996 cases were run three times each in separate empty working
directories with `LC_ALL=C`:

| Case | Product state | Repetitions | Result |
|---|---|---|---|
| `smoo-mdes-160` | MDES `SMOO`, no QDES, 160/0.15 | 3 | byte-identical |
| `smoo-qdes-mdes-160` | QDES, blank, MDES `SMOO`, 160/0.15 | 3 | byte-identical |
| `smoo-mdes-default` | MDES `SMOO`, no QDES, product defaults | 3 | byte-identical |
| `smoo-qdes-mdes-default` | QDES, blank, MDES `SMOO`, product defaults | 3 | byte-identical |

The integration tests use the default panel state and assert:

```text
   0   0.000000  -0.036881   0.630534  -0.065507
   1   0.899565  -0.003197   0.658267  -0.029856
```

for the no-QDES case and:

```text
   1   0.899560  -0.003182   0.658265  -0.029848
```

for the QDES-then-MDES case. The matching stock console records are:

```text
 Qspec 1 :  alpha =   0.0000    CL =   0.267221    CM =  -0.056649
 Qspec 1 :  alpha =  -0.0948    CL =   0.255625    CM =  -0.056490
```

No oracle file is copied into the repository.

## Linux and gate results

```text
Full suite, GCC Debug:           2080/2080
Full suite, GCC Release:         2080/2080
Full suite, Clang Debug:         2080/2080
Full suite, Clang Release:       2080/2080
Full suite, ASan/UBSan:          2080/2080
Reference verification:            236/236
Reference determinism:             236/236
Harness families:                   16/16
S19 focused unit:                    4/4
S19 focused integration:             2/2
Valgrind representative matrix:     PASS
Valgrind focused S19 unit:           4/4
Valgrind focused S19 integration:    2/2
Permanent gates:                    PASS
Existing product goldens changed:      0
Existing 6.99 references changed:      0
```

The separate `make test-core` target was also checked against an unchanged
archive of the starting HEAD. It fails to link there because the target's
core-only link set omits app symbols already referenced by
`TestSupport.cpp` and `IoTests.cpp`. This is a pre-existing target/link-set
issue, not an S19 regression. All core tests are included in the full suite
results above.

## Windows minimal policy

Exactly one focused Windows process is required after the green Linux matrix:

```text
integration.SmooTargetS19
2/2, 0 failures
```

Exactly one focused Windows process was run:

```text
integration.SmooTargetS19
2/2, 0 failures
```

Additional Windows processes: 0. No full Windows suite and no Windows golden
are part of S19.

## Focused R1 acceptance — X6996-S19-R1

R1 was performed after the focused Windows process and before the single S19
implementation commit. It confirms:

- S19 is limited to the MDES `SMOO` first-column correction; no general
  inverse refactor, QDES/MIXED redesign, S20 input change or format/default
  change is included.
- The upstream 6.99/6.996 `xmdes.f` difference is exactly
  `KQSP = KQTARG` to `KQSP = 1`.
- `mdesSmoothTargetSegment` always smooths `QSPEC(:,1)`, independent of
  `KQTARG`; `QGAMM` remains unchanged and the existing `CNCALC`, `QSPCIR`,
  `QSPINT` and report sequence is retained.
- The focused unit tests pass `4/4` and the focused integration tests pass
  `2/2`; exact DUMP bytes and console records match the stock 6.996 oracle.
- The Linux matrix is green.
- Exactly one focused Windows process is green; no further Windows process
  was run.
- No product golden, canonical 6.99 reference, Fortran source, binary,
  patch, product version, banner or global tolerance was changed.
- No push, fetch, amend, tag or release was performed.

## Protected state and publication freeze

- Fortran sources, binaries and patches unchanged.
- `tests/fortran-reference/**` unchanged.
- Product version and banner unchanged; `v1.0.0` unchanged.
- No global tolerance change and no existing golden change.
- No push, fetch, amend, tag or release.

## Final decision

X6996-S19: PASS
X6996-S19-R1: PASS
