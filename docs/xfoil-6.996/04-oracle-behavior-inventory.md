# U4 — XFOIL 6.99 and 6.996 oracle behavior inventory

## Purpose

Record controlled behavioral comparisons between the canonical XFOIL 6.99
executable and the diagnostic XFOIL 6.996 executable. U4 identifies what the
two binaries do on a small representative corpus before any C++ integration or
reference migration.

## Scope

- Execute `fortran-bin/xfoil` (canonical 6.99 oracle) and
  `fortran-bin/xfoil-6.996` (diagnostic binary) with identical input scripts.
- Cover startup, inviscid and viscous points, sequences, polar and BL output,
  geometry and GDES paths, defaults, limits, command/session behavior, MDES/
  QDES, file I/O, new exports and error behavior.
- Classify each case exactly once and record the first observable divergence.
- Keep all run directories, transcripts and generated artifacts outside the
  repository under
  `/local/Olaf/Entwicklung/.audit-xfoil-next/xfoil-6.996/u4/`.

## Explicit non-goals

- No Fortran or C++ source change.
- No declaration of 6.996 as canonical.
- No replacement of existing references or Goldens.
- No new Windows or Linux Goldens.
- No tolerance derivation or change.
- No compiler or build-flag change.
- No product-version or product-banner change.
- No copy of `osmap.dat` to 6.996.
- No plot-functionality port.
- No commit and no push.
- No claim that an observed difference is caused by a specific source change
  without the U3 diff and a reproducible build.

## Oracle status

- XFOIL 6.99 remains the canonical executable oracle.
- XFOIL 6.996 remains diagnostic.
- Observed differences are facts about the two binaries, not automatic
  acceptance of the 6.996 behavior.
- The binaries were not produced by the same documented reproducible build.
  Differences may therefore combine source changes with different original
  build environments.
- Attribution to a concrete Fortran change requires the U3 source diff and,
  later, a reproducible 6.996 build in U5.

## Execution environment

- Date: 2026-11-08 (UTC).
- Host: Ubuntu 24.04.5 LTS, Linux 7.0.0-42-generic, x86-64.
- glibc: 2.39.
- `xvfb-run` and `Xvfb`: not installed.
- `DISPLAY=:0`; the existing X server was available but not used for plotting.
- X core fonts visible via `xlsfonts`: 1145; fontconfig entries: 1483.
- Both binaries were run in the same environment with `LC_ALL=C`, a private
  `HOME` per run and a fixed timeout.
- Graphics were disabled by the standard `PLOP` / `g` preamble where present;
  the remaining existing cases use only non-graphical commands. No graphical
  window, plot port or Xvfb run was required.
- Binary hashes:
  - `fortran-bin/xfoil`:
    `f5779d6954d0b8fdc152e0e0633f18732e00b7b69228d561d629bd524bba598c`
  - `fortran-bin/xfoil-6.996`:
    `000b0468d9a7f4fce42d6fac24bd14147d7d326c8a9035909c8e9f98d180f50e`
- Both are dynamically linked against `libX11`, `libgfortran`, `libm` and
  `libc`.

## Case-corpus design

The corpus has two parts:

- **A — existing representative 6.99 cases** copied from
  `tests/fortran-reference/cases/` without changing the reference corpus.
- **B — targeted 6.996 cases** written as new external scripts for defaults,
  limits, sharp TE, new commands, new exports and error behavior.

Each case runs in a separate empty directory for each binary. Input files are
copied per run; no generated file is reused between 6.99 and 6.996.

## Capture and comparison method

Each run captures `input.txt`, `stdout.raw`, `stderr.raw`, `exit-code.txt`,
`timeout-status.txt`, `generated-files.tsv`, `SHA256SUMS.txt` and
`environment.txt`.

Comparison follows four contracts:

- **Process:** exit code, timeout, signal/crash, stderr, NaN/Inf/runtime
  markers.
- **Text:** banner separately, prompts and command echoes, warnings, errors,
  convergence messages and menu order.
- **Files:** generated names and set, sizes, SHA-256, headers, columns, row
  counts and append/overwrite behavior.
- **Numerics:** final operating points, CL, CD, CDp, CM, alpha, transition
  positions, iteration count, convergence status and relevant geometry/panel
  coordinates.

Intermediate iterations are not compared position by position. The primary
evidence is structure, first productive divergence, final state and generated
files.

Every non-identical or failed case was executed twice per binary in separate
directories. All cases reproduced the same exit code, standard output and
generated file set. For crash cases the backtrace addresses differ because of
ASLR, but the error text and exit code are stable.

## Baseline cases

The existing-case part uses these reference cases without modification:

| Case | Existing case | Focus |
|---|---|---|
| `a02-inv-0012` | `aero-inviscid/inv-0012-0` | NACA 0012, inviscid alpha 0 |
| `a03-inv-2412-seq` | `aero-inviscid/seq-inv-2412` | NACA 2412, inviscid alpha sequence |
| `a04-clarky-visc` | `aero-viscous/visc-clark-y` | Clark Y, viscous Re=1e6 alpha 4 |
| `a05-visc-seq-0012` | `aero-viscous/seq-visc-0012` | NACA 0012, viscous sequence |
| `a06-polar-inv` | `polar/polar-inv-0012` | inviscid polar |
| `a07-polar-visc` | `polar/polar-visc-0012` | viscous polar with dump |
| `a08-bldump` | `file-export/dump-visc-clark-y` | BLDUMP output |
| `a09-gdes-cang` | `geometry-edit/gdes-cang` | GDES CANG |
| `a10-camb` | `design-state/camb-setc` | CAMB SETC |
| `a11-norm-load` | `geometry/norm-load-policy-cycle` | LOAD/NORM policy |
| `a12-msav-isav` | `top-file-export/msav-isav-roundtrip` | MSAV/ISAV roundtrip |
| `a13-save-overwrite` | `file-io/save-overwrite-decline-accept` | SAVE overwrite contract |
| `a14-plop-menu` | `design-state/plop-menu-return` | plot menu text only |
| `a15-vpar-default` | `oper-state/vpar-default-display` | VPAR defaults |

## Targeted 6.996 cases

The new cases cover the requested targeted behaviors:

| Case | Focus |
|---|---|
| `a01-start-quit` | startup banner and immediate exit |
| `a16-mdes-menu` | short non-graphical MDES help/exit |
| `a17-qdes-menu` | short non-graphical QDES help/exit |
| `a18-wdef-rdef` | WDEF/RDEF defaults roundtrip |
| `a19-pane-default` | cold-start NACA 0012 + PANE |
| `a20-ppar500` | panel count 500, above old IQX limit |
| `a21-ppar602` | panel count 602, above new limit |
| `a22-nacs0012` | sharp-TE NACA 0012 |
| `a23-nacs2412` | sharp-TE cambered NACA 2412 |
| `a24-sqte` | GDES SQTE |
| `a25-circ` | GDES CIRC/TECIRC |
| `a26-sytb` | GDES SYTB independent top/bottom scaling |
| `a27-xcen` | GDES XCEN |
| `a28-bldump3` | BLOU/BLDUMP3 |
| `a29-svg` | SVG export availability |
| `a30-dxf` | DXF export availability |
| `a31-nspl` | GDES NSPL, plot-only command |
| `a32-long-name` | 200-character NAME followed by SAVE |
| `a33-eof-in-gdes` | EOF inside GDES, abort/error control |

The full machine-readable manifest is external `case-manifest.tsv`.

## Result classifications

Each case has exactly one main classification. Secondary observations are
recorded in the report and external summaries.

| Classification | Cases | Meaning |
|---|---:|---|
| `default-change` | 15 | First substantive change is default paneling/menu/default persistence, or a file-size change caused by the 160/240 panel count. |
| `new-feature` | 13 | A command, export or capacity exists only in 6.996, or a new command behavior is observed. |
| `file-format-change` | 2 | Polar file format changes or the automatic BL side-file set changes. |
| `small-numerical-change` | 2 | Final values are close but not identical (CD 0.01211 -> 0.01214; QDES CL 0.2554 -> 0.2556). |
| `error-handling-change` | 1 | 6.996 aborts while creating a viscous polar dump; 6.99 completes. |
| `text-only` | 0 main | Text-only changes occur as secondary findings, especially the removed `OFS:` line and added `CLf`/`CDp` fields. |
| `banner-only` | 0 main | The banner changes are always accompanied by a default or other change. |
| `large-final-result-change` | 0 | No large final result change was observed in the executed corpus. |
| `convergence-path-change` | 0 main | No separate convergence-path-only case was isolated. |
| `removed-feature` | 0 main | The missing automatic `:00.bl` side file is recorded under file-format-change/default-change. |
| `not-observable` | 0 main | G02, G11 and G13 are marked not observable in the U3 coverage table rather than assigned a case. |
| `blocked-by-missing-data` | 0 main | G13 is blocked by missing `osmap.dat` and no non-graphical trigger. |

## Important observed differences

### Defaults and paneling

- `NPAN` is 240 in 6.996 instead of 160.
- `CTERAT` is 0.30 in 6.996 instead of 0.15.
- The startup menu, `PANE`, `VPAR`-adjacent menu text and `WDEF` file show the
  new values.
- `PPAR N=500` is accepted with 500 nodes in 6.996; 6.99 clamps to 364.
- `PPAR N=602` clamps to 595 in 6.996 and to 364 in 6.99.

### NACA buffer and inviscid output

- `NACA 0012` and `NACA 2412` create a 399-point buffer in 6.996 instead of the
  245-point buffer in 6.99.
- The 6.996 `SPECAL` path does not print the `OFS: alfa = ... cl = ...` line.
  Inviscid single points and sequences therefore show no final inviscid
  operating-point record in stdout. The U3 source-difference inventory did not
  record this text removal.
- The 6.99 binary often emits a stderr note
  `IEEE_DIVIDE_BY_ZERO`; 6.996 normally emits no such note. This is treated as
  a runtime/build diagnostic difference, not a numeric result difference.

### Polar, BL and export formats

- Inviscid polar header/column spacing changes; the last 8-degree point is
  CL 0.9634 vs 0.9638 and CDp -0.00115 vs -0.00104.
- The polar dump case `a07-polar-visc` aborts in 6.996 with a runtime error in
  `xpol.f` (sequential READ/WRITE after EOF). The dump file is 0 bytes and the
  polar file is partial (470 bytes); 6.99 completes with a 53160-byte dump and
  an 877-byte polar.
- The automatic `:00.bl` side file produced by 6.99 VISCAL is absent in 6.996
  in the viscous cases.
- The `DUMP` file in `a08-bldump` is byte-identical between the binaries.
- 6.996 adds `CLf`, `CDp` and `CDp+CDf` text fields to viscous result lines.
- `BLOU`/`BLDUMP3` exists only in 6.996 and writes a 42704-byte file.
- `SVG` and `DXF` exist only in 6.996 and write 4941-byte and 16013-byte files
  respectively. Their inclusion in the non-graphical product remains a scope
  decision.

### Commands and geometry

- `NACS`, `SQTE`, `CIRC`/`TECIRC`, `SYTB`, `XCEN`, `NSPL`, `BLOU`, `SVG` and
  `DXF` are accepted only by 6.996 in the executed corpus.
- `NACS 0012` and `NACS 2412` create sharp-TE geometry in 6.996; 6.99 reports
  the command as unknown and writes an empty geometry file when asked to save.
- `SQTE`, `CIRC` and `SYTB` change the 6.996 geometry files; 6.99 reports the
  commands as unknown.
- `XCEN` prints top/y/t/bottom points in 6.996; 6.99 reports it as unknown.
- `NSPL` is accepted only by 6.996 and is plot-only.
- MDES and QDES menus are largely unchanged; QDES alpha=0 CL changes from
  0.2554 to 0.2556.

### Error and abort behavior

- `a07-polar-visc`: 6.99 exits 0; 6.996 exits 2 after a Fortran runtime error.
- `a33-eof-in-gdes`: both binaries abort with exit 2 on EOF inside GDES. This
  is a control case and does not establish a differential error-handling
  change; the XCEN availability difference is the primary finding there.
- Crash backtraces are not position-stable because of ASLR; exit code, error
  text and generated files are stable.

## U3-group coverage

| Group | Cases or status |
|---|---|
| G01 state/limits | `a02`, `a20`, `a21` |
| G02 numerics/SEGSPLD | not observable through executable corpus; focused harness required later |
| G03 geometry/NORM/LEFIND/CANG | `a02`, `a03`, `a09`, `a11`, `a24`, `a25`, `a26` |
| G04 sharp TE/NACA/GDES | `a10`, `a22`-`a27` |
| G05 paneling/QDCALC | `a02`, `a03`, `a19`-`a24` |
| G06 boundary layer/CFT | `a04`, `a05`, `a07`, `a08`, `a28` |
| G07 defaults | `a04`, `a05`, `a15`, `a18`, `a19` |
| G08 file/polar/export formats | `a06`, `a07`, `a08`, `a12`, `a13`, `a18`, `a28`-`a30` |
| G09 command/session | baseline and targeted command cases, including `a01`, `a16`, `a17`, `a22`-`a31`, `a33` |
| G10 inverse design | `a16`, `a17` |
| G11 input capacity | not isolated; NAME is truncated to 48 characters in both binaries; focused input-buffer harness required |
| G12 plot-only | `a14`, `a31` |
| G13 OSMAP | blocked-by-missing-data; no non-graphical OSMAP trigger and no 6.996 `osmap.dat` |

QDCALC matrix internals are not printed by the CLI; the group is covered at the
final inviscid operating-point level only.

## CLI-unreachable changes

- `SEGSPLD` segment-end behavior: no direct CLI path; focused harness needed.
- QDCALC influence-matrix internals: only final inviscid results are visible.
- CFT derivative internals: only integrated viscous results are visible.
- Input-buffer capacity 128 -> 256: the 200-character NAME test is truncated
  by the fixed 48-character name field in both binaries, so the parser-capacity
  change is not isolated.
- Multi-element polar element columns: the executed corpus is single-element;
  a focused multi-element polar case is still required.
- Polar read/append: each case starts in an empty directory and cross-binary
  artifact reuse is forbidden, so read/append needs a focused two-pass harness.
- BLDUMP3 field-by-field semantics: the file is produced, but U4 does not
  establish field-level correctness.
- OSMAP: `NTCALC` is called from `FRPLOT`; no safe non-graphical trigger was
  found. The 6.99 `osmap.dat` was not copied or used as a canonical 6.996 file.

## OSMAP diagnostic policy

- No `osmap.dat` was copied to 6.996.
- No OSMAP error was triggered by start/quit or the non-graphical viscous
  cases.
- G13 remains blocked by missing data and by the absence of a safe CLI trigger.
- A later diagnostic may set `OSMAP` explicitly to the 6.99 data file and mark
  the run as a format test only. No such run was made in U4.

## Consequences for later integration

- Do not adopt 6.996 defaults automatically. Oracle cases for the C++ port
  must pin `NPAN`/`CTERAT` where paneling is not the subject of the test.
- Treat the removed inviscid `OFS:` line and the polar-dump abort as
  behavioral findings that need diagnosis before porting. The polar dump
  failure is likely in U3 group G08, but the source/build attribution is not
  proven.
- Treat `NACS`, `SQTE`, `CIRC`/`TECIRC`, `SYTB`, `XCEN`, `NSPL`, `BLOU`,
  `SVG` and `DXF` as new-feature scope items, not as unchanged behavior.
- Keep `SVG` and `DXF` behind a product-scope decision. U4 only establishes
  that the 6.996 executable can write them.
- Decide explicitly whether the 399-point standard NACA buffer and the absent
  automatic `:00.bl` side file are intended.
- Use U5 to establish a reproducible 6.996 build before assigning causes to
  compiler, runtime-check or link differences.
- Use focused harnesses for G02, G11, multi-element polar columns, QDCALC
  internals, CFT internals and OSMAP. Do not infer those internals from the
  executed CLI corpus.
- Do not use 6.996 output as a replacement for any canonical 6.99 reference.

## Open questions

- Is the 399-point NACA buffer intentional or a side effect of the sharp-TE
  NACA changes?
- Is the removal of the inviscid `OFS:` line intentional? It is not documented
  in the U3 source inventory.
- What causes the `xpol.f` polar-dump runtime error in 6.996?
- Is the `:00.bl` absence intentional, or a side effect of the BL/VISCAL
  changes?
- Are the polar element columns only visible with multi-element input?
- Can input capacity be isolated through a focused parser harness?
- What are the actual compiler, runtime-check and link differences between the
  two binaries? U5 is expected to answer this.
- Are SVG/DXF in the non-graphical product scope?

## Acceptance criteria

- The 33-case corpus executes in isolated directories for both binaries.
- Every case has exactly one main classification and a repeatability result.
- Important deviations are repeatable or explicitly marked unstable.
- G01-G13 are covered or marked not observable with a concrete reason.
- No canonical data, Golden, reference, tolerance, source or product version is
  changed.
- Gates exit 0 and the worktree contains only the two U4 documents.

## Git and publication policy

- U4 creates no commit and no push.
- Only the two U4 documentation files may appear as untracked worktree changes.
- `v1.0.0` and all existing references remain unchanged.
- No push before the cumulative XFOIL.next 1.1.0 acceptance.

## Exact next allowed step

`X6996-U4-R1-Focused-Oracle-Behavior-Acceptance`.
