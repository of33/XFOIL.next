# XFOIL 6.996 — S22 OSMAP diagnostic/compatibility

Status: PASS — `X6996-S22` and `X6996-S22-R1` accepted as a diagnostic,
verification and documentation slice. No product implementation is included.

Slice: `S22` of U6, group G13. Scope is the OSMAP diagnostic/compatibility
record after the accepted `G-OSMAP` gate.

## Gate basis

```text
G-OSMAP: PROVEN-6.99-COMPATIBILITY
X6996-G-OSMAP: PASS
```

G-OSMAP commit:
`35b49cabde41dd0e86afa1a102ad2af47f05dc3d`

G-OSMAP parent:
`63d0fd6c556b2800b905a353fe09c2d8ea8dbbf4`

The gate commit contains only the G-OSMAP document pair. It authorizes S22
only as a diagnostic/compatibility record, not as a product OSMAP
implementation.

## S22 contract from U6

- U6 slice: `S22`, group G13.
- Scope: OSMAP diagnostic/compatibility only.
- Fortran paths: `osmap.f`, `ntcalc.f`, `frplot.f`, `getosfile.c`.
- C++ mapping: OSMAP mapping only after the gate; no product implementation.
- Dependencies: `S09` (accepted) and `G-OSMAP` (accepted).
- Oracle: `H-S22-OSMAP`; `a04`/`a05` remain untriggered.
- Expected result: diagnostic record only; no data file.
- Commit boundary: one documentation-only S22 verification commit.

## Product contract

XFOIL.next retains the canonical XFOIL 6.99 REAL*4 OSMAP data and the existing
C++ REAL*4 decoder:

- canonical data: `fortran-src/xfoil-6.99/osmap.dat`;
- size: 825136 bytes;
- SHA-256:
  `744d0e5a6948de9a5864a04177735d9620ccbbcb1abcb8fd1af284fd52a9b949`;
- format: Fortran unformatted sequential, 4-byte little-endian record markers,
  REAL*4 table values;
- dimensions: `NR=28`, `NW=41`, `NH=18`.

The existing C++ `OrrSommerfeldMap`/`NtCalc` core and
`decodeOrrSommerfeldRaw` codec remain the retained compatibility contract.
They are not exposed by a product CLI, session path, environment loader or
public API.

There is no 6.996 OSMAP data artifact to adopt. `osmapDP.f`/`osmapDP.dat`
remain inactive. `getosfile.c` is accepted only as the same environment-helper
role used by the canonical oracle builds; it does not create a product
contract.

## Explicit non-activation

S22 must not:

- add a product OSMAP loader, environment path, CLI command or public API;
- copy or generate `osmap.dat` or `osmapDP.dat`;
- change the active REAL*4 / inactive DP selection;
- activate `FRPLOT`, `BLPLOT` or any plot backend;
- alter the canonical 6.99 data or C++ decoder;
- add a cross-endian or Windows compatibility claim;
- change product sources, tests, fixtures, goldens, references or Makefile;
- run the full Linux matrix or any Windows process.

## Verification plan

- Confirm the G-OSMAP commit and its exact document boundary.
- Confirm the protected binary and data hashes and the 825136-byte size.
- Confirm `osmap.f`, `ntcalc.f` and `frplot.f` source parity.
- Confirm active REAL*4 / inactive DP selection and the absence of a 6.996
  data artifact.
- Re-run the external three-point OSMAP probe on both protected binaries.
- Confirm identical `ALFR`/`ALFI` values and the full-output hash.
- Confirm unset and missing-file behavior: `OK=0`, all outputs zero.
- Confirm the C++ core/codec inventory and absence of a product path.
- Run the documentation gates and stage only this document pair.

## Platform boundary

The retained data contract is the existing little-endian 4-byte-record-marker
Fortran file. No cross-endian, Windows or platform-expansion claim is added.
Any future product runtime path would need its own scope and `G-WINDOWS`
treatment.

## Commit boundary

One documentation-only local commit:

`Record X6996-S22 OSMAP compatibility verification`

Only this document and its report are committed.
