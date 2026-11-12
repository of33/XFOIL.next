# XFOIL 6.996 — G-OSMAP gate decision

Status: PASS — the gate is decided and the S21 verification commit is
confirmed. This document changes no product code, test, fixture, golden,
Fortran source, binary or patch.

Gate: `G-OSMAP`

Decision:

```text
G-OSMAP: PROVEN-6.99-COMPATIBILITY
X6996-G-OSMAP: PASS
```

The decision is to retain the canonical XFOIL 6.99 REAL*4 OSMAP database and
the existing C++ REAL*4 decoder contract. XFOIL.next 1.1.0 adopts no 6.996
OSMAP data artifact, because none exists, and introduces no new OSMAP data
contract.

## U6 gate contract

U6 defines OSMAP as the Orr-Sommerfeld stability-map path, not an
operating-system map:

- `osmap.f` is the Orr-Sommerfeld map reader/interpolator.
- `ntcalc.f` integrates the amplification rate.
- `frplot.f` displays the resulting frequency amplitudes.
- `getosfile.c` reads the `OSMAP` environment variable.
- The data file is an external binary Fortran unformatted file.

U6 requires `G-OSMAP` to establish:

- data provenance;
- file format;
- runtime trigger;
- control values;
- compatibility with existing 6.99 data and the current C++ reader.

U6 options are: proven 6.99 compatibility, a new data contract, or defer.
`S22` remains a diagnostic/decision slice; no data copy or product
implementation is allowed before the gate.

## Starting state after S21

- S21 verification commit already exists:
  `63d0fd6c556b2800b905a353fe09c2d8ea8dbbf4`.
- S21 parent: `fbff332ef2d89bcc86e7ecbecbe28b1bd4baa429`.
- S21 contains only the S21 document pair and both S21 PASS markers.
- HEAD is the S21 commit; worktree clean; staged 0; untracked 0.
- No S21 duplicate or amend was created.
- Tag `v1.0.0` remains
  `dc7305b057286574cd137cb6f1af170b74650ec5`.
- No push, fetch, tag or release was performed.

## Upstream finding

| File | 6.99 | 6.996 | Finding |
|---|---|---|---|
| `osmap.f` | present | present | byte-identical |
| `ntcalc.f` | present | present | byte-identical |
| `frplot.f` | present | present | byte-identical |
| `getosfile.c` | source not in 6.99 subset | present | environment helper; binary function same role in both builds |
| `osmap.dat` | REAL*4 canonical data | absent | no 6.996 data artifact exists |

The 6.996 source addition of `getosfile.c` is not a numerical or data-format
change. The canonical 6.99 binary already contains the same environment-helper
function.

## Canonical data contract

The active data file remains:

```text
fortran-src/xfoil-6.99/osmap.dat
```

Measured properties:

| Property | Value |
|---|---|
| Size | 825136 bytes |
| SHA-256 | `744d0e5a6948de9a5864a04177735d9620ccbbcb1abcb8fd1af284fd52a9b949` |
| Format | Fortran unformatted sequential, 4-byte record markers |
| Record-marker byte order | little-endian |
| Table values | REAL*4 |
| Dimensions | `NR=28`, `NW=41`, `NH=18` |
| Header record | 12 bytes |
| Index record | 288 bytes |
| Coordinate records | `RL`, `WL`, `HL` |

The inactive `osmapDP.f`/`osmapDP.dat` pair remains untouched and inactive.
No data is copied into `fortran-src/xfoil-6.996/` or any product data path.

## Runtime trigger and errors

- `NTCALC` is called only from `FRPLOT`.
- `FRPLOT` is reached from the plot-only `BLPLOT` `FRPL` path.
- No non-graphical CLI trigger exists in the product.
- `OSMAP` must name the data file when the path is first used.
- If `OSMAP` is unset, the source reports the missing environment variable and
  returns zero amplification.
- If the file cannot be opened, the source reports the missing database and
  returns zero amplification.
- A missing file sets the no-file state for the rest of the process; there is
  no retry.

The gate measurement therefore used an external direct symbol probe, not a
product command. The probe does not create or change a product trigger.

## Decision consequences

- Keep the 6.99 REAL*4 database byte-for-byte.
- Keep the existing C++ `OrrSommerfeldMap` and REAL*4 codec contract.
- Do not copy 6.99 data as a claimed 6.996 artifact.
- Do not introduce a new data contract or generator in this exposure.
- Do not expose a new product OSMAP command, environment loader or public API.
- Do not activate the `FRPLOT`/`BLPLOT` plot path.
- Do not change fixtures, goldens, canonical references or `osmapDP` selection.
- `G-PLOT-HEADLESS` remains open and deferred.
- `G-EXPORT` remains open; S14/SVG/DXF remain blocked.

## S22 release

`S22` is released only for a diagnostic/compatibility record:

```text
S22 (G13): OSMAP diagnostic/compatibility record only
Scope: document the retained 6.99 data/reader contract and external probe.
No product implementation, no data copy, no product CLI/API, no plot backend.
```

`S09` is accepted and `G-OSMAP` is now decided. No further gate is required
for this diagnostic-only scope. A later product OSMAP integration would be a
new, separately authorized scope, not part of S22 as defined.

## Verification plan

- Confirm S21 commit identity, scope, markers and clean state.
- Verify source identity for `osmap.f`, `ntcalc.f` and `frplot.f`.
- Verify 6.99 data hash, size and record layout.
- Measure 6.99 and 6.996 binaries on the same 6.99 data and control values.
- Measure unset and missing-environment/file error behavior.
- Run only the documentation gates; no full Linux suite and no Windows run.
- Commit only the two G-OSMAP documents.

## Non-goals

- No S22 implementation.
- No product code or test change.
- No Fortran source, binary or patch change.
- No new data file, data generation or data copy.
- No OSMAP command, environment loader or public API.
- No plot backend, cursor path or `FRPLOT` activation.
- No S14/SVG/DXF or `G-EXPORT` work.
- No push, fetch, amend, tag or release.

## Commit boundary

One documentation-only local commit:

`Decide XFOIL 6.996 OSMAP contract`

Only this document and its report are committed.
