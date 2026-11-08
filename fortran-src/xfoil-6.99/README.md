# Original XFOIL Fortran reference sources

These files are the original XFOIL 6.99 Fortran sources. They serve as the
behavioural reference for the interactive XFOIL 6.99 command-line interface
and for differential tests.

- Upstream: XFOIL 6.99
- Authors: Mark Drela, Harold Youngren
- License: GPL-2.0-or-later, according to the original source file headers
- Use in this project: reference and differential tests only
- Normal C++ build: these files are not compiled or linked

The sources are kept content-unmodified. Changes made to the C++ port are
not back-ported into this reference copy.

## OSMAP artifacts

The Orr-Sommerfeld lookup is provided in two variants in the XFOIL 6.99
distribution. This project uses only the REAL*4 variant.

Active, canonical runtime path:

```text
fortran-src/osmap.f   +   fortran-src/osmap.dat
```

Inactive original alternative (native-precision toolchain variant), kept as
an upstream original but never compiled, linked, or loaded by this project:

```text
fortran-src/osmap_DP.f   +   fortran-src/osmapDP.dat
```

The `osmap_DP.f` / `osmapDP.dat` pair is not part of the active build and is
not offered as a runtime option.

## Integrity

`fortran-src/SHA256SUMS` is the integrity inventory for this directory. It
covers both the active OSMAP pair (`osmap.f`, `osmap.dat`) and the inactive
original pair (`osmap_DP.f`, `osmapDP.dat`), together with all other
reference sources. Verify with:

```text
sha256sum --check fortran-src/SHA256SUMS
```
