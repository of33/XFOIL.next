Reference Fortran binary: xfoil

This is the canonical XFOIL 6.99 Fortran reference binary. It is the
executable reference oracle used to generate and verify canonical reference
artifacts for the C++ port. It is NOT the product binary; the product binary
is built from the C++ sources.

Platform:  Linux x86-64.
Source:    built from the canonical sources under fortran-src/.
Purpose:   reference and differential validation only.

Integrity:
  The binary's SHA-256 is pinned in fortran-bin/SHA256SUMS:

    f5779d6954d0b8fdc152e0e0633f18732e00b7b69228d561d629bd524bba598c  xfoil

  Verify with:

    sha256sum --check fortran-bin/SHA256SUMS

Reproducibility:
  The reference build used GCC 11.4.0 with -O (equivalent to -O1) and
  -fdefault-real-8 (default REAL(8)). The sources contain no DOUBLE
  PRECISION declarations, so -fdefault-double-8 has no effect on this binary.
  The active Orr-Sommerfeld path is fortran-src/osmap.f + fortran-src/osmap.dat
  (REAL*4); the native-precision variant fortran-src/osmap_DP.f +
  fortran-src/osmapDP.dat is an inactive upstream original and is not built
  into or loaded by this binary.

License and provenance:
  This binary is built from the original XFOIL 6.99 sources, which are
  GPL-2.0-or-later per their own headers (Copyright (C) 2000 Mark Drela, some
  jointly with Harold Youngren). Provenance and attribution are recorded in
  NOTICE and LICENSE at the repository root.
