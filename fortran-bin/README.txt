Reference Fortran binary: xfoil

This is the historical XFOIL 6.99 Fortran reference binary. It is the
executable binary oracle used to generate and verify the existing binary-based
canonical reference artifacts for the C++ port. It is NOT the product binary;
the product binary is built from the C++ sources.

Platform:  Linux x86-64.
Source:    built from an earlier local XFOIL 6.99 source state.
Purpose:   historical binary reference and differential validation only.

The archived source tree under fortran-src/xfoil-6.99/ has since been restored
to the original upstream state. This binary is unchanged and remains the
behavior oracle for its existing binary-based references. It is not an exact
rebuild of the restored source tree. Source-based harness references follow the
restored sources; binary-based references remain tied to this binary. See
docs/xfoil-6.996/36-xfoil-699-source-provenance-correction.md.

Integrity:
  The binary's SHA-256 is pinned in fortran-bin/SHA256SUMS:

    f5779d6954d0b8fdc152e0e0633f18732e00b7b69228d561d629bd524bba598c  xfoil

  Verify with:

    sha256sum --check fortran-bin/SHA256SUMS

Reproducibility:
  The historical reference build used GCC 11.4.0 with -O (equivalent to -O1) and
  -fdefault-real-8 (default REAL(8)). The sources contain no DOUBLE
  PRECISION declarations, so -fdefault-double-8 has no effect on this binary.
  The active Orr-Sommerfeld path is fortran-src/xfoil-6.99/osmap.f + fortran-src/xfoil-6.99/osmap.dat
  (REAL*4); the native-precision variant fortran-src/xfoil-6.99/osmap_DP.f +
  fortran-src/xfoil-6.99/osmapDP.dat is an inactive upstream original and is not built
  into or loaded by this binary.

License and provenance:
  This binary is derived from XFOIL 6.99 sources, which are GPL-2.0-or-later
  per their own headers (Copyright (C) 2000 Mark Drela, some jointly with
  Harold Youngren). The archived source tree has since been restored to the
  original upstream state; this binary was not rebuilt. Provenance and
  attribution are recorded in NOTICE and LICENSE at the repository root.
