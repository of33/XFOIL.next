# XFOIL 6.996 — S22 OSMAP diagnostic/compatibility report

Status: PASS — `X6996-S22` and `X6996-S22-R1` accepted. This is a
documentation-only diagnostic and verification commit. No product
implementation is included.

```text
G-OSMAP: PROVEN-6.99-COMPATIBILITY
X6996-S22: PASS
X6996-S22-R1: PASS
```

## Starting state

- S22 start HEAD: `35b49cabde41dd0e86afa1a102ad2af47f05dc3d`
  (`Decide XFOIL 6.996 OSMAP contract`).
- Parent: `63d0fd6c556b2800b905a353fe09c2d8ea8dbbf4`
  (`Record X6996-S21 plot-only no-port verification`).
- G-OSMAP commit status: already existed; no duplicate and no amend.
- G-OSMAP commit contains exactly the G-OSMAP plan/report pair and both gate
  markers.
- Worktree clean, staged 0, untracked 0, ahead 40 behind 0.
- Tag `v1.0.0` unchanged at
  `dc7305b057286574cd137cb6f1af170b74650ec5`.
- No S22 commit existed.
- No push, fetch, amend, tag or release.

## G-OSMAP decision and gate commit

```text
G-OSMAP: PROVEN-6.99-COMPATIBILITY
X6996-G-OSMAP: PASS
```

Gate commit:
`35b49cabde41dd0e86afa1a102ad2af47f05dc3d`

Gate parent:
`63d0fd6c556b2800b905a353fe09c2d8ea8dbbf4`

The gate authorizes only the S22 diagnostic/compatibility record. It does not
authorize a product loader, CLI/API, environment integration, data copy or
`FRPLOT` activation.

## U6 S22 scope

- U6 slice: `S22`, group G13.
- Scope: OSMAP diagnostic/compatibility only.
- Expected result: diagnostic record; no data file.
- Dependencies: `S09` accepted; `G-OSMAP` accepted.
- Oracle: `H-S22-OSMAP`; `a04`/`a05` remain untriggered.
- Commit boundary: one documentation-only S22 verification commit.
- No product implementation before or during S22.

## Protected provenance

| Artifact | SHA-256 / property |
|---|---|
| `fortran-bin/xfoil` | `f5779d6954d0b8fdc152e0e0633f18732e00b7b69228d561d629bd524bba598c` |
| `fortran-bin/xfoil-6.996` | `000b0468d9a7f4fce42d6fac24bd14147d7d326c8a9035909c8e9f98d180f50e` |
| `fortran-src/xfoil-6.99/osmap.dat` | `744d0e5a6948de9a5864a04177735d9620ccbbcb1abcb8fd1af284fd52a9b949` |
| `osmap.dat` size | 825136 bytes |
| Provenance | canonical XFOIL 6.99 active REAL*4 OSMAP data |

## Data contract

| Property | Value |
|---|---|
| Format | Fortran unformatted sequential |
| Record markers | 4-byte little-endian length prefix/suffix |
| Table values | REAL*4 |
| Dimensions | `NR=28`, `NW=41`, `NH=18` |
| Header record | 12 bytes |
| Index record | 288 bytes |
| Active database | `fortran-src/xfoil-6.99/osmap.dat` |
| Inactive alternative | `osmap_DP.f` / `osmapDP.dat`, not selected |

No `osmap.dat` exists in `fortran-src/xfoil-6.996/`. No 6.996 data artifact is
copied or generated. No repository OSMAP data was changed, moved or copied.

## Source parity

| File | 6.99 SHA-256 | 6.996 SHA-256 | Result |
|---|---|---|---|
| `osmap.f` | `3c806b1d5bd62b80d2a2180b93ea2db95cb5f56c5ec7e55c07868fa20065e719` | identical | byte-identical |
| `ntcalc.f` | `f47e31d970312cb4d095de9e9aea6ea305b688156f472e5c7a1bb947b24628ba` | identical | byte-identical |
| `frplot.f` | `45653f7547ffc132eb0507903f4918f4779fea004099e7e228143bf8d152fe1e` | identical | byte-identical |
| `getosfile.c` | source not in 6.99 subset | `ef601fbb6f5e0b3c4673065d027a40399c7fa783e13c1b6bfdce5142afc11b06` | helper only |

The 6.996 source addition of the OSMAP environment helper does not change the
numerical or file-format contract.

## Three-point oracle measurement

External audit area: `xfoil-6.996/s22/`.

Method: direct external symbol probe of OSMAP after Fortran runtime startup.
Both protected binaries use the canonical `-fdefault-real-8` interface while
the database remains REAL*4. The probe is diagnostic only and does not create
a product trigger.

Control points and results:

| `RSP` | `WSP` | `HSP` | 6.99 `ALFR` | 6.996 `ALFR` | 6.99 `ALFI` | 6.996 `ALFI` |
|---:|---:|---:|---|---|---|---|
| 1000 | 0.01 | 2.5 | `3.95229781367356231e-02` | identical | `2.48110216583021263e-03` | identical |
| 300 | 0.01 | 2.5 | `3.43858626613551752e-02` | identical | `7.72928783925969868e-03` | identical |
| 2000 | 0.01 | 2.5 | `4.27881429058473012e-02` | identical | `-3.93202563713288390e-04` | identical |

Additional first-point controls:

```text
ALFRW_W = -5.48767162458986704e+01
ALFIW_H = -1.28575341295473633e+00
```

The complete three-point outputs for both binaries are byte-identical:

```text
SHA-256 f76b7b0a27f90a29774d8ec6e9d340ec9d7c9272da75df05b7d3dd867d37a34c
```

## Unset and missing-file behavior

| Condition | 6.99 | 6.996 |
|---|---|---|
| `OSMAP` unset | source diagnostic; `OK=0`; all outputs zero | source diagnostic; `OK=0`; all outputs zero |
| `OSMAP` names a missing file | source diagnostic; `OK=0`; all outputs zero | source diagnostic; `OK=0`; all outputs zero |

The CASE-only diagnostic outputs are byte-identical for both binaries and both
error conditions:

```text
SHA-256 7d9fef781f22d01941f4dd9c7e56f5fefe9a848db5e06a885504bc15539124c4
```

No retry occurs after the no-file state is set. The measured behavior matches
the source-level error contract.

## Current C++ inventory

| Area | Inventory | S22 result |
|---|---|---|
| `include/xfoil/core/inverse/OrrSommerfeldMap.h` | double-valued OSMAP/NTCALC core over REAL*4 table | retained |
| `src/core/inverse_design/OrrSommerfeldMap.cpp` | tricubic lookup and amplification integration; no file/environment I/O | retained |
| `include/xfoil/io/OrrSommerfeldCodec.h` | REAL*4 Fortran-unformatted decoder | retained |
| `src/io/OrrSommerfeldCodec.cpp` | little-endian 4-byte record decoding | retained |
| `tests/OsmapLoader.*` | test-only loader for canonical 6.99 data | unchanged |
| session/CLI/public API | no OSMAP loader, command, env path or output | not activated |
| plotting | no `FRPLOT`/`BLPLOT` product path | not activated |

The existing C++ core and codec remain the complete retained compatibility
contract. No product runtime path is exposed.

## Platform boundary

- The retained data contract is the existing little-endian, 4-byte-record-marker
  Fortran file.
- No cross-endian or platform-expansion promise is added.
- No Windows process or Windows golden is produced.
- `getosfile.c` is oracle/build helper material only and is not a product
  environment integration.
- `G-PLOT-HEADLESS` remains open and deferred.

## Protected state

- Product implementation: none.
- Product code/tests/fixtures/goldens: unchanged.
- Canonical 6.99 references: unchanged.
- Fortran sources/binaries/patches: unchanged.
- `tests/fortran-reference/**`: unchanged.
- OSMAP data copied or generated in the repository: none.
- `osmapDP` selection: inactive.
- `FRPLOT`, `BLPLOT` and plot backend: inactive.
- Product version and banner unchanged; `v1.0.0` unchanged.
- `G-EXPORT` remains open; S14/SVG/DXF remain blocked.
- No push, fetch, amend, tag or release.

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

The two SHA manifests, license-header self-test and live check, release
inventory and public-documentation gate all completed without findings.

## R1 — focused document acceptance

R1 is the final documentation check before the S22 commit. It confirms:

- G-OSMAP is committed and PASS; no duplicate gate commit was made.
- S22 is diagnostic/documentation only.
- The 6.99 and 6.996 OSMAP/NTCALC values are identical on the three control
  points.
- The data SHA-256 and 825136-byte size match.
- The REAL*4 Fortran-unformatted contract is documented.
- The full three-point output hash and error-output hash match.
- Unset and missing-file behavior is identical.
- `osmapDP` remains inactive and no data was copied.
- No product loader, CLI, public API, environment path or `FRPLOT` activation
  was added.
- No product, test, fixture, golden, Makefile, Fortran or reference file was
  changed.
- No full Linux suite and no Windows process ran.
- Both S22 PASS markers are present and the staged diff contains only the S22
  document pair.

## Git status

At S22 start:

```text
Worktree: clean
Staged: 0
Untracked: 0
Ahead/behind: 40/0
S22 start HEAD: 35b49cabde41dd0e86afa1a102ad2af47f05dc3d
```

The final S22 commit hash is reported by the outer task because it cannot be
embedded in the same commit.

## Next step

All defined production slices through S22 are closed except the conditional
S14 export work, which remains blocked by `G-EXPORT`.

The next U6 step is the `G-EXPORT` gate decision. Do not implement S14/SVG/DXF
while `G-EXPORT` is open. `G-PLOT-HEADLESS` remains open and deferred. Do not
start the cumulative 1.1.0 final acceptance without a separate instruction.

## Final decision

G-OSMAP: PROVEN-6.99-COMPATIBILITY
X6996-S22: PASS
X6996-S22-R1: PASS
