# Canonical XFOIL 6.99 Fortran reference basis

This directory holds the reproducible canonical reference basis used to
validate the direct port of the XFOIL 6.99 Fortran implementation to the C++
core.

The reference basis was generated with the provided Fortran binary:

```text
fortran-bin/xfoil
  SHA-256: f5779d6954d0b8fdc152e0e0633f18732e00b7b69228d561d629bd524bba598c
```

The Fortran sources under `fortran-src/` remain the highest domain authority.
The binary is the executable reference oracle.

The reference basis is independent of any C++ golden files under
`tests/golden/`. C++ results are not target values for the core.

## Current state

- **236** reference cases
- **232** `canonical-exact`
- **4** `canonical-raw-only` (`aero-viscous/aseq-nseqex`,
  `aero-viscous/visc-nonconverging`, `geometry-edit/msave-behavior`,
  `polar/polar-nonconverging`)
- **236/236** deterministic and verified (`all-verify`, 0 errors)
- `manifest.tsv`: 237 lines (1 header + 236 cases)
- **16** Fortran harness builder families (see `make test-harnesses`)

## Layout

```text
tests/fortran-reference/
  cases/<category>/<case-name>/
    commands.in          stdin input lines (with PLOP/g graphics deactivation)
    input/               optional input files (copied into the working dir)
    metadata.txt         key: value metadata (purpose, status, parameters, ...)
    stdout.raw.txt       unchanged standard output
    stderr.raw.txt       unchanged standard error
    generated/           all files produced by the Fortran program
    run-info.txt         exit status, timeout, duration, binary checksum, locale
    artifacts.sha256     SHA-256 of the canonical artifact set
  manifest.tsv           machine-readable manifest of all cases
```

## Categories and cases

- `geometry/`        NACA 0012 / 2412 / 23012, loaded Clark-Y profile;
                     FCPP037 LOAD cases `load-naca2412-buffer` /
                     `load-naca0012-buffer` (LE x,y output)
- `paneling/`        standard paneling, PPAR (non-standard + limit), PCOP
- `panel-coord/`     panel coordinates (PANE/PPAR/PCOP/Clamp) and ABCOPY limit
                     cases `abcopy-365-at-limit` / `abcopy-366-above-limit`
- `geometry-edit/`   GDES (CANG, BEND, TSET, ADEG+DERO, FLAP), INTX, MSAVE
                     (success `msave-success`, error `msave-behavior`)
- `aero-inviscid/`   inviscid single points and short sequences
- `aero-viscous/`    viscous single points, sequences, non-converging cases
- `state/`           state behaviour (ascending/descending, target point,
                     inviscid->viscous, CL)
- `polar/`           polar and dump artifacts (inviscid and viscous)

## Manifest `role` field

The manifest (`manifest.tsv`) carries a documented `role` field (default `-`)
that marks the domain meaning of a case:

- `negative-control`     GDES cases without `EXEC` (`gdes-tset`, `gdes-flap`,
                         `gdes-rotate-dero`): only the buffer is edited and the
                         current contour stays unchanged (correct Fortran
                         behaviour; not a numerical edit reference).
- `positive-edit-reference` GDES cases with `EXEC` (`gdes-tset-exec`,
                         `gdes-flap-exec`, `gdes-rotate-exec`,
                         `gdes-dero-exec`): the edited current contour is
                         written.
- `fcpp037-reference`    LOAD cases `load-naca2412-buffer` /
                         `load-naca0012-buffer` (canonical LE x,y output).
- `fcpp009-abcopy-limit` ABCOPY limit cases `abcopy-365-at-limit` /
                         `abcopy-366-above-limit`.

Other role values mark additional specific reference kinds (for example
`state-frame-reference` and per-file save/load policy references).

## Reproduction

The tool `tools/fortran-ref.sh` runs cases in separate, empty temporary
directories with `LC_ALL=C` and a forced timeout.

```sh
# verify all cases against the stored references (no overwrite)
tools/fortran-ref.sh all-verify

# determinism: each case twice in separate directories, byte compare
tools/fortran-ref.sh all-determinism

# rebuild the manifest
tools/fortran-ref.sh manifest
```

Single cases:

```sh
tools/fortran-ref.sh generate    <case>
tools/fortran-ref.sh verify      <case>
tools/fortran-ref.sh determinism <case>
```

`<case>` is the relative case path including the category, for example
`aero-viscous/visc-2412-re1e6`.

`generate` overwrites reference artifacts and is used only after explicit
domain authorization. `verify` and `determinism` change nothing.

## Graphics deactivation

Every `commands.in` begins with exactly three lines that disable graphics
before the real commands:

```text
PLOP
g
<blank>
```

`PLOP` opens the plot-options menu, `g` sets the graphics-enable flag to
false, and the blank line leaves the menu.

## No normalization

The Fortran output is **not** normalized. Numeric values, signs, rounding,
exponential form, field widths, spacing, line order, and
diagnostic/convergence messages stay unchanged. There is no global
signed-zero normalization.

## Portable artifact names

Artifact **contents** are never normalized. For artifact **file names**
there is exactly one documented exception: the original XFOIL 6.99
`VISCAL` routine (`xoper.f:3061-3077`) encodes the trip position
`xtrip/c * 100` into three characters, and for the untripped value `1.0`
the tens digit 10 renders as `CHAR(10 + '0') = ':'`, producing `:00.bl`
on POSIX. Windows rejects `:` in file names. The harness runner
(`tools/fortran-ref.sh`) therefore maps exactly this one name to the
technically intended portable name `100.bl` during capture — the file
content is never changed, and no other produced name is ever rewritten.
The Fortran sources themselves remain untouched.

## Authority and protection

- `verify` and `determinism` change **no** reference; they run the case in a
  temporary directory and compare.
- `generate` **overwrites** reference artifacts and may be used **only after
  domain authorization**.
- The Fortran sources under `fortran-src/` remain the **higher domain
  authority**.
- The binary `fortran-bin/xfoil` is the **executable reference oracle** for
  deterministic visible output and generated files.
- Historical C++ golden files under `tests/golden/` are **not** a Fortran
  domain reference.
- Batch goldens remain **protected** (unchanged).

See also `docs/fortran-reference.md` and `fortran-src/README.md`.

## Leading edge (LE x,y)

The discrete coordinate point of an input file and the spline LE position
computed by LOAD are different quantities. A coordinate file may contain
`0.00000 0.00000` at the LE while LOAD, after SCALC/SEGSPL/LEFIND/SEVAL,
prints `-0.00000 -0.00000` for the same contour. The input coordinates are
therefore not changed to match the visible LOAD result.
