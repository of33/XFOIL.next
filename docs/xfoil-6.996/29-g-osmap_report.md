# XFOIL 6.996 — G-OSMAP gate report

Status: PASS — the gate is decided. This is a documentation-only gate record.
No product code, test, fixture, golden, Fortran source, binary or patch is
changed.

```text
G-OSMAP: PROVEN-6.99-COMPATIBILITY
X6996-G-OSMAP: PASS
```

## Starting state after S21

- S21 commit status: already existed.
- S21 verification commit:
  `63d0fd6c556b2800b905a353fe09c2d8ea8dbbf4`.
- S21 parent: `fbff332ef2d89bcc86e7ecbecbe28b1bd4baa429`.
- S21 contains exactly the S21 document pair; both S21 PASS markers are
  committed; protected product/test paths are absent from its diff.
- HEAD at gate start is the S21 commit.
- Worktree clean, staged 0, untracked 0, ahead 39 behind 0.
- Tag `v1.0.0` remains
  `dc7305b057286574cd137cb6f1af170b74650ec5`.
- No S21 duplicate, amend, push, fetch, tag or release was created.
- No G-OSMAP commit or prior gate document existed.

## U6 gate contract

U6 defines OSMAP as the Orr-Sommerfeld stability map, not an operating-system
map:

- `osmap.f` reads and tricubically interpolates the Orr-Sommerfeld table;
- `ntcalc.f` integrates amplification over the boundary layer;
- `frplot.f` displays frequency amplitudes;
- `getosfile.c` reads the `OSMAP` environment variable;
- the table is an external binary Fortran unformatted file.

`G-OSMAP` must establish data provenance, file format, runtime trigger,
control values and compatibility. The U6 options are proven 6.99 compatibility,
a new data contract, or defer. `S22` is diagnostic-only and may not copy data
or implement a product path before the gate.

## Upstream comparison

| Item | 6.99 | 6.996 | Finding |
|---|---|---|---|
| `osmap.f` | present | present | byte-identical; SHA-256 `3c806b1d5bd62b80d2a2180b93ea2db95cb5f56c5ec7e55c07868fa20065e719` |
| `ntcalc.f` | present | present | byte-identical; SHA-256 `f47e31d970312cb4d095de9e9aea6ea305b688156f472e5c7a1bb947b24628ba` |
| `frplot.f` | present | present | byte-identical; SHA-256 `45653f7547ffc132eb0507903f4918f4779fea004099e7e228143bf8d152fe1e` |
| `getosfile.c` | source absent from subset | present | environment helper, not a data-format change |
| `osmap.dat` | REAL*4 canonical data | absent | no 6.996 data artifact exists |

The 6.99 and 6.996 binaries both contain the `getosfile_` symbol with the same
small function size and the same `getenv`/`strcpy`/`strlen` behavior. The
source-level addition of `getosfile.c` in 6.996 therefore does not create a new
runtime contract.

There is no source-level 6.996 OSMAP data change. `osmapDP.f` and
`osmapDP.dat` remain inactive and are not selected.

## Data provenance and format

The retained canonical data is `fortran-src/xfoil-6.99/osmap.dat`.

| Property | Value |
|---|---|
| Provenance | XFOIL 6.99 canonical active REAL*4 OSMAP data; repository-protected |
| Size | 825136 bytes |
| SHA-256 | `744d0e5a6948de9a5864a04177735d9620ccbbcb1abcb8fd1af284fd52a9b949` |
| Format | Fortran unformatted sequential |
| Record markers | 4-byte little-endian length prefix/suffix |
| Table precision | REAL*4 |
| Header | `NR=28`, `NW=41`, `NH=18`, 12 bytes |
| Coordinates | `RL` 28 values, `WL` 41 values, `HL` 18 values |
| Index record | 288 bytes (`IR1`, `IR2`, `IW1`, `IW2` per H level) |
| Table order | `IC`, `IH`, `IW`, `IR`, eight derivative tables |

The 6.996 source set contains no `osmap.dat`; the 6.99 data is not copied or
reclassified as a 6.996 upstream artifact. The active/inactive OSMAP selection
and hashes remain protected by the existing release inventory.

## Current C++ mapping

| Area | Current behavior |
|---|---|
| `include/xfoil/core/inverse/OrrSommerfeldMap.h` | double-valued OSMAP/NTCALC core over a REAL*4 table |
| `src/core/inverse_design/OrrSommerfeldMap.cpp` | tricubic lookup and amplitude integration; no file or environment I/O |
| `include/xfoil/io/OrrSommerfeldCodec.h` | REAL*4 Fortran-unformatted decoder |
| `src/io/OrrSommerfeldCodec.cpp` | 4-byte little-endian record decoder |
| `tests/OsmapLoader.*` | test-only adapter for the canonical 6.99 data |
| session/CLI/public API | no OSMAP loader, command, environment integration or output |
| plotting | no `FRPLOT`/`BLPLOT` product path |

The C++ core already matches the canonical interface shape: double-valued
public computations over the REAL*4 database. It is not reachable from a
product command, so this gate activates no new product behavior.

## Runtime trigger and errors

- The only Fortran call chain is `FRPLOT` → `NTCALC` → `OSMAP`.
- `FRPLOT` is reached from the plot-only `BLPLOT` `FRPL` path.
- No non-graphical CLI trigger exists.
- If `OSMAP` is unset, the source reports
  `OSMAP: Environment variable OSMAP not defined` and returns zero
  amplification.
- If the file cannot be opened, the source reports the missing
  Orr-Sommerfeld database and returns zero amplification.
- The no-file state is sticky for the process; there is no retry.

The gate measurement used an external direct symbol probe after Fortran
runtime startup. That probe is diagnostic only and does not create a product
trigger.

## Oracle measurement

External audit area: `xfoil-6.996/g-osmap/`.

Binaries:

| Role | SHA-256 |
|---|---|
| `fortran-bin/xfoil` | `f5779d6954d0b8fdc152e0e0633f18732e00b7b69228d561d629bd524bba598c` |
| `fortran-bin/xfoil-6.996` | `000b0468d9a7f4fce42d6fac24bd14147d7d326c8a9035909c8e9f98d180f50e` |

Both binaries are built with the canonical `-fdefault-real-8` contract.
`OSMAP` therefore has a REAL(8) call interface while the table storage remains
`REAL*4`.

Control values and results, using the 6.99 data:

| `RSP` | `WSP` | `HSP` | 6.99 `ALFR` | 6.996 `ALFR` | 6.99 `ALFI` | 6.996 `ALFI` |
|---:|---:|---:|---|---|---|---|
| 1000 | 0.01 | 2.5 | `3.95229781367356231e-02` | identical | `2.48110216583021263e-03` | identical |
| 300 | 0.01 | 2.5 | `3.43858626613551752e-02` | identical | `7.72928783925969868e-03` | identical |
| 2000 | 0.01 | 2.5 | `4.27881429058473012e-02` | identical | `-3.93202563713288390e-04` | identical |

At `RSP=1000`, `WSP=0.01`, `HSP=2.5`, both binaries also return
`ALFRW_W=-5.48767162458986704e+01` and
`ALFIW_H=-1.28575341295473633e+00` identically.

The complete three-point output files are byte-identical:

```text
SHA-256 f76b7b0a27f90a29774d8ec6e9d340ec9d7c9272da75df05b7d3dd867d37a34c
```

Error measurements:

| Condition | 6.99 | 6.996 |
|---|---|---|
| `OSMAP` unset | `OK=0`, all outputs zero | `OK=0`, all outputs zero |
| `OSMAP` names a missing file | `OK=0`, all outputs zero | `OK=0`, all outputs zero |

The error-output files for both binaries share SHA-256
`7d9fef781f22d01941f4dd9c7e56f5fefe9a848db5e06a885504bc15539124c4`.
Both binaries therefore have identical lookup and error behavior for the
measured contract.

## Platform effect

- `getosfile.c` uses portable C `getenv`, `strcpy` and `strlen`; no POSIX-only
  call is introduced.
- The data contract remains the existing little-endian, 4-byte-record-marker
  Fortran file. No cross-endian claim is made.
- The C++ decoder already implements that existing contract.
- Windows is not run or claimed by this gate. No new Windows-specific surface
  is introduced. Any future product OSMAP path would remain subject to
  `G-WINDOWS` in its own slice.
- No plot library, display, cursor or headless backend is involved.

## Options evaluation

| Criterion | Prove 6.99 compatibility | New data contract | Defer |
|---|---|---|---|
| 6.996 parity | reader/ntcalc/frplot source identical; measured parity | no source or data support | parity evidence ignored |
| 6.99 compatibility | preserved | unproven migration | preserved but undecided |
| Product API | unchanged | would require new API | unchanged |
| Session state | unchanged | would require new state | unchanged |
| File formats | retained unchanged | new unsupported format | retained |
| POSIX | unchanged | new loader/path scope | unchanged |
| Windows | unchanged; not expanded | new format/platform work | unchanged |
| Existing goldens | unchanged | migration risk | unchanged |
| Canonical references | protected | new reference needed | protected |
| Migration | none | required | postponed |
| S22 | diagnostic record released | would need new scope | remains blocked |
| Maintainability | one retained contract | hybrid unsupported contract | unresolved |

The proven-compatibility option is selected. A new data contract has no
upstream artifact and no generation/provenance evidence. Defer is unnecessary
because the existing data contract is measured and source-identical.

## Decision

XFOIL.next 1.1.0 retains the canonical XFOIL 6.99 REAL*4 OSMAP data and the
existing C++ REAL*4 decoder. It does not adopt or copy a 6.996 data artifact
because none exists.

`getosfile.c` is accepted only as the same environment-helper role already
present in the 6.99 binary. It does not create a new data format, product
loader, command or API.

No new OSMAP product implementation is authorized by this gate. The existing
`OrrSommerfeldMap` core and codec remain as the retained compatibility contract.

## S22 release

`S22` is released for the diagnostic/compatibility record only.

```text
U6 slice: S22, group G13
Scope: OSMAP diagnostic/compatibility record
Allowed: document the retained 6.99 data/reader contract and external probe
Not allowed: product implementation, data copy, new data format, CLI/API,
             plot backend, FRPLOT activation
```

Dependencies: `S09` is accepted and `G-OSMAP` is now PASS. No further gate is
required for the diagnostic-only scope. A future product integration of OSMAP
is a separate product-scope decision, not this S22.

Required S22 verification, if the diagnostic record is written:

- repeat the external three-point probe;
- retain the data and output hashes;
- recheck unset/missing-file behavior;
- recheck the protected 6.99 data and inactive `osmapDP` selection;
- no product test, no Windows process and no full Linux suite for the
  documentation-only record.

## Protected state

- Product code changed: 0.
- Tests changed: 0.
- Fixtures/goldens changed: 0.
- Canonical 6.99 references changed: 0.
- Fortran sources/binaries/patches changed: 0.
- OSMAP data copied or generated: none.
- Product version and banner unchanged; `v1.0.0` unchanged.
- `G-PLOT-HEADLESS` remains open and deferred.
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

## Git status

At gate start:

```text
Worktree: clean
Staged: 0
Untracked: 0
Ahead/behind: 39/0
S21 commit: 63d0fd6c556b2800b905a353fe09c2d8ea8dbbf4
```

This gate is committed only after the two-document boundary, private-path
check, documentation gates and staged-diff check pass. The final commit hash is
reported by the outer task because it cannot be embedded in the same commit.

## Next step

Write the S22 diagnostic/compatibility record only. Do not implement OSMAP
product code, do not copy data and do not activate `FRPLOT`. Keep
`G-PLOT-HEADLESS`, `G-EXPORT` and S14/SVG/DXF open/blocked.

## Final decision

G-OSMAP: PROVEN-6.99-COMPATIBILITY
X6996-G-OSMAP: PASS
