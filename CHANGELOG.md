# Changelog

All notable changes to XFOIL.next are documented here. The format follows
a plain release log; this project does not advertise a strict
keep-a-changelog layout.

## Unreleased

- Added reproducible portable runtime packaging for Linux x86-64 and
  Windows x86-64. The Linux asset is a single ELF with static
  libstdc++/libgcc and a glibc 2.35 floor built in a pinned Ubuntu 22.04
  builder. The Windows asset is a single PE with the static MinGW-w64
  runtime and no MSYS2/MSYS DLL dependency. Archives contain `LICENSE`,
  `NOTICE` and the executable; `SHA256SUMS` is separate. The product
  version is unchanged.

## 1.2.0

The 1.2.0 release makes the validated XFOIL.next core available as an
installable C++20 in-memory library while preserving the complete native CLI.

- Added the public C++20 library API with the stateful `xfoil::Session`,
  the one-shot `xfoil::analyze()`, owning result values and structured
  diagnostics. The compatibility facades remain available.
- Added optional synchronous progress reporting and a separate
  `CancellationToken`. Callback failure and cancellation are distinct, and
  the host process remains in control.
- Added public Cp and Cf results, the owning `SkinFrictionDistribution`, and
  complete operating-point drag results with canonical `CDF` and owning Cf
  data that remain valid after a one-shot call returns.
- Completed live alpha-sequence `PolarPoint` rows with `CD`, `CDp`, `CDF`,
  XFOIL `CDv`, the executed iteration count and status. Stored polar points
  remain compact and expose the stored CDp/CDv columns without a canonical
  CDF or iteration count.
- Added installable static and shared libraries, public headers, direct
  build-tree linking, local-prefix and staged installation, optional
  pkg-config metadata and static/shared consumer checks on Linux and Windows.
- Added the standalone `library-demo` external consumer and the finished
  XFOIL.next Library User Manual, version 1.2.0.
- Added the public `CITATION.cff` metadata with the Zenodo concept DOI.
- Verified the memory-only contract with isolated sessions, the armed
  static/shared no-I/O proof, library dependency and symbol checks, and the
  concurrency diagnostic. Concurrent use of one session is not supported.
- Preserved full CLI command, batch, file-format and numerical parity with
  the validated product.
- Validated the cumulative Linux and Windows matrix with 2266/2266 tests in
  the full Debug and Release suites.

Limits remain explicit: no stable cross-compiler C++ ABI, no C ABI, no CMake
package, no language bindings, no guaranteed parallel execution, and no
graphical plot backend.

## 1.1.0

Cumulative XFOIL 6.996 integration, based on the validated 1.0.0 product.

- XFOIL 6.996 capacity and workspace limits, geometry primitives and the
  NACA/NACS 399-point contract.
- GDES and geometry commands, updated paneling defaults, and the validated
  geometry normalization behaviour.
- Polar text metadata, dump/element metadata and the BLDUMP2/BLDUMP3 plus
  BLOU file contracts.
- State invalidation and sequence behaviour, text/input capacity alignment,
  and the SMOO target correction.
- DXF R12 export and the hidden top-level `DXF` command.
- Installable static/shared C++ library artifacts, staged install,
  pkg-config metadata, public export annotations, and external consumer
  smoke tests.
- A verified memory-only library contract: isolated sessions, an armed
  static/shared no-I/O proof, library dependency/symbol checks, and a
  diagnostic concurrency/TSan test. Concurrent use of one session is not
  supported, and parallel use of separate sessions is not part of the
  supported public contract.
- Cumulative Linux and Windows validation of the integrated 6.996 scope.

Explicit non-goals remain SVG (`NIMP-02`), NTS/LPPORT/NSPL (`NIMP-01`) and
the deferred headless plot backend (`G-PLOT-HEADLESS`).

## 1.0.0

First public release.

- Complete port of the canonical non-graphical XFOIL 6.99 functionality to
  C++20. The interactive command set, session layer, and numerical core are
  validated directly against the original Fortran sources and the reference
  Fortran binary.
- Modern, session-based public C++ API: `xfoil::XFoil` and
  `xfoil::XFoilSession`, with value-typed configuration, results, geometry,
  and an instance-scoped cancellation token.
- Native interactive XFOIL command-line interface with the canonical menus,
  prompts, file handling, and `xfoil.def` settings semantics.
- Native non-interactive batch mode (`--batch`) that reuses the same command
  interpreter, session layer, and numerical core, with a documented job
  format, CLI options, exit codes, prefix policy, and machine-readable
  `results.dat`.
- Fortran validation infrastructure: SHA-pinned reference sources, 236
  canonical reference cases with verification and determinism checks, and 16
  Fortran harness families.
- The graphical plot and cursor backend is deliberately not included. Plot
  and cursor commands are recognised and report that plotting is not
  available in this build.
