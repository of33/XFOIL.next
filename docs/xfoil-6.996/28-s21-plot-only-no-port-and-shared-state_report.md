# XFOIL 6.996 — S21 plot-only no-port and shared-state report

Status: PASS — `X6996-S21` and `X6996-S21-R1` accepted as a
verification-only record. No product code, product test, fixture, golden or
Fortran file is changed.

```text
NIMP-01: ACTIVE — NTS / LPPORT / NSPL remain not ported.
G-PLOT-HEADLESS: OPEN — deferred; no headless plot backend required.
X6996-S21: PASS
X6996-S21-R1: PASS
```

## Starting state

- HEAD: `fbff332ef2d89bcc86e7ecbecbe28b1bd4baa429`
  (`Integrate XFOIL 6.996 input capacities`).
- Parent: `583461fd63a1096219a370b162e5c87af4d0674c`
  (`Integrate XFOIL 6.996 SMOO target correction`).
- Worktree clean, staged 0, untracked 0, ahead 38 behind 0.
- Tag `v1.0.0` unchanged at
  `dc7305b057286574cd137cb6f1af170b74650ec5`.
- No S21 commit, no prior S21 work and no S21 document pair existed.
- No push, fetch, amend, tag or release.

## U6 scope and decision

S21 is group G12, plot-only observations, with `S08` and `S10` as read-only
dependencies. U6 defines the slice as verification only:

- `NTS`/`LPPORT`/`NSPL` are `NIMP-01`.
- The listed plot routines are not product implementation slices.
- `S21` records the non-port decision.
- No C++ and no product test is expected.
- `G-PLOT-HEADLESS` is an open decision and does not block non-graphical
  acceptance.

The recorded decision is:

```text
NIMP-01: NTS / LPPORT / NSPL remain not ported.
G-PLOT-HEADLESS: OPEN — deferred; keep external plot library, no stub required.
```

S21 does not decide or start `S22`; that slice remains gated by `G-OSMAP`.

## Upstream evidence

### `NTS`

- `XFOIL.INC:120` declares `NTS` in `COMMON/CI04`.
- `XFOIL.INC:401` documents `NTS` as the number of points per spline interval
  for airfoil plots.
- `xfoil.f:593` initialises `NTS = 20`.
- The consumers are the `PLTAIR` plot calls in `xgdes.f`, `xplots.f`,
  `pntops.f` and the plot part of `xtcam.f`.
- `xgdes.f:1061-1068` is the only assignment path, through `NSPL`.

No non-graphical state, file format, numerical kernel or session contract
reads `NTS`.

### `LPPORT`

- `XFOIL.INC:42` and `XFOIL.INC:142` declare `LPPORT` in `COMMON/CL01`.
- `xfoil.f:794` sets `LPPORT = .TRUE.` at plot initialisation.
- `xfoil.f:333` passes it to OPLSET.
- `plutil.f:24-25` receives it; `plutil.f:56` derives the inverse portrait
  flag; `plutil.f:115-122` toggles it and calls `PLPORT`/`PLUNPORT`.

`LPPORT` is a hardcopy/plot orientation flag. It changes no geometry, panel,
analysis or file state.

### `NSPL`

- `xgdes.f:155` lists `NSPL` only in the GDES plot-menu text.
- `xgdes.f:1061-1071` accepts an integer or prompt, assigns `NTS`, clamps a
  prompted value to at least 1, then calls `PLTINI` and `PLOTG`.
- There is no geometry-buffer mutation and no `LGSAME` change.

The S15 accepted contract already records `NSPL` as excluded, unknown in the
product GDES command path and absent from GDES help.

### Plot routines

The affected upstream renderers are plot-only:

- `blplot.f` reads `MDEF`/`PDEF`/`KDEF` for BL variable plots;
- `polplt.f` and `pplot.f` read `CPOLSD`/`CPOLEL` for polar plots;
- `xplots.f`, `pntops.f` and the plot parts of `xtcam.f` pass `NTS` to
  `PLTAIR`;
- `plutil.f` contains the OPLSET menu state and the SCALIT scale helper.

No renderer is added to the product.

## Current C++ state

Static search over `include/` and `src/` finds no reference to `NTS`,
`LPPORT` or `NSPL`. The only `NSPL` references are accepted S15 tests and
earlier planning/acceptance documents.

| Area | Current product state | S21 result |
|---|---|---|
| `NTS` | absent | remains not ported |
| `LPPORT` | absent | remains not ported |
| `NSPL` | not a GDES command; absent from help | remains not ported |
| BL plots | no `blplot.f` port and no drawing backend | remains not ported |
| polar plots | no `polplt.f`/`pplot.f` port and no drawing backend | remains not ported |
| airfoil plot calls | no `xplots.f`/`pntops.f`/`xtcam` plot port | remains not ported |
| plot commands | recognised but unavailable; no state change | unchanged |
| PLOP menu/frame helpers | existing non-drawing state and GOFINI/GOFSET/SCALIT math | retained, not expanded |

The accepted S15 integration test still establishes:

- `NSPL` reports `Command NSPL not recognized`;
- `NSPL` leaves the saved buffer unchanged;
- `NSPL` is absent from GDES help.

Existing `PlotFrame` helpers compute canonical plot limits and scaling only.
They do not draw, do not use `NTS`/`LPPORT`/`NSPL`, and do not turn
`G-PLOT-HEADLESS` into a closed decision.

## `H-S21-SHARED` audit

The shared-state check is source-level and uses accepted S08/S10/S11/S12/S18
evidence. No new numerical test is required.

| State | C++ owner | Non-plot consumer | Plot consumer | Result |
|---|---|---|---|---|
| `CLF` | `ViscalState::clf`, `ViscalResult::clf` | accepted S18 console text | none | independent |
| `DSSR` | `ViscalState::dssr`, march/update fields | accepted S13 BL export | `blplot.f`, not ported | independent |
| `MDEF` | `ViscalState::mdef`, march/update fields | none required | `blplot.f`, not ported | no product dependency |
| `PDEF` | `ViscalState::pdef`, march/update fields | none required | `blplot.f`, not ported | no product dependency |
| `KDEF` | `ViscalState::kdef`, march/update fields | none required | `blplot.f`, not ported | no product dependency |
| `CPOLEL` | `PolarDataset::elementData` | accepted S11/S12 polar text/dump | `pplot.f`, not ported | independent |
| `CPOLSD` | `PolarDataset::sideData` | accepted polar/session IO | `polplt.f`/`pplot.f`, not ported | independent |

Findings:

- S08 producers write `dssr`, `mdef`, `pdef`, `kdef` and `clf` in the existing
  boundary-layer march/update paths, independent of any plot flag.
- No C++ plot module includes or reads `ViscalState`, `MarchInput`,
  `UpdateInput` or `PolarDataset`.
- S10 `elementData` is consumed by the accepted non-plot polar text/dump
  paths. That consumer predates S21 and does not require the `pplot.f` path,
  so it does not trigger `NIMP-01` re-evaluation.
- `NTS`, `LPPORT` and `NSPL` are absent from the product, so they cannot be a
  hidden shared-state dependency.
- No accepted S08/S10 initialisation, copy, sort, delete, reset or session
  isolation path requires plot state.

`H-S21-SHARED`: PASS.

## `G-PLOT-HEADLESS` decision

`G-PLOT-HEADLESS` remains open and is deliberately deferred in S21.

- The external plot library remains the oracle-build dependency.
- No headless backend, stub, renderer or drawing substitute is introduced.
- No CI/build-scope change is made.
- The existing recognised-but-unavailable plot commands remain unchanged.
- A future accepted product plot contract is still required before this
  gate can be reopened.

This matches the U6 option “keep external plot library; prove a stub; defer”
and does not turn deferred status into a closed implementation decision.

## Verification boundary

S21 is verification only:

- no product code changed;
- no product test changed or added;
- no product test was run;
- no fixture, golden, Makefile or canonical reference changed;
- no `fortran-src/**`, `fortran-bin/**` or `patches/**` changed;
- no full Linux suite and no Windows process;
- no external audit artifact is required.

Accepted S08/S10/S11/S12/S15/S18 evidence is reused without modification.
The S15 `NSPL` exclusion is the current product evidence for the command
decision. `a14` and `a31` remain oracle observations; S21 makes no new
product plot claim from them.

## Documentation gates

The non-writing repository gates were run after this document pair was
complete:

| Gate | Result |
|---|---|
| `git diff --cached --check` after explicit staging | PASS |
| `make check-fortran-sha` | PASS |
| `make check-license-headers` | PASS |
| `make check-release-inventory` | PASS |
| `make check-public-docs` | PASS |

The three SHA manifests, license-header self-test and live check, release
inventory and public-documentation gate all completed without findings.

## Protected state and publication freeze

- Product version and banner unchanged; `v1.0.0` unchanged.
- No product output, golden, fixture or canonical 6.99 reference changed.
- No plot backend, plot state or plot command activated.
- No S22/`G-OSMAP` decision or implementation.
- No S14/SVG/DXF/`G-EXPORT` work.
- No push, fetch, amend, tag or release.
- The only commit is the documentation-only S21 record.

## Next U6 step

After this record, the next gated U6 item is the `G-OSMAP` decision.
`S22` remains blocked and is not implemented or decided by S21.

## Final decision

X6996-S21: PASS
X6996-S21-R1: PASS
