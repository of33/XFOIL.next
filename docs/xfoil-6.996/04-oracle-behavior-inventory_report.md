# XFOIL 6.996 — U4 oracle-behavior inventory report

Status: PASS

This report records controlled behavioral comparisons between the canonical
XFOIL 6.99 executable and the diagnostic XFOIL 6.996 executable. Generated
run artifacts remain in the external audit area.

## Starting state

- HEAD: `520f8a55b3ccb8554a67101911148d3c91638223`.
- `origin/main`: `58bca7d17f847e1aef6b1c8d3d4e167f40f69945`.
- Ahead/behind: `0 6`.
- Tag: `v1.0.0` unchanged.
- Working tree before U4: clean; staged 0.
- Binary hashes:
  - `fortran-bin/xfoil`:
    `f5779d6954d0b8fdc152e0e0633f18732e00b7b69228d561d629bd524bba598c`
  - `fortran-bin/xfoil-6.996`:
    `000b0468d9a7f4fce42d6fac24bd14147d7d326c8a9035909c8e9f98d180f50e`

## Environment

Ubuntu 24.04.5 LTS, Linux 7.0.0-42-generic, x86-64, glibc 2.39. `xvfb-run`
and `Xvfb` are not installed. `DISPLAY=:0` was available but no graphical case
was required. `xlsfonts` reports 1145 X core fonts; fontconfig reports 1483
entries. Both binaries were run with `LC_ALL=C`, a private `HOME` and a fixed
timeout. Full environment data is external `environment.txt`.

## Case inventory and execution

- 33 cases: 14 existing 6.99 reference cases and 19 targeted new cases.
- 132 process runs: 33 cases × 2 binaries × 2 repetitions.
- No timeout occurred.
- 31 cases exit 0 in both binaries.
- `a07-polar-visc`: 6.99 exits 0, 6.996 exits 2.
- `a33-eof-in-gdes`: both binaries exit 2 at EOF inside GDES.
- Every case is deterministic in exit code, stdout and generated file set.
  Crash backtrace addresses differ because of ASLR; the error text and exit
  code are stable.
- The full case manifest, result summary, file comparison, numeric comparison
  and first-divergence table are external TSV files under `u4/`.

## Classification distribution

| Classification | Count |
|---|---:|
| `default-change` | 15 |
| `new-feature` | 13 |
| `file-format-change` | 2 |
| `small-numerical-change` | 2 |
| `error-handling-change` | 1 |
| `large-final-result-change` | 0 |
| `convergence-path-change` | 0 main |
| `banner-only` | 0 main |
| `text-only` | 0 main |
| `removed-feature` | 0 main |
| `not-observable` | 0 main |

Text-only and removed-feature effects occur as secondary findings in several
cases, especially the removed inviscid `OFS:` line and the absent automatic
`:00.bl` side file.

## Main deviations

- The banner changes from XFOIL 6.99 to XFOIL 7.00.
- The defaults `NPAN=160`/`CTERAT=0.15` change to `NPAN=240`/`CTERAT=0.30`.
  This changes startup menu text, `PANE`, panel counts, default panel files and
  the `WDEF` file.
- `PPAR N=500` is accepted with 500 nodes in 6.996; 6.99 clamps to 364.
  `PPAR N=602` clamps to 595 in 6.996 and to 364 in 6.99.
- Standard `NACA 0012` and `NACA 2412` create a 399-point buffer in 6.996
  instead of 245 points.
- The inviscid `OFS: alfa = ... cl = ...` result line is absent in 6.996.
  This is a stdout text regression not recorded in the U3 source inventory.
- Inviscid polar header/column spacing changes. The last 8-degree point is
  CL 0.9634 vs 0.9638 and CDp -0.00115 vs -0.00104.
- `a07-polar-visc` aborts in 6.996 while creating the polar dump: `xpol.f`
  reports a sequential READ/WRITE after EOF. The 6.996 dump is 0 bytes and the
  polar is partial (470 bytes); 6.99 completes with a 53160-byte dump and an
  877-byte polar.
- The automatic `:00.bl` side file from 6.99 VISCAL is absent in 6.996 in the
  executed viscous cases. The explicit `DUMP` file in `a08-bldump` is
  byte-identical between the binaries.
- 6.996 adds `CLf`, `CDp` and `CDp+CDf` fields to viscous result text.
- New commands/exports are accepted only by 6.996: `NACS`, `SQTE`,
  `CIRC`/`TECIRC`, `SYTB`, `XCEN`, `NSPL`, `BLOU`/`BLDUMP3`, `SVG` and `DXF`.
  `NSPL` is plot-only. SVG and DXF inclusion in the product remains a scope
  decision.
- `NACS 0012` and `NACS 2412` create sharp-TE geometry in 6.996; 6.99 reports
  the command as unknown and writes an empty geometry file when asked to save.
- `BLOU`/`BLDUMP3` writes a 42704-byte file in 6.996; 6.99 reports the command
  as unknown.
- `SVG` and `DXF` write 4941-byte and 16013-byte files in 6.996; 6.99 reports
  both commands as unknown.
- QDES alpha=0 CL changes from 0.2554 to 0.2556; this is a small numerical
  change. Viscous sequence CD at alpha=8 changes from 0.01211 to 0.01214.
- The 6.99 binary often emits a stderr `IEEE_DIVIDE_BY_ZERO` note; 6.996
  normally emits no such note. This is treated as a runtime/build diagnostic
  difference, not a numerical result change.
- `a33-eof-in-gdes` confirms that both binaries abort with exit 2 on EOF
  inside GDES; it is a control case, not a differential error-handling finding.

## Repeatability

All cases were repeated in separate directories. Exit code, stdout and the
generated file set were identical between repetitions. The only repeat
variation is ASLR-dependent backtrace text in the two crash cases
(`a07-polar-visc`, `a33-eof-in-gdes`); the error message and exit code are
stable.

## U3 coverage

G01, G03-G10 and G12 are covered by executed cases. G02 (`SEGSPLD`) is not
observable through the executable corpus. G11 input capacity is not isolated:
the 200-character NAME test is truncated by the fixed 48-character name field
in both binaries. G13 OSMAP is blocked by missing data and the absence of a
safe non-graphical trigger.

QDCALC matrix internals, CFT derivative internals, multi-element polar element
columns and BLDUMP3 field-by-field semantics are not fully observable through
the CLI and remain candidates for focused harnesses.

## OSMAP

No `osmap.dat` was copied to 6.996. Start/quit and the non-graphical viscous
cases did not trigger OSMAP. `NTCALC` is called from `FRPLOT`; no safe
non-graphical trigger was found. G13 remains blocked-by-missing-data. A later
format diagnostic may set `OSMAP` explicitly to the 6.99 data file, but no
such run was made in U4.

## Non-observable points

- `SEGSPLD` segment-end behavior: focused harness required.
- QDCALC influence-matrix internals: only final inviscid results are visible.
- CFT derivative internals: only integrated viscous results are visible.
- Input-buffer capacity 128 -> 256: not isolated by the executed NAME case.
- Multi-element polar element columns: not exercised.
- Polar read/append: not separately exercised; a focused two-pass harness is
  required and no cross-binary generated file was reused.
- BLDUMP3 field-level semantics: file production observed, correctness not
  established.
- OSMAP: no data file and no non-graphical trigger.

## Gates and protected data

- `make check-fortran-sha`: 6.99 48/48 and 6.996 49/49, exit 0.
- `make check-license-headers`: exit 0.
- `make check-release-inventory`: exit 0.
- `make check-public-docs`: exit 0.
- `git diff --check`: exit 0.
- 6.99 sources unchanged; 6.996 sources unchanged; both binaries unchanged.
- No Golden, reference, tolerance, product code or product-version change.
  Product version remains 1.0.0.

## Repository state and publication hold

- Only the two U4 documents are untracked:
  - `docs/xfoil-6.996/04-oracle-behavior-inventory.md`
  - `docs/xfoil-6.996/04-oracle-behavior-inventory_report.md`
- Staged 0; no commit and no push.
- HEAD `520f8a55b3ccb8554a67101911148d3c91638223`; `origin/main`
  `58bca7d17f847e1aef6b1c8d3d4e167f40f69945`; ahead/behind `0 6`; tag
  `v1.0.0` unchanged.
- Publication hold remains in force until cumulative XFOIL.next 1.1.0
  acceptance.

## Decision

`X6996-U4: PASS`

## Exact next allowed step

`X6996-U4-R1-Focused-Oracle-Behavior-Acceptance`.

## Focused independent acceptance — X6996-U4-R1

Decision: PASS

- Corpus and isolation: 33 cases (14 existing, 19 targeted), 132 isolated
  runs, no timeouts. Input scripts are identical within each case, 6.99 and
  6.996 use separate run directories, generated files are captured
  independently, and no output is reused between versions.
- Key measurements were re-checked from the stored runs: NPAN 240/CTERAT 0.30;
  NACA buffer 399 vs 245 points; inviscid `OFS:` present in 6.99 and absent in
  6.996; PPAR limit 364 vs 500/595; new `NACS` behavior; QDES CL 0.2554 ->
  0.2556; viscous CD 0.01211 -> 0.01214.
- The removed `OFS:` line is retained as a secondary text-only/command-output
  finding in the default-change cases `a02`/`a03`; it is explicitly documented
  and not hidden by the main classification.
- Polar dump: the diagnostic 6.996 executable reproducibly exits 2 and writes
  a 0-byte dump in both repetitions; 6.99 exits 0 with a 53160-byte dump. This
  remains an unresolved diagnostic result, is not accepted target behavior and
  is not treated as a 6.996 Golden.
- `:00.bl`: the file is present only in the 6.99 runs of the checked viscous
  cases and absent in 6.996. The existing portable `:00.bl` -> `100.bl`
  collection contract remains unchanged; no Golden or collection logic was
  modified.
- G01-G13: all observable groups have executed cases. G02, G11 and G13 are
  marked not observable with concrete reasons; OSMAP remains
  blocked-by-missing-data; no CLI case was invented; plot-only behavior is not
  treated as mandatory product integration.
- Repeatability: exit codes, stdout and generated file sets are identical
  between repetitions. Only ASLR-dependent backtrace addresses differ in
  `a07` and `a33`; no deterministic drift was found.
- Interpretation limit: the binaries were not produced by the same documented
  reproducible build. U4 records behavior, not proven source attribution. The
  next build investigation is
  `X6996-U5-Reproducible-XFOIL-6.996-Oracle-Build`.
- Protected data: 6.99 hashes 48/48 and 6.996 hashes 49/49; both binary hashes
  unchanged; no C++ change, Golden change, reference change, tolerance change
  or product-version change. Product version remains 1.0.0.
- Gates: `make check-fortran-sha`, `make check-license-headers`,
  `make check-release-inventory`, `make check-public-docs` and
  `git diff --check` all exit 0.
- Repository state: only the two U4 documents are untracked; staged 0; HEAD
  `520f8a55b3ccb8554a67101911148d3c91638223`; `origin/main`
  `58bca7d17f847e1aef6b1c8d3d4e167f40f69945`; ahead/behind `0 6`; tag
  `v1.0.0` unchanged. Publication hold remains in force.
- Next allowed local commit:
  `Document XFOIL 6.99 and 6.996 oracle behavior differences`, limited to the
  two U4 paths. No push.

`X6996-U4-R1: PASS`
