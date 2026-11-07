# Changelog

All notable changes to XFOIL.next are documented here. The format follows
a plain release log; this project does not advertise a strict
keep-a-changelog layout.

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
