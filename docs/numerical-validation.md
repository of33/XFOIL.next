# Numerical validation

The numerical behaviour of XFOIL.next is validated against the canonical
Fortran implementation.

## Authority

The Fortran sources under `fortran-src/xfoil-6.99/` are the numerical authority. The
precompiled `fortran-bin/xfoil` is the executable reference oracle for
deterministic visible output and generated files.

Reference target values are taken exclusively from the Fortran reference.

## Precision model

The canonical Fortran reference build uses default REAL(8)
(`-fdefault-real-8`). Accordingly, the C++ port computes in `double`.

The reference binary was built with GCC 11.4.0 using `-O` (equivalent to
`-O1`) and `-fdefault-real-8`. The focused Fortran harnesses are rebuilt with
`gfortran` using `-fdefault-real-8 -fdefault-double-8`; the
`-fdefault-double-8` flag has no effect on the reference binary because the
sources contain no `DOUBLE PRECISION` declarations.

The one explicit `REAL*4` special case is the Orr-Sommerfeld database
`fortran-src/xfoil-6.99/osmap.dat`, which is read as native single precision on the
active path. This is a documented, directly proven exception, not a general
rule.

## OSMAP path

The active, canonical runtime path is:

```text
fortran-src/xfoil-6.99/osmap.f  +  fortran-src/xfoil-6.99/osmap.dat
```

The native-precision toolchain variant is preserved as an upstream original
but is inactive: it is never compiled, linked, or loaded.

```text
fortran-src/xfoil-6.99/osmap_DP.f   (inactive, not built)
fortran-src/xfoil-6.99/osmapDP.dat  (inactive, not loaded)
```

A static check confirms that `osmap_DP` appears in no build source, rule, or
harness builder as a compilation target.

## Validation assets and gates

```text
Fortran source integrity (SHA256SUMS): 48 (6.99) + 49 (6.996) files
Canonical reference cases:            236
Fortran harness families:              16
```

Gates:

```sh
make check-fortran-sha              # 48/48 and 49/49 source integrity
make test-reference                 # 236/236 verification
make test-reference-determinism     # 236/236 byte-deterministic reruns
make test-harnesses                 # 16/16 harness rebuild/check
```

Reference verification and determinism use the precompiled Fortran binary and
do not require a Fortran compiler. Harness rebuilds require `gfortran`.
Reference regeneration is used only after explicit domain authorization;
verification and determinism never modify references.

## Compiler, mode, and tool coverage

The suite is validated with GCC and Clang, in debug, release, and sanitizer
builds, with `-Werror` and no warning suppressions. AddressSanitizer and
UndefinedBehaviorSanitizer runs are clean. Representative API, session,
solver, CLI, and batch paths are additionally run under Valgrind with no
definite leaks or errors.

## Real-defect regressions

Genuine porting defects found during development were fixed and are pinned as
permanent product regression tests (not as accepted differences). Examples,
named by product behaviour:

- inviscid PACC / PLXADD dump byte parity;
- CSEQ point-head raw bits via Fortran-faithful CLCALC association and
  radian carry;
- wake capacity growth after wake expansion;
- ASEQ-to-CSEQ Newton-state progression.

These remain as regression tests; they are not listed as known differences.

## Porting discipline

The port is 0-based in C++ against 1-based Fortran arrays, with the index
mapping documented per ported unit. Formulas, constants, floating-point
operation order, loop and summation order, tolerances, iteration limits, and
convergence criteria are not changed without a separately proven defect and a
Fortran-backed justification.
