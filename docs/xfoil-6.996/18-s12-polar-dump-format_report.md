# XFOIL 6.996 — S12 polar-dump format report

Status: PASS - `X6996-S12-Polar-Dump-PLXINI-PLXADD-Format` implemented and
green on Linux and in the focused Windows run. The S12 commit follows this
report update.

## Starting state

- HEAD: `0db3c37ce7ac2f28c9284a15c61494aaae60ab7a`
  (`Integrate XFOIL 6.996 polar text metadata`).
- Parent: `b6cf509af9f0d899dd1d60ebb5dd8edb1b6b1e93`.
- Worktree clean, staged 0, ahead 24.
- Tag `v1.0.0` unchanged at `dc7305b057286574cd137cb6f1af170b74650ec5`.
- No push or fetch.

## Oracle status

Patched 6.996 with the accepted P1 overlay:

- exit 0;
- non-empty dump;
- a07 two independent runs byte-identical, 40816 bytes, SHA-256
  `a442b07a0aa05ea3bc583a2d5890d07657027053084dc0c0c23a2b516d3514c5`.

Unpatched 6.996:

- exit 2;
- 0-byte dump;
- `Sequential READ or WRITE not allowed after EOF marker`.

The P1 `REWIND(LU)` changes only the file state before the header write. It
does not reorder records or fields. S12 adds no second Fortran patch.

Oracle hashes:

```text
patched binary       031d667f3990c326b5b90d448fbdd10327b127a4e56d249fd6fcec71b55a69ce
unpatched binary     000b0468d9a7f4fce42d6fac24bd14147d7d326c8a9035909c8e9f98d180f50e
P1 patch             be992f7c48cee137293e15b8fe2dc169b3e5838b5f46c95475b479048cca0572
6.996 xpol.f         048c491773bd896d7325fa4e00928f1c18551283fae6ed9f45b42f979c5e3f8f
patched xpol.f       f29b6b04b8a18052c8a2996fae12174931243145a3e9677ff4c22fefc84b528d
```

## Record structure

The controlled format is little-endian Fortran sequential unformatted:

- 4-byte record markers;
- 8-byte real and 4-byte integer fields;
- five header records from `PLXINI`;
- four point records per `PLXADD` group;
- no text normalization or extra padding.

The complete field table and per-record byte formulas are in the S12 plan
document. The a07 target has 21 records and point-group payload sizes
`(56,16,4752,4704)`, `(56,16,5040,4416)`, `(56,16,5040,4416)`,
`(56,16,5136,4320)`. The inviscid group is `(56,16,96,96)` with
`NSIDE=(2,2)`, `IBLTE=(0,0)`.

No S08 field is added to the 6.996 dump. The record set and order are the
same as 6.99; the 6.996 header version value is different.

## 6.99 and 6.996 compatibility

- Existing 6.99 fixtures decode and re-encode byte-identically.
- The S09 Clark Y dump fixture differs from the patched-6.996 a07 target
  only in the 8-byte header `VERSION` field (6.99 versus 6.996).
- The S09 fixture was not rewritten or reinterpreted; it stays the 6.99
  compatibility fixture.
- New productive dump headers use the S12 target version 6.996.
- Existing inviscid and CSEQ 6.99 references remain unchanged. Their
  regression expectations receive only the explicit header-version
  migration; every other byte remains exact.
- No numeric tolerance was introduced.

## Implementation

Changed productive paths:

- `include/xfoil/io/DumpCodec.h`;
- `src/io/DumpCodec.cpp`;
- `src/app/cli/CoreOperHandlers.h`, `CoreOperHandlers.cpp`;
- `src/app/cli/CommandInterpreter.cpp`.

Added:

- `kDumpHeaderVersion = 6.996`;
- checked header/point encoders with size, count and marker validation;
- exact-length decode checks and `NSIDE`/`IBLTE` consistency checks;
- `checkDumpAppend` and `appendDumpPointFile` preflight before mutation;
- shared `makeDumpHeader` with the S12 version.

Append rejection leaves the existing file byte-identical. Decode publishes
no partial file model. Binary reads/writes never apply text line-ending
normalization.

## Fixtures

New under `tests/data/xfoil-6.996/s12/`:

| File | Size | SHA-256 |
|---|---:|---|
| `polar-visc-clark-y.dump` | 40816 | `a442b07a0aa05ea3bc583a2d5890d07657027053084dc0c0c23a2b516d3514c5` |
| `polar-visc-clark-y-truncated.dump` | 40811 | `b376600a0db364502edc355f7bec100187b4eae43f04725228cff020b777fde8` |
| `metadata.txt` | - | `4a38e59b5a7c322b4a47efcb12eb858aec1991ce0097b6bf186b88b9bafb1019` |

The a07 fixture was reproduced twice byte-identically from independent
patched-oracle runs. The truncated fixture is derived by removing five
bytes and is only a negative decode case.

## Tests

New focused tests under `unit.PolarDumpS12`:

- exact a07 decode/re-encode, record count and sizes, selected fields;
- unchanged 6.99 fixture round trip;
- empty, truncated, bad-marker, bad-count, bad-IBLTE and oversized-marker
  rejection;
- append preflight compatibility and byte-identical refusal;
- checked header encoding and 6.996 version.

Updated regression ownership:

- `regression.PaccDumpByteParityMatchesXfoil6996S12Target` compares the
  productive a07 dump exactly with the S12 fixture;
- inviscid and CSEQ regressions keep their 6.99 references with the explicit
  version-only expected migration;
- a06 and a08 remain green.

## Linux acceptance

| Check | Result |
|---|---|
| GCC 13 Debug full suite | 2027/2027 |
| GCC 13 Release full suite | 2027/2027 |
| Clang 18 Debug full suite | 2027/2027 |
| Clang 18 Release full suite | 2027/2027 |
| core / unit / integration / regression / batch | 460 / 1726 / 246 / 5 / 47 |
| ASan/UBSan full suite | 2027/2027 |
| Valgrind `unit.PolarDumpS12` | 5/5, no definite leaks |
| Reference / determinism / harness families | 236 / 236 / 16 |
| Permanent gates | all pass |

The first GCC release build exposed a `-Wnull-dereference` warning in the
new file-read helper. The helper was changed to the project's established
`rdbuf`/`ostringstream` pattern; all four compiler configurations are green
after the fix.

## External inviscid measurement

The patched 6.996 a06 old-default run was made twice (3608 bytes,
`6b77488d4bbd286a4c0001af168548279a356cd6d6a6a5254792fac0f42f0a4e`).
It differs from the old 6.99 a06 reference in out-of-scope geometry/numeric
bytes, not only in the dump header. No S12 inviscid target fixture is
therefore claimed. This is recorded as a slice-boundary finding, not hidden
by a tolerance; a06 remains a 6.99 regression and is green.

## Windows acceptance

`X6996-S12-R1`: PASS on 2026-11-11.

The focused Windows run:

```text
./build/debug/xfoil_tests unit.PolarDumpS12
```

reported all five tests OK and `0 failure(s) in 5 test(s)`. The S12 commit
follows this documentation update. No full Windows matrix is planned.

## Protected state and Git

- `fortran-src/**`, `fortran-bin/**`, `patches/**`: unchanged.
- Existing 6.99 dump fixtures, goldens, references and S09 fixture:
  unchanged.
- Polar text I/O from S11: unchanged.
- Product version, banner and tolerances: unchanged.
- No push, fetch, tag or release.
- No S12 commit exists before this report update; the path-scoped S12
  commit follows.
