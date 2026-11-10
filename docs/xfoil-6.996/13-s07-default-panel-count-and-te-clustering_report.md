# XFOIL 6.996 — S07 default panel count and TE clustering report

Status: PASS — `X6996-S07` and `X6996-S07-R1` accepted on Linux and Windows;
local commit prepared.

## Starting state

- HEAD at G1/implementation start:
  `2cd8b7260e3997228a5424b1642af55bb79f9391`.
- Parent: `dcd93d99d6f23f36f7a30b9853b5a81ead958696` (S05).
- `v1.0.0` unchanged; ahead 18 commits before S07.
- Pre-existing untracked S07 documents: only the two G1 documents.

## G-DEFAULTS decision

```text
G-DEFAULTS: ACCEPT

XFOIL.next 1.1.0 adopts the XFOIL 6.996 cold-start defaults NPAN=240 and
CTERAT=0.3. Existing 6.99 reference artifacts remain unchanged. Tests whose
purpose is unrelated to default initialization must use explicit settings
instead of depending accidentally on the old defaults.
```

## Implemented product change

| File | Change |
|---|---|
| `include/xfoil/core/paneling/PanelingTypes.h` | `panelCount` 160 -> 240; `teToLeDensityRatio` 0.15 -> 0.3; INIT comments |
| `include/xfoil/session/SessionState.h` | `initDefaultsValues()` NPAN/CTERAT -> 240/0.3 |
| `include/xfoil/core/paneling/Pangen.h` | default documentation 160 -> 240 |
| `src/app/cli/CommandInterpreter.cpp` | stale INIT-default comment 160 -> 240 |

Unchanged: CVPAR 1.0, CTRRAT 0.2, PANGEN formulas, GETPAN clamp/override,
NACA buffer 245 -> 399, commands, file formats, product version and banner.

## WDEF / RDEF contract

- Order and text format remain `NPAN, CVPAR, CTERAT, CTRRAT`.
- Cold start -> WDEF writes `240 / 1.0000 / 0.3000 / 0.2000`.
- Mutation -> RDEF restores the saved values.
- Explicit old values `160 / 0.15` are read losslessly.
- No format version, column change or new defaults file.

Measured external evidence remains in
`.audit-xfoil-next/xfoil-6.996/s07/measurements/`: `6996_wdef_rdef/`
shows 240 -> 120/0.2 -> 240/0.3.

## Test classification

External matrix:
`.audit-xfoil-next/xfoil-6.996/s07/defaults-impact-matrix.tsv` (80 rows).

| Classification | Rows |
|---|---:|
| `update-to-6.996-default` | 14 |
| `preserve-6.99-with-explicit-setting` | 14 |
| `make-test-independent-of-default` | 28 |
| `versioned-6.996-target-needed` | 1 |
| `needs-owner-decision` | 1 |
| `unaffected` | 22 |

True default tests were updated to 240/0.300. Default-independent engine,
BL, inverse, QDES/MDES, GDES, OPER, FMOM/VELS and parity tests now request
explicit `NPAN=160`, `CTERAT=0.15`. Canonical 6.99 references stay
byte-identical. The 399-point NACA buffer remains a separate
`G-NACA-BUFFER` / `needs-owner-decision` item.

Test-only compatibility support was added in
`tests/regression/LegacyDefaults.h`: it writes the old 160/0.15 defaults via
the existing codec and removes only the test-injected RDEF preamble and the
startup menu's migrated PANE display before 6.99 parity comparison.


### Changed non-golden test paths

```text
tests/core/BoundaryLayerCorrelationTests.cpp
tests/core/BoundaryLayerTests.cpp
tests/core/CoreCpiBitTests.cpp
tests/core/CoreLinkCheck.cpp
tests/core/InviscidTests.cpp
tests/core/PanelingTests.cpp
tests/core/QdcalcSharpTeTests.cpp
tests/core/XycmReferenceTests.cpp
tests/integration/CommandInterpreterTests.cpp
tests/integration/DefaultsCommandsTests.cpp
tests/integration/NativeBatchTests.cpp
tests/integration/OperP7CProductiveTests.cpp
tests/integration/OperPlotBlockedTests.cpp
tests/integration/OperVparTests.cpp
tests/integration/PanelingCommandsTests.cpp
tests/integration/TopFileCommandsTests.cpp
tests/integration/TopFileExportTests.cpp
tests/regression/CseqHeadDumpParityTests.cpp
tests/regression/LegacyDefaults.h
tests/regression/PaccInviscidDumpParityTests.cpp
tests/regression/Wake2Alfa6ConvergenceParityTests.cpp
tests/unit/BlfFblgetBitTests.cpp
tests/unit/CoreGdesMenuHandlerTests.cpp
tests/unit/CoreGeometryHandlerTests.cpp
tests/unit/CoreIntxHalfHandlerTests.cpp
tests/unit/CoreMdesMenuHandlerTests.cpp
tests/unit/CoreOperSinglePointHandlerTests.cpp
tests/unit/CoreOperSinglePointTests.cpp
tests/unit/CoreQdesMenuHandlerTests.cpp
tests/unit/FmomMhingeBitTests.cpp
tests/unit/MdesFileCodecTests.cpp
tests/unit/OperBlfCoreTests.cpp
tests/unit/OperDampCoreTests.cpp
tests/unit/OperFmomCoreTests.cpp
tests/unit/SessionCommitTests.cpp
tests/unit/SessionCoreStateWiringTests.cpp
tests/unit/SessionDefaultsOpsTests.cpp
tests/unit/SessionMdesCommandTests.cpp
tests/unit/SessionQdesCommandTests.cpp
tests/unit/VelsPsilinBitTests.cpp
```

## Focused S07 test

`integration.DefaultPanelingAndDefaultsRoundtripUsesXfoil6996Values` covers:

- cold-start NPAN=240 / CTERAT=0.3;
- PPAR display;
- NACA 0012 -> PANE with 240 nodes and CTERAT=0.3;
- WDEF with the new first line;
- mutation to 120/0.2;
- RDEF restoration to 240/0.3;
- an independent second session with the same defaults.

It does not activate the 399-point NACA buffer.

## Golden diff list

Only default-dependent product goldens changed. Every change is a measured
result of the 160/0.150 -> 240/0.300 migration; canonical
`tests/fortran-reference/**`, `tests/data/**` and tolerances are untouched.

| Path | Case | Old default contract | New contract | Diff (+/-) | Oracle evidence |
|---|---|---|---:|---:|---|
| `tests/golden/batch/clark_y_results.dat` | `clark_y` | default geometry 160 / 0.150 | default geometry 240 / 0.300 | 30/30 | a04/a05/a19 |
| `tests/golden/batch/clark_y_transcript.txt` | `clark_y` | default geometry 160 / 0.150 | default geometry 240 / 0.300 | 2039/1879 | a04/a05/a19 |
| `tests/golden/batch/naca0012_results.dat` | `naca0012` | default geometry 160 / 0.150 | default geometry 240 / 0.300 | 29/30 | a04/a05/a19 |
| `tests/golden/batch/naca0012_transcript.txt` | `naca0012` | default geometry 160 / 0.150 | default geometry 240 / 0.300 | 6085/2343 | a04/a05/a19 |
| `tests/golden/batch/naca2312_results.dat` | `naca2312` | default geometry 160 / 0.150 | default geometry 240 / 0.300 | 30/30 | a04/a05/a19 |
| `tests/golden/batch/naca2312_transcript.txt` | `naca2312` | default geometry 160 / 0.150 | default geometry 240 / 0.300 | 1734/1153 | a04/a05/a19 |
| `tests/golden/batch/naca4415_results.dat` | `naca4415` | default geometry 160 / 0.150 | default geometry 240 / 0.300 | 30/30 | a04/a05/a19 |
| `tests/golden/batch/naca4415_transcript.txt` | `naca4415` | default geometry 160 / 0.150 | default geometry 240 / 0.300 | 1079/1505 | a04/a05/a19 |
| `tests/golden/cli/file-inte-pcop-quit.stdout.txt` | `file-inte-pcop-quit` | 160 / 0.150 startup/PANE | 240 / 0.300 | 6/6 | a15/a19/H-S07-OVERRIDE |
| `tests/golden/cli/file-load-clark-y-quit.stdout.txt` | `file-load-clark-y-quit` | 160 / 0.150 startup/PANE | 240 / 0.300 | 1/1 | a15/a19/H-S07-OVERRIDE |
| `tests/golden/cli/file-naca-save-load-quit.stdout.txt` | `file-naca-save-load-quit` | 160 / 0.150 startup/PANE | 240 / 0.300 | 8/8 | a15/a19/H-S07-OVERRIDE |
| `tests/golden/cli/file-name-ninc-xycm-quit.stdout.txt` | `file-name-ninc-xycm-quit` | 160 / 0.150 startup/PANE | 240 / 0.300 | 2/2 | a15/a19/H-S07-OVERRIDE |
| `tests/golden/cli/file-norm-reve-deli-quit.stdout.txt` | `file-norm-reve-deli-quit` | 160 / 0.150 startup/PANE | 240 / 0.300 | 1/1 | a15/a19/H-S07-OVERRIDE |
| `tests/golden/cli/help-quit.stdout.txt` | `help-quit` | 160 / 0.150 startup/PANE | 240 / 0.300 | 2/2 | a15/a19/H-S07-OVERRIDE |
| `tests/golden/cli/oper-help.stdout.txt` | `oper-help` | 160 / 0.150 startup/PANE | 240 / 0.300 | 1/1 | a15/a19/H-S07-OVERRIDE |
| `tests/golden/cli/oper-inviscid-alpha.stdout.txt` | `oper-inviscid-alpha` | 160 / 0.150 startup/PANE | 240 / 0.300 | 1/1 | a15/a19/H-S07-OVERRIDE |
| `tests/golden/cli/oper-sequence.stdout.txt` | `oper-sequence` | 160 / 0.150 startup/PANE | 240 / 0.300 | 1/1 | a15/a19/H-S07-OVERRIDE |
| `tests/golden/cli/oper-viscous-alpha.stdout.txt` | `oper-viscous-alpha` | 160 / 0.150 startup/PANE | 240 / 0.300 | 1/1 | a15/a19/H-S07-OVERRIDE |
| `tests/golden/cli/pane-loaded-quit.stdout.txt` | `pane-loaded-quit` | 160 / 0.150 startup/PANE | 240 / 0.300 | 3/3 | a15/a19/H-S07-OVERRIDE |
| `tests/golden/cli/plot-unavailable-quit.stdout.txt` | `plot-unavailable-quit` | 160 / 0.150 startup/PANE | 240 / 0.300 | 1/1 | a15/a19/H-S07-OVERRIDE |
| `tests/golden/cli/ppar-change-apply.stdout.txt` | `ppar-change-apply` | 160 / 0.150 startup/PANE | 240 / 0.300 | 3/3 | a15/a19/H-S07-OVERRIDE |
| `tests/golden/cli/ppar-plot-unavailable.stdout.txt` | `ppar-plot-unavailable` | 160 / 0.150 startup/PANE | 240 / 0.300 | 3/3 | a15/a19/H-S07-OVERRIDE |
| `tests/golden/cli/ppar-show-return.stdout.txt` | `ppar-show-return` | 160 / 0.150 startup/PANE | 240 / 0.300 | 3/3 | a15/a19/H-S07-OVERRIDE |
| `tests/golden/cli/startup-quit.stdout.txt` | `startup-quit` | 160 / 0.150 startup/PANE | 240 / 0.300 | 1/1 | a15/a19/H-S07-OVERRIDE |
| `tests/golden/cli/unknown-quit.stdout.txt` | `unknown-quit` | 160 / 0.150 startup/PANE | 240 / 0.300 | 1/1 | a15/a19/H-S07-OVERRIDE |
| `tests/golden/cli/vpar-show.stdout.txt` | `vpar-show` | 160 / 0.150 startup/PANE | 240 / 0.300 | 1/1 | a15/a19/H-S07-OVERRIDE |

CLI files with only the startup menu line changed: `file-load-clark-y`,
`file-norm-reve-deli`, `oper-help`, `oper-inviscid-alpha`, `oper-sequence`,
`oper-viscous-alpha`, `plot-unavailable`, `startup`, `unknown`, `vpar-show`.
The larger `file-inte-pcop` and `file-naca-save-load` changes are directly
caused by the default panel contour participating in INTE/SAVE/LOAD; no
non-default formatting or banner/OFS change was taken over.

## Oracle regression status

- `a15`: cold-start PPAR is 240/0.300; startup display tests updated.
- `a18`: WDEF/RDEF writes/restores 240/0.300; old explicit values still
  readable.
- `a19`: NACA 0012 -> PANE is 240 nodes; focused test covers it.
- `a04`: Clark Y viscous loads 151 nodes without PANE; 6.99 reference case
  remains valid; the parity test continues to pass unchanged.
- `a05`: NACA 0012 viscous sequence is default-dependent; the NACA/PANE
  parity cases request explicit old 160/0.15 and keep the canonical 6.99
  artifacts byte-identical.
- `H-S07-OVERRIDE`: explicit N=160/T=0.15 still gives 160 nodes and
  CTERAT=0.150.

## Linux matrix

Toolchain: GCC 13.3.0, Clang 18.1.3, Valgrind 3.26.0.

| Check | Result |
|---|---|
| GCC 13 Debug `make test` | 1990/1990 pass |
| GCC 13 Release | 1990/1990 pass |
| Clang 18 Debug | 1990/1990 pass |
| Clang 18 Release | 1990/1990 pass |
| `make test-core` | 429/429 pass |
| `make test-unit` | 1689/1689 pass |
| `make test-integration` | 246/246 pass |
| `make test-regression` | 5/5 pass |
| `make test-batch` | 47/47 pass |
| `make test-sanitize` (ASan/UBSan) | 1990/1990 pass |
| focused S07 Valgrind | `integration.DefaultPanelingAndDefaultsRoundtripUsesXfoil6996Values` 1/1 pass, no definite leaks |
| `make test-reference` | 236/236 pass |
| `make test-reference-determinism` | 236/236 pass |
| `make test-harnesses` | 16 families, 0 failures |
| `make check-dependencies` | PASS |
| `make check-fortran-sha` | PASS |
| `make check-release-inventory` | PASS |
| `make check-license-headers` | PASS (421 hand-written sources) |
| `make check-public-docs` | PASS (17 public documents) |
| `make core-link-check` | PASS |
| `make session-link-check` | PASS |

New test count: full/integration +1 (1990, 246). Core/unit/regression/batch
counts are unchanged (429/1689/5/47).

## Windows focused run

PASS. Single focused process:

```text
[ RUN      ] integration.DefaultPanelingAndDefaultsRoundtripUsesXfoil6996Values
[       OK ] integration.DefaultPanelingAndDefaultsRoundtripUsesXfoil6996Values
0 failure(s) in 1 test(s)
```

No full Windows suite was run.

## Protected data and Git status

Unchanged:

- `fortran-src/**`, `fortran-bin/**`, `patches/**`
- canonical `tests/fortran-reference/**`
- canonical `tests/data/**` reference corpora
- tolerances, product version and banner
- tag `v1.0.0`

Golden changes are limited to the default-dependent files listed above.
The S07 change set is prepared for the local commit
`Adopt XFOIL 6.996 paneling defaults`. No push, fetch, new tag or release was
performed.

## Acceptance checklist

- [x] Central defaults 240/0.3; CVPAR/CTRRAT unchanged.
- [x] WDEF/RDEF order/format and old-value compatibility preserved.
- [x] Default tests updated; non-default tests explicit 160/0.15.
- [x] 6.99 references and tolerances unchanged.
- [x] Golden diffs documented and measured.
- [x] Linux full/compiler/sanitizer/Valgrind/reference/harness/gate matrix green.
- [x] Focused Windows process: 1/1 pass.
- [x] `X6996-S07` and `X6996-S07-R1` acceptance.
- [x] Local commit `Adopt XFOIL 6.996 paneling defaults`.
