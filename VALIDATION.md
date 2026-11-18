# Validation baseline

This document records the compiler and platform baseline used to validate
the XFOIL.next 1.2.0 release. Compiler and runtime differences can affect
floating-point evaluation and the byte representation of generated
numerical reference artifacts.

## Release

- Product: XFOIL.next
- Version: 1.2.0

## Tested release candidate

- HEAD: `27fb0227400be5430368a4d1e87d390f9e5fd2af`
- Tree: `9a2f01ea4c69a151b61cdedb807a1a7b79e6068d`
- Manual edition: 1.2.0
- Manual pages: 109
- Manual PDF SHA-256: `e5c2aec673a71bf63110251f72f83f87107b9c0e17581fe53bab356ac88995f9`

## Canonical Linux reference platform

- Distribution: Ubuntu 24.04.5 LTS
- Kernel: Linux 7.0.0-42-generic
- Architecture: x86_64
- GNU Fortran: 13.3.0
- GNU C++: 13.3.0
- C library: GNU libc 2.39
- Linker: GNU ld (GNU Binutils for Ubuntu) 2.42
- C++ standard: C++20
- C++ build policy: warnings enabled and treated as errors (`-Werror`)
- Fortran reference flags include: `-std=legacy -fdefault-real-8`

The exact distribution and GNU libc values above must match the recorded
validation host. Linux is the canonical platform for the bit-exact and
byte-exact reference contracts.

## Additional C++ validation

| Compiler | Version | Debug | Release | Sanitizer | Valgrind |
|---|---:|---:|---:|---:|---:|
| GCC | 13.3.0 | PASS | PASS | PASS | PASS |
| GCC | 16.2.0 | PASS | PASS | — | — |
| Clang | 18.1.3 | PASS | PASS | — | — |

The GCC 16 validation used the released GNU Compiler Collection point
release GCC 16.2.0, installed side-by-side on the validation host (not
an Ubuntu distribution package). All listed builds use the repository's
warning policy, including warnings treated as errors.

## Reference contracts

- Fortran SHA: 48/48
- Reference verification: 236/236
- Determinism: 236/236
- Harness families: 16/16
- Full suite: 2266/2266

## Windows validation

The release tree was also validated manually with:

- Windows 11
- MSYS2 UCRT64
- GCC/MinGW 16.1.0 Rev5
- GNU Make 4.4.1
- Debug and Release builds with warnings treated as errors
- Core tests: 465/465
- Full debug suite: 2266/2266
- Unit tests: 1868/1868
- Integration tests: 343/343
- Regression tests: 5/5
- Batch tests: 47/47
- Release full suite: 2266/2266
- PACC diagnostic: 1/1

Windows uses narrow, measured, test-specific semantic contracts for the
documented floating-point differences. Linux retains the strict bit-exact
and byte-exact reference contracts. No Windows-specific golden files are
used.

The 1.2.0 release preparation changes only the product version metadata and
its banner goldens. It does not change the accepted cumulative numerical test
basis or the reference artifacts.

## Hosted Windows evaluation (not part of the accepted baseline)

The GitHub-hosted Windows Server 2022 evaluation used MSYS2 UCRT64
GCC/MinGW 16.2.0 Rev4. Core, unit, integration and regression checks
passed after enforcing LF fixture checkouts, but two of the 47 batch
tests followed substantially different convergence paths and produced
large final-result deviations:
`batch.ClarkYNativeBatchMatchesGolden` (88 instead of 33 iterations in
one results row) and `batch.Naca0012NativeBatchMatchesGolden` (final
polar deviations of up to approximately 0.147). This toolchain is
therefore not part of the accepted Windows validation baseline.

No tolerance and no golden file was changed for this finding, and no
claim is made that GCC 16.2 is generally defective; the investigation
remains open. The hosted evaluation runs only as a manual, on-demand
diagnostic workflow (`windows-diagnostic.yml`), never as a release
gate. A durable hosted pin to the accepted GCC/MinGW 16.1.0 Rev5
toolchain could not be established because MSYS2 removes old
rolling-repository packages without any guaranteed archive stability.

## Regeneration policy

The shipped golden and reference artifacts are immutable release inputs.
They must not be regenerated merely because another compiler or runtime
produces different floating-point bytes.

Any regeneration or replacement of reference artifacts requires a
separate, reviewed reference migration. The documented canonical GNU
Fortran baseline must be used for canonical reference verification.
