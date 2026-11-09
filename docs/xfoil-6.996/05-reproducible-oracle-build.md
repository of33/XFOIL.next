# U5 — Reproducible XFOIL 6.996 oracle build

## Purpose

Establish and document a reproducible build contract for the diagnostic
XFOIL 6.996 executable with the existing canonical Linux toolchain, and
diagnose the polar-dump abort observed in U4. U5 does not integrate anything
into the C++ product and does not promote 6.996 to a canonical oracle.

## Scope

- Reconstruct the full upstream build from the local `xfoil6.996.tgz`
  archive (B1).
- Build the 49 frozen repository sources with an external plot library (B2).
- Analyse headless-link feasibility without adopting a new stub (B3).
- Determine the exact compatibility flags and their scope.
- Measure byte and semantic reproducibility.
- Compare B1/B2 binaries with the existing diagnostic binary.
- Re-run a focused U4 behavior subset against every built binary.
- Diagnose the polar-dump abort and separate build, runtime and source
  effects.
- Define the OSMAP build/runtime contract.
- Provide a repository-owned build adapter if B2 is sufficiently reproducible.

## Explicit non-goals

- No change to the 49 frozen upstream sources.
- No full plot library or full upstream archive in the public repository.
- No replacement of 6.99 Goldens or references.
- No new canonical 6.996 Golden or reference.
- No tolerance change.
- No C++ product-code integration.
- No product-version or banner change.
- No `osmap.dat` added as a 6.996 upstream file.
- No unchecked replacement of the existing diagnostic binary.
- No compiler or compiler-baseline change.
- No push, tag or publication.
- No claim that a reconstructed binary is canonical merely because it builds.

## Protected source and binary baselines

- XFOIL 6.99 frozen sources: 48/48 hashes green.
- XFOIL 6.996 frozen sources: 49/49 hashes green.
- Canonical 6.99 binary:
  `f5779d6954d0b8fdc152e0e0633f18732e00b7b69228d561d629bd524bba598c`
- Existing diagnostic 6.996 binary:
  `000b0468d9a7f4fce42d6fac24bd14147d7d326c8a9035909c8e9f98d180f50e`
- Local archive `xfoil6.996.tgz`:
  `0feb71b58070d8514d830fcb0b609a10a11399d362493d7b9bbfb7343dff3f23`
- All extracted upstream files and all build products remain external under
  `/local/Olaf/Entwicklung/.audit-xfoil-next/xfoil-6.996/u5/`.
- The existing binary is not replaced by any U5 build.

## Build variants

| Variant | Source basis | Plot library | Purpose |
|---|---|---|---|
| B1 | Full extracted `Xfoil/` archive | Built from archive `Xfoil/plotlib` | Proves the original upstream layout can be built unchanged |
| B2 | 49 files in `fortran-src/xfoil-6.996/` | External `libPlt_gDP.a` from B1 | Proves the frozen subset is sufficient for the main binary |
| B3 | Analysis only | Existing plot API / possible stub | Assesses headless-link feasibility without promoting a new stub |

## Canonical compiler environment

- Ubuntu 24.04.5 LTS, Linux 7.0.0-42-generic, x86-64.
- GNU Fortran 13.3.0 (`gfortran -dumpfullversion -dumpversion` = `13.3.0`).
- GNU C compiler 13.3.0 (`cc -dumpfullversion` = `13.3.0`).
- GNU ld 2.42.
- glibc 2.39.
- The builds use the default system X11 development headers and
  `libX11.so.6`.

## Upstream build description

The archive contains:

- `Xfoil/src/`: XFOIL main sources and include files.
- `Xfoil/osrc/`: `osmap.f`, `getosfile.c`, `x.log`.
- `Xfoil/bin/Makefile_gfortran`: upstream main-program makefile.
- `Xfoil/plotlib/`: the Xplot11 plot library sources and makefiles.

The reproduced B1 build is:

```sh
# plot library, from Xfoil/plotlib
make gfortranDP
# -> libPlt_gDP.a

# XFOIL main program, from Xfoil/bin
cp Makefile_gfortran Makefile_u5
# add -fallow-argument-mismatch only to xgdes.o and blplot.o
make -f Makefile_u5 BINDIR=. INSTALLCMD=true xfoil
```

`BINDIR=.` and `INSTALLCMD=true` keep the binary in the external build root
and avoid the upstream `install -s` step, which would strip it. The linked
object list and link order are:

```sh
gfortran -o xfoil \
  xfoil.o xpanel.o xoper.o xtcam.o xgdes.o xqdes.o xmdes.o \
  xsolve.o xbl.o xblsys.o xpol.o xplots.o pntops.o xgeom.o xutils.o \
  modify.o blplot.o polplt.o aread.o naca.o spline.o plutil.o iopol.o \
  gui.o sort.o dplot.o profil.o userio.o frplot.o ntcalc.o osmap.o \
  getosfile.o ../plotlib/libPlt_gDP.a -L/usr/X11R6/lib -lX11
```

Compile flags are `-O -fdefault-real-8` for the main sources and
`-O2 -fdefault-real-8` for the plot library; `-fallow-argument-mismatch` is
added only to `xgdes.f` and `blplot.f`. The plot library is linked before
`-lX11`.

## Required compatibility flags

| File | Diagnostic without flag | Result with flag | Reason |
|---|---|---|---|
| `xgdes.f` | Error: rank mismatch `READR` scalar/rank-1 at lines 736/750 | Warning, compilation succeeds | Legacy argument association |
| `blplot.f` | Error: rank mismatch `PLGRID` scalar/rank-1 at lines 282/1050 | Warning, compilation succeeds | Legacy argument association |

No other translation unit required `-fallow-argument-mismatch`; the complete
build succeeds with the flag only on these two files. The flag was not applied
globally and warnings were not otherwise suppressed. Both files still emit the
rank-mismatch warning when the flag is used.

## Plot-library dependency

- Plot library: `libPlt_gDP.a`.
- B1 plot-library SHA-256:
  `f11d2e432433bde33f989823fab600c4967675ee9305231ff7d9a73699d52418`.
- Plot-library build: `make gfortranDP` in `Xfoil/plotlib`.
- Fortran flags: `-O2 -fdefault-real-8`.
- C flags: `-O2 -DUNDERSCORE -DDBL_ARGS`.
- Link requirement: `-L/usr/X11R6/lib -lX11` (the system resolves
  `libX11.so.6`; no RPATH/RUNPATH is recorded).
- The library contains `Xwin2.o`, which references 68 X11/gwx symbols.
- The main program requires 47 symbols provided by `libPlt_gDP.a`, including
  `plot_`, `plotabs_`, `plopen_`, `plclose_`, `newpen_`, `xaxis_`, `yaxis_`,
  `annot_`, cursor and clip routines.

## External full-archive reproduction

- The archive was extracted twice; `source-orig` and a fresh extraction were
  byte-identical.
- B1 build 1 and build 2 used fully separate build roots and independently
  built plot libraries.
- Both builds produced:
  - binary SHA-256
    `000b0468d9a7f4fce42d6fac24bd14147d7d326c8a9035909c8e9f98d180f50e`
  - Build-ID `1f07dff6b67a3bf0235e21e1ac957cbd8b5f6d4a`
  - size `860704` bytes
  - plot-library SHA-256
    `f11d2e432433bde33f989823fab600c4967675ee9305231ff7d9a73699d52418`.
- All 45 tracked object hashes are identical between B1 build 1 and build 2.
- The rebuilt binary is byte-identical to the existing diagnostic binary.

## Reduced-source reproduction

B2 uses:

- `buildroot/src` -> `fortran-src/xfoil-6.996/`
- `buildroot/osrc` -> `fortran-src/xfoil-6.996/`
- `buildroot/plotlib` -> the B1 plot-library directory.

No frozen source is copied or modified. B2 build 1 and build 2 both produce the
same binary hash and Build-ID as B1 and the existing diagnostic binary.

- `dplot1.f`, `xqdes1.f` and `x.log` are not needed for the main binary.
- `osmap.dat` is not a build dependency.
- The plot library is an external link dependency.

## Binary reproducibility criteria

- **Byte-reproducible:** all B1/B2 builds produce the same 860704-byte binary,
  SHA-256 and Build-ID.
- Build metadata is identical: ELF64 PIE, `.comment` GCC 13.3.0, no
  RPATH/RUNPATH, NEEDED `libX11.so.6`, `libgfortran.so.5`, `libm.so.6`,
  `libgcc_s.so.1`, `libc.so.6`, 795 symbols and 114 dynamic symbols, not
  stripped.
- Text/data/BSS sizes are identical to the existing binary.
- All object files and the plot-library hash are identical across repetitions.

## Behavioral equivalence criteria

A focused 11-case subset of U4 was run against five binaries:

- existing diagnostic `xfoil-6.996`
- B1 build 1 and build 2
- B2 build 1 and build 2

Cases cover start/quit, defaults/PPAR, NACA buffer, inviscid `OFS:`, sharp TE,
viscous point, QDES, polar write, polar-dump abort, new command and export.
Each run is isolated and repeated. Raw stderr differs for crash cases only
because of ASLR backtrace addresses; normalized stderr, exit code, stdout and
generated file sets are identical. The comparison is external
`build-behavior-comparison.tsv`.

## Polar-dump diagnosis

The diagnostic binary and every B1/B2 rebuild reproducibly abort the U4 polar
dump case:

- exit code 2;
- dump file 0 bytes;
- no timeout or process-start failure;
- same input script for 6.99 and 6.996;
- no canonical 6.99 reference changed.

The abort occurs in `xpol.f` `PLXINI`: a new unformatted dump file is opened,
a `READ` reaches EOF at line 435, control jumps to label 60, and the code
writes the header at line 522 without `REWIND`, `BACKSPACE` or reopening the
unit. GCC 13.3 reports:

`Fortran runtime error: Sequential READ or WRITE not allowed after EOF marker`

A minimal external reproduction with GCC 13.3 shows the same runtime error and
a 0-byte file. The 6.99 binary was built with GCC 11.4 and does not abort on
the U4 case; the 6.996 binary and all rebuilds use GCC 13.3. The element-column
payload is not the trigger: the failure occurs before a point is written. This
is classified as a latent source defect exposed by the GCC 13.3 runtime, with
the 6.996 append-path changes as a close source context but not the immediate
statement. Without a patch overlay it remains a promotion blocker; the empty
dump is not accepted as target behavior.

## Polar-dump patch overlay

The fix is a repository-owned overlay, not an edit of the frozen upstream
source. The patch lives at:

```text
patches/xfoil-6.996/0001-rewind-polar-dump-unit-before-header.patch
```

It changes only `xpol.f` and adds exactly:

```fortran
      REWIND(LU)
```

at label 60, immediately before the new dump header write. The frozen
`fortran-src/xfoil-6.996/` bytes remain identical to upstream.

The build adapter keeps two explicit modes:

- unpatched: reproduce the upstream source and the known diagnostic binary;
- `--apply-polar-dump-patch`: copy the frozen source set to the external build
  root, verify the exact `xpol.f` input hash, apply the patch with
  `patch --batch --fuzz=0 --forward`, and build the patched variant.

The adapter checks:

- the `xpol.f` hash in `fortran-src/xfoil-6.996/SHA256SUMS`;
- the same hash in `tools/fortran-provenance.json`;
- the frozen file's actual hash;
- the patch-file hash;
- the patched `xpol.f` output hash;
- that only `xpol.f` changed and the patch tree contains no extra files;
- that exactly one `REWIND(LU)` was inserted at the expected header-write
  site.

Build manifests record the source baseline, patch path, patch status, patch
hash, frozen input hash, build input hash, patched output hash and binary
hashes. The patched binary remains external and is not copied into
`fortran-bin/`.

The overlay is a candidate contract until the independent P1 audit accepts it.
The polar-save column-order finding is a separate file-format issue and is not
part of this patch.

## OSMAP build and runtime policy

- Build: `osmap.dat` is not required for compile or link. B1/B2 build without
  it.
- Runtime: `getosfile.c` reads the `OSMAP` environment variable. If it is
  unset, `osmap.f` prints `OSMAP: Environment variable OSMAP not defined` and
  returns zero amplification. If the named file cannot be opened, it prints a
  file-not-found message and returns zero amplification.
- `NTCALC` is called only from `FRPLOT`; no safe non-graphical CLI trigger was
  found.
- No `osmap.dat` was copied into the 6.996 source tree or repository.
- No format compatibility is claimed. A later diagnostic may reference the
  6.99 data file externally and must be marked as a format experiment.

## Proposed repository-owned build adapter

The adapter `tools/build-xfoil-6.996-oracle.sh` is implemented because B2 is
byte-reproducible. It:

- reads the 49 frozen sources without modifying them;
- requires an external build root and an external `libPlt_gDP.a`;
- checks GNU Fortran and GNU C 13.3;
- records compiler, flags, source manifest, plot-library hash and object
  hashes;
- applies `-fallow-argument-mismatch` only to `xgdes.f` and `blplot.f`;
- supports an unpatched build and an explicit `--apply-polar-dump-patch` build
  that copies the frozen source to the external root before applying the
  repository-owned overlay;
- never downloads, never modifies sources, never copies into `fortran-bin/`;
- creates a build manifest in the external build root;
- fails clearly for missing prerequisites or an in-repository build root.

Positive build, wrong-compiler, missing-plotlib and in-repository-build-root
tests are recorded externally. The adapter produces the same
`000b0468...` binary.

## Canonical-oracle promotion criteria

The 6.996 binary may be promoted only after:

1. a reproducible build from the frozen sources is independently accepted;
2. the plot/headless link contract is settled;
3. compiler and flags are fully documented;
4. the U4 corpus is reproduced from a promoted build;
5. the polar-dump abort is fixed by the accepted repository-owned patch
   overlay or explicitly accepted as a known defect;
6. the OSMAP contract is understood;
7. binary provenance and hashes are documented;
8. independent acceptance is complete.

U5 does not promote the binary. The byte-reproducible build is established;
the polar-dump overlay is a candidate pending independent P1 acceptance. The
polar-save column-order mismatch remains a separate target-contract issue.

## Open questions

- Is the minimal `REWIND(LU)` overlay accepted as the polar-dump oracle fix?
  (P1 candidate: yes, with the frozen source unchanged.)
- Should the patched build become the normal 6.996 oracle build only after the
  P1 audit?
- Can a headless plot stub be proven behaviorally equivalent to the real plot
  library, or must the external plot library remain mandatory?
- What compiler built the canonical 6.99 binary, and should its build contract
  also be documented?
- Which plot-library version/commit is the accepted external dependency?
- Should the adapter be wired into CI, and if so with which external plotlib
  artifact?
- How should polar read/append be exercised after the dump path is fixed?

## Acceptance criteria

- B1 and B2 each build twice in separate external roots.
- All builds are classified as byte-reproducible or semantically reproducible.
- Built binaries are compared statically and behaviorally with the existing
  diagnostic binary.
- The polar-dump abort is diagnosed sufficiently to identify the failure path
  and is explicitly marked as a promotion blocker.
- OSMAP build/runtime separation is documented.
- The adapter passes syntax, positive-build and negative-prerequisite tests.
- No protected source, binary, Golden, reference, tolerance or product version
  changes.
- Gates exit 0 and only the allowed U5 files are added.

## Git and publication policy

- U5 may add the two U5 documents and the build adapter; it does not commit or
  push.
- Frozen sources, binaries, Goldens, references and product code remain
  unchanged.
- No push before the cumulative XFOIL.next 1.1.0 acceptance.
- A focused U5 acceptance is the next step; no C++ integration follows before
  that acceptance.

## Exact next allowed step

`X6996-U5-R1-Focused-Reproducible-Oracle-Build-Acceptance`.
