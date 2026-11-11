# XFOIL 6.996 — S15 geometry command acceptance and NACA buffer decision

Status: PASS — `X6996-S15` and `X6996-S15-R1` accepted; the implementation
is committed and the documentation-only R1 follow-up is recorded.

Decision: `G-NACA-BUFFER: ACCEPT-XFOIL-6996`.

Command set confirmed:

```text
NACS
SQTE
CIRC
SYTB
XCEN
Adeg
```

`NSPL` is excluded from S15. It remains plot-only `NIMP-01` work and is
deferred to `S21`.

## Slice contract from U6

U6 assigns S15 the command registration, menus and invalidation for
`NACS`, `SQTE`, `CIRC`, `SYTB`, `XCEN` and `Adeg`, without new kernels,
formats or defaults. S15 depends on the accepted S03 geometry primitives,
S04 NACA/LTES kernel and S05 GDES kernels. The command/file oracle cases are
`a22`–`a27`; the 6.99 regressions are `a09`–`a11`.

## G1 scope

G1 is analysis and documentation only:

- decide the NACA/NACS buffer point contract;
- confirm the exact S15 command set;
- record the command/dispatcher/prompt/mutation/invalidation contracts from
  the 6.99/6.996 sources;
- inventory affected product tests, goldens and references;
- define the later implementation boundary.

No C++, test, fixture, golden, reference, Fortran source, patch, product
version, banner or tolerance is changed.

## G-NACA-BUFFER decision

Accepted option A:

```text
G-NACA-BUFFER: ACCEPT-XFOIL-6996
NACA buffer: 399 points, normal/open TE
NACS buffer: 399 points, sharp TE
```

The decision is based on the external S15 measurements and the impact
inventory in `21-s15-geometry-command-acceptance_report.md` and the external
`s15-impact-matrix.tsv`.

Key evidence:

- 6.99 `NACA` uses `NSIDE=123` / `NB=245` (`IQX=370`).
- 6.996 `NACA`/`NACS` use `NSIDE=IQX/3=200` / `NB=399` (`IQX=601`).
- A controlled 6.996 build with only `NSIDE=123` forced produces buffers
  byte-identical to the 6.99 `NACA` buffers for 0012, 2412 and 23012.
- `NACS` produces the expected sharp TE (`gap=0`) at both 245 and 399
  points; `NACA` keeps the normal open TE (`gap=0.00252`).
- 399 points remains within the S01 capacity. `CIRC` grows 399 to 429,
  below `IQX=601`; atomic capacity rejection remains required.
- The 399-point shape metrics and final inviscid point differ only in the
  expected discretization range; no API signature or serialized format is
  broken.
- The existing product goldens contain explicit 245-point NACA text/metadata
  and must be migrated individually, not globally regenerated.

## Command preanalysis summary

| Command | Menu | Arguments/prompts | Kernel | Buffer mutation | Invalidation |
|---|---|---|---|---|---|
| `NACS` | top level | `NACS i`; same validation as `NACA`; hidden in 6.996 help | S04 NACA4/NACA5 with `LTES=true` | replaces buffer, then PANGEN | buffer/panel/analysis via NACA path |
| `SQTE` | GDES | none | S05 `squareTrailingEdge` | TE coordinates only | GDES `LGSAME=false`; EXEC needed for current panel |
| `CIRC` | GDES | no argument; always `NHCIRC=15` | S05 `TECIRC`/`addTrailingEdgeCircle` | +15 points per side, normally 399→429 | atomic capacity rejection; `LGSAME=false`; EXEC needed |
| `SYTB` | GDES | 2 inline factors, or 1 + bottom prompt, or 2 prompts | S05 `scaleTopBottomY` | y scaling; X unchanged | `LGSAME=false`; EXEC needed |
| `XCEN` | GDES | 2 inline values or `x/c`, `y/t` prompts | query only; LEFIND/SEVAL math | none | none; reports top/y-t/bottom points |
| `Adeg` | GDES | 1 inline value or prompt; 6.996 alias `A`; `ARAD` remains | `ROTATE` | rotates buffer; APX angles updated | no SCALC; `LGSAME=false`; EXEC needed |
| `NSPL` | GDES | excluded | plot-only `NTS`/PLTINI/PLOTG | none | none; `NIMP-01`/S21 |

Detailed prompt, mutation and test-impact rows are in the report and the
external impact matrix.

## Later implementation contract

The later S15 commit may:

- add `NACS` top-level dispatch and route both NACA variants through the
  existing S04 kernel with an explicit sharp-TE flag;
- change the `nacaAirfoil` command default to `NSIDE=200`/`NB=399`;
- add GDES menu branches for `SQTE`, `CIRC`, `SYTB`, `XCEN` and `Adeg`;
- add session wrappers only where no kernel/wrapper exists;
- update only the impacted product goldens and add focused command tests;
- add narrowly versioned S15 fixtures.

It must not:

- implement `NSPL`;
- change S03/S04/S05 kernels or tolerances;
- regenerate canonical `tests/fortran-reference/**`;
- globally regenerate NACA/GDES goldens;
- introduce a second geometry model or plot dependency;
- change formats, defaults or product version in S15.

## Implementation record — X6996-S15

Implementation commit: `8d4164125ae1e7e3f5e3fb0f6b1462a17ef4a52f`,
parent `53b34fbc1877543b1d81ab8e3911cfedda23cbcf`. The commit was not
amended. A separate documentation-only follow-up records the R1 closure.

- `nacaAirfoil` now defaults to `NSIDE=200` / `NB=399`; `NACS` dispatches
  through the same kernel with the sharp-TE flag. The kernel keeps an
  explicit `nside` argument so legacy reference tests can pin `123`.
- `NACS` remains hidden from top-level help.
- GDES `SQTE`, `CIRC` (`NHCIRC=15`, inline argument ignored), `SYTB`,
  `XCEN` and `A` (degree alias) are wired in the existing menu/session
  path. `NSPL` remains an unknown command.
- `XCEN` is query-only; it prints top, y/t and bottom points and does not
  change the buffer or panel.
- S15 oracle fixtures are under `tests/data/xfoil-6.996/s15/` for six
  NACA/NACS 399-point cases, five GDES probe buffers and the targeted
  regression/top-export references.
- Targeted product goldens were migrated for the 399-point NACA default.
  Tests whose purpose is independent of S15 use the explicit
  `NSIDE=123`/`NB=245` baseline through `testing::handleLegacyNacaCore`
  or an explicit `nacaAirfoil(..., false, 123)` call.
- Canonical `tests/fortran-reference/**` is unchanged.

Evidence, including the targeted existing-golden migration table and the
versioned fixture inventory, is recorded in
`21-s15-geometry-command-acceptance_report.md`. The Linux matrix and the
single focused Windows process are green.

## Non-goals

- No S14/SVG/DXF decision; `G-EXPORT` remains open and S14 remains blocked.
- No S16 `BLOU`/SVG/DXF command work.
- No S17 session/warm-start behavior beyond the direct command contracts.
- No push, fetch, tag or release before cumulative XFOIL.next 1.1.0
  acceptance.

## Gates

G1 uses only the non-mutating gates:

```text
git diff --check
make check-fortran-sha
make check-license-headers
make check-release-inventory
make check-public-docs
```

No Linux or Windows test matrix is required for G1. All five G1 gates
passed.
