# XFOIL.next standalone library demo

The directory is named `library-demo`; the executable remains
`xfoil-next-lib-demo`.

`xfoil-next-lib-demo` is an external C++20 consumer of the installed
XFOIL.next library. The application reads an airfoil coordinate file and
passes the coordinates only through public in-memory API values. The
library performs no file or console I/O.

## Build

Use the installed package through `pkg-config`:

```sh
PKG_CONFIG_PATH=/usr/local/lib/pkgconfig make -C library-demo
```

For a staged installation, point both variables at the stage:

```sh
PKG_CONFIG_PATH="$PWD/build/l6-stage/usr/lib/pkgconfig" \
PKG_CONFIG_SYSROOT_DIR="$PWD/build/l6-stage" \
make -C library-demo
```

The default build is shared. A static build uses the installed static
archive:

```sh
PKG_CONFIG_PATH=/usr/local/lib/pkgconfig \
make -C library-demo LINK_MODE=static
```

The executable is written to `library-demo/build/<mode>/`. The demo does not
include repository sources or build against the source tree.

If `pkg-config` is unavailable, point the Makefile at the installed
prefix instead:

```sh
make -C library-demo \
  XFOIL_NEXT_PREFIX=/usr/local \
  LINK_MODE=shared
```

`XFOIL_NEXT_PREFIX` may be overridden by `XFOIL_NEXT_INCLUDE` and
`XFOIL_NEXT_LIBDIR`. A staged prefix is
`<stage>/usr`; this fallback still uses only installed headers and
libraries.

## Usage

```text
xfoil-next-lib-demo
  --airfoil PATH
  --re VALUE
  --alpha-start DEG
  --alpha-end DEG
  --alpha-step DEG
  --cl VALUE
  [--mach VALUE]
  [--iterations COUNT]
  [--panels COUNT]
```

All six analysis parameters except the optional values are required.
`--alpha-end` is exclusive. `--re 0` selects the inviscid library path;
`--re > 0` selects the viscous path with that Reynolds number. Negative
or non-finite Reynolds numbers are command-line errors.

When omitted, `--mach`, `--iterations`, and `--panels` keep the current
public `xfoil::AnalysisConfig{}` and `xfoil::PanelingOptions{}` defaults.
The current defaults are Mach 0, 100 iterations, and 240 panel nodes.

Example:

```sh
xfoil-next-lib-demo \
  --airfoil library-demo/profiles/naca0012.dat \
  --re 1000000 \
  --alpha-start -2 \
  --alpha-end 8 \
  --alpha-step 1 \
  --cl 0.6
```

The demo prints the effective configuration, the alpha sequence table, and
one specified-CL table row. Non-converged points are printed, and the
process returns exit code 6 when any executed point did not converge.
`xfoil::PolarPoint` does not expose a per-point iteration count, so the
iteration column is `n/a` for sequence rows; the specified-CL row reports
`xfoil::OperatingPointData::iterations`.

## Exit codes

```text
0  sequence and CL target executed and converged
2  command-line error
3  profile file open or parse error
4  geometry or paneling error
5  configuration or analysis error without a valid result
6  executed results exist but at least one did not converge,
   or the sequence ended partially
```

## Profile format

The demo accepts the minimal two-column coordinate format:

- an optional first non-numeric name line;
- one `x y` pair per data line;
- blank lines and lines whose first non-whitespace character is `#`;
- no extra columns;
- at least three finite coordinate pairs.

The name is taken from the first text line, otherwise from the file stem.
The file is read only by the demo, never by the library. No temporary file
is created.

## Example profile

`profiles/naca0012.dat` is a 160-point NACA 0012 coordinate sequence with
the name line `NACA 0012`. It is derived from the canonical XFOIL 6.99
NACA generator output retained in
`tests/fortran-reference/cases/geometry/naca-0012/generated/naca0012.dat`
and is reused here as plain example data. It has no independent numerical
reference role.

## Library defaults and state

The demo constructs `xfoil::AnalysisConfig{}` and
`xfoil::PanelingOptions{}` first and overrides only explicitly supplied
options. The alpha sequence and the specified-CL point use the same
`xfoil::Session` and configuration. The demo contains no solver,
sequence, interpolation, or aerodynamic recomputation logic.
