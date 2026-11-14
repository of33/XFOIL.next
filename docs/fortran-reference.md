# Fortran reference basis

The numerical reference basis is the canonical XFOIL 6.99 Fortran
implementation. It is used to validate the C++ port and is never derived
from C++ output.

## `fortran-src/`

The original XFOIL 6.99 Fortran sources. They are the highest domain
authority and are kept content-unmodified; C++ changes are not back-ported
into them. The 1.0.1 source correction restored `xoper.f` and `xgdes.f`
byte-identically to the external XFOIL 6.99 originals. They are not compiled
or linked in the normal C++ build. See `fortran-src/README.md`.

Integrity is pinned by `fortran-src/SHA256SUMS` (48 entries, including the
active and inactive OSMAP artifacts):

```sh
make check-fortran-sha          # sha256sum --check fortran-src/SHA256SUMS
```

## `fortran-bin/xfoil`

The retained historical 1.0.0 reference oracle: a Linux x86-64 binary built
from an earlier local XFOIL 6.99 source state. That state contained the local
OFS extension in `xoper.f` and the READR adjustment in `xgdes.f`. The binary
still emits the historical OFS line and its archived reference outputs remain
historical compatibility records. It was not rebuilt from the restored
upstream sources in the 1.0.1 correction and is not the product binary. Its
SHA-256 is pinned in `fortran-bin/SHA256SUMS`:

```text
f5779d6954d0b8fdc152e0e0633f18732e00b7b69228d561d629bd524bba598c  xfoil
```

See `fortran-bin/README.txt`.

## `tests/fortran-reference/`

The reproducible canonical reference basis: 236 cases across categories
(geometry, paneling, panel-coord, geometry-edit, aero-inviscid, aero-viscous,
state, polar). The machine-readable manifest `manifest.tsv` has 237 lines
(1 header + 236 cases). Each case stores its input, metadata, raw stdout and
stderr, generated files, run info, and an artifact SHA. See
`tests/fortran-reference/README.md`.

## Modes: generate, verify, determinism, manifest

The tool `tools/fortran-ref.sh` runs cases in isolated empty temporary
directories with `LC_ALL=C` and a forced timeout.

```text
verify      run a case and compare against stored references; changes nothing
determinism run a case twice in separate directories; byte-compare; changes nothing
generate    overwrite reference artifacts; only after explicit domain authorization
manifest    rebuild manifest.tsv
```

`make test-reference` and `make test-reference-determinism` run the
non-mutating modes over all cases.

## No-normalization rule

Fortran output is not normalized. Numeric values, signs, rounding,
exponential form, field widths, spacing, line order, and
diagnostic/convergence messages are kept unchanged. There is no global
signed-zero normalization. A coordinate input file is never altered to match
a visible computed result (for example, a file may contain `0.00000 0.00000`
at the leading edge while `LOAD` prints `-0.00000 -0.00000` for the same
contour).

The only documented name exception (contents stay untouched): the original
`VISCAL` (`xoper.f:3061-3077`) renders the untripped `xtrip/c = 1.0` as
`CHAR(10 + '0') = ':'` and produces `:00.bl` on POSIX. Because Windows
rejects `:` in file names, the harness runner captures this single known
name under the intended portable name `100.bl`; no other name is rewritten
and no file content is ever changed.

## Graphics deactivation preamble

Every `commands.in` begins with exactly three lines that disable graphics
before the real commands:

```text
PLOP
g
<blank>
```

`PLOP` opens the plot-options menu, `g` sets the graphics-enable flag to
false, and the blank line leaves the menu.

## Manifest `role` field

The manifest carries a documented `role` field (default `-`):

- `negative-control` — GDES cases without `EXEC`: only the buffer is edited
  and the current contour is unchanged (correct Fortran behaviour, not a
  numerical edit reference).
- `positive-edit-reference` — GDES cases with `EXEC`: the edited current
  contour is written.

## Active and inactive OSMAP

Active canonical path: `fortran-src/osmap.f` + `fortran-src/osmap.dat`
(REAL*4). Inactive upstream originals, preserved and SHA-pinned but never
built or loaded: `fortran-src/osmap_DP.f` + `fortran-src/osmapDP.dat`.

## Harness builds

Sixteen focused Fortran harness families rebuild small Fortran programs and
compare their output byte-for-byte against stored references:

```sh
make test-harnesses          # 16 families, --check only (never --write)
```

Each builder internally verifies the active OSMAP selection and never
compiles `osmap_DP.f`. Harness rebuilds require `gfortran`.

## Rules for reference changes

- Never change reference data merely to make a test pass.
- Reference values come only from the Fortran oracle, never from C++ output.
- `generate` is used only after explicit domain authorization.
- The Fortran sources remain the higher domain authority; the binary is the
  executable oracle for deterministic visible output and generated files.
- Historical C++ golden files are not Fortran references.

## License and provenance

The Fortran sources are GPL-2.0-or-later per their original headers
(Copyright (C) 2000 Mark Drela, some jointly with Harold Youngren) and are
kept unmodified. Provenance is recorded in `NOTICE` and `LICENSE`.
