# Changelog

All notable changes to XFOIL.next are documented here. The format follows
a plain release log; this project does not advertise a strict
keep-a-changelog layout.

## Unreleased

- Added optional synchronous progress reporting for geometry, paneling,
  analysis phases, viscous iterations and sequence points. The separate
  cancellation token is now observed at safe viscous-iteration boundaries;
  callback failures return `progress.callbackFailure` without terminating
  the host process.
- Added `Session::currentSkinFriction()` and the owning
  `SkinFrictionDistribution` value type. The projection exposes separate
  upper/lower boundary-layer stations, excludes wake stations, preserves
  the existing `tau/(0.5*qinf^2)` sign and performs no solver work or I/O.
- Completed `OperatingPointData` with optional `dragCoefficientFriction`
  (`CDF`) and an optional owning `skinFriction` distribution. One-shot
  `analyze()` results now retain Cf after the temporary session is gone.
  `currentSkinFriction()` remains and uses the same projection. Inviscid
  points leave both optionals empty.
- Completed live sequence `PolarPoint` rows with optional pressure/form
  drag `CDp`, friction-only `CDF`, XFOIL `CDv`, and the executed iteration
  count. `CDF` comes directly from the canonical final VISCAL state and is
  never reconstructed as `CD - CDp` or integrated from Cf. Sequence points
  remain compact scalar rows without Cp/Cf distributions; stored polar
  points expose the stored CDp/CDv columns but no canonical CDF or
  iteration count.
- Extended the standalone `library-demo` to show the CL target's friction
  drag `CDF` and compact upper/lower Cf station counts from
  `OperatingPointData`; inviscid runs show `n/a`.
- Extended the standalone `library-demo` alpha-sequence table with the
  per-point `CDp`, friction-only `CDF`, XFOIL `CDv` and canonical iteration
  count from `PolarPoint`. The CL target table keeps its `CDF` and Cf
  station-count output.
- Published the XFOIL.next Library User Manual as a PDF. The LaTeX source
  remains local and is not part of the public repository. The manual
  documents product version 1.1.0, the current public API, progress and
  cancellation, the skin-friction projection, and the standalone
  `library-demo` consumer.

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
