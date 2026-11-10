# S07 - Accepted default panel count and TE clustering

Status: PASS - `X6996-S07` and `X6996-S07-R1` accepted on Linux and Windows;
local commit prepared.

## Slice contract from U6

- Slice ID: `S07`.
- U3 group: G07 defaults.
- Scope: `NPAN=240`, `CTERAT=0.3` initialization, display and persistence
  only.
- Fortran: `xfoil.f` INIT/`VPAR`, `WDEF`/`RDEF`.
- C++ area: App defaults/session; core paneling parameter values.
- Dependencies: `S01`, `S06`, `G-DEFAULTS`.
- Risk: medium-high.
- Oracle: `a15`, `a18`, `a19`, `H-S07-OVERRIDE`.
- 6.99 regressions: `a04`, `a05`.
- Expected files: defaults C++ and focused tests; no commands/formats.
- Non-goals: commands, exports or formats; no implicit adoption of the
  399-point NACA buffer; no BL/QDCALC change; no product version/banner
  change.
- Commit boundary: one local commit after `G-DEFAULTS`.

## G-DEFAULTS decision

```text
G-DEFAULTS: ACCEPT

XFOIL.next 1.1.0 adopts the XFOIL 6.996 cold-start defaults NPAN=240 and
CTERAT=0.3. Existing 6.99 reference artifacts remain unchanged. Tests whose
purpose is unrelated to default initialization must use explicit settings
instead of depending accidentally on the old defaults.
```

## Fortran pre-analysis

| Location | XFOIL 6.99 | XFOIL 6.996 |
|---|---|---|
| INIT defaults (`xfoil.f`) | `NPAN=160`, `CTERAT=0.15` | `NPAN=240`, `CTERAT=0.3` |
| GETDEF line 1 read | `NPAN, CVPAR, CTERAT, CTRRAT` | same |
| WRTDEF line 1 write | `NPAN, CVPAR, CTERAT, CTRRAT` | same |
| GETPAN N | sets `NPAN`, clamp `IQX-6` | same clamp plus lower bound 2 |
| GETPAN T | sets `CTERAT` | same |
| PANGEN | `N=NPAN`, `CVTE=CVAVG*CTERAT` | same formulas with new defaults |
| Menu/parameter display | uses current `NPAN`/`CTERAT` | same |

The only initialization value change is in `xfoil.f`; WDEF/RDEF and GETPAN
are value transports, not separate defaults. The 6.996 NACA buffer size
399 and the 6.996 OFS/banner differences are separate slices/decisions and
must not be attributed to S07.

## Inventory result

External matrix:
`.audit-xfoil-next/xfoil-6.996/s07/defaults-impact-matrix.tsv`.

Rows: 64. Classification counts:

| Classification | Rows |
|---|---:|
| `update-to-6.996-default` | 12 |
| `preserve-6.99-with-explicit-setting` | 13 |
| `make-test-independent-of-default` | 15 |
| `versioned-6.996-target-needed` | 1 |
| `needs-owner-decision` | 1 |
| `unaffected` | 22 |

Primary product defaults:

- `include/xfoil/core/paneling/PanelingTypes.h`
- `include/xfoil/session/SessionState.h`

Primary tests that must change for the new default:

- `PanelingParametersDefaults`
- `SessionDefaultsInitialStateMatchesFortranInit`
- cold-start NACA/PANE/PPAR CLI text tests
- `tests/integration/CommandInterpreterTests.cpp`
- `tests/integration/PanelingCommandsTests.cpp`
- `tests/integration/DefaultsCommandsTests.cpp`
- `tests/golden/cli/*.stdout.txt`
- `tests/golden/batch/*.txt`

Primary 6.99 references that must stay unchanged:

- `tests/fortran-reference/cases/**`
- `tests/fortran-reference/harnesses/**`
- `tests/data/core-*-reference/**`
- CLI/regression consumers of those references must request explicit old
  `NPAN=160`, `CTERAT=0.15`.

No command, export, format, version or banner change belongs to S07.

## H-S07-OVERRIDE

External evidence:
`.audit-xfoil-next/xfoil-6.996/s07/oracle-measurements.md`.

Measured cases:

1. cold-start PPAR display;
2. NACA -> PANE with new defaults;
3. explicit old defaults (`N=160`, `T=0.15`);
4. WDEF -> mutation -> RDEF roundtrip;
5. independent sessions;
6. PANE with `NPAN=240`, `CTERAT=0.3`;
7. explicit old values as the 6.99 compatibility case.

Measured results:

| Case | Result |
|---|---|
| cold start PPAR 6.99 | 160 / 0.150 |
| cold start PPAR 6.996 | 240 / 0.300 |
| NACA 0012 -> PANE 6.99 | 160 nodes, TE gap 0.00252 |
| NACA 0012 -> PANE 6.996 | 240 nodes, TE gap 0.00252 |
| explicit PPAR N=160/T=0.15 on 6.996 | 160 nodes, 0.150 |
| WDEF first line 6.996 | 240 / 1.0000 / 0.3000 / 0.2000 |
| RDEF after mutation | restores 240 / 0.3000 |
| independent sessions | byte-identical default display |

## Implementation result

`X6996-S07-Accepted-Default-Panel-Count-and-TE-Clustering` changes only the
central defaults:

- `PanelingParameters::panelCount` 160 -> 240;
- `PanelingParameters::teToLeDensityRatio` 0.15 -> 0.3;
- `SessionState::initDefaultsValues()` NPAN/CTERAT to 240/0.3;
- associated stale comments in `Pangen.h` and `CommandInterpreter.cpp`.

CVPAR remains 1.0 and CTRRAT remains 0.2. WDEF/RDEF keeps the same
NPAN, CVPAR, CTERAT, CTRRAT order and text format. Explicit GETPAN values,
clamps, paneling formulas, NACA buffer size, commands, file formats,
product version and banner are unchanged.

The aggregated test
`integration.DefaultPanelingAndDefaultsRoundtripUsesXfoil6996Values` covers
cold-start defaults, PPAR display, NACA 0012 -> PANE (240 nodes/0.3),
WDEF mutation and RDEF restoration, and an independent second session.

Detailed implementation evidence, including the golden diff list, is in the
S07 report.
