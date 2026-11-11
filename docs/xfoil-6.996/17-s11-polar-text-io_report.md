# XFOIL 6.996 - S11 polar text I/O and G-POLAR-SAVE report

Status: PASS - `X6996-S11-G1` analysis complete;
`X6996-S11-Polar-Text-Read-Write-Append` implemented and green on Linux and
in the focused Windows run.

Decision: `G-POLAR-SAVE: PRESERVE-CORRECT-EXISTING-FORMAT`.

## Starting state

- HEAD: `b6cf509af9f0d899dd1d60ebb5dd8edb1b6b1e93`
  (`Integrate XFOIL 6.996 polar element metadata`).
- Parent: `b83a1031d50bd61124b08669ea1765583b67220d`.
- Worktree clean before G1; staged 0; ahead 23; tag `v1.0.0`.
- No fetch, push, tag or release.

This slice changed only these two S11 documents. No C++, test, fixture,
golden, reference, Fortran, patch, CLI, version or banner file was changed.

## Sources checked

- `fortran-src/xfoil-6.99/iopol.f`, `xpol.f`, `PINDEX.INC`, `PPLOT.INC`,
  `XFOIL.INC`;
- `fortran-src/xfoil-6.996/iopol.f`, `xpol.f`, `PINDEX.INC`, `PPLOT.INC`,
  `XFOIL.INC`;
- `patches/xfoil-6.996/0001-rewind-polar-dump-unit-before-header.patch`;
- current `src/io/PolarText.cpp`, `include/xfoil/io/PolarText.h` and
  `src/io/TextFormat.cpp`.

The accepted dump patch adds only `REWIND(LU)` before the unformatted dump
header. It does not modify `POLWRIT`, `POLREAD`, `PLRSET`, `PLRINI` or
`PLRADD`. Therefore 6.996 and 6.996-with-dump-patch have the same **text**
format contract.

## External measurements

Controlled probe:
`external audit area: xfoil-6.996/s11-g1/format-probe/`.
Matrix: `external audit area: xfoil-6.996/s11-g1/matrix.tsv`.

The probe uses canonical 6.99/6.996 `POLWRIT` and `POLREAD` and controlled
values:

```text
side 1: Xtr=0.11, Itr=11
side 2: Xtr=0.21, Itr=21
element 1: Cx=1.1, Cy=2.1, Cm=3.1, CDv=4.1
element 2: Cx=2.1, Cy=4.1, Cm=6.1, CDv=8.1
```

Selected byte results:

| File | Bytes | SHA-256 |
|---|---:|---|
| 6.99 base | 628 | `8f902f8abbe2082b4aee87d95d76c4d5e0eaec7b789dcbed642b5930f840e475` |
| current C++ base | 628 | same as 6.99 base |
| 6.996 base | 638 | `bfc07a6424acb0dcfe40622254da918bac6bf2b1764222446ad62b3b61bfcfb7` |
| 6.996 one element | 678 | `2a4a516477369eda72ec85eb06a52e7e51f558575c8fb3ff7f84d2949c35b440` |
| 6.996 four elements | 790 | `db5db6fe2f9577047dc842f19fe1e4f8555d58d210376307f15a7d85cd5adf83` |
| 6.996 two elements | 1181 | `3c7262a9548e931dfe2fc9718b472c097d8dc6e16ef10e59be2bc8dc89568074` |
| 6.996 compatible append | 638 | same bytes as 6.996 base |
| 6.996 incompatible append | 649 | `4f7e63ab2d2a870da6270109fa4378d45b301ead6d6c1aebd58c63249b2e22dc` |
| 6.996 roundtrip from 6.99 | 681 | `d2e6c55ef9e48d1316251f14dd50003ca3d30d11d2d32f1d9468a7901bf8e855` |
| 6.996 self-roundtrip | 681 | `a291d7c09848d3f62198427371ea70cac233ab5374f7f6c5bb346b0acdd5638d` |

The current C++ base writer is byte-identical to the controlled 6.99 base
file. This is the basis for preserving the current format.

## Exact 6.996 failure

The controlled 6.996 base row is:

```text
0.000 0.1000 0.01000 0.00100 -0.1000 0.1100 11.0000 0.2100 21.0000
```

under:

```text
alpha CL CD CDp CM Top_Xtr Bot_Xtr Top_Itr Bot_Itr
```

The values are in physical order:

```text
side1 Xtr=0.11, side1 Itr=11, side2 Xtr=0.21, side2 Itr=21
```

The header expects:

```text
Top_Xtr, Bot_Xtr, Top_Itr, Bot_Itr
```

so the reader assigns:

```text
Top_Xtr=0.11    correct
Bot_Xtr=11      wrong (this is side1 Itr)
Top_Itr=0.21    wrong (this is side2 Xtr)
Bot_Itr=21      correct
```

Root cause is localized in `fortran-src/xfoil-6.996/iopol.f`:

- header loop `iopol.f` 631-662: `J` outer, `N` inner, producing
  `Top/Bot` pairs per side quantity;
- data write `iopol.f` 752-756: `IS` outer, `J` inner, producing
  side-major data;
- the base `CPOL` block is unchanged and remains correct;
- the element block is `N` outer / `K` inner in both header and data.

This is not only an element-column issue. It affects the ordinary default
side pair `Xtr/Itr` even when `NKPOL` is zero.

The 6.996 reader reads by header-label positions. It therefore reproduces
the wrong side assignment when reading its own output, with no error.
A 6.99 reader reading a 6.996 file sees the same wrong assignment.

## Element-column findings

- `POLREAD` initializes local `KPOL` to zero and never parses
  `CPOLNNAME`/element labels. It reads extra element fields into `RINP` but
  writes zero `CPOLEL`; the element data are discarded.
- The synthetic `POLWRIT` `NBX=5` path writes element headers/data
  consistently in `N`-major order, but the main XFOIL calls pass
  `NBX=1`/`NBL=1` and `NKPOL=0`. The main XFOIL path therefore does not
  produce element columns.
- `PPLOT.INC` declares `NBX=5`, but the main solver's `XFOIL.INC` has
  `CPOLEL(NAX,1,KPTOT,NPX)`. The two contexts are not interchangeable.
- The current C++ reader has the same lack of element support and uses
  substring label matching. In the controlled element probe it added a
  base `CDv` match from `CDv_1`; element data were not represented
  correctly. This is unsafe for S11 and is classified as a reader change,
  not a G1 implementation.
- 6.99 cannot write element columns. It accepts extra values without an
  error but ignores them.

## Reader behavior

| Input | 6.99 reader | 6.996 reader (`NBX=1`) | current next reader |
|---|---|---|---|
| 6.99 base | exact | exact | exact |
| 6.996 base | no error, side values transposed | no error, side values transposed | no error, side values transposed |
| 6.996 one/four elements | side transpose; elements lost | side transpose; `CPOLEL=0` | side transpose; elements not modelled |
| 6.996 two elements | `NBL` clamped to 1; lossy | `NBL` clamped to 1; lossy | parses `NBL=2` but no element model |
| unknown extra column | no error, ignored | no error, ignored | no error, ignored |
| truncated row | no error, missing value zero/uninitialized | no error, missing value zero/uninitialized | no error, missing value zero |
| contradictory labels | no error, follows labels | no error, follows labels | no error, follows labels |

`POLREAD` does not validate the full column count, duplicates or a matching
header/data sequence. Missing optional columns are not a safe contract in
6.99; `RINP(0)` is uninitialized. Current C++ already defines absent
columns as zero, which must remain an explicit S11 decision.

## Write and append behavior

- `POLWRIT` emits base columns, side columns and element columns. It does
  not compare a header to a requested layout; the caller supplies `IPOL`,
  `JPOL`, `KPOL`, `NBL`.
- `PLRADD` appends data-only rows through `POLWRIT(LHEAD=.FALSE.)`.
- There is no append-schema check. The controlled incompatible append
  (`base header + point 1`, then `base+sides+Cx` data for point 2) is
  accepted and produces a 649-byte mixed-schema file with no error.
- Compatible append bytes equal a single two-point write for the controlled
  case.
- 6.996 read-to-write of a 6.99 file changes the side order and is not
  byte-identical; 6.996 self-roundtrip is also not byte-identical.

## Decision

`A` is rejected: upstream 6.996 bytes have a confirmed side-column
transposition and are not self-consistent.

`B` is not selected: patching a new 6.996 byte target would not repair
already-written 6.996 files and would create an unnecessary second base
contract. It remains a possible diagnostic target only if a future
requirement explicitly demands 6.996-native bytes.

`C` is selected: preserve the current 6.99/current base and side contract,
and add element columns in one canonical, explicit order. This keeps
existing files readable and extends the format without adopting the known
upstream defect.

## S11 implementation handoff

Reader:

- accept 6.99/current base files exactly;
- exact token matching for base, side and element labels;
- reject duplicate/unknown/contradictory columns unless an explicit
  compatibility mode is selected;
- reject missing required values and incompatible append schemas;
- never silently treat `CDv_1` as base `CDv`;
- provide an explicit, documented 6.996 side-order migration mode keyed by
  `Version 7.00` and user opt-in; never auto-guess.

Writer:

- retain the current base format;
- write `Top_/Bot_` pairs in `JPOL` order with matching data;
- append element columns after sides in `N`-outer / `K`-inner order;
- write `Cx_n`, `Cy_n`, `Cm_n`, `CDv_n` exactly;
- write no element block for `NKPOL=0`;
- validate append header/column structure before writing;
- deterministic LF output.

The later `X6996-S11-Polar-Text-Read-Write-Append` still requires its own
implementation and focused read/write/append tests. This G1 slice did not
run Linux or Windows tests because it changed documentation only.

## S11 implementation result

`X6996-S11-Polar-Text-Read-Write-Append` is implemented. The G1 decision
is unchanged: the existing 6.99/current base and side bytes are preserved,
and element columns are added in the canonical order selected in G1.

Implemented files:

- `include/xfoil/io/PolarText.h`;
- `src/io/PolarText.cpp`;
- `src/app/cli/CoreOperHandlers.h`, `CoreOperHandlers.cpp`;
- `src/app/cli/CommandInterpreter.cpp`;
- `tests/core/IoTests.cpp`;
- `tests/data/xfoil-6.996/s11/` and its `metadata.txt`;
- `.gitattributes` (fixed-width polar fixtures intentionally retain
  trailing spaces).

Implementation summary:

- one `PolarLayout` describes header and data for reader, writer and append
  preflight;
- exact header tokens and strict row widths; `CDv_1` cannot alias base
  `CDv`;
- canonical side pairs and `N`-outer / `K`-inner element columns;
- element selection/element rows are carried through parsed dataset,
  session writer requests and append;
- append preflights layout, `NBL` and version before opening the file;
- the known 6.996 transpose is available only through the explicit
  `xfoil6996BrokenSideOrder` mode keyed by `Version 7.00`;
- no CLI command, product version, banner, tolerance, Fortran source, old
  fixture or golden changed.

Fixtures and tests:

- `canonical-base.pol`, `canonical-elem1.pol`,
  `canonical-elem4.pol`, `canonical-multiel.pol`;
- `broken-6996-base-v700.pol`;
- `unknown-column.pol`, `duplicate-column.pol`, `truncated-row.pol`;
- `incompatible-append.pol`;
- five focused tests under `unit.PolarTextS11`, plus the existing
  `unit.IoPolar*` base round-trip coverage.

Linux result:

| Check | Result |
|---|---|
| GCC 13 Debug full suite | 2022/2022 |
| GCC 13 Release full suite | 2022/2022 |
| Clang 18 Debug full suite | 2022/2022 |
| Clang 18 Release full suite | 2022/2022 |
| core / unit / integration / regression / batch | 455 / 1721 / 246 / 5 / 47 |
| ASan/UBSan full suite | 2022/2022 |
| Valgrind `unit.PolarTextS11` | 5/5, no definite leaks |
| Reference / determinism / harness families | 236 / 236 / 16 |
| Permanent gates | all pass |

Sanitizer acceptance found one out-of-bounds read in the new
element-selection duplicate guard. The guard array was corrected to include
the 1-based quantity index; the complete sanitizer suite is green after the
fix.

Windows acceptance: `X6996-S11-R1` PASS on 2026-11-11. The focused run
`./build/debug/xfoil_tests unit.PolarTextS11` reported all five tests OK
and `0 failure(s) in 5 test(s)`. The S11 commit follows this documentation
update.

## Protected state and gates

- `fortran-src/**`, `fortran-bin/**`, `patches/**`: unchanged.
- `src/**`, `include/**`, `tests/**`, goldens, references, tolerances,
  product version and banner: G1 analysis changed none of these. The S11
  implementation changes only the files listed above plus the new S11
  fixtures.
- G1 created no new polar fixture; S11 adds the nine fixtures documented
  in the implementation section.
- No push, fetch, tag or release.
- `git diff --check`: PASS.
- `check-fortran-sha`: PASS.
- `check-license-headers`: PASS.
- `check-release-inventory`: PASS.
- `check-public-docs`: PASS.
- No S11 commit exists before this documentation update; the path-scoped
  S11 commit follows.
