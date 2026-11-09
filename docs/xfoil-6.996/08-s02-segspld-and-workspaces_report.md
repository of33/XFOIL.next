# S02 - SEGSPLD fix and spline/solver workspaces report

Status: PASS - `X6996-S02a`, `X6996-S02a-R1`, `X6996-S02b` and
`X6996-S02b-R1` accepted. Decision: X6996-S02: PASS.

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

## S02b — NMAX, NVX and LUDCMP workspaces

Decision: PASS

### Fortran changes

Compared files and external diffs:

- `fortran-src/xfoil-6.99/spline.f` vs `.../xfoil-6.996/spline.f`
  (`spline.diff`);
- `fortran-src/xfoil-6.99/xsolve.f` vs `.../xfoil-6.996/xsolve.f`
  (`xsolve.diff`).

| Symbol | 6.99 | 6.996 | Fortran routine | Meaning | C++ mapping |
|---|---:|---:|---|---|---|
| `NMAX` | 1000 | 2500 | `SPLINE`, `SPLIND` | cubic spline coefficient workspace | `core::splineWorkspaceCapacity`, legacy `kMaxSplinePoints` |
| `NVX` | 500 | 515 | `LUDCMP` | LU scaling workspace | `core::linearSolverWorkspaceCapacity`, legacy LU scaling array |
| `KMAX` | 32 | 32 | `SPLNXY` | local four-point scratch | unchanged |
| `NSIZ` | caller | caller | `LUDCMP`, `BAKSUB` | matrix dimension | span lengths |

The `LUDCMP` pivot-loop change from an arithmetic `IF` to
`IF (ABS(...) .GT. ABS(...))` is syntactically equivalent; no formula or
operation order changed. `BAKSUB` has no `NVX` parameter and required no
workspace change.

### C++ limits and changed paths

New central limits in `include/xfoil/core/Limits.h`:

```text
splineWorkspaceCapacity         = 2500
linearSolverWorkspaceCapacity   = 515
```

Changed paths:

- `src/core/numerics/Spline.cpp`: canonical 0-based `spline`/`splind`
  limit 1000 -> 2500; coefficient workspace moved from fixed stack arrays
  to heap-backed vectors.
- `src/core/math/Spline.cpp`: existing 1-based mirror limit 600 -> 2500;
  fixed workspace size 601 -> 2501 (reserved index 0 plus 2500 active
  indices).
- `src/core/numerics/LinearAlgebra.cpp`: canonical `ludcmp` limit
  500 -> 515 and error text updated.
- `src/core/math/LinearAlgebra.cpp`: legacy `factorLuInPlace` scaling
  workspace and active limit now use 515 (array 516 for 1-based indices).
- Header comments updated to the 6.996 limits.

The active length remains separate from storage capacity. Limits are checked
before any workspace write or matrix mutation. No global replacement of
numeric literals was performed.

### Memory and stack

- Canonical spline workspace: three heap vectors of 2500 doubles, about
  60,000 bytes per `spline`/`splind` call.
- Legacy 1-based spline mirror: fixed 2501-double arrays; measured stack
  frame 60,568 bytes for `cubicSplineDerivatives`.
- Canonical `ludcmp`: scaling array 515 doubles, about 4,120 bytes on stack.
- Legacy LU mirror: scaling array 516 doubles, about 4,128 bytes; measured
  `factorLuInPlace` stack frame 4,584 bytes.
- No state object size changes; active lengths continue to use dynamic state
  containers.

### Focused H-S02-NMAX / H-S02-NVX harness

External only:
`.audit-xfoil-next/xfoil-6.996/s02b/nmax_harness.f`,
`nvx_harness.f`, `nmax-699`, `nmax-6996`, `nvx-699`, `nvx-6996`.
Built with gfortran 13.3 and `-fdefault-real-8 -fdefault-double-8`.

NMAX results:

| N | 6.99 | 6.996 |
|---:|---|---|
| 1000 | OK | OK |
| 1001 | overflow | OK |
| 1202 | overflow | OK |
| 2499 | overflow | OK |
| 2500 | overflow | OK |
| 2501 | overflow | overflow |

NVX results:

| N | 6.99 | 6.996 |
|---:|---|---|
| 500 | OK | OK |
| 501 | overflow | OK |
| 514 | overflow | OK |
| 515 | overflow | OK |
| 516 | overflow | overflow |

### CAMB-INPC full path

`CoreCambInpcAcceptsFullTwoIqxPath` feeds exactly `2*IQX = 1202` points.
The canonical INPC loop ends at 1202 without consuming a further blank line;
the next line is the menu command. The test uses `CLR` directly after the
1202 points and confirms no `try again` and a reset added-camber length of 2.
No 1203rd point is consumed by INPC. The first actual spline-invalid count is
2501 and is rejected without output mutation in
`MathSplineWorkspaceLimits`.

No independent CAMB capacity blocker remains for the 1202 path.

### Tests and non-regression

New/updated focused tests:

- `unit.MathSplineWorkspaceLimits` (1000, 1001, 1202, 2499, 2500 accepted;
  2501 rejected without mutation);
- `unit.MathLudcmpNvxLimits` (500, 501, 514, 515 accepted with consistent
  pivots/solution; 516 rejected without mutation);
- `unit.CubicSplineWorkspaceLimits` (legacy mirror: 600, 601, 1000, 1001,
  1202, 2499, 2500 accepted; 2501 rejected without mutation);
- `unit.LuFactorNvxBoundary` (legacy LU mirror: 515 accepted, 516 rejected);
- `unit.CoreCambInpcAcceptsFullTwoIqxPath` (1202-point CAMB path).

All S02a tests remain unchanged and green.

Linux results:

| Check | Result |
|---|---|
| GCC 13.3 Debug `make test` | 1976/1976 pass |
| GCC 13.3 Release | 1976/1976 pass |
| Clang 18.1.3 Debug | 1976/1976 pass |
| Clang 18.1.3 Release | 1976/1976 pass |
| `make test-core` | 418/418 pass |
| `make test-unit` | 1676/1676 pass |
| `make test-integration` | 245/245 pass |
| `make test-regression` | 5/5 pass |
| `make test-batch` | 47/47 pass |
| `make test-sanitize` | 1976/1976 pass |
| `make test-valgrind` plus focused workspace/S02a filters | pass, no definite leaks |
| `make test-reference` | 236/236 pass |
| `make test-reference-determinism` | 236/236 pass |
| `make test-harnesses` | 16 families, 0 failures |
| Gates | dependencies, Fortran SHA, release inventory, license, public docs, core/session link checks pass |

### Protected data and Git state

No changes to `fortran-src/**`, `fortran-bin/**`, `patches/**`,
`tests/golden/**`, existing canonical reference data, product version or
banner. Current changes are limited to central math limits, the two existing
math/numerics mirrors, focused tests and the S02 documents. The single local
S02b commit is created after acceptance; no push, tag or release is created.

### S02b checklist

- [x] Fortran NMAX/NVX/LUDCMP/BAKSUB contracts analyzed.
- [x] C++ mappings classified; no blind numeric replacement.
- [x] Canonical and legacy workspace limits updated.
- [x] Focused NMAX/NVX/LUDCMP boundary tests added.
- [x] CAMB-INPC 1202 path completed and tested.
- [x] S02a non-regression tests remain green.
- [x] Linux full/compiler/sanitizer/Valgrind/reference evidence.
- [x] Windows focused/unit/core/integration and release build evidence.
- [x] S02b R1 focused acceptance.
- [ ] Create the single local S02b commit.


### Windows evidence

Source: `.audit-xfoil-next/xfoil-6.996/s02b/log.tgz`.

- Windows Debug full suite: `1976/1976` PASS.
- Windows `test-unit`: `1676/1676` PASS.
- Windows `test-core`: `418/418` PASS.
- Windows `test-integration`: `245/245` PASS.
- Windows focused NMAX/NVX/LUDCMP/CAMB and S02a tests: `1/1` each PASS.
- Windows Release build/link: PASS (`build/release/xfoil.next.exe`).
- The accepted Windows baseline (`g++`/`uname`) is unchanged; no new system
  recording was needed.

## Focused acceptance — X6996-S02b-R1-Focused-Workspace-Acceptance

Decision: **PASS**

The focused review confirms:

- `NMAX` is 2500 for `SPLINE`/`SPLIND`; 2500 is accepted, 2501 is rejected
  before mutation; 1202 is accepted.
- `NVX` is 515 for `LUDCMP`; 515 is accepted with consistent pivot/solution
  data, 516 is rejected before mutation; `BAKSUB` remains dimension-compatible
  and needs no `NVX` workspace.
- The canonical and legacy C++ mirrors agree with the frozen Fortran boundary
  behavior; no blind numeric replacement occurred.
- `CAMB-INPC` processes exactly 1202 points end-to-end; no independent
  capacity blocker remains for that path.
- All S02a tests and expected values remain unchanged.
- ASan/UBSan and Valgrind reached the new boundary tests without findings.
- No Fortran source, binary, Golden or existing canonical reference data was
  changed.
- No forbidden path was changed.

### S02 close-out

Both S02 stages are complete. The S02a SEGSPLD contract remains unchanged by
S02b. The only remaining action is the local S02b commit.

Decision: **X6996-S02: PASS**
