# Changelog

All notable changes to XFOIL.next are documented here. The format follows
a plain release log; this project does not advertise a strict
keep-a-changelog layout.

## 1.1.0

Cumulative XFOIL 6.996 integration, based on the accepted 1.0.0 product.

- XFOIL 6.996 capacity and workspace limits, geometry primitives and the
  NACA/NACS 399-point contract.
- GDES and geometry commands, updated paneling defaults, and the accepted
  geometry normalization behaviour.
- Polar text metadata, dump/element metadata and the BLDUMP2/BLDUMP3 plus
  BLOU file contracts.
- State invalidation and sequence behaviour, text/input capacity alignment,
  and the SMOO target correction.
- DXF R12 export and the hidden top-level `DXF` command.
- Cumulative Linux and Windows acceptance of the integrated 6.996 scope.

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
  canonical reference cases with verification and determinism gates, and 16
  focused Fortran harness families.
- The graphical plot and cursor backend is deliberately not included. Plot
  and cursor commands are recognised and report that plotting is not
  available in this build.
