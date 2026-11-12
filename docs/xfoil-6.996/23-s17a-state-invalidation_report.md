# XFOIL 6.996 — S17a state/invalidation report

Status: PASS — `X6996-S17a` and `X6996-S17a-R1` accepted. Implementation,
Linux matrix, focused Windows process and R1 documentation check are green.

## Starting state

- HEAD: `de40db120096f5ba48576431f07ddf79bcf26ff2`
  (`Record X6996-S16-R1 focused acceptance`).
- Parent: `9c58a984f147a2be166cc4941d66a911c6f2ebf5`.
- Worktree clean, staged 0, untracked 0, ahead 31 behind 0.
- Tag `v1.0.0` unchanged at
  `dc7305b057286574cd137cb6f1af170b74650ec5`.
- No S17a commit existed at the start.
- No push, fetch, tag or release.

## U6 scope and substage boundary

`S17a` is the first internal stage of `S17`: state/invalidation after
geometry and file commands. `S17b` (warm starts, sequences, convergence
and menu behavior) is not implemented or pre-empted.

S14/SVG/DXF remain blocked by open `G-EXPORT`. No format, default,
product-version or banner change is included.

## Upstream analysis

Compared the 6.996 dispatchers and state blocks in `xfoil.f`, `xgdes.f`
and `xoper.f`. The decisive distinction is:

- `NACA`/`NACS` and `PANE` call `PANGEN`, whose flag block invalidates
  panel-dependent analysis state.
- `INTE`, `INTX` and `HALF` write only the buffer.
- `LOAD` writes the buffer and calls `ABCOPY`; only `ABCOPY` controls the
  current panel and the `N!=NB` `LBLINI` branch.
- GDES direct edits remain working-buffer changes until `EXEC`.
- Query and export paths do not mutate the session.

External audit area: `xfoil-6.996/s17a/`.

Stock-6.996 measurement for the INTE state probe:

- `NACA 2412 -> VISC -> ALFA 4 -> CPWR before.cp`;
- `INTE` from two files;
- `OPER -> CPWR after.cp`;
- `before.cp == after.cp` byte-identically (5783 bytes, SHA-256
  `7413a7ee27a105a07c4f514412b71e528582041982c3a6f3e9fd42dbc6178883`).

The pre-fix product invalidated the current panel at INTE and could no
longer enter OPER or write the second Cp file. The corrected product
reproduces the stock behavior and both Cp files byte-identically.

## Current C++ mapping and corrections

- `session::setBufferOnlyGeometry` is the new buffer-only adoption path:
  it updates buffer, name, validity and buffer revision while preserving
  current panel, provenance, influence system, wake, BL, mapping, inverse,
  polar, result and operating state.
- `INTE` now uses `setBufferOnlyGeometry` instead of the full
  `setBufferGeometry` path.
- `LOAD` now uses `setBufferOnlyGeometry` followed by
  `copyBufferToCurrent`, matching the canonical buffer-then-`ABCOPY`
  order. The previous current panel is retained long enough for `ABCOPY`
  to evaluate its `N!=NB` branch.
- `LOAD` rejects a buffer above `kMaxAbcopyNodeCount` before mutating state.
- `setBufferGeometry` remains the full invalidation path for `NACA` and
  other genuine buffer replacements.
- GDES direct edits, `EXEC`, `PANE`, `PCOP`, query and export paths are
  unchanged in contract.

Corrections:

1. **INTE:** full panel/analysis invalidation was incorrect. INTE is a
   buffer-only command; the current panel stays installed until PCOP/PANE.
2. **LOAD warm start:** the previous full-reset path forced `LBLINI=false`
   even when the previous panel count equaled the loaded buffer count.
   The corrected path preserves `LBLINI` only for `N==NB` and resets it for
   `N!=NB`, as in `ABCOPY`.

## State-transition evidence

| Path | Expected transition | Test |
|---|---|---|
| `INTE` | buffer revision changes; panel/analysis stay | unit and integration S17a |
| `INTE -> PCOP` | new panel from new buffer; fresh point matches fresh path | integration S17a |
| `NACA` after viscous point | panel regenerated; system/wake/BL/convergence invalidated | unit and integration S17a |
| `PCOP`/`EXEC` after GDES | buffer-only edit preserves analysis; EXEC invalidates | unit and integration S17a |
| `XCEN`/export projections | no buffer, panel, analysis or result mutation | unit S17a |
| `LOAD` after viscous point | fresh follow-up point matches fresh loaded path | integration S17a |
| `LOAD` equal point count | `LBLINI` preserved; unequal count resets | unit S17a |
| failed `LOAD` | previous Cp and state remain | integration S17a |
| export commands | `CPWR`, `BLOU`, `DUMP`, `DMP`, `SAVE` leave the point state intact | integration S17a |

## Tests and fixtures

New unit coverage:

- `unit.StateInvalidationS17aBufferOnlyAdoptionPreservesCurrentPanel`
- `unit.StateInvalidationS17aBufferOnlyAdoptionRejectsInvalidBuffer`
- `unit.StateInvalidationS17aLoadWarmStartFollowsAbcopyPointCount`
- `unit.StateInvalidationS17aNacaReplacesPanelAndInvalidatesAnalysis`
- `unit.StateInvalidationS17aGdesMutationKeepsAnalysisUntilExec`
- `unit.StateInvalidationS17aQueryAndExportProjectionsAreMutationFree`

New integration coverage:

- `integration.StateInvalidationS17aIntePreservesCurrentSolution`
- `integration.StateInvalidationS17aIntePcopMatchesFreshPath`
- `integration.StateInvalidationS17aNacaAfterViscousMatchesFreshPath`
- `integration.StateInvalidationS17aLoadAfterViscousMatchesFreshPath`
- `integration.StateInvalidationS17aGdesKeepsSolutionUntilExec`
- `integration.StateInvalidationS17aExportsDoNotInvalidateSolution`
- `integration.StateInvalidationS17aFailedLoadKeepsPreviousState`

No new S17a fixture or golden is required. The existing INTE state test was
corrected to the canonical buffer-only contract. No canonical 6.99
reference, Fortran source, binary, patch, product version, banner or global
tolerance was changed.

## Linux and gate results

```text
Full suite:                  2067/2067
Reference:                    236/236
Determinism:                  236/236
Harness families:              16/16
S17a focused unit:              6/6
S17a focused integration:       7/7
GCC 13.3 Debug:                 PASS
GCC 13.3 Release:               PASS
Clang 18.1.3 Debug:             PASS
Clang 18.1.3 Release:           PASS
ASan/UBSan:                     PASS
Focused Valgrind:               PASS
Permanent gates:                PASS
Existing goldens changed:           0
```

The full-suite count rose from 2054 to 2067 through the 13 new S17a tests.
A direct product run of the stock INTE probe matches both stock Cp files.

## Windows minimal policy

Exactly one focused Windows process is required after the green Linux
matrix:

```text
integration.StateInvalidationS17a
7/7, 0 failures
```

Exactly one focused Windows process was run:

```text
integration.StateInvalidationS17a
7/7, 0 failures
```

No full Windows suite and no Windows golden were run. The earlier S15/S16
Windows processes were not counted for S17a.

## Protected state and publication freeze

- Fortran sources, binaries and patches unchanged.
- `tests/fortran-reference/**` unchanged.
- Product version and banner unchanged; `v1.0.0` unchanged.
- No global tolerance change and no existing golden change.
- No push, fetch, amend, tag or release.

## Focused R1 acceptance — X6996-S17a-R1

R1 was performed before the single S17a integration commit. It confirms:

- S17a is limited to state/invalidation after geometry and file commands.
- S17b warm starts, sequences, convergence and menu behavior are not
  implemented or pre-empted.
- S14/SVG/DXF remain untouched; `G-EXPORT` remains open.
- The INTE and LOAD corrections match the 6.996 buffer/ABCOPY state model.
- Query and export paths remain mutation-free.
- Error and failed-load paths remain atomic.
- No existing golden, canonical 6.99 reference, Fortran source, binary,
  patch, product version, banner or global tolerance was changed.
- Exactly one focused Windows process ran:
  `integration.StateInvalidationS17a`, 7/7, 0 failures.
- No further Windows process was run.
- No push, fetch, amend, tag or release occurred.

The two S17a documents are the only documentation finalization for this
slice and are committed with the implementation in one local commit.

## Final decision

X6996-S17a: PASS
X6996-S17a-R1: PASS
