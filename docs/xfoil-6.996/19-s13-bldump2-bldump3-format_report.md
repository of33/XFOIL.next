# XFOIL 6.996 — S13 BL output and sidefile report

Status: PASS - `X6996-S13-G1` and Linux/Windows implementation complete.

Decision: `G-BL-SIDEFILE: PRESERVE-EXISTING-BLDUMP-AND-ADD-VERSIONED-BLDUMP3`.

## Starting state

- HEAD: `f3ef3acab60bcef358a85c7d35c30df6054d9b78`
  (`Integrate XFOIL 6.996 polar dump format`).
- Parent: `0db3c37ce7ac2f28c9284a15c61494aaae60ab7a`.
- Worktree clean, staged 0, ahead 25.
- Tag `v1.0.0` unchanged at `dc7305b057286574cd137cb6f1af170b74650ec5`.
- No push, fetch, tag or release.

G1 changed only the two S13 documents. No C++, test, fixture, golden,
reference, Fortran, patch, product-version, banner or tolerance file was
changed.

## Sources and method

Compared `fortran-src/xfoil-6.99/xoper.f`, `fortran-src/xfoil-6.996/xoper.f`,
`fortran-src/xfoil-6.99/xbl.f` and `fortran-src/xfoil-6.996/xbl.f`, plus the
headers needed for `H`, `H*`, `DSSR` and the VISCAL sidefile. The current C++
path was traced through `ExportFormats`, `ExportData`,
`currentBoundaryLayerDumpInput`, `handleOperDump` and `handleOperDmp`.

External measurements are in `external audit area: xfoil-6.996/s13/`;
the complete field matrix is `xfoil-6.996/s13/field-matrix.tsv`.

## Sidefile finding

The automatic `:00.bl` file is written inside `VISCAL`, not by `BLDUMP`.
In 6.99 the block computes the three-character trip-name and writes a
side-1 station file. In 6.996 the same block is inside `if(.false.)` and is
therefore intentionally disabled.

Measured behavior:

- 6.99 viscous a08 reference: `100.bl` (portable name for `:00.bl`) present;
- 6.996 a08: no automatic sidefile;
- current C++: no automatic sidefile writer, only explicit `DUMP`/`DMP`.

The automatic file is not a hidden part of the BLDUMP command contract. The
existing explicit export commands are the supported path.

## Format comparison

| Format | Header | Data columns | Side/wake | 6.99 | 6.996 |
|---|---|---|---:|---|---|
| BLDUMP | 14 labels blank delimiter; 8 labels comma/tab | 12 surface / 8 wake; 8 with delimiter | surface GAM-selected, wake side 2 | yes | same bytes |
| BLDUMP2 | fixed 24 labels + index line | 24 | side 1, then side 2 including wake | yes | same schema; RH value change |
| BLDUMP3 | fixed 20 labels + index line | 20 | side 1, then side 2 including wake | no command/format | new |

BLDUMP details:

- blank header names include `tau` and `Di`, but the blank data branch writes
  only the first 12 values; `tau` and `Di` are header-only;
- wake rows contain only `s, x, y, Ue/Vinf, Dstar, Theta, Cf, H`;
- KDELIM comma/tab reduces header and data to the first 8 columns;
- a08 BLDUMP is byte-identical between 6.99 and 6.996.

BLDUMP2 details:

- `KDELIM` is read but ignored; 24 fixed blank columns;
- at M=0, 6.99 and 6.996 output is byte-identical;
- at M=0.3, 6.996 applies `RH` only to columns 12, 13, 15, 16, 19, 20, 21
  and 24; headers and widths are unchanged;
- `RH=(HERAT/HIRAT)^(1/GAMM1)`, `HIRAT=1-0.5*HSTINV*QINF^2`,
  `HERAT=1-0.5*HSTINV*UE^2`;
- this is a value normalization variant, not a new layout.

BLDUMP3 details:

- `BLOU` is accepted only by 6.996; 6.99 rejects it and writes no file;
- 20 fixed blank columns; `KDELIM` is ignored;
- side 2 contains the wake; blank lines after side 1 and at `IBLTE(2)`;
- `b` is a constant `1.0`, not a physical quantity;
- `Cp` uses `1-UE^2` for MINF<0.05 and the compressible relation otherwise;
- `rhoe/rhoinf`, `Me` and `Rtheta` are density/compressibility corrected;
- `delta**` is `DSSR`, the S08 density-thickness field;
- write-only: no reader, no embedded version record, no roundtrip contract.

## Field matrix

Complete matrix location:
`xfoil-6.996/s13/field-matrix.tsv` (58 rows, all requested columns).

BLDUMP3 columns:

| # | Header | Fortran expression | Meaning |
|---:|---|---|---|
| 1 | x | `X(I)` | x coordinate |
| 2 | y | `Y(I)` | y coordinate |
| 3 | s | `XS=XSSI(IBL,IS)` plus side-2 wake shift | arc coordinate |
| 4 | b | `1.0` | constant marker |
| 5 | Cp | `1-UE^2` or compressible `PE` formula | pressure coefficient |
| 6 | Ue/Vinf | `UE=UEDG(IBL,IS)` | edge velocity ratio |
| 7 | rhoe/rhoinf | `RHOE=(HERAT/HIRAT)^(1/GAMM1)` | density ratio |
| 8 | Me | `SQRT(AMSQ)` | edge Mach |
| 9 | Hk | `HKIN(H,AMSQ)` | kinematic shape factor |
| 10 | Rtheta | `RH*UE*TH/AMUE`; `HVRAT=0.35` | Re based on theta |
| 11 | m | `DS*RHOE*UE` | mass defect |
| 12 | P | `TH*RHOE*UE^2` | pressure defect |
| 13 | K | `0.5*TS*RHOE*UE^3` | energy defect |
| 14 | Cf/2 | `0.5*TAU/(0.5*QINF^2)` | half skin friction |
| 15 | CD | `DIS/QINF^3` | dissipation coefficient |
| 16 | Ctau | `CTAU(IBL,IS)` | transition indicator |
| 17 | delta* | `DSTR(IBL,IS)` | displacement thickness |
| 18 | theta | `THET(IBL,IS)` | momentum thickness |
| 19 | theta* | `TSTR(IBL,IS)` | energy thickness |
| 20 | delta** | `DSSR(IBL,IS)` | density thickness |

BLDUMP2 RH columns:

| # | Header | 6.99 | 6.996 |
|---:|---|---|---|
| 12 | m | `DS*UE` | `DS*RH*UE` |
| 13 | P | `TH*UE^2` | `TH*RH*UE^2` |
| 15 | Pp | `TH*UE^2-PTAUI` | `RH*TH*UE^2-PTAUI` |
| 16 | K/2 | `0.5*TS*UE^3` | `0.5*TS*RH*UE^3` |
| 19 | -m du/ds | `-UE*DS*DUDS` | `-RH*UE*DS*DUDS` |
| 20 | dP/ds | `TAU-UE*DS*DUDS` | `TAU-RH*UE*DS*DUDS` |
| 21 | DV | `TH*UE^2-DPREI*DK` | `TH*RH*UE^2-DPREI*DK` |
| 24 | Rtheta | `UE*TH*REINF` | `RH*UE*TH*REINF` |

Column 14 (`Pf`) is `PTAUI` in both variants and is not RH-scaled.

## Controlled measurements

| Case | Format | Binary | Exit | Size | SHA-256 |
|---|---|---|---:|---:|---|
| a08 | BLDUMP | 6.99 | 0 | 19216 | `29756edd29f710b1950802186d1ad42b693d0db889b803b5c9aa4d20005ed1e6` |
| a08 | BLDUMP | 6.996 | 0 | 19216 | same |
| DMP M=0 | BLDUMP2 | 6.99 | 0 | 44784 | `9ad3d496b5436c1a55759273dc9d0beeb4b009fc2b5ac1511cf51abe82eee532` |
| DMP M=0 | BLDUMP2 | 6.996 | 0 | 44784 | same |
| DMP M=0.3 | BLDUMP2 | 6.99 | 0 | 44784 | `917a4fe37e4e1962d6759a0143102a171f5da43e84c50f5d373855dc323afd5e` |
| DMP M=0.3 | BLDUMP2 | 6.996 | 0 | 44784 | `19680d092f120958c6bcfe95e9cc0b29c9926080a30ea4dec2ac762b8dbdbfcd` |
| a28 | BLDUMP3 | 6.996 | 0 | 42704 | `04d81ba85130f109ef5d06f260a318f52e648bac9de7e792f1112d7047138c5d` |
| a28 | BLDUMP3 | 6.99 | 2 | no file | `BLOU` not recognized |
| H-S13 M=0.3 | BLDUMP3 | 6.996 | 0 | 42704 | `96fc1866202c1449fedb1ae17df4a62f5c9db75ff134fc28241d1f1de788b289` |

Line structure: a08 has 174 lines / 173 data rows, no blank separator.
DMP has 177 lines / 173 data rows and blank lines at 85/155. BLDUMP3 a28
has 177 lines / 173 data rows and blanks at 85/155; the M=0.3 H-S13 case
has blanks at 84/155 because its side-1 station count differs.

Repeatability: two independent a08 runs, two a28 runs and two runs of each
DMP M=0/M=0.3 variant were byte-identical.

`delta**` was checked against the S08 `DSSR` instrumentation for the same
Clark Y M=0.3 case. After mapping the S08 surface/wake rows to the two
BLDUMP3 side loops, the values agree to the `F13.8` print precision (max
observed rounding difference below `5e-9`). The S08 table has more digits,
so no byte equality is claimed.

## C++ mapping and deviations

| Fortran | C++ | Caller | Existing tests | Difference |
|---|---|---|---|---|
| BLDUMP | `boundaryLayerDumpText`, `boundaryLayerDumpRow` | `handleOperDump` | `IoBldumpMatchesFortran`, `OperFileExportTests` | matches 6.99 |
| BLDUMP2 | `boundaryLayerExtendedDumpText`, `boundaryLayerExtendedDumpRows` | `handleOperDmp` | `ExportBlExtendedDumpRows`, `OperFileExportTests`, `BoundaryLayerOutputS13*` | default 6.99 values; explicit 6.996 RH contract |
| BLDUMP3 | `boundaryLayerDump3TextChecked`, `boundaryLayerDump3Text` | S16 follow-up | `BoundaryLayerOutputS13Bldump3*` | exact 6.996 20-column writer |
| `:00.bl` | none | explicit exports only | portable-name tests | 6.996 absence already |

`CommandInterpreter` has no `BLOU` handler. `BoundaryLayerDumpInput` and
`BlSideStations` have no `DSSR` member. The S13 implementation must extend
the existing export types, not create a second BL data model.

Historical clarification: the preceding sentence describes the G1 state
before implementation only. S13 subsequently added the optional `dssr`
member to `BoundaryLayerDumpInput` and wired `viscals.dssr` into
`currentBoundaryLayerDumpInput`; `BlSideStations` was not extended because
no second BL data model was introduced. The G1 text is intentionally kept
unchanged as a historical record.

## Decision

`X6996-S13-G1: PASS`

`G-BL-SIDEFILE: PRESERVE-EXISTING-BLDUMP-AND-ADD-VERSIONED-BLDUMP3`

Contract split:

1. **Existing writer:** keep BLDUMP bytes and the current BLDUMP2/DMP 6.99
   value contract as default. Do not silently change compressible DMP.
2. **BLDUMP2:** expose 6.996 RH-corrected values only through an explicit
   versioned writer/API; M=0 remains byte-identical.
3. **BLDUMP3:** add an exact 6.996 20-column writer as a separate versioned
   format; write-only; no reader, no automatic detection, no roundtrip
   claim.
4. **Sidefile:** accept the 6.996 absence of the automatic `:00.bl`; keep
   explicit `DUMP`/`DMP` as the export path. Do not add a hidden sidefile
   writer.
5. **Command activation:** `BLOU` remains S16. S13 adds no command,
   defaults, product version or banner change.
6. **Compatibility:** existing 6.99 BL references/fixtures remain
   unchanged; new BLDUMP3 format tests and versioned fixtures are created
   only in the implementation slice.

## Dependencies

- `S08` hard: BL fields, especially `DSSR`/`delta**`.
- `S09` hard: CFT results consumed by the station data.
- `S16` downstream: `BLOU` command consumes the S13 BLDUMP3 writer.
- `S17` downstream: session invalidation and sequence behavior.
- `G-BL-SIDEFILE` is closed by this decision; no automatic sidefile is
  added.

## Non-goals

- No `BLOU` command registration.
- No new BL numerics or CFT changes.
- No polar text/dump changes (S11/S12).
- No automatic `:00.bl` writer.
- No plot backend, product version, banner or tolerance change.
- No Fortran source or patch change.
- No C++/test/fixture/golden/reference change in G1.

## Next implementation step

`X6996-S13-BLDUMP2-and-Versioned-BLDUMP3`

It implements the explicit 6.996 BLDUMP2 value variant, the versioned
20-column BLDUMP3 writer and focused format/error tests, while preserving
the default BLDUMP/DMP bytes. It remains command-free; S16 owns `BLOU`.

## Protected state and gates

- No repository file other than the two S13 G1 documents is changed.
- No fixture, C++, test, golden, reference, Fortran, patch, product version,
  banner or tolerance change.
- No push, fetch, tag or release.
- No Linux or Windows test matrix was run; G1 is documentation and external
  measurement only.
- Final gates: `git diff --check`, `check-fortran-sha`,
  `check-license-headers`, `check-release-inventory`,
  `check-public-docs`.


## Implementation slice result

- Added `BoundaryLayerExtendedDumpContract`; the default keeps
  `Xfoil699`, and the explicit `Xfoil6996DensityCorrected` variant changes
  only the eight RH-scaled BLDUMP2 columns.
- Extended `BoundaryLayerDumpInput` with optional `dssr` and wired
  `currentBoundaryLayerDumpInput` to `viscals.dssr` using the existing
  one-based/placeholder mapping.
- Added separate write-only BLDUMP3 checked/wrapper APIs with exact header,
  widths, side/wake loop, blank separators, `Cp` branches, `RHOE`, `Me`,
  `Hk`, `Rtheta`, density-scaled defects and `delta** = DSSR`.
- Error contract: invalid dimensions/ranges, missing DSSR for viscous
  BLDUMP3, non-finite values and invalid compressibility denominators return
  `core::Result` errors before output publication.
- Fixtures and metadata:
  `tests/data/xfoil-6.996/s13/bldump2-m03.bl`,
  `bldump2-m03-legacy.bl`, `bldump3-a28.bl`,
  `bldump3-hs13-m03.bl`, `metadata.txt`; M=0 reuses the existing 6.99 DMP
  fixture because the bytes are identical.
- Tests: five focused `unit.BoundaryLayerOutputS13*` cases; full Linux suite
  `2032/2032`; ASan/UBSan full suite `2032/2032`. Existing `DUMP`/`DMP` byte references remain green.
- No `BLOU` activation, no automatic `:00.bl`, no product-version/banner
  change, no S11/S12 change, no Fortran/patch/golden/reference change.
- Windows focused acceptance: `5/5` tests pass.

## Focused acceptance — X6996-S13-R1

Decision: PASS

Evidence (2026-11-11; no code, fixture or expected-value change during R1):

| Check | Result | Evidence |
|---|---:|---|
| GCC 13.3 debug full suite | 2032/2032 | `/tmp/opencode/fulltest2.log` |
| GCC 13.3 release full suite | 2032/2032 | `/tmp/opencode/s13-r1-gcc13-release-full.log` |
| Clang 18.1 debug full suite | 2032/2032 | `/tmp/opencode/s13-r1-clang18-debug-full.log` |
| Clang 18.1 release full suite | 2032/2032 | `/tmp/opencode/s13-r1-clang18-release-full.log` |
| ASan/UBSan full suite | 2032/2032 | `/tmp/opencode/sanitize2.log` |
| Focused Valgrind, `unit.BoundaryLayerOutputS13` | 5/5, no errors | `/tmp/opencode/s13-r1-valgrind-focused.log` |
| Focused Windows acceptance | 5/5 | operator-run S13 focused output |
| Reference verification | 236/236 | `/tmp/opencode/s13-r1-reference.log` |
| Reference determinism | 236/236 | `/tmp/opencode/s13-r1-determinism.log` |
| Harness families | 16/16 | `/tmp/opencode/s13-r1-harnesses.log` |
| Permanent gates | green | S13-R1 gate run |

Permanent gates covered: `git diff --check`, `make check-fortran-sha`,
`make check-license-headers`, `make check-release-inventory` and
`make check-public-docs`.

At R1 start HEAD was already `a0325b2` (`Integrate XFOIL 6.996
boundary-layer output formats`), parent `f3ef3acab60bcef358a85c7d35c30df6054d9b78`.
No duplicate S13 integration commit is created; the R1 report update is a
documentation-only local follow-up. No push, fetch, tag or release was made.
`v1.0.0` remains `dc7305b057286574cd137cb6f1af170b74650ec5`.
