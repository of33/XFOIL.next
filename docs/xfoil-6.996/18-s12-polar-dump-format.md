# S12 - Polar dump format and patched 6.996 oracle

Status: PASS - `X6996-S12-Polar-Dump-PLXINI-PLXADD-Format` implemented and
green on Linux and in the focused Windows run.

Decision: adopt the patched-6.996 binary dump format, retain the accepted
P1 overlay as the only Fortran patch, and keep existing 6.99 dump fixtures
unchanged and readable.

## Scope

S12 owns `PLXINI`, `PLXADD` and the binary polar-dump record contract:

- header and point-record structure;
- field order, field count and record sizes;
- integer, real and record-marker representation;
- point, wake and boundary-layer data;
- new-file initialization, append and multi-point behaviour;
- 6.99 read compatibility where the format is unchanged;
- exact patched-6.996 target bytes;
- deterministic output and controlled rejection of malformed files.

Non-goals: polar text/SAVE columns from S11, `POLREAD`/`POLWRIT`, `BLDUMP3`,
further Fortran patches, CFT/BL numerics, CLI command registration,
defaults, product version/banner, global tolerances and plotting.

## Sources and P1 overlay

Primary sources:

- `fortran-src/xfoil-6.99/xpol.f`;
- `fortran-src/xfoil-6.996/xpol.f`;
- `patches/xfoil-6.996/0001-rewind-polar-dump-unit-before-header.patch`.

The P1 patch is build-overlay only. It inserts `REWIND(LU)` immediately
before the new-file header write in `PLXINI`; it does not reorder any record
or field.

P1 status measured with the frozen sources:

| Binary | Exit | Dump | Result |
|---|---:|---:|---|
| unpatched 6.996 | 2 | 0 bytes | `Sequential READ or WRITE not allowed after EOF marker` |
| patched 6.996 | 0 | 40816 bytes | non-empty, deterministic a07 target |

Two independent patched-oracle runs produced the same SHA-256. S12 adds no
second Fortran patch.

## Record contract

The files are Fortran sequential unformatted records on the controlled
oracle platform:

- 4-byte little-endian record markers, one before and one after each payload;
- 8-byte IEEE-754 reals (`-fdefault-real-8`);
- 4-byte little-endian integers;
- no text normalization, CRLF conversion, padding or trailing marker change.

`PLXINI` writes header records 1-5. `PLXADD` writes one four-record group
6-9 per accepted polar point.

| Record | Field | Type | Dimension | 6.99 | 6.996 | Change |
|---|---|---|---|---|---|---|
| 1 | `NAME` | character | 48 | yes | yes | none |
| 1 | `CODE` | character | 8 | yes | yes | none |
| 1 | `VERSION` | real | 1 | 6.99 | 6.996 | dump code version |
| 2 | Mach, Re/1e6, Ncrit top, Ncrit bottom | real | 4 | yes | yes | none |
| 3 | `MATYP`, `RETYP` | integer | 2 | yes | yes | none |
| 4 | `IIX`, `ILEX`, `ITEX`, `IIBX` | integer | 4 | yes | yes | none |
| 5 | geometry `X,Y` | real | `2*IIBX` | yes | yes | none |
| 6 | `ALFA/DTOR, CL, CDTOT, 0.0, CM, XT1, XT2` | real | 7 | yes | yes | none |
| 7 | `NSIDE1, NSIDE2, IBLTE1, IBLTE2` | integer | 4 | yes | yes | none |
| 8 | side 1 `XX,CP,THET,DSTR,CF,CTAU` | real | `6*NSIDE1` | yes | yes | none |
| 9 | side 2 `XX,CP,THET,DSTR,CF,CTAU` | real | `6*NSIDE2` | yes | yes | none |

Record payload sizes in bytes, excluding the two 4-byte markers:

```text
record 1: 64
record 2: 32
record 3:  8
record 4: 16
record 5: 16*IIBX
record 6: 56
record 7: 16
record 8: 48*NSIDE1
record 9: 48*NSIDE2
```

For the a07 target: five header records, then four point groups. Point-group
payload sizes are `(56,16,4752,4704)`, `(56,16,5040,4416)`,
`(56,16,5040,4416)` and `(56,16,5136,4320)`. The first point has
`NSIDE=(99,98)`, `IBLTE=(77,76)`; the last has `NSIDE=(107,90)`,
`IBLTE=(85,68)`.

The inviscid point group is `(56,16,96,96)` with `NSIDE=(2,2)` and
`IBLTE=(0,0)`. 6.996 adds no S08 field to this dump: the record set and field
order are unchanged from 6.99 apart from the header version value. The
6.996 `PLXADD` open mode (`ACCESS='APPEND'`) and the 6.99 `BOTTOMX` path
address the same end position for the accepted cases.

## Format decision

| Finding | Decision |
|---|---|
| P1 only changes file state before header write | accept P1, no further patch |
| Header and record layout unchanged 6.99 -> 6.996 | preserve exact layout |
| Header `VERSION` field differs | target 6.996 for new dumps |
| S09 fixture differs from a07 only in `VERSION` | keep S09 unchanged, add S12 fixture |
| 6.996 inviscid oracle differs from old reference in out-of-scope numeric/geometry bytes | no S12 inviscid target fixture; retain a06 6.99 regression with an explicit version-only expected migration |
| Append without schema check | add deterministic preflight before file mutation |

Existing 6.99 dump fixtures remain unchanged. New files use
`kDumpHeaderVersion = 6.996`; decoding preserves the version stored in the
file, so old 6.99 fixtures still round-trip byte exactly. Append accepts the
existing file's code/version fields and validates the PLXINI-relevant header
values and geometry size instead.

## C++ mapping

| Fortran contract | C++ routine/type | Caller | Test | S12 change |
|---|---|---|---|---|
| header records | `io::DumpHeader`, `encodeDumpHeaderChecked` | `CommandInterpreter::initPolarDumpFile` | dump fixtures | checked encode, 6.996 version |
| point group | `io::DumpPoint`, `encodeDumpPointChecked` | `appendPolarDumpPoint` | PACC tests | checked encode |
| decode | `io::decodeDump` | init and tests | `IoDump*`, regressions | exact sizes, bounds, counts |
| append | `io::checkDumpAppend`, `io::appendDumpPointFile` | `appendPolarDumpPoint` | `unit.PolarDumpS12AppendPreflight` | preflight before open |
| header construction | `makeDumpHeader` | init/append | integration/regression | one shared layout |

No second dump implementation was introduced.

## Validation and append

- All record payload sizes are exact; extra and missing values are rejected.
- Record markers must match front/back; truncated and oversized records are
  rejected.
- `NSIDE` must be at least 2; `IBLTE` must be non-negative and not exceed
  the corresponding `NSIDE`.
- Geometry length must equal `IIBX`; side vectors must equal `NSIDE`.
- Checked encoders reject inconsistent counts, integer overflow and
  record-marker overflow before bytes are written.
- Decode builds a local file model and publishes it only on success.
- Append reads and validates the existing file before opening it for write.
  An incompatible or malformed file remains byte-identical.
- Binary mode is used throughout; LF/CRLF normalization never touches dump
  bytes.

## Fixtures

New fixtures under `tests/data/xfoil-6.996/s12/`:

| File | Size | SHA-256 | Purpose |
|---|---:|---|---|
| `polar-visc-clark-y.dump` | 40816 | `a442b07a0aa05ea3bc583a2d5890d07657027053084dc0c0c23a2b516d3514c5` | patched-6.996 a07 target |
| `polar-visc-clark-y-truncated.dump` | 40811 | `b376600a0db364502edc355f7bec100187b4eae43f04725228cff020b777fde8` | truncated-record negative |
| `metadata.txt` | - | `4a38e59b5a7c322b4a47efcb12eb858aec1991ce0097b6bf186b88b9bafb1019` | provenance, oracle hashes, repeatability |

The existing S09 fixture `tests/data/xfoil-6.996/s09/polar-visc-clark-y.dump`
is unchanged and remains a 6.99 compatibility fixture. It is not the S12
target; the only difference is the 8-byte header version field.

## Tests

Five focused tests share `unit.PolarDumpS12`:

- target round trip, record count/sizes and selected field values;
- unchanged 6.99 fixture round trip;
- malformed/truncated/bad-marker/bad-count rejection;
- append preflight and byte-identical refusal;
- checked header encoding and 6.996 version.

Regression ownership:

- `regression.PaccDumpByteParityMatchesXfoil6996S12Target` compares the
  productive a07 dump byte-exactly with the S12 fixture;
- `regression.PaccInviscidDumpByteParityMatchesFortranReference` keeps the
  a06 6.99 reference and applies only the explicit header-version migration;
- `regression.CseqHeadDumpParityMatchesFortranReference` keeps its 6.99
  reference and applies the same header-version migration to `s2.dump`.

No numeric tolerance was added or changed.

## Linux acceptance

| Check | Result |
|---|---|
| GCC 13 Debug full suite | 2027/2027 |
| GCC 13 Release full suite | 2027/2027 |
| Clang 18 Debug full suite | 2027/2027 |
| Clang 18 Release full suite | 2027/2027 |
| `make test-core` | 460/460 |
| Unit filter | 1726/1726 |
| Integration filter | 246/246 |
| Regression filter | 5/5 |
| Batch filter | 47/47 |
| ASan/UBSan full suite | 2027/2027 |
| Valgrind `unit.PolarDumpS12` | 5/5, no definite leaks |
| Reference | 236/236 |
| Reference determinism | 236/236 |
| Harness families | 16/16 |
| Permanent gates | all pass |

## Windows acceptance

`X6996-S12-R1`: PASS on 2026-11-11.

The focused Windows run:

```text
./build/debug/xfoil_tests unit.PolarDumpS12
```

reported all five tests OK and `0 failure(s) in 5 test(s)`. The S12 commit
follows this documentation update. No full Windows matrix is planned for
this slice.

## Handoff

Linux and Windows acceptance are complete. The path-scoped S12 commit
follows this documentation update. The slice does not change
`POLREAD`/`POLWRIT`, `BLDUMP3`, product version or banner, and it adds no
Fortran patch.
