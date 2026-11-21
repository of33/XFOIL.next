# Changelog

All notable changes to XFOIL.next are documented here. The format follows
a plain release log; this project does not advertise a strict
keep-a-changelog layout.

## Unreleased

- Added an optional Qt 6.12.x GUI build skeleton with the embeddable
  `xfoil::gui::XFoilWidget` shell, vendored QCustomPlot 2.1.1, offscreen Qt
  tests, a public-header link check and GUI dependency/vendoring gates.
- Added the GUI definition model and input surface: memory/NACA/NACS
  geometry, paneling, operating conditions, alpha/CL/alpha-sequence targets,
  an owning validated run snapshot and a geometry/panel preview.
- Revised the GUI workspace to a 1x4 definition row, a Cp/Airfoil plus
  seven-tab result area and a fixed right results column with point-selection
  placeholders, a 4x3 n/a matrix and setup diagnostics.
- Added the G3 GUI analysis worker: one persistent worker thread and one
  worker-owned session run alpha, CL and detailed alpha-sequence analyses
  asynchronously. Owning results, progress, cancellation, structured errors,
  job/generation stale-result rejection, busy setup guards and safe worker
  shutdown are covered by tests. Cp/Cf/polar plot binding and matrix
  calculation remain later work.
- Added the optional owning detailed alpha sequence: each executed point
  keeps its compact polar row plus its full Cp surface and, for viscous
  points, its Cf distribution and CDF. The compact sequence API remains
  unchanged, and both forms share one sequence/solver path.
- Adopted the current 2.0 GUI development baseline: GCC/MinGW 16.2.0 Rev7
  on Windows and Qt 6.12.0 for Linux and Windows GUI qualification. The
  1.2.1 release evidence remains on GCC/MinGW 16.1.0 Rev5.
- This is not a finished GUI application and does not change the product
  version, CLI behavior, solver behavior or library API.

## 1.2.1 — 2026-11-20

- Added reproducible portable x86_64 command-line packages for Linux and
  Windows.
- The Linux executable statically links the GCC C++ runtime and requires
  glibc 2.35 or newer.
- The Windows executable statically links the MinGW/GCC runtime and runs
  without MSYS2 or additional runtime DLLs.
- Added deterministic package generation, SHA-256 checksums, binary
  dependency audits and third-party runtime notices.
- No aerodynamic, numerical, command-line, library API or file-format
  behavior changed.

No library SDK binary assets are included.

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
