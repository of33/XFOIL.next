# S02 - SEGSPLD fix and spline/solver workspaces report

Status: S02a PASS (`X6996-S02a` and `X6996-S02a-R1` accepted); S02 overall
IN PROGRESS until S02b.

## Starting state

- HEAD before S02a: `6c2287490b33371c27a4e5c8593af6d603095bcc`.
- `origin/main`: `58bca7d17f847e1aef6b1c8d3d4e167f40f69945`.
- Ahead/behind: `0 12`.
- Tag `v1.0.0` unchanged.
- Working tree before S02a: clean, 0 staged.
- Audit data: external only, under
  `.audit-xfoil-next/xfoil-6.996/s02a/`.

## Fortran diff

The compared routines are:

- `fortran-src/xfoil-6.99/spline.f`, `SUBROUTINE SEGSPLD(X,XS,S,N,XS1,XS2)`
  at line 562;
- `fortran-src/xfoil-6.996/spline.f`,
  `SUBROUTINE SEGSPLD(X,XS,S,N,XS1,XS2)` at line 561.

The routine diff is stored externally as `segspld-fortran.diff`; the full
file diff is `spline-fortran-full.diff`.

The signatures and `SPLIND` formula are unchanged. The 6.996 change is:

- local `XS1T`/`XS2T` are initialized to `-999.0` for every segment;
- the caller's `XS1` is copied only if the segment starts at the global first
  point (`ISEG0 == 1`);
- the caller's `XS2` is copied only if the segment ends at the global last
  point (`ISEG0+NSEG-1 == N`);
- internal segment joints therefore use the zero-third-derivative condition
  instead of the global specified derivatives;
- the STOP text changes from `SEGSPL:` to `SEGSPLD:`.

6.99 passed `XS1`/`XS2` to every segment. Its internal segment end therefore
used the global endpoint derivative value (for the segmented test case
`XS(3)=12`) instead of the segment-local zero-third condition. 6.996 yields
`XS(3)=-1/3` and `XS(4)=-10` for the same input. The index/neighbor logic and
the tridiagonal formula were not changed.

Segment detection remains:

```text
S(ISEG) == S(ISEG+1)  ->  segment boundary
segment 1: ISEG0..ISEG
next segment: ISEG+1..N
```

The first/last global points remain protected by the duplicate STOP checks.
At an exact duplicated parameter, `DEVAL` selects the right interval; the
one-sided left derivative is observed immediately below the joint.

## C++ mapping

| Fortran | C++ | Header | Existing tests | Result |
|---|---|---|---|---|
| `SEGSPL` | `xfoil::core::math::segspl` | `include/xfoil/core/math/Spline.h` | `MathSegspl`, paneling/geometry consumers | unchanged |
| `SEGSPLD` | `xfoil::core::math::segsplD` | `include/xfoil/core/math/Spline.h` | `MathSegsplD`, new boundary/no-mutation tests | fixed |
| `SEGSPL` | `xfoil::math::spline::segmentedSplineDerivatives` | `include/xfoil/math/Spline.h` | `SegmentedSplineAcceptsInnerDuplicateParameter` | unchanged |
| `SEGSPLD` | `xfoil::math::spline::segmentedSplineDerivativesWithEndpoints` | `include/xfoil/math/Spline.h` | legacy `SplineTests`, new boundary/no-mutation tests | fixed |
| `SPLIND` | `xfoil::core::math::splind` and legacy `cubicSplineDerivatives` | both spline headers | existing SPLIND tests | unchanged |

The canonical product path is the 0-based `xfoil::core::math` mirror. The
existing 1-based `xfoil::math::spline` mirror was corrected in the same minimal
way so that both existing implementations follow the same 6.996 contract. No
parallel implementation was added.

## Product change

Both existing segmented-spline routines now select endpoint derivatives per
segment:

- first segment: global `XS1`;
- last segment: global `XS2`;
- internal segment joints: `-999.0` for the relevant end.

No public API, return type or error contract changed. No non-segmented spline
path changed. No new tolerance was introduced. `-Werror` remains active.

## H-S02-SEGSPLD harness

The external harness is
`.audit-xfoil-next/xfoil-6.996/s02a/segspld_harness.f`. It is compiled
against both frozen baselines with gfortran 13.3 using
`-std=legacy -O0 -ffp-contract=off -fdefault-real-8 -fdefault-double-8`.
No repository reference file is written or overwritten.

Covered cases:

- non-segmented control;
- segmented data with interior duplicate `S(3) == S(4)`;
- point immediately before the joint, exact joint and immediately after;
- derivative at the end of the first segment and start of the second;
- global first and last boundaries;
- a case where 6.99 and 6.996 differ.

Input:

```text
S = {0, 1, 2, 2, 3}
X = {0, 1, 1, 1, 2}
XS1 = 2, XS2 = 12
```

Derivative arrays:

| Position | 6.99 | 6.996 |
|---|---:|---:|
| `XS(1)` | 2.0 | 2.0 |
| `XS(2)` | -2.75 | 1/3 |
| `XS(3)` | 12.0 | -1/3 |
| `XS(4)` | 2.0 | -10.0 |
| `XS(5)` | 12.0 | 12.0 |

Adjacent evaluation values:

| Quantity | 6.99 | 6.996 |
|---|---:|---:|
| `DEVAL(1.999999)` | 11.99995750002776 | -0.3333326666666666 |
| `DEVAL(2.0)` | 2.0 | -10.0 |
| `DEVAL(2.000001)` | 1.999974000035996 | -9.999977999999999 |
| `SEVAL(2.0)` | 1.0 | 1.0 |
| `D2VAL(2.0)` | -26.0 | 22.0 |

The non-segmented control gives the same derivatives in both baselines:
`{2, 7/6, -13/6, 12}`.

A temporary C++ probe compiled against both existing mirrors reproduces the
6.996 values exactly (to the printed 15-digit representation), including the
left/right joint neighborhoods.

## Tests

Added focused tests:

- `unit.MathSegsplDGlobalEndpointsOnly` (canonical 0-based mirror);
- `unit.MathSegsplDRejectsInvalidInputsWithoutMutation`;
- `unit.SegmentedSplineUsesExplicitDerivativesOnlyAtGlobalEnds` (legacy mirror);
- `unit.SegmentedSplineWithEndpointsRejectsWithoutMutation`.

The existing 6.99-capacity and non-regression tests remain green. No Golden or
existing Fortran reference file was changed.

Linux results:

| Check | Result |
|---|---|
| GCC 13.3 Debug `make test` | 1974/1974 pass |
| GCC 13.3 Release `build/release/xfoil_tests` | 1974/1974 pass |
| Clang 18.1.3 Debug | 1974/1974 pass |
| Clang 18.1.3 Release | 1974/1974 pass |
| `make test-core` | 417/417 pass |
| `make test-unit` | 1674/1674 pass |
| `make test-integration` | 245/245 pass |
| `make test-regression` | 5/5 pass |
| `make test-batch` | 47/47 pass |
| `make test-sanitize` (ASan/UBSan) | 1974/1974 pass |
| `make test-valgrind` plus four focused filters | pass, no definite leaks |
| `make test-reference` | 236/236 pass |
| `make test-reference-determinism` | 236/236 pass |
| `make test-harnesses` | 16 families, 0 failures |
| Gates | dependencies, Fortran SHA, release inventory, license, public docs, core/session link checks pass |

## S02b boundary

Untouched in S02a:

- `NMAX` remains 1000 in the core numerics mirror;
- `NVX`/`LUDCMP` remains 500;
- the full 1202-point CAMB/INPC path remains blocked by the existing spline
  limit and is deferred to S02b;
- S01 capacities `IQX=601`, `NPX=20` remain unchanged.

## Protected data and Git state

No changes to:

- `fortran-src/**`;
- `fortran-bin/**`;
- `patches/**`;
- `tests/golden/**`;
- existing `tests/fortran-reference/**`;
- product version or banner.

Current changes are limited to the two existing spline mirrors, their
headers, focused tests and the S02 documents. The single local S02a commit is
created after acceptance; no push, tag or release is created. S02b remains
pending.

## S02a checklist

- [x] 6.99/6.996 SEGSPLD diff analyzed.
- [x] Existing C++ mappings identified.
- [x] Minimal product fix applied to both existing mirrors.
- [x] Focused tests added, including no-mutation checks.
- [x] External H-S02-SEGSPLD comparison against both Fortran baselines.
- [x] Linux full/compiler/sanitizer/Valgrind/reference evidence.
- [x] Windows focused spline/unit/core evidence.
- [x] Focused R1 acceptance.
- [ ] Create the single local S02a commit.


## Windows evidence

Source: `.audit-xfoil-next/xfoil-6.996/s02a/logs.tgz`.

- Windows focused tests: `1/1` each for
  `unit.MathSegsplDGlobalEndpointsOnly`,
  `unit.MathSegsplDRejectsInvalidInputsWithoutMutation`,
  `unit.SegmentedSplineUsesExplicitDerivativesOnlyAtGlobalEnds`, and
  `unit.SegmentedSplineWithEndpointsRejectsWithoutMutation`.
- Windows `test-unit`: `1674/1674` PASS.
- Windows `test-core`: `417/417` PASS.
- The accepted Windows baseline (`g++` and `uname`) is unchanged; no new
  system recording was needed.
- The complete Windows matrix is intentionally bundled with S02b because S02a
  is a deterministic low-level mathematical correction.

## S02a — SEGSPLD segment-end derivative fix

Decision: **PASS**

- Fortran 6.99 passes the global `XS1`/`XS2` to every segment. Fortran
  6.996 uses `-999.0` at internal segment ends and keeps the caller values
  only at the global first/last point. The spline formula and index logic are
  unchanged.
- The existing C++ mirrors `core::math::segsplD` and
  `xfoil::math::spline::segmentedSplineDerivativesWithEndpoints` now implement
  the 6.996 endpoint selection. No new spline implementation, no API change.
- Focused tests cover the non-segmented control, interior duplicate joint,
  one-sided neighborhoods, exact joint, both global ends, too-small input,
  duplicate endpoints and no partial mutation.
- The external H-S02-SEGSPLD harness shows the deliberate 6.99/6.996
  difference: segmented `XS` is `{2, -2.75, 12, 2, 12}` for 6.99 and
  `{2, 1/3, -1/3, -10, 12}` for 6.996. The C++ probe matches the 6.996 values
  at the printed precision.
- Linux full/core/unit/integration/regression/batch, GCC/Clang Debug/Release,
  ASan/UBSan, Valgrind, reference, determinism and harness evidence is green.
- Windows focused, unit and core evidence is green.
- No Fortran source, binary, Golden or existing reference file changed.
- S02b remains untouched: `NMAX=1000`, `NVX`/`LUDCMP=500`, no 1202-point
  activation.

## Focused acceptance — X6996-S02a-R1-Focused-SEGSPLD-Acceptance

Decision: **PASS**

The focused review confirms:

- the code diff is limited to the two existing SEGSPLD mirrors and their
  headers;
- the tests use expected values derived from the frozen 6.996 Fortran harness,
  not from the C++ implementation;
- error paths remain controlled and non-mutating for the tested invalid
  inputs;
- no S02b capacity was pulled forward;
- no forbidden path was changed;
- Windows focused/unit/core logs are complete for the S02a scope.

The only remaining action is the local S02a commit. S02 remains IN PROGRESS
until S02b is accepted.
