# XFOIL.next 1.0.1 maintenance report

Companion to [maintenance-1.0.1.md](maintenance-1.0.1.md).

## Status

```text
XFOILNEXT-1.0.1-MAINTENANCE: PASS
XFOILNEXT-1.0.1-SOURCE-CORRECTION: PASS
XFOILNEXT-1.0.1-R1: PASS
```

The product remains `XFOIL.next 1.0.1`. The source-correction commit on
`release/1.0.x` restores the two archived XFOIL 6.99 sources and updates the
affected provenance documentation. The previously published tag `v1.0.1`
is moved to the correction commit only after this acceptance and only with
the owner's explicit authorization.

## Basis

- Branch: `release/1.0.x`
- Base commit: `58bca7d17f847e1aef6b1c8d3d4e167f40f69945`
  (`v1.0.0^{commit}`)
- Previous 1.0.1 maintenance commit: `43c60a74e1f842449dd89fbf68a27654f68ccaaf`
- Previous public `v1.0.1` target:
  `43c60a74e1f842449dd89fbf68a27654f68ccaaf`
- Product version: `1.0.1`
- `main`: unchanged at `1fbd0d5700db5dc67ecba03c2bfd9ae9c40b49e0`
- `v1.0.0`: unchanged at `58bca7d17f847e1aef6b1c8d3d4e167f40f69945`

## Source correction

The provenance audit found local modifications in the archived Fortran copy.
They are now removed exactly:

| File | Historical local SHA-256 | Restored original SHA-256 |
|---|---|---|
| `fortran-src/xoper.f` | `303191f22c055480561c2a037447ae62c859c7fa70fb21b1c8a1216ff34bba22` | `23f0b6354d6c544b2539ee0ad01b9975838da265f59493b026c28326b69f6524` |
| `fortran-src/xgdes.f` | `4d32ef2f75d2069e1708f32ba789940517c512369de82eaa242cf994792dcf16` | `513d71d2eea3b69b5fdb3544ff506bdda68b2b51c2edbf3703f8ac268c4bd622` |

The removed differences were exactly:

- `xoper.f`: local `OF Special` block with label 333 and `write(*,333)`;
- `xgdes.f`: `CALL READR(1,XRF(1),ERROR)` instead of
  `CALL READR(2,XRF,ERROR)`.

No other archived Fortran source changed. `fortran-src/SHA256SUMS` was
updated only for these two entries and verifies with
`make check-fortran-sha`.

## Historical oracle binary

`fortran-bin/xfoil` remains the historical 1.0.0 reference binary:

```text
f5779d6954d0b8fdc152e0e0633f18732e00b7b69228d561d629bd524bba598c  xfoil
```

It was built from the earlier local source state, still emits the historical
local OFS line, and is deliberately not rebuilt from the restored upstream
sources in this correction. `fortran-bin/SHA256SUMS` is unchanged. Its
archived reference outputs remain historical compatibility records and were
not migrated.

## Product result

The C++ OPER and QDES/SPECAL paths no longer print the non-upstream `OFS:`
line. The `specifiedPointOutput` event and the SPECAL snapshot remain
available for chronology and structured results. The product banner and batch
metadata report `1.0.1`.

No product C++, test source, golden, canonical reference, Fortran binary or
binary SHA manifest changed in the source-correction commit.

## Provenance documentation

The current provenance statements were corrected:

- `NOTICE`
- `fortran-bin/README.txt`
- `docs/fortran-reference.md`
- `docs/numerical-validation.md`
- `README.md`
- this maintenance pair

The current archived sources are now described as byte-identical upstream
originals. The historical oracle is described separately, including its
earlier source state and retained OFS behavior.

## Linux validation

```text
make check-fortran-sha                 OK
make check-license-headers             OK
make check-dependencies                OK
make check-release-inventory           OK
make check-public-docs                 OK
make test                              1970/1970
make release                           OK
build/release/xfoil_tests              1970/1970
make test-reference                    236/236
make test-reference-determinism        236/236
debug --version                        XFOIL.next 1.0.1
release --version                      XFOIL.next 1.0.1
```

The suite composition remains core 414, unit 1669, integration 246,
regression 5, batch 47.

## Windows validation

No new Windows process was started. The correction changes only archived
Fortran sources, the source SHA manifest and provenance/maintenance
documentation. The previously accepted focused Windows 1.0.1 run remains
valid:

```text
MINGW64_NT-10.0-26100 DESKTOP-00M0NIP
g++.exe (Rev5, Built by MSYS2 project) 16.1.0
GNU Make 4.4.1
version: XFOIL.next 1.0.1
integration.OfsRemoval101: OK
0 failure(s) in 1 test(s)
XFOILNEXT-1.0.1-R1: PASS
```

## Tag correction and release state

The owner authorized moving the annotated tag `v1.0.1` to the source-correction
commit after this acceptance. The correction commit is pushed normally to
`release/1.0.x`; only `refs/tags/v1.0.1` is force-updated. No branch force-push
is authorized.

No GitHub Release is created in this step. The 1.0.1 GitHub Release remains a
separate manual action after the corrected branch and tag are confirmed.
