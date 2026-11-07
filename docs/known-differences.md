# Known differences

This is a deliberately narrow ledger of the controlled differences between
the C++ build and the specific Fortran reference binary. Fixed porting
defects are not listed here; they are pinned as permanent regression tests.

## 1. No plot or cursor backend

There is no graphical backend. Plot and cursor commands are recognised and
report exactly:

```text
Plotting is not available in this build.
```

Session state is unchanged and the current menu is kept. The Fortran binary
may call X11 or abort on a missing display; the C++ contract is the
deterministic unavailability message above.

## 2. Reference-build last-bit class

A small number of diagnostic values in the SCINIT `max(dw)` path, the MAPGEN
`DCNMAX` path, and the source-identical PERT Newton evaluation differ from the
Fortran reference only in the last bits. These differences are attributed to
optimization- and probe-sensitive code generation in the canonical Fortran
reference build. The C++ path is bit-stable between its own debug and release
builds.

This exception applies only to the directly proven diagnostic values in that
execution chain. It must not be extended to other numerical paths.

## 3. Controlled BLF missing-data guard

When boundary-layer data are not usable, the Fortran binary follows an
invalid-memory-access path. The C++ build ends only that error path in a
controlled, mutation-free way with:

```text
 Boundary-layer data are not available.
```

The successful BLF path is proven against Fortran. This guard covers only the
error path.

## 4. EOF handling

Some Fortran prompt paths end at end-of-input with a gfortran runtime error
and a backtrace. The C++ build ends in a controlled way instead (exit 0).
Backtrace addresses are not treated as a deterministic reference.

## 5. GDES SOPPS warning count

For one narrowly classified GDES SOPPS case, the number of warnings is
build-dependent (the reference binary emits a binary-specific count). The
final geometry and the following values agree. The warning count is not a
product contract.

## 6. PLRSUM at zero Reynolds number

At Reynolds number zero, Fortran uses an uninitialized exponent (undefined
behaviour). The C++ build produces a defined value (0) for that single
Re-0 row of the PSUM table instead. This is limited to that row.

## Not listed here

Genuine porting defects that were found and fixed (for example inviscid
PACC/PLXADD dump parity, CSEQ head raw bits, wake capacity growth, and
ASEQ-to-CSEQ Newton-state progression) are permanent regression tests, not
accepted differences.
