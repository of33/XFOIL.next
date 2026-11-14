# Fortran reference basis

The numerical reference basis is the canonical XFOIL 6.99 Fortran
implementation. It is used to validate the C++ port and is never derived
from C++ output.

## `fortran-src/xfoil-6.99/`

The restored original XFOIL 6.99 Fortran sources. They are the highest domain
authority and are kept content-unmodified; C++ changes are not back-ported
into them. They are not compiled or linked in the normal C++ build. The two
previously modified files are restored to their original hashes:

```text
xoper.f  23f0b6354d6c544b2539ee0ad01b9975838da265f59493b026c28326b69f6524
xgdes.f  513d71d2eea3b69b5fdb3544ff506bdda68b2b51c2edbf3703f8ac268c4bd622
```

See `fortran-src/xfoil-6.99/README.md`.

Integrity is pinned by `fortran-src/xfoil-6.99/SHA256SUMS` (48 entries, including the
active and inactive OSMAP artifacts) and by
`fortran-src/xfoil-6.996/SHA256SUMS` (49 entries for the frozen reduced
6.996 baseline). Both manifests are repository-owned metadata:

```sh
make check-fortran-sha          # verifies both source manifests
```

## `fortran-bin/xfoil`

The historical binary oracle: a Linux x86-64 binary built from an earlier
local XFOIL 6.99 source state. It is used only to generate and verify the
existing binary-based canonical reference artifacts. It is not the product
binary. The binary is unchanged and is not an exact rebuild of the restored
archive; source-based harness references now follow the restored sources while
binary-based references remain tied to this binary. Its SHA-256 is pinned in
`fortran-bin/SHA256SUMS`:

```text
f5779d6954d0b8fdc152e0e0633f18732e00b7b69228d561d629bd524bba598c  xfoil
```

See `fortran-bin/README.txt` and [NOTICE](../NOTICE) for the source/binary
separation.

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

Active canonical path: `fortran-src/xfoil-6.99/osmap.f` + `fortran-src/xfoil-6.99/osmap.dat`
(REAL*4). Inactive upstream originals, preserved and SHA-pinned but never
built or loaded: `fortran-src/xfoil-6.99/osmap_DP.f` + `fortran-src/xfoil-6.99/osmapDP.dat`.

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
- The restored Fortran sources remain the higher domain authority; the
  historical binary is the executable oracle for the existing binary-based
  references and generated files.
- Historical C++ golden files are not Fortran references.

## License and provenance

The Fortran sources are GPL-2.0-or-later per their original headers
(Copyright (C) 2000 Mark Drela, some jointly with Harold Youngren) and are
kept unmodified. Provenance is recorded in `NOTICE` and `LICENSE`.
