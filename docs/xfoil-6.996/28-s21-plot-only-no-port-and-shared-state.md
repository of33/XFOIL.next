# XFOIL 6.996 — S21 plot-only no-port and shared-state

Status: PASS — `X6996-S21` and `X6996-S21-R1` accepted as a
verification-only record. No product code or product test is changed.

Slice: `S21` of U6, group G12. Scope is the plot-only no-port decision for
`NTS`, `LPPORT`, `NSPL` and the listed plot paths, plus the `H-S21-SHARED`
check that accepted S08/S10 state does not depend on a plot port.

## S21 contract from U6

- U6 slice: `S21`.
- U6 group: G12 plot-only observations.
- Fortran scope: `blplot.f`, `polplt.f`, `pplot.f`, `plutil.f`, `xplots.f`,
  `pntops.f`, `xtcam.f` plot parts, `NSPL`, and the `NTS`/`LPPORT` state.
- C++ area: no product mapping; verification record only.
- Dependencies: `S08` and `S10`, both read-only here.
- Oracle: `a14`, `a31`, focused `H-S21-SHARED`.
- Regressions: `a06`, `a08`, `a28`.
- Commit boundary: no implementation commit; one documentation-only record.
- Test policy: verification only, no product tests.

Decision summary:

```text
NIMP-01: ACTIVE — NTS / LPPORT / NSPL remain not ported.
G-PLOT-HEADLESS: OPEN — deferred; no headless plot backend required.
S22: not activated; it remains gated by G-OSMAP.
```

## Upstream classification

| Item | 6.996 use | Classification |
|---|---|---|
| `NTS` | `COMMON/CI04` plot subdivision count, initialized to 20, passed only to `PLTAIR` calls | plot-only cosmetic state |
| `LPPORT` | `COMMON/CL01` portrait/landscape flag, set at initialization and toggled by OPLSET | plot/hardcopy orientation state |
| `NSPL` | GDES command that assigns `NTS` and calls `PLTINI`/`PLOTG` | plot-only command; no geometry mutation |
| `blplot.f` | BL variable plots, including `MDEF`/`PDEF`/`KDEF` reads | plot renderer |
| `polplt.f`, `pplot.f` | polar plots, including `CPOLSD`/`CPOLEL` reads | plot renderer |
| `xplots.f`, `pntops.f` | airfoil plots using `NTS`/`PLTAIR` | plot renderer |
| `xtcam.f` plot parts | airfoil/camber plot calls using `NTS`/`PLTAIR` | plot renderer; non-plot text parts belong to S18 |
| `plutil.f` | OPLSET menu state and SCALIT scale helper | mixed menu/state and numerical frame path; no `NTS`/`LPPORT` product port |

`NSPL` only changes the plot subdivision count and requests a replot. It does
not change buffer geometry, panels, analysis state or any file. `NTS` and
`LPPORT` have no non-graphical producer or consumer in 6.996.

## No-port decision

`NIMP-01` remains active. The product does not acquire:

- `NTS` subdivision state;
- `LPPORT` orientation state;
- the GDES `NSPL` command or its help text;
- BL plots from `blplot.f`;
- polar plots from `polplt.f`/`pplot.f`;
- airfoil plot calls from `xplots.f`, `pntops.f` or the `xtcam.f` plot parts;
- a headless or drawing plot backend.

Existing non-drawing C++ plot-adjacent code predates S21 and is retained
without expansion. It covers canonical PLOP menu text/default state, recognised
but unavailable plot commands, and the numerical GOFINI/GOFSET/SCALIT frame
helpers. It performs no drawing, cursor interaction or hardcopy and does not
implement `NTS`, `LPPORT` or `NSPL`.

## Shared-state verification plan

`H-S21-SHARED` is a static and traceability check, not a new numerical test:

| State | Owner | Non-plot consumer | Plot consumer | S21 result |
|---|---|---|---|---|
| `CLF` | S08 BL state | S18 console text, accepted | none | independent |
| `DSSR` | S08 BL state | S13 BL export, accepted | `blplot.f`, not ported | independent |
| `MDEF`/`PDEF`/`KDEF` | S08 BL state | none required | `blplot.f`, not ported | no product dependency |
| `CPOLEL`/`elementData` | S10 polar state | S11/S12 polar text/dump, accepted | `pplot.f`, not ported | independent |
| `CPOLSD`/`sideData` | S10 polar state | accepted polar/session paths | `polplt.f`/`pplot.f`, not ported | independent |
| `NTS`/`LPPORT`/`NSPL` | NIMP-01 | none | plot paths only | absent and not ported |

The check requires:

- no C++ source or public header references `NTS`, `LPPORT` or `NSPL`;
- `NSPL` remains unknown in GDES and absent from GDES help;
- no C++ plot module reads or writes `ViscalState` or `PolarDataset` state;
- no S08/S10 producer is conditional on a plot flag;
- no plot state is required to initialise, copy, sort, reset or isolate
  accepted S08/S10 state.

## Acceptance and documentation plan

- Complete the source-level no-port and shared-state audit.
- Reuse accepted S08/S10/S11/S12/S15/S18 evidence; run no product tests.
- Record `NIMP-01` as active and `G-PLOT-HEADLESS` as open/deferred.
- Run the non-writing documentation and repository gates.
- Commit one documentation-only S21 record; no product or test file.

## Non-goals

- No S22 or `G-OSMAP` decision.
- No S14, SVG, DXF or `G-EXPORT` work.
- No plot backend, stub, renderer, cursor path or hardcopy path.
- No `NTS`, `LPPORT` or `NSPL` implementation.
- No product code, test, fixture, golden, Makefile, Fortran source, binary or
  patch change.
- No product test run, full Linux matrix or Windows process.
- No product-version, banner, tolerance or reference change.
- No push, fetch, amend, tag or release.

## Commit boundary

One documentation-only local commit:

`Record X6996-S21 plot-only no-port verification`

Only this document and its report are committed.
