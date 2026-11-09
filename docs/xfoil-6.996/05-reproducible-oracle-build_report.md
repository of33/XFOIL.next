# XFOIL 6.996 — U5 reproducible-oracle-build report

Status: PASS

This report records build reconstruction, binary comparison and focused
runtime diagnosis for the diagnostic XFOIL 6.996 executable. Generated
sources, libraries, binaries and logs remain in the external audit area.

## Starting state

- HEAD: `2b2fd26f73d8b5683c46166f5d2134f5c8250e8d`.
- `origin/main`: `58bca7d17f847e1aef6b1c8d3d4e167f40f69945`.
- Ahead/behind: `0 7`.
- Tag: `v1.0.0` unchanged.
- Working tree before U5: clean; staged 0.
- Frozen sources: 6.99 48/48 and 6.996 49/49 green.
- Canonical 6.99 binary:
  `f5779d6954d0b8fdc152e0e0633f18732e00b7b69228d561d629bd524bba598c`.
- Existing diagnostic 6.996 binary:
  `000b0468d9a7f4fce42d6fac24bd14147d7d326c8a9035909c8e9f98d180f50e`.
- Archive `xfoil6.996.tgz`:
  `0feb71b58070d8514d830fcb0b609a10a11399d362493d7b9bbfb7343dff3f23`.

## Compiler environment

- Ubuntu 24.04.5 LTS, Linux 7.0.0-42-generic, x86-64.
- GNU Fortran 13.3.0.
- GNU C compiler 13.3.0.
- GNU ld 2.42; glibc 2.39.
- Both B1 and B2 use the existing system X11 headers and `libX11.so.6`.

## Build variants

### B1 — full upstream archive

The archive was extracted twice and the two extractions were byte-identical.
Each B1 repetition used a separate build root and rebuilt the plot library.

Commands:

```sh
# in Xfoil/plotlib
make gfortranDP
# in Xfoil/bin, with a local Makefile copy
make -f Makefile_u5 BINDIR=. INSTALLCMD=true xfoil
```

The local `Makefile_u5` is the upstream `Makefile_gfortran` with
`-fallow-argument-mismatch` added only to the `xgdes.o` and `blplot.o` rules.
`BINDIR=.` and `INSTALLCMD=true` keep the binary in the external root and avoid
the upstream stripping install step.

### B2 — frozen repository sources plus external plot library

B2 uses symlinks from the external build root:

- `src` and `osrc` -> `fortran-src/xfoil-6.996/`
- `plotlib` -> the B1 plot-library directory

No frozen source is copied or modified. B2 builds twice with the same
per-file flags and the external `libPlt_gDP.a`.

### B3 — headless analysis

The main program requires 47 symbols from `libPlt_gDP.a`. Upstream
`frplot0.f` is only a dummy for `FRPLOT`; `hpdf-stubs.f` only covers PDF;
`dplot1.f` and `xqdes1.f` are alternate full implementations, not plot stubs.
There is no existing complete headless plot backend. A new stub would have to
cover the 47 plot/cursor/clip symbols and could change plot and menu behavior;
it was not adopted. The external plot library remains the canonical B1/B2
dependency.

## Compatibility flags

| File | Without flag | With flag | Scope |
|---|---|---|---|
| `xgdes.f` | Error: `READR` rank mismatch, lines 736/750 | Warning only, compiles | `-fallow-argument-mismatch` |
| `blplot.f` | Error: `PLGRID` rank mismatch, lines 282/1050 | Warning only, compiles | `-fallow-argument-mismatch` |

No other file required the flag. It was not applied globally, and the two
warnings were not otherwise suppressed.

## Build reproducibility

B1 and B2 each built twice in completely separate external roots. Every build
produced the same binary and plot library:

| Build | Binary SHA-256 | Build-ID | Plotlib SHA-256 |
|---|---|---|---|
| existing diagnostic | `000b0468d9a7f4fce42d6fac24bd14147d7d326c8a9035909c8e9f98d180f50e` | `1f07dff6b67a3bf0235e21e1ac957cbd8b5f6d4a` | — |
| B1 build 1 | `000b0468d9a7f4fce42d6fac24bd14147d7d326c8a9035909c8e9f98d180f50e` | `1f07dff6b67a3bf0235e21e1ac957cbd8b5f6d4a` | `f11d2e432433bde33f989823fab600c4967675ee9305231ff7d9a73699d52418` |
| B1 build 2 | `000b0468d9a7f4fce42d6fac24bd14147d7d326c8a9035909c8e9f98d180f50e` | `1f07dff6b67a3bf0235e21e1ac957cbd8b5f6d4a` | `f11d2e432433bde33f989823fab600c4967675ee9305231ff7d9a73699d52418` |
| B2 build 1 | `000b0468d9a7f4fce42d6fac24bd14147d7d326c8a9035909c8e9f98d180f50e` | `1f07dff6b67a3bf0235e21e1ac957cbd8b5f6d4a` | `f11d2e432433bde33f989823fab600c4967675ee9305231ff7d9a73699d52418` |
| B2 build 2 | `000b0468d9a7f4fce42d6fac24bd14147d7d326c8a9035909c8e9f98d180f50e` | `1f07dff6b67a3bf0235e21e1ac957cbd8b5f6d4a` | `f11d2e432433bde33f989823fab600c4967675ee9305231ff7d9a73699d52418` |

- Size: 860704 bytes.
- ELF64 PIE, GCC 13.3.0 `.comment`, no RPATH/RUNPATH.
- NEEDED: `libX11.so.6`, `libgfortran.so.5`, `libm.so.6`, `libgcc_s.so.1`,
  `libc.so.6`.
- 795 symbols, 114 dynamic symbols, not stripped.
- All 45 object-file hashes are identical across B1/B2 repetitions.
- Classification: **byte-reproducible**, not merely semantically reproducible.
- The rebuilt binary is byte-identical to the existing diagnostic binary.

## Behavioral comparison

An 11-case U4 subset was executed twice against each of five binaries:
existing diagnostic, B1 build 1/2 and B2 build 1/2 (110 runs). Cases cover
start/quit, defaults/PPAR, NACA buffer, inviscid `OFS:`, sharp TE, viscous
point, QDES, polar write, polar dump, a new command and an export.

All 55 binary/case rows match the existing diagnostic binary in exit code,
stdout and generated file set. Crash-case raw stderr differs only in ASLR
backtrace addresses; normalized stderr also matches. External
`build-behavior-comparison.tsv` records the results.

## Polar-dump diagnosis

The diagnostic binary and every rebuilt binary reproduce the U4 abort:

- exit code 2;
- `polar-visc-0012.dump` size 0;
- no timeout or process-start failure;
- identical input script for 6.99 and 6.996;
- no canonical 6.99 reference changed.

The failure is in `xpol.f` `PLXINI`: a new unformatted dump file is opened, a
`READ` reaches EOF at line 435, control jumps to label 60, and the header is
written at line 522 without `REWIND`, `BACKSPACE` or reopening the unit. GCC
13.3 reports:

`Fortran runtime error: Sequential READ or WRITE not allowed after EOF marker`

A minimal external program with the same open/read-EOF/write pattern fails
identically under GCC 13.3 and leaves a 0-byte file. The 6.99 oracle binary was
built with GCC 11.4; the 6.996 binary and all rebuilds use GCC 13.3. The
element-column payload is not the trigger because the abort occurs before a
point is written.

Classification: **latent source defect exposed by the GCC 13.3 runtime**,
with the 6.996 append-path changes as close source context. It remains
unresolved and is a promotion blocker. The empty dump is not accepted as target
behavior.

## OSMAP

- Build: `osmap.dat` is not required for compile or link. B1/B2 build without
  it.
- Runtime: `getosfile.c` reads `OSMAP`. If unset, `osmap.f` prints
  `OSMAP: Environment variable OSMAP not defined` and returns zero
  amplification. A missing named file prints a file-not-found message and
  returns zero amplification.
- `NTCALC` is called only from `FRPLOT`; no safe non-graphical CLI trigger was
  found.
- No `osmap.dat` was copied into the 6.996 source tree or repository.
- No format compatibility is claimed.

## Build adapter

`tools/build-xfoil-6.996-oracle.sh` is implemented. It:

- reads the 49 frozen sources without modifying them;
- requires an external empty build root and an external `libPlt_gDP.a`;
- checks GNU Fortran/GCC 13.3;
- records the source manifest, compiler, flags, plot-library hash and object
  hashes;
- applies `-fallow-argument-mismatch` only to `xgdes.f` and `blplot.f`;
- does not download, does not copy into `fortran-bin/`, and writes a build
  manifest.

Tests:

- `bash -n`: pass.
- positive build: succeeds and produces `000b0468...f50e`.
- wrong compiler `13.2.0`: fails clearly.
- missing `libPlt_gDP.a`: fails clearly.
- build root inside the repository: fails before creating the directory.

## Promotion blockers

- Polar-dump abort remains unresolved; a source fix or explicitly accepted
  diagnostic defect is required before promotion.
- The headless link contract is analysed but not implemented or accepted.
- The canonical 6.99 build provenance is not reconstructed in U5.
- The external plot-library version/commit contract should be pinned.
- U4 behavior equivalence is established for the focused subset, not a new
  independent acceptance of all 6.996 behavior.

## Gates and protected data

- `make check-fortran-sha`: 6.99 48/48 and 6.996 49/49, exit 0.
- `make check-license-headers`: exit 0.
- `make check-release-inventory`: exit 0.
- `make check-public-docs`: exit 0.
- `make check-dependencies`: exit 0.
- `git diff --check`: exit 0.
- `bash -n tools/build-xfoil-6.996-oracle.sh`: exit 0.
- 6.99/6.996 sources, both checked-in binaries, Goldens, references,
  tolerances and product version are unchanged. Product version remains 1.0.0.

## Repository state and publication hold

- Allowed untracked changes:
  - `docs/xfoil-6.996/05-reproducible-oracle-build.md`
  - `docs/xfoil-6.996/05-reproducible-oracle-build_report.md`
  - `tools/build-xfoil-6.996-oracle.sh`
- Staged 0; no commit and no push.
- HEAD `2b2fd26f73d8b5683c46166f5d2134f5c8250e8d`; `origin/main`
  `58bca7d17f847e1aef6b1c8d3d4e167f40f69945`; ahead/behind `0 7`; tag
  `v1.0.0` unchanged.
- Publication hold remains in force until cumulative XFOIL.next 1.1.0
  acceptance.

## Decision

`X6996-U5: PASS`

The diagnostic 6.996 binary remains diagnostic. A byte-reproducible build
contract is established, but the polar-dump defect and headless/plot contract
remain promotion blockers.

## Exact next allowed step

`X6996-U5-R1-Focused-Reproducible-Oracle-Build-Acceptance`.

## Focused independent acceptance — X6996-U5-R1

Decision: PASS

- Build evidence: B1 build 1/2 and B2 build 1/2 were checked together with the
  existing diagnostic binary. All five have binary SHA-256
  `000b0468d9a7f4fce42d6fac24bd14147d7d326c8a9035909c8e9f98d180f50e`,
  Build-ID `1f07dff6b67a3bf0235e21e1ac957cbd8b5f6d4a`, size `860704` bytes,
  and byte-identical content.
- The plot library built independently by B1 build 1 and B1 build 2 has SHA-256
  `f11d2e432433bde33f989823fab600c4967675ee9305231ff7d9a73699d52418`.
- B1 is based on the externally extracted full upstream archive; the extraction
  is byte-identical to a fresh extraction. B2 uses symlinks to the 49 frozen
  repository sources and the external B1 plot library. Build roots were
  separate; no object file was reused between builds; all 45 object hashes are
  identical across repetitions. GNU Fortran/GCC 13.3.0 and unchanged sources
  were confirmed.
- The adapter `tools/build-xfoil-6.996-oracle.sh` has valid shell syntax, the
  required license header and executable bit. It checks GNU Fortran/GCC 13.3,
  requires an external build root and external plot library, reads frozen
  sources without modifying them, performs no downloads, does not copy into
  `fortran-bin/`, logs compiler/flags/source manifest/object hashes/output hash,
  and applies `-fallow-argument-mismatch` only to `xgdes.f` and `blplot.f`.
  The positive control build reproduced the same binary. Wrong-compiler,
  missing-plotlib and in-repository-build-root controls fail clearly.
- The 49 frozen files are sufficient for the main binary. `dplot1.f`,
  `xqdes1.f` and `x.log` are not needed. `osmap.dat` is not a compile or link
  dependency; `libPlt_gDP.a` remains an external build dependency, and X11 and
  runtime dependencies match the checked-in diagnostic binary.
- Polar dump: all five binaries reproducibly exit 2 with a 0-byte dump, no
  timeout, the same normalized runtime message and the same first failing
  location. `xpol.f` reaches EOF at line 435 and then writes the header at line
  522 without rewinding or reopening the unit. This is classified as a latent
  upstream source defect exposed by the GNU Fortran 13.3 runtime, not as an
  accepted target contract. No Golden or tolerance was created from the empty
  file; the binary remains diagnostic.
- OSMAP: no build dependency; ordinary paths and start/quit work without the
  data file; the `OSMAP` environment variable is required only when the
  `NTCALC`/`FRPLOT` path reaches `OSMAP`; no 6.99 data file was copied into the
  6.996 set; format compatibility remains open.
- Gates: `make check-fortran-sha` (6.99 48/48, 6.996 49/49),
  `make check-license-headers`, `make check-release-inventory`,
  `make check-public-docs`, `make check-dependencies`, `git diff --check` and
  `bash -n tools/build-xfoil-6.996-oracle.sh` all exit 0. Both checked-in
  binary hashes are unchanged; no C++/Golden/reference/tolerance/product-version
  change was made.
- Repository state before the U5 commit: only the two U5 documents and the
  adapter are untracked; staged 0; HEAD
  `2b2fd26f73d8b5683c46166f5d2134f5c8250e8d`; `origin/main`
  `58bca7d17f847e1aef6b1c8d3d4e167f40f69945`; ahead/behind `0 7`; tag
  `v1.0.0` unchanged. Publication hold remains in force.
- Allowed local commit: `Reproduce XFOIL 6.996 Fortran oracle build`, limited
  to the two U5 documents and `tools/build-xfoil-6.996-oracle.sh`. After the
  commit, `main` is eight commits ahead of `origin/main`; no push.
- Next technical step after the commit:
  `X6996-U5-D1-Polar-Dump-Write-After-EOF-Diagnosis`. No
  `X6996-U6-Functional-Integration-Plan` is allowed until a patch/oracle
  contract for the polar-dump path is defined.

`X6996-U5-R1: PASS`

## Polar-dump write-after-EOF diagnosis — X6996-U5-D1

Decision: PASS

- Failure site: `PLXINI` in `xpol.f` opens a nonexistent unformatted dump,
  the first `READ` reaches EOF at line 435 and jumps to label 60, and the
  header write at line 522 follows without `REWIND`, `BACKSPACE` or reopening
  unit 11. The unit is therefore still positioned after its EOF marker when
  the write is attempted.
- Focused reproduction: a minimal GCC 13.3 Fortran program with the same
  open/read/EOF/write sequence exits 2 with a 0-byte file. Adding `REWIND`
  after the EOF branch, or closing and reopening the unit, makes the same
  program exit 0 and write a valid record.
- Binary results: the existing diagnostic binary and all four B1/B2 rebuilds
  reproducibly exit 2 with a 0-byte dump, no timeout, the same normalized
  stderr and the same failing statement. The existing 6.99 binary (build
  comment GCC 11.4) exits 0 and writes a 53160-byte dump. The identical
  `PLXINI` text is present in the 6.99 and 6.996 sources. This is a latent
  source defect exposed by the GCC 13.3 runtime, not a 6.996-only statement
  change.
- Patch candidates tested externally:
  - preferred: `REWIND(LU)` immediately before the header write at label 60;
  - alternative: close unit 11 and reopen it with `STATUS='UNKNOWN'`,
    `FORM='UNFORMATTED'` before writing.
  The patched builds are byte-reproducible; the preferred build has SHA-256
  `031d667f3990c326b5b90d448fbdd10327b127a4e56d249fd6fcec71b55a69ce`
  and the reopen variant
  `6a088d0eccedb802875a10f58cdb85f6306215f926740dd1c700bd91e67c50ed`.
  A second clean build of the preferred variant was byte-identical.
- Both variants produce byte-identical dump output for a new file, an
  existing empty file, a second append pass and a 6.99-seeded file. The new
  dump is 77000 bytes (`d2b0b6803cc0ea0e1500a2a7ed748310d05d75f7e9a86edbe5f91034f0980e6f`),
  the second pass is 150000 bytes
  (`5d08380b603dea07854a2e5d1487aa3b4c7aa3d8bc70ed4c2fb99a5f742d1b22`),
  and the 6.99-seeded append is 126160 bytes. Record markers and payloads
  parse consistently. The 6.99 binary can read a preferred-variant dump and
  append to it (127440 bytes).
- Regression: the preferred patched binary was run over all 33 U4 cases.
  Only `a07-polar-visc` differs from the diagnostic binary, in the intended
  abort-to-success change; the other 32 cases match exit code, stdout,
  generated-file set and normalized stderr.
- Secondary observation: once the dump aborts are bypassed, the viscous polar
  save in `a07-polar-visc` shows a header/data order mismatch. The header is
  `Top_Xtr Bot_Xtr Top_Itr Bot_Itr`, while the row order is
  `Top_Xtr Top_Itr Bot_Xtr Bot_Itr`. This is separate from the dump path, is
  not classified or accepted here, and requires its own behavior case before
  promotion.
- Frozen sources remain byte-identical to upstream: no patch was applied to
  `fortran-src/xfoil-6.996/xpol.f`; no source, binary, Golden, reference,
  tolerance or product-version change was made. The 6.99 and 6.996 source
  manifests remain 48/48 and 49/49.
- Recommended patch contract: use the minimal `REWIND(LU)` fix as a
  repository-owned overlay, not as a frozen-source edit, for example
  `patches/xfoil-6.996/0001-rewind-polar-dump-unit-before-header.patch` or an
  equivalent reproducible application in the build adapter. Close/reopen is a
  verified byte-identical alternative but is not preferred.
- Remaining promotion criteria: implement and review the patch-overlay
  contract, reproduce the patched binary from frozen sources plus the
  overlay, rerun the U4 corpus, exercise new/empty/append/mismatch polar-dump
  paths, classify the polar-save column order, and complete independent
  acceptance. `X6996-U6-Functional-Integration-Plan` remains blocked until
  this patch/oracle contract is defined.
- This is a docs-only change from clean HEAD
  `417326595f801071b7a0563819c127b68b3502e6`. The local commit is
  `Record XFOIL 6.996 polar-dump diagnosis`; no push, tag or release follows.

`X6996-U5-D1: PASS`
