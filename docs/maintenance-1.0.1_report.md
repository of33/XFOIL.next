# XFOIL.next 1.0.1 maintenance report

Companion to [maintenance-1.0.1.md](maintenance-1.0.1.md).

## Status

Linux and the single focused Windows UCRT64 process are green.

```text
XFOILNEXT-1.0.1-MAINTENANCE: PASS
XFOILNEXT-1.0.1-R1: PASS
```

The local maintenance commit and annotated tag are created from this state;
nothing is pushed.

## Basis

- Branch: `release/1.0.x`
- Base commit: `58bca7d17f847e1aef6b1c8d3d4e167f40f69945`
  (`v1.0.0^{commit}`)
- Product version: `1.0.1`
- No file under `fortran-src/`, `fortran-bin/`, `patches/` or
  `tests/fortran-reference/` changed.

## Upstream evidence

| Item | Value |
|---|---|
| `../Xfoil-6.99/src/xoper.f` SHA-256 | `23f0b6354d6c544b2539ee0ad01b9975838da265f59493b026c28326b69f6524` |
| `../Xfoil-6.99/src/xgdes.f` SHA-256 | `513d71d2eea3b69b5fdb3544ff506bdda68b2b51c2edbf3703f8ac268c4bd622` |
| Local OFS addition | `fortran-src/xoper.f:2816`, `333 format(...)` and `write(*,333)` |
| Local `xgdes.f` difference | `READR(2,XRF,ERROR)` to `READR(1,XRF(1),ERROR)`; not OFS and not ported |

The external tree was read only. No Git operation was run there.

## Product result

The OPER and QDES/SPECAL paths no longer print the non-upstream `OFS:` line.
The `specifiedPointOutput` event and the SPECAL snapshot remain available for
chronology and structured results. The product banner and batch metadata now
report `1.0.1`.

## Changed test and golden areas

- New focused checks: `unit.OfsRemoval101` and
  `integration.OfsRemoval101`.
- Interactive OPER tests use structured `RunSummary` point records or the
  canonical remaining output lines instead of parsing the removed OFS line.
- CLI goldens: 18 banner updates; `oper-inviscid-alpha`, `oper-sequence` and
  `oper-viscous-alpha` also lose their OFS lines.
- Batch goldens: 4 transcript banners and OFS lines; 4 results metadata lines.
- `Wake2Alfa6ConvergenceParityTests` and `CseqHeadDumpParityTests` remove the
  known OFS block from their reference copy at test time with counted
  replacements (2 and 6). The reference files are byte-unchanged.
- No Windows golden or tolerance change.

## Linux validation

```text
make check-fortran-sha                 OK
make check-license-headers             OK
make check-dependencies                OK
make check-release-inventory           OK
make check-public-docs                 OK
make test                              1970/1970
make test CXX=clang++ BUILDROOT=build/clang
                                       1970/1970
release full suite                     1970/1970
make test-sanitize                     1970/1970
make test-reference                    236/236
make test-reference-determinism        236/236
make test-valgrind                     representative paths OK
```

The suite composition is core 414, unit 1669, integration 246, regression 5,
batch 47.

## Windows validation

One focused MSYS2 UCRT64 process, no full Windows matrix:

```text
MINGW64_NT-10.0-26100 DESKTOP-00M0NIP
g++.exe (Rev5, Built by MSYS2 project) 16.1.0
GNU Make 4.4.1
version: XFOIL.next 1.0.1
integration.OfsRemoval101: OK
0 failure(s) in 1 test(s)
XFOILNEXT-1.0.1-R1: PASS
```

## Release action

Local commit `Prepare XFOIL.next 1.0.1 maintenance release`, annotated tag
`v1.0.1` (`XFOIL.next 1.0.1`). No push/fetch/tag push/release.
