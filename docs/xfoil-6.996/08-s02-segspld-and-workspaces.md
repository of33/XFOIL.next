# S02 - SEGSPLD fix and spline/solver workspaces

Status: `S02a` PASS; `S02b` not started. S02 overall IN PROGRESS.

## Scope

S02 is the bounded numerics slice from the 6.996 integration plan. It is
split into two independent implementation stages:

- **S02a - SEGSPLD segment-end derivative fix**
  - Analyze the 6.99/6.996 `SEGSPLD` change in `spline.f`.
  - Apply the minimal C++ correction to the existing segmented-spline
    implementations.
  - Add focused boundary and interior-joint tests.
  - Compare the result against both frozen Fortran baselines.
- **S02b - NMAX/NVX/LUDCMP workspaces**
  - Raise the local spline `NMAX` from 1000 to 2500.
  - Raise `NVX` from 500 to 515.
  - Adjust the `LUDCMP` workspace to the larger system size.
  - Update the full 1202-point `CAMB`/`INPC` path and its capacity tests.
  - Keep the S02a contract unchanged.

S02b is deliberately not part of this implementation stage. In particular,
S02a does not change any workspace capacity, `IQX`, `NPX`, or the existing
1202-point blocker.

## S02a Fortran contract

The 6.99 and 6.996 signatures are identical:

```fortran
SUBROUTINE SEGSPLD(X,XS,S,N,XS1,XS2)
```

`SEGSPLD` splines `X(S)` piecewise. A boundary between segments is defined by
identical consecutive `S` values, `S(ISEG) == S(ISEG+1)`. Each segment is
solved with `SPLIND`; `XS1`/`XS2` are specified endpoint derivatives, with the
canonical `-999.0` sentinel selecting the zero-third-derivative end condition.

The 6.996 change is not a change of the spline formula. It changes which
endpoint derivatives are passed to each segment:

- 6.99 passes the caller's `XS1`/`XS2` to every segment, including internal
  segment joints. Internal segment ends therefore incorrectly receive the
  global endpoint derivative constraints.
- 6.996 initializes local `XS1T`/`XS2T` to `-999.0` for every segment and
  copies the caller's `XS1` only when the segment starts at the global first
  point, and the caller's `XS2` only when the segment ends at the global last
  point.
- The first segment therefore keeps the specified first global derivative;
  the last segment keeps the specified last global derivative; internal
  segment ends use the zero-third-derivative condition.
- The loop/index logic and the `SPLIND` formula are unchanged. The fix is a
  segment-local boundary-condition selection, not a new spline algorithm.

The STOP messages are also corrected from `SEGSPL:` to `SEGSPLD:`; the C++
API already reports controlled errors and does not expose the Fortran STOP
texts.

## C++ mapping

Two existing C++ mirrors implement the same Fortran routines:

| Fortran | C++ | Header | Existing tests | S02a change |
|---|---|---|---|---|
| `SEGSPL` | `xfoil::core::math::segspl` | `include/xfoil/core/math/Spline.h` | `MathSegspl`, paneling/geometry consumers | none |
| `SEGSPLD` | `xfoil::core::math::segsplD` | `include/xfoil/core/math/Spline.h` | `MathSegsplD` plus new boundary test | apply 6.996 endpoint selection |
| `SEGSPL` | `xfoil::math::spline::segmentedSplineDerivatives` | `include/xfoil/math/Spline.h` | `SegmentedSplineAcceptsInnerDuplicateParameter` | none |
| `SEGSPLD` | `xfoil::math::spline::segmentedSplineDerivativesWithEndpoints` | `include/xfoil/math/Spline.h` | legacy `SplineTests` plus new boundary test | apply 6.996 endpoint selection |
| `SPLIND` | `xfoil::core::math::splind` / legacy `cubicSplineDerivatives` | both spline headers | existing SPLIND tests | none |

The canonical product path is the 0-based `xfoil::core::math` implementation.
The 1-based `xfoil::math::spline` implementation is an existing mirror used by
the legacy geometry algorithms and unit tests. Both existing mirrors are
minimally corrected; no third implementation is introduced.

## Focused test contract

The new focused cases use:

```text
S = {0, 1, 2, 2, 3}
X = {0, 1, 1, 1, 2}
XS1 = 2, XS2 = 12
```

The duplicate `S(3) == S(4)` separates two segments. The 6.996 expected
derivatives are:

```text
XS = {2, 1/3, -1/3, -10, 12}
```

The non-segmented control uses:

```text
S = {0, 1, 2, 3}
X = {0, 1, 1.5, 2.5}
XS1 = 2, XS2 = 12
XS = {2, 7/6, -13/6, 12}
```

Both cases cover the global first/last ends, the one-sided derivative at the
joint, and the global-endpoint preservation. Existing duplicate/error tests
remain unchanged.

## Acceptance

- Focused `SEGSPLD` tests pass in both C++ mirrors.
- `H-S02-SEGSPLD` compares 6.99 and 6.996 and confirms the expected difference.
- `make test-core`, full suites, GCC/Clang Debug/Release, ASan/UBSan,
  Valgrind, reference, determinism and harness gates pass.
- Windows focused spline/unit/core evidence is recorded.
- No Fortran, binary, Golden or existing reference file changes.
- S02b remains open and its workspace capacities remain untouched.

## Commit boundary

S02a is one local commit:

```text
Integrate XFOIL 6.996 SEGSPLD fix
```

No push, tag or release. S02 remains IN PROGRESS until S02b is complete.
