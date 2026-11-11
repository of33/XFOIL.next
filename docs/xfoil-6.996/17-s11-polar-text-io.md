# S11 - Polar text I/O and G-POLAR-SAVE decision

Status: PASS - G1 decision accepted; `X6996-S11-Polar-Text-Read-Write-Append`
implemented and green on Linux and in the focused Windows run.

Decision: `G-POLAR-SAVE: PRESERVE-CORRECT-EXISTING-FORMAT`.

## Scope

This G1 analysis determines the exact polar save-file contract of:

- XFOIL 6.99;
- XFOIL 6.996 unpatched;
- XFOIL 6.996 with the accepted dump patch;
- current XFOIL.next.

It covers `POLREAD`, `POLWRIT`, `PLRSET`, `PLRINI` and `PLRADD`, column
ordering, formats, element columns, read/write/append semantics, backwards
compatibility and malformed-column behavior.

Explicit non-goals: no C++, test, fixture, golden or reference change; no
product implementation; no new writer; no Linux or Windows test run.

## Sources and evidence

Fortran sources compared:

- `fortran-src/xfoil-6.99/iopol.f`, `xpol.f`, `PINDEX.INC`, `PPLOT.INC`,
  `XFOIL.INC`;
- `fortran-src/xfoil-6.996/iopol.f`, `xpol.f`, `PINDEX.INC`, `PPLOT.INC`,
  `XFOIL.INC`;
- `patches/xfoil-6.996/0001-rewind-polar-dump-unit-before-header.patch`.

The accepted `P-6996-P1` patch changes only the binary dump `REWIND`; it
does not change any text `POLWRIT`/`POLREAD` line. The patched and unpatched
6.996 text contracts are therefore identical.

External controlled matrix:
`external audit area: xfoil-6.996/s11-g1/format-probe/`.
The matrix table is at `external audit area: xfoil-6.996/s11-g1/matrix.tsv`.

The probe compiles the canonical 6.99/6.996 `POLWRIT`/`POLREAD` against the
canonical includes, writes controlled values, and compares the current C++
reader/writer through `xfoil/io/PolarText`. It creates no repository fixture.

## Canonical base format

For the default selection, both 6.99 and current XFOIL.next write:

```text
alpha  CL  CD  CDp  CM  Top_Xtr  Bot_Xtr  Top_Itr  Bot_Itr
```

The actual default `IPOL` order is `IAL, ICL, ICD, ICP, ICM`; therefore the
header order is `alpha, CL, CD, CDp, CM`, not the `CM, CDp` order that would
be suggested by the declaration order in `PINDEX.INC`.

The side block is written as `Top_/Bot_` pairs in `JPOL` order. In the
default `JPOL = JTN, JTI`, the pairs are:

```text
Top_Xtr  Bot_Xtr  Top_Itr  Bot_Itr
```

Data under the 6.99/current contract follows the same order:
`JTN side 1, JTN side 2, JTI side 1, JTI side 2`.

## Column formats

`CPOLFORM`/`CPOLSFORM` are fixed-width Fortran formats:

| Base/side field | Format |
|---|---|
| alpha | `F7.3` |
| CL | `F9.4` |
| CD | `F10.5` |
| CDp | `F10.5` |
| CM | `F9.4` |
| Mach | `F8.4` |
| Re | `E11.3` |
| Chinge | `F10.5` |
| Cpmin | `F9.4` |
| CDh | `F11.5` |
| Cmdot | `F9.5` |
| Ncrit | `F7.3` |
| Xtrip / Xtr / Itr | `F9.4` |

Element formats from `PINDEX.INC`:

| Element field | Header code | Format |
|---|---|---|
| Cx | `KCX=1`, `Cx_n` | `F9.4` |
| Cy | `KCY=2`, `Cy_n` | `F9.4` |
| Cm | `KCM=3`, `Cm_n` | `F9.4` |
| CDv | `KCDV=4`, `CDv_n` | `F10.5` |

## Exact 6.996 defect

The defect is in `fortran-src/xfoil-6.996/iopol.f` `POLWRIT`, not only in
element columns.

### Header construction

`POLWRIT` builds the side header in this order (`iopol.f` 631-662):

```text
for N = 1..NBL
  for J = 1..NJPOL
    Top_<side-name>
    Bot_<side-name>
```

For one element and the default two side quantities this produces:

```text
Top_Xtr  Bot_Xtr  Top_Itr  Bot_Itr
```

### Data construction

The same routine writes side data in this order (`iopol.f` 752-756):

```text
for IS = 1..2*NBL
  for J = 1..NJPOL
    CPOLSD(IA, IS, JPOL(J))
```

For one element this produces:

```text
side 1 Xtr, side 1 Itr, side 2 Xtr, side 2 Itr
```

### Consequence

The header and data orders are transposed for side quantities. The base
`CPOL` values are not transposed; the element `CPOLEL` block is written
`N=1..NBL` outer, `K=1..NKPOL` inner, matching its header order.

`POLREAD` builds positions from the header labels and then assigns data by
those positions. It therefore reads the 6.996 side values under the wrong
labels. For the default side selection, the observed assignment is:

```text
Top_Xtr = side 1 Xtr      (correct)
Bot_Xtr = side 1 Itr      (wrong)
Top_Itr = side 2 Xtr      (wrong)
Bot_Itr = side 2 Itr      (correct)
```

A controlled pair with side-1 `Xtr=0.11`, `Itr=11`, side-2
`Xtr=0.21`, `Itr=21` is written as `0.11 11 0.21 21` and read back as
`Top_Xtr=0.11`, `Bot_Xtr=11`, `Top_Itr=0.21`, `Bot_Itr=21`.

The same transposition appears in ordinary 6.996 output, not only in the
synthetic element probe. For example, the external `a07` output has
`0.6877 32.3265 0.6877 208.6735` under the header
`Top_Xtr Bot_Xtr Top_Itr Bot_Itr`; the physical top transition is 0.6877,
not 32.3265.

### Reader-side omissions

`POLREAD` initializes its local `KPOL` array to zero and never parses
element labels. Its element mapping block (`iopol.f` 383-391) therefore
always writes zero `CPOLEL`; the extra element values in a data line are
read into `RINP` but discarded. Element headers are not validated.

The reader also has no exact-name/token parser. In particular, a base name
such as `CDv` can match the element label `CDv_1` by substring, and unknown
columns are not rejected. This is unsafe for the S11 reader and must be
replaced by an exact field/token mapping.

For absent optional columns, 6.99 reads `RINP(IPOL)` with `IPOL=0`, i.e.
the uninitialized `RINP(0)`; 6.996 guards the base loop but still does not
provide a general missing-column contract. Current XFOIL.next deliberately
sets absent columns to zero. This difference must remain explicit in S11.

## Format matrix

The complete measured table is in the external `matrix.tsv`. Summary:

| Variant | Header | Data | Internally consistent? | 6.99 read | 6.996 read | XFOIL.next read |
|---|---|---|---|---|---|---|
| 6.99 base | base + Top/Bot pairs | same order | yes | exact | exact | exact |
| 6.996 base | base + Top/Bot pairs | side-major transpose | no | no error, wrong side values | no error, wrong side values | no error, wrong side values |
| 6.996 one element | base + sides + `Cx_1` | side-major + `Cx_1` | side block no; element block yes | no error, sides wrong, element ignored | no error, sides wrong, element lost | no error, sides wrong, element not modelled |
| 6.996 four elements | base + sides + `Cx_1..CDv_1` | side-major + elements | side block no; element block yes | same loss | same loss | same loss |
| 6.996 multiple elements | N-major side/element header | side-major data; element N-major | no | reads with NBL clamped/lossy | reads with NBL clamped/lossy | parses, no element support |
| append | data only | data only | not checked | rows parse | rows parse | rows parse |
| read to write 6.99 -> 6.996 | same base/side header | side order changes | no | not applicable | not byte-identical | not applicable |
| contradictory header | labels follow labels | positions follow labels | no | no error, values follow labels | no error, values follow labels | no error, values follow labels |
| unknown extra column | extra label ignored | extra value remains | no | no error, extra ignored | no error, extra ignored | no error, extra ignored |
| truncated data row | full header | missing value | no | no error, missing becomes zero/uninitialized | no error, missing becomes zero/uninitialized | no error, missing becomes zero |

The 6.996 writer with `KPOL=0` is already broken for side data. The element
columns are a second, separate path: their own header/data order is
consistent in the synthetic `NBX=5` probe, but the main XFOIL caller uses
`NBX=1`, `NKPOL=0`, and `POLREAD` does not recover them.

## Read, write and append semantics

- `POLREAD` is label-position driven, not a fixed positional grammar.
  Changing the header changes the interpretation of the same data row.
- `POLWRIT` writes base columns first, then side columns, then element
  columns. `LHEAD=.FALSE.` writes data only and is the append path.
- `PLRADD` opens an existing save file for append and calls `POLWRIT` with
  `LHEAD=.FALSE.`; it does not read or validate the existing header or
  column set before appending.
- `PLRSET` compares header values and geometry, but does not validate the
  column schema.
- No canonical writer rejects an incompatible append; changing `JPOL`,
  `KPOL` or `NBL` between appends can silently mix row widths.
- No canonical reader rejects duplicate, contradictory or unknown columns.
  Unknown columns are ignored; truncated rows are not rejected.

## Decision options

### A - Use upstream 6.996 bytes unchanged

Rejected. The 6.996 base writer is not internally consistent: every default
side block is transposed, independently of element columns. The output is
not self-roundtripping and 6.99/current readers produce wrong side values.

### B - Patch the 6.996 writer to the intended order

This would fix new 6.996 output, but would create a new byte target that is
not the existing 6.99/current contract and would not repair already-written
6.996 files. It is only justified if a distinct 6.996 byte target is
explicitly required. It is not needed to preserve current XFOIL.next files.

### C - Preserve current format and add element columns coherently

Selected. The current XFOIL.next writer already matches the 6.99 base and
side bytes for the controlled case (SHA-256
`8f902f8abbe2082b4aee87d95d76c4d5e0eaec7b789dcbed642b5930f840e475` for
the probe file). S11 should keep that base contract and append element
columns in one canonical order, with exact header/data consistency.

Decision:

```text
G-POLAR-SAVE: PRESERVE-CORRECT-EXISTING-FORMAT
```

## S11 reader contract

The later reader must:

- accept current/6.99 base files exactly;
- accept only exact, documented side and element labels;
- map element columns with token boundaries, not substrings;
- not silently treat `CDv_1` as base `CDv`;
- reject duplicate, unknown or contradictory columns unless an explicit
  compatibility mode is selected;
- reject a data row with missing required fields; do not guess;
- reject an append whose column structure differs from the seed file;
- not silently accept the upstream 6.996 side transpose as if it were
  correct.

For 6.996 legacy files, the defect is not reliably distinguishable from a
6.99 file by header alone in all cases. The safe contract is therefore an
explicit `6.996 broken-side-order` migration mode, keyed by a documented
`Version 7.00` plus explicit user opt-in, which applies the known side
transpose once. It must never be the default and must not guess element
presence. Files outside that mode are rejected when the side schema cannot
be validated.

## S11 writer contract

- Preserve the current base column order and widths.
- Write side columns as `JPOL` pairs: `Top_/Bot_` for each selected JP in
  order, with data in the same order.
- Append element columns after the side block:
  for `N=1..NBL` outer and `K=1..NKPOL` inner, write `Cx_n`, `Cy_n`,
  `Cm_n`, `CDv_n` in the selected `KPOL` order; data follows the same
  order.
- Write `KPOL=0`/`NKPOL=0` exactly as the old base format.
- Keep header and data generated from one canonical layout description.
- Append only after validating the existing header, column names, widths
  and element count; otherwise refuse without modifying the file.
- Emit deterministic LF line endings.

## Polar text I/O implementation - X6996-S11

The implementation keeps the G1 contract without changing product commands,
the product version, the banner, tolerances, Fortran sources, old 6.99
fixtures or goldens.

### Code

- `include/xfoil/io/PolarText.h` now exposes `PolarFieldKind`,
  `PolarField`, `PolarLayout`, `PolarReadCompatibility`,
  `makePolarLayout`, `writePolarTextChecked`, `checkPolarAppend`,
  `ParsedPolar::layout/elementSelection/elementData/errorMessage` and
  `PolarWriteRequest::elementSelection/elementData`.
- `src/io/PolarText.cpp` uses one canonical layout for reader, writer and
  append preflight. Element selection is the S10
  `PolarElementSelection`; element rows are the S10 `PolarElementRow`.
- `polarDatasetFromParsed` copies parsed element rows and the session keeps
  the parsed element selection. `makePolarWriteRequest` copies the slot
  element rows and session selection into every writer request.
- `appendPolarSavePoint` now reads and checks the existing file before
  opening it for append. An incompatible file is left byte-for-byte
  unchanged and the caller reports that the point was not written.
- `PolarPoint::side` has room for `2*kMaxPolarElements` side rows and
  `kNpx` is 20 (the S10 element capacity).

### Reader

The reader accepts the existing 6.99/current base and side format exactly.
It tokenizes header labels with exact names: `CDv_1` is an element label,
never base `CDv`. Required base columns `alpha`, `CL`, `CD` and `CM` are
mandatory; duplicates, unknown labels, contradictory side pairs, missing
element columns, extra values and truncated rows are rejected with a
structured line/message. Optional base columns retain their defined
zero/default behavior. Element labels accept only `Cx_n`, `Cy_n`, `Cm_n`
and `CDv_n` with `1 <= n <= NBL` and `NBL <= 5`. Side headers may repeat
`Top_/Bot_` labels for multiple elements; occurrence order maps them to
element 1..NBL.

The explicit compatibility value
`PolarReadCompatibility::xfoil6996BrokenSideOrder` is the only path for the
known 6.996 side transpose. It requires `Version 7.00`, applies exactly one
side transposition, and is never selected by the canonical path or by an
automatic guess. Broken 6.996 files are rejected without it.

### Writer

`makePolarLayout` builds the one canonical layout:

```text
base fields in IPOL order
side fields: JPOL outer, Top/Bot pair, N = 1..NBL
element fields: N outer, selected KPOL order inner
```

The writer emits base bytes unchanged for `NKPOL=0`, canonical
`Top_/Bot_` side pairs with data in the same order, and element columns
`Cx_n`, `Cy_n`, `Cm_n`, `CDv_n` after the side block. Header and data are
generated from the same `PolarLayout`; line endings are LF-only. Invalid
element counts, duplicate/empty selections and element-data/point count
mismatches return a checked error.

### Append

`checkPolarAppend` parses the existing file, builds the requested layout and
compares the full layout, `NBL` and version. The productive append path runs
this check before opening the file. On refusal, no header, row, partial line
or timestamp changes the existing file.

### Tests and fixtures

Nine fixtures are under `tests/data/xfoil-6.996/s11/` with SHA-256 metadata:

- `canonical-base.pol` (6.99 base contract);
- `canonical-elem1.pol`, `canonical-elem4.pol`,
  `canonical-multiel.pol` (element layouts and byte-identical round trip);
- `broken-6996-base-v700.pol` (canonical rejection plus opt-in migration);
- `unknown-column.pol`, `duplicate-column.pol`, `truncated-row.pol`
  (exact-token/row rejection);
- `incompatible-append.pol` (append-schema refusal).

Five focused tests share the `unit.PolarTextS11` prefix. Existing
`unit.IoPolar*` base tests still pass, including the exact 6.99 round trip.
`.gitattributes` marks the fixed-width `.pol` fixtures as intentional
whitespace.

### Linux acceptance

| Check | Result |
|---|---|
| GCC 13 Debug full suite | 2022/2022 |
| GCC 13 Release full suite | 2022/2022 |
| Clang 18 Debug full suite | 2022/2022 |
| Clang 18 Release full suite | 2022/2022 |
| `make test-core` | 455/455 |
| Unit filter | 1721/1721 |
| Integration filter | 246/246 |
| Regression filter | 5/5 |
| Batch filter | 47/47 |
| ASan/UBSan full suite | 2022/2022 |
| Valgrind `unit.PolarTextS11` | 5/5, no definite leaks |
| Reference | 236/236 |
| Reference determinism | 236/236 |
| Harness families | 16/16 |
| Permanent gates | all pass |

Sanitizer acceptance found and fixed one stack-buffer-overflow in the new
element-selection validation (`seenQuantity` was sized without the 1-based
quantity index). The full ASan/UBSan suite is green after the fix.

### Windows acceptance

`X6996-S11-R1`: PASS on 2026-11-11.

The focused Windows run:

```text
./build/debug/xfoil_tests unit.PolarTextS11
```

reported all five tests OK and `0 failure(s) in 5 test(s)`. The S11 commit
follows this documentation update. No full Windows matrix is planned for
this slice.

### Protected state

- `fortran-src/**`, `fortran-bin/**`, `patches/**`: unchanged.
- Existing 6.99 fixtures, goldens, references and tolerances: unchanged.
- No product command, version or banner change.
- No push, tag or release.

## Handoff

`X6996-S11-Polar-Text-Read-Write-Append` is implemented and green on
Linux and Windows. The path-scoped S11 commit follows this documentation
update. The slice does not change `PLXINI`/`PLXADD` dump behavior, product
version or banner, and it does not migrate old goldens automatically. No
full Windows matrix is planned.
