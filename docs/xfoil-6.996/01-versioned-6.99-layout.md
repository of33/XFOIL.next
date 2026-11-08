# XFOIL 6.996 — U1 versioned XFOIL 6.99 source layout

## Purpose

Move the canonical XFOIL 6.99 source set from the flat `fortran-src/`
directory into the versioned directory `fortran-src/xfoil-6.99/` without
changing source bytes, file modes, numerical behavior, Golden data, reference
data, or the reference binary.

`fortran-src/xfoil-6.996/` is a sibling baseline and must remain unchanged.

## Target layout

```text
fortran-src/
  xfoil-6.99/
    SHA256SUMS
    BLPAR.INC
    ... (48 files)
  xfoil-6.996/
    SHA256SUMS
    ... (49 files)
```

The authoritative list of the 48 XFOIL 6.99 files is
`tools/fortran-provenance.json`, entry `xfoil-6.99`. `SHA256SUMS` moves with
the source set and is not itself one of the 48 hashed files.

## Moved files (48)

```text
BLPAR.INC   CIRCLE.INC  PINDEX.INC  PPLOT.INC   PXPLOT.INC
README.md   XBL.INC     XDES.INC    XFOIL.INC   aread.f
blplot.f    blu.f       dplot.f     frplot.f    getarg.f
gui.f       iopol.f     modify.f    naca.f      ntcalc.f
osmap.dat   osmap.f     osmapDP.dat osmap_DP.f  plutil.f
pntops.f    polfit.f    polplt.f    pplot.f     profil.f
pxplot.f    sort.f      spline.f    userio.f    xbl.f
xblsys.f    xfoil.f     xgdes.f     xgeom.f     xmdes.f
xoper.f     xpanel.f    xplots.f    xpol.f      xqdes.f
xsolve.f    xtcam.f     xutils.f
```

`fortran-src/README.md` is one of the 48 protected source files. It moves
byte-identically; its internal historical path statements are not edited.

## Path consumers

| Consumer | Required change |
|---|---|
| `Makefile` (`check-fortran-sha`) | `fortran-src/xfoil-6.99/SHA256SUMS` |
| `.gitattributes` | binary OSMAP paths under `fortran-src/xfoil-6.99/` |
| `tools/build-*.sh` | compile sources and OSMAP data from `fortran-src/xfoil-6.99/` |
| `tools/check-osmap-selection.sh` | contract comments name `fortran-src/xfoil-6.99/` |
| `tools/check-release-inventory.py` | OSMAP hashes and `SHA256SUMS` required path |
| `tools/check-public-docs.py` | public `fortran-src/README.md` path |
| `tools/extract-fortran-routines.py` | source paths are supplied by callers; no path change |
| `tests/core/IoTests.cpp`, `tests/core/InverseTests.cpp` | read `fortran-src/xfoil-6.99/osmap.dat` |
| `README.md`, `NOTICE`, `CONTRIBUTING.md` | canonical source path |
| `docs/fortran-reference.md`, `docs/numerical-validation.md` | canonical source and OSMAP paths |
| `fortran-bin/README.txt` | source paths |
| `tests/fortran-reference/README.md`, `tests/data/core-polar-io-reference/README.md`, `sample/README.md` | canonical source paths |
| `docs/xfoil-6.996/README.md` | target layout block reflects the achieved layout |
| `tools/fortran-provenance.json` | 6.99 `root` becomes `fortran-src/xfoil-6.99` |

Historical artifacts are not rewritten:

- `docs/xfoil-6.996/00-inventory.md`
- `docs/xfoil-6.996/00-inventory_report.md`
- `fortran-src/xfoil-6.99/README.md` (protected bytes)

`tools/fortran-ref.sh` uses only `fortran-bin/xfoil` and the external
reference cases; it needs no change. `fortran-bin/xfoil` and
`fortran-bin/xfoil-6.996` are not rebuilt or moved.

## Migration sequence

1. Record the pre-move inventory (path, mode, size, SHA-256) externally.
2. Create `fortran-src/xfoil-6.99/`.
3. Move the 48 manifest files and `SHA256SUMS` with `git mv`.
4. Rewrite only the path column in the moved `SHA256SUMS`; hashes stay.
5. Change only the `xfoil-6.99` `root` in the provenance manifest.
6. Update the path consumers listed above.
7. Run the integrity, gate, C++ test, reference, determinism and harness
   matrix.
8. Verify the post-move inventory is byte-identical to the pre-move
   inventory and that `fortran-src/xfoil-6.996/` is unchanged.

## Invariants

- 48/48 moved files byte-identical and mode-identical.
- 48/48 `SHA256SUMS` entries verify from the repository root.
- No copy, symlink, duplicate or mixed-version source set.
- No 6.996 file is edited, moved or used by the 6.99 build.
- No C++ product code, Golden data or reference data changes.
- `fortran-bin/xfoil` remains
  `f5779d6954d0b8fdc152e0e0633f18732e00b7b69228d561d629bd524bba598c`.
- `fortran-bin/xfoil-6.996` remains
  `000b0468d9a7f4fce42d6fac24bd14147d7d326c8a9035909c8e9f98d180f50e`.

## Acceptance

- `check-dependencies`, `check-fortran-sha`, `check-release-inventory`,
  `check-license-headers`, `check-public-docs`, `check-core-link`,
  `check-session-link` pass.
- C++ suites: core 414, unit 1668, integration 245, regression 5, batch 47,
  full 1968.
- Reference: 236/236; determinism: 236/236; harnesses: 16/16.
- Working tree ends with no staged paths; U1 does not commit or push.
