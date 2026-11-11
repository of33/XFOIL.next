# S13 - BL output and sidefile format decision

Status: PASS - `X6996-S13-G1` analysis/decision and Linux/Windows implementation complete.

Decision: `G-BL-SIDEFILE: PRESERVE-EXISTING-BLDUMP-AND-ADD-VERSIONED-BLDUMP3`.

## Scope

S13 owns the boundary-layer text-export contract:

- `BLDUMP` (OPER `DUMP`);
- `BLDUMP2` (OPER `DMP`);
- `BLDUMP3` (6.996 OPER `BLOU`, command activation in S16);
- side/wake station loops, headers, formats and field semantics;
- the automatic VISCAL `:00.bl` side file;
- the future versioned writer and compatibility strategy.

G1 changes no C++, test, fixture, golden, reference, Fortran source, product
version, banner or tolerance.

## Sources

Compared:

- `fortran-src/xfoil-6.99/xoper.f`;
- `fortran-src/xfoil-6.996/xoper.f`;
- `fortran-src/xfoil-6.99/xbl.f`;
- `fortran-src/xfoil-6.996/xbl.f`;
- `XFOIL.INC`/`XBL.INC` only where the dump fields require them.

The automatic side file is written in `VISCAL` (`xoper.f`), not in
`BLDUMP`. In 6.99 it writes `:00.bl` for the untripped `XTRIP=1` case. In
6.996 the same block is guarded by `if(.false.)`; the file is intentionally
absent.

## Existing formats

### BLDUMP

- Fixed blank format header with 14 labels:
  `s, x, y, Ue/Vinf, Dstar, Theta, Cf, H, H*, P, m, K, tau, Di`.
- Surface rows write 12 values in the blank-delimiter branch. The labels
  `tau` and `Di` are header-only; they are not written by this branch.
- Wake rows write 8 values: `s, x, y, Ue/Vinf, Dstar, Theta, Cf, H`.
- With `KDELIM=1` (comma) or `2` (tab), both header and rows use only
  `s, x, y, Ue/Vinf, Dstar, Theta, Cf, H`.
- Surface loop `I=1..N`; side is selected by `GAM(I)<0`. Wake loop
  `I=N+1..N+NW` uses side 2 and `IBL=IBLTE(2)+I-N`.
- Inviscid/unavailable BL data use `Dstar=Theta=Tstar=Cf=0`,
  `H=1`, `H*=2`; the edge velocity is the transformed GAM value.
- 6.99 and 6.996 a08 output are byte-identical (19216 bytes).

### BLDUMP2

- Fixed 24-column header and a second column-index line. `KDELIM` is read
  but not used for the header or data; output is always blank fixed-width.
- Surface and wake are one side-2 station sequence. The loop is
  `IS=1,2`, `IBL=2..NBL(IS)`; a blank line is emitted after side 1 and at
  `IBLTE(2)`.
- Side-2 wake station coordinate:
  `XS = XSSI - XSSI(IBLTE(2),2) + XSSI(IBLTE(1),1)`.
- 6.99 field values use no density factor.
- 6.996 keeps the same 24-column schema and widths but applies
  `RH = (HERAT/HIRAT)^(1/GAMM1)` to selected normalized values:
  columns 12, 13, 15, 16, 19, 20, 21 and 24.
  `HIRAT = 1 - 0.5*HSTINV*QINF^2`,
  `HERAT = 1 - 0.5*HSTINV*UE^2`.
- `RH=1` at Mach 0; the controlled M=0 DMP files are byte-identical
  between 6.99 and 6.996. At M=0.3 only those eight columns differ.
- BLDUMP2 is therefore a value-normalization change, not a new column
  format.

### BLDUMP3

- Signature `SUBROUTINE BLDUMP3(FNAME1)`; new in 6.996.
- Fixed 20-column blank header and index line. `KDELIM` is read but not
  applied.
- Loop `IS=1,2`, `IBL=2..NBL(IS)`; side 2 includes the wake. Blank lines
  after side 1 and at `IBLTE(2)`.
- `b` is a constant `1.0`, not a stored physical field.
- `Cp` uses `1-UE^2` for `MINF<0.05`; otherwise the compressible pressure
  relation.
- `rhoe/rhoinf` and `Me` are density/compressibility quantities.
- `delta**` is the S08 `DSSR(IBL,IS)` field.
- The format is write-only. It has no reader, no embedded version record
  and no roundtrip contract. Its 20-label header distinguishes it from the
  14-label BLDUMP and 24-label BLDUMP2 headers.
- 6.99 does not recognize `BLOU`: no file is written and the command is
  rejected.

## Field matrix

The full external matrix is in the audit area at
`xfoil-6.996/s13/field-matrix.tsv` with the requested columns:
`format, column_index, header_name, fortran_expression, physical_meaning,
unit_or_normalization, surface_or_wake, 6.99_presence, 6.996_presence,
cpp_mapping`.

BLDUMP3 field summary (all columns are surface/wake; side 2 contains the
wake):

| # | Header | Fortran expression | Meaning |
|---:|---|---|---|
| 1 | `x` | `X(I)` | x coordinate |
| 2 | `y` | `Y(I)` | y coordinate |
| 3 | `s` | `XS=XSSI(IBL,IS)` with side-2 wake shift | arc coordinate |
| 4 | `b` | `1.0` | constant marker |
| 5 | `Cp` | `1-UE^2` or compressible `PE` formula | pressure coefficient |
| 6 | `Ue/Vinf` | `UE=UEDG(IBL,IS)` | edge velocity ratio |
| 7 | `rhoe/rhoinf` | `RHOE=(HERAT/HIRAT)^(1/GAMM1)` | edge density ratio |
| 8 | `Me` | `SQRT(AMSQ)` | edge Mach number |
| 9 | `Hk` | `HKIN(H,AMSQ)` | kinematic shape factor |
| 10 | `Rtheta` | `RH*UE*TH/AMUE` with `HVRAT=0.35` | Reynolds number based on theta |
| 11 | `m` | `DS*RHOE*UE` | mass defect |
| 12 | `P` | `TH*RHOE*UE^2` | pressure defect |
| 13 | `K` | `0.5*TS*RHOE*UE^3` | energy defect |
| 14 | `Cf/2` | `0.5*TAU/(0.5*QINF^2)` | half skin friction |
| 15 | `CD` | `DIS/QINF^3` | dissipation coefficient |
| 16 | `Ctau` | `CTAU(IBL,IS)` | transition/intermittency indicator |
| 17 | `delta*` | `DSTR(IBL,IS)` | displacement thickness |
| 18 | `theta` | `THET(IBL,IS)` | momentum thickness |
| 19 | `theta*` | `TSTR(IBL,IS)` | energy thickness |
| 20 | `delta**` | `DSSR(IBL,IS)` | density thickness |

BLDUMP and BLDUMP2 field/column details, including the header-only BLDUMP
`tau/Di` labels and the exact 6.996 RH columns, are in the external matrix.

## C++ current state

| Fortran format | Current C++ routine | Product caller | Existing test | Deviation |
|---|---|---|---|---|
| BLDUMP | `io::boundaryLayerDumpText`, `core::export_data::boundaryLayerDumpRow` | `CommandInterpreter::handleOperDump` (`DUMP`) | `IoBldumpMatchesFortran`, `OperFileExportTests` | matches 6.99; a08 identical to 6.996 |
| BLDUMP2 | `io::boundaryLayerExtendedDumpText`, `core::export_data::boundaryLayerExtendedDumpRows` | `CommandInterpreter::handleOperDmp` (`DMP`) | `ExportBlExtendedDumpRows`, `OperFileExportTests` | 6.99 values, no RH; no 6.996 compressible target |
| BLDUMP3 | absent | absent (`BLOU` not implemented) | none | new 20-column format not represented |
| automatic `:00.bl` | absent | only explicit `DUMP`/`DMP` | portable-name tests only | matches 6.996 absence, not 6.99 automatic sidefile |

No second BL implementation is needed. The existing `ExportFormats` and
`ExportData` types are the natural home for a versioned BLDUMP2 variant and
BLDUMP3.

## Oracle measurements

External audit area: `xfoil-6.996/s13/`; measurement summary is
`xfoil-6.996/s13/measurements.tsv`.

| Case | Format | Binary | Exit | Size | SHA-256 | Rows/columns |
|---|---|---|---:|---:|---|---|
| a08 | BLDUMP | 6.99 | 0 | 19216 | `29756edd29f710b1950802186d1ad42b693d0db889b803b5c9aa4d20005ed1e6` | 174 lines, 12/8 data values |
| a08 | BLDUMP | 6.996 | 0 | 19216 | same as 6.99 | byte-identical |
| DMP M=0 | BLDUMP2 | 6.99 | 0 | 44784 | `9ad3d496b5436c1a55759273dc9d0beeb4b009fc2b5ac1511cf51abe82eee532` | 177 lines, 24 columns |
| DMP M=0 | BLDUMP2 | 6.996 | 0 | 44784 | same as 6.99 | identical at M=0 |
| DMP M=0.3 | BLDUMP2 | 6.99 | 0 | 44784 | `917a4fe37e4e1962d6759a0143102a171f5da43e84c50f5d373855dc323afd5e` | 24 columns |
| DMP M=0.3 | BLDUMP2 | 6.996 | 0 | 44784 | `19680d092f120958c6bcfe95e9cc0b29c9926080a30ea4dec2ac762b8dbdbfcd` | columns 12,13,15,16,19,20,21,24 differ |
| a28 | BLDUMP3 | 6.996 | 0 | 42704 | `04d81ba85130f109ef5d06f260a318f52e648bac9de7e792f1112d7047138c5d` | 177 lines, 173 data, 20 columns |
| a28 | BLDUMP3 | 6.99 | 2 | none | n/a | `BLOU` not recognized |
| H-S13 M=0.3 | BLDUMP3 | 6.996 | 0 | 42704 | `96fc1866202c1449fedb1ae17df4a62f5c9db75ff134fc28241d1f1de788b289` | 177 lines, 20 columns |

Repeatability: a08 and a28 external runs are byte-identical across two
runs; a07 DMP M=0 and M=0.3 each had two identical runs. BLDUMP3 a28 has
blank-line separators at lines 85 and 155; the M=0.3 H-S13 case has them at
84 and 155 because its side-1 station count differs.

For H-S13, the `delta**` column was compared with the S08 instrumentation
`DSSR` field for the same Clark Y M=0.3 case. After mapping the S08
surface/wake rows to the two BLDUMP3 side loops, `delta**` agrees to the
`F13.8` output precision (max observed print-rounding difference below
`5e-9`); no byte equality is claimed because the S08 table carries more
digits.

The automatic 6.99 `:00.bl` sidefile is present in the 6.99 a08 reference
set and absent in 6.996. The current C++ product has no automatic sidefile
writer; explicit `DUMP`/`DMP` output remains deterministic.

## Decision

`G-BL-SIDEFILE: PRESERVE-EXISTING-BLDUMP-AND-ADD-VERSIONED-BLDUMP3`

Reasons:

- BLDUMP is byte-identical between 6.99 and 6.996, including the a08 case.
  Replacing it has no benefit.
- BLDUMP2 has the same 24-column schema; 6.996 changes only density-scaled
  values. Silent replacement would change compressible output and break the
  existing DMP contract.
- BLDUMP3 is a genuinely new 20-column, write-only format with DSSR and
  density/compressibility fields. It must be a separate versioned writer.
- The automatic `:00.bl` sidefile is intentionally disabled in 6.996 and
  the C++ product already has an explicit export path. Do not add a hidden
  automatic writer in S13.

Contract split:

- **Existing writer:** preserve BLDUMP bytes and preserve the existing
  BLDUMP2/DMP 6.99 value contract as the default; do not silently switch
  compressible DMP output.
- **New BLDUMP2 variant:** expose the 6.996 RH-corrected values only through
  an explicit versioned writer/API; at M=0 it remains byte-identical.
- **New BLDUMP3:** exact 6.996 20-column writer, write-only, no reader and
  no roundtrip claim; command activation remains S16.
- **Reader:** no automatic BL-format reader and no guessed detection. If a
  later reader is requested, its format ID must be explicit.
- **Compatibility:** existing 6.99 references and fixtures unchanged;
  BLDUMP3 gets separate versioned tests/fixtures only after the next
  implementation slice.
- **Command activation:** no `BLOU` command in S13; S16 owns registration.

## Dependencies and handoff

- `S08` supplies BL fields, including `DSSR`.
- `S09` supplies the accepted CFT results consumed by the station data.
- `S16` will register `BLOU` and consume the S13 BLDUMP3 writer.
- `S17` owns session invalidation and sequence behavior.
- `G-BL-SIDEFILE` is closed by this decision; no automatic sidefile is
  added.

Next implementation slice:

`X6996-S13-BLDUMP2-and-Versioned-BLDUMP3`

Its scope is the explicit 6.996 BLDUMP2 value variant, the versioned
BLDUMP3 20-column writer and focused format/error tests, preserving the
existing default BLDUMP/DMP bytes. It does not register `BLOU` and does not
change S11/S12.

## Protected state

No repository file other than the two S13 G1 documents is changed. No
fixture, C++, test, golden, reference, Fortran source, patch, product
version, banner or tolerance changes.


## Implementation result

Linux implementation is complete. Changes are limited to the S13 export
surface and focused tests:

- `include/xfoil/io/ExportFormats.h`,
  `src/io/ExportFormats.cpp`:
  `BoundaryLayerExtendedDumpContract` (`Xfoil699`, default;
  `Xfoil6996DensityCorrected`), checked BLDUMP2 writers, and separate
  `boundaryLayerDump3TextChecked`/`boundaryLayerDump3Text` writers.
- `BoundaryLayerDumpInput` now carries optional `dssr`; no second BL data
  model was introduced.
- `src/session/SessionOps.cpp`: `currentBoundaryLayerDumpInput` forwards
  `viscals.dssr` with the existing Fortran placeholder convention.
- No `BLOU` command, menu, help, prompt, redo or bang path was added; no
  automatic sidefile writer was added.
- `boundaryLayerExtendedDumpText` keeps the existing 6.99 default bytes.
  The 6.996 variant applies RH only to columns 12, 13, 15, 16, 19, 20, 21
  and 24; column 14 remains `PTAUI`.
- BLDUMP3 is write-only, ignores `KDELIM`, uses the exact 20-column order
  and keeps side-1/side-2 blank rows and the side-2 wake `s` shift. Invalid
  dimensions, non-finite values and invalid compressibility states fail
  before a result string is returned.

New focused tests use the `unit.BoundaryLayerOutputS13` prefix:
BLDUMP2 M=0 (both contracts byte-identical), BLDUMP2 M=0.3 (6.99 and
6.996 oracle bytes), BLDUMP3 a28, BLDUMP3 H-S13 M=0.3, and invalid-input
rejection. The tests drive the real session state, not reconstructed
rounded output.

Fixtures are under `tests/data/xfoil-6.996/s13/` with `metadata.txt`;
M=0 reuses the existing 6.99 DMP fixture (byte-identical). Linux full
suite: `2032/2032` tests pass; ASan/UBSan full suite: `2032/2032`. `integration.DmpViscousFileMatchesFortranReference`
and `unit.IoBldumpMatchesFortran` remain green, so existing BLDUMP/DMP
contracts are unchanged. Windows focused acceptance: `5/5` tests pass.
