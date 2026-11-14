Reference Fortran binary: xfoil

This is the retained historical XFOIL 6.99 reference binary from the 1.0.0
validation baseline. It is the executable oracle used to generate and verify
the archived canonical reference artifacts for the C++ port. It is NOT the
product binary; the product binary is built from the C++ sources.

Platform:  Linux x86-64.
Source:    built from an earlier local XFOIL 6.99 source state. That state
           contained the local OFS extension in xoper.f and the READR
           adjustment in xgdes.f. The archived Fortran sources in
           fortran-src/ were restored byte-identically to the external XFOIL
           6.99 originals in the 1.0.1 source correction; this binary was not
           rebuilt from that restored tree.
Purpose:   historical reference and differential validation only.

Integrity:
  The binary's SHA-256 is pinned in fortran-bin/SHA256SUMS:

    f5779d6954d0b8fdc152e0e0633f18732e00b7b69228d561d629bd524bba598c  xfoil

  Verify with:

    sha256sum --check fortran-bin/SHA256SUMS

Historical source state:
  xoper.f  SHA-256 303191f22c055480561c2a037447ae62c859c7fa70fb21b1c8a1216ff34bba22
  xgdes.f  SHA-256 4d32ef2f75d2069e1708f32ba789940517c512369de82eaa242cf994792dcf16

  The retained binary still emits the historical local OFS line. Its
  existing reference outputs remain historical compatibility records. No
  reference migration and no binary rebuild are part of this correction.

Reproducibility:
  The historical reference build used GCC 11.4.0 with -O (equivalent to
  -O1) and -fdefault-real-8 (default REAL(8)). The current restored upstream
  sources do not contain the local OFS or READR changes, so rebuilding them
  would not reproduce this binary's historical behavior or exact hash.
  The active Orr-Sommerfeld path is fortran-src/osmap.f + fortran-src/osmap.dat
  (REAL*4); the native-precision variant fortran-src/osmap_DP.f +
  fortran-src/osmapDP.dat is an inactive upstream original and is not built
  into or loaded by this binary.

License and provenance:
  This binary was built from XFOIL 6.99 sources, which are GPL-2.0-or-later
  per their own headers (Copyright (C) 2000 Mark Drela, some jointly with
  Harold Youngren). Provenance and attribution are recorded in NOTICE and
  LICENSE at the repository root.
