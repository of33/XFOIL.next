# Portable runtime packaging

This directory contains the deterministic packaging path for the
single-executable XFOIL.next runtime.

## Contracts

| Platform | Executable | Runtime contract | Archive |
| --- | --- | --- | --- |
| Linux x86-64 | `xfoil.next-cli` | single ELF, static libstdc++/libgcc, Ubuntu 24.04 system glibc, no older-glibc promise | `XFOIL.next-<version>-linux-x86_64.tar.xz` |
| Windows x86-64 | `xfoil.next-cli.exe` | single PE, static MinGW-w64 runtime, Windows UCRT | `XFOIL.next-<version>-windows-x86_64.zip` |

Each archive contains exactly `LICENSE`, `NOTICE` and the executable at the
archive root. The additional `SHA256SUMS` file is distributed separately.
No shared project library, SDK, installer or additional runtime DLL is part
of these assets.

The archive version is read from `src/app/ProductInfo.h`. Packaging does not
change the product version and does not create a tag or release.

## Commands

Linux, from the repository root:

```sh
make portable-linux
make portable-check-linux
make portable-repro-linux
```

Windows, from an MSYS2 UCRT64 shell in the repository root:

```sh
make portable-windows
make portable-check-windows
make portable-repro-windows
```

Both platforms, after both archives exist:

```sh
make portable-sha256sums
```

Default outputs:

```text
build/portable/linux-x86_64/release/xfoil.next-cli
build/portable/windows-x86_64/release/xfoil.next-cli.exe
build/portable/dist/
```

Portable assets are packaging intermediates and intentionally stay below
`build/portable/`. The normal product build writes its executable below
`bin/<mode>/`.

`make clean` removes the generated portable build and distribution area.

## Pinned build environments

Linux is built natively on Ubuntu 24.04 x86-64 with GCC 13.3.0. The
executable is linked with:

```text
g++ -static-libstdc++ -static-libgcc
```

The accepted baseline requires a maximum GLIBC symbol version of 2.38. The
static GCC C++ runtime means that no GLIBCXX runtime symbol version is
required. `make portable-check-linux` reports both values from the actual
binary; no older-glibc compatibility is promised. The packaging script
enforces the Ubuntu 24.04 and GCC 13.3.x baseline.

Windows uses the supported Windows 11 + MSYS2 UCRT64 + GCC/MinGW 16.2.0
Rev7 build baseline and links with:

```text
g++ -static -static-libgcc -static-libstdc++ -Wl,--no-insert-timestamp
```

The Windows package is built on that baseline; cross-building is not part of
the contract. The 1.2.1 release evidence remains on GCC/MinGW 16.1.0 Rev5.

## Determinism

`SOURCE_DATE_EPOCH` defaults to the current commit timestamp. Both packagers
use fixed member order, fixed modes, zero uid/gid, fixed timestamps and no
private build paths. The Linux archive is written with Python `tarfile` and
XZ; the Windows archive with Python `zipfile`. The PE timestamp is zero.

`make portable-repro-linux` exports the current commit twice, builds both
clean trees on the native Ubuntu 24.04 / GCC 13.3.0 baseline, packages both,
and compares the executable and archive bytes.
`make portable-repro-windows` performs the same check from two clean source
copies on the Windows baseline.

## Source identity

The generated `NOTICE` records the product version, source URL, commit,
tree, source tag and `SOURCE_DATE_EPOCH`. Builds from a clean source
archive have no `.git` directory and therefore need:

```text
XFOIL_SOURCE_COMMIT
XFOIL_SOURCE_TREE
XFOIL_SOURCE_REF       (optional; default "not tagged")
SOURCE_DATE_EPOCH
```

The Make targets and build scripts supply these values automatically in a
Git checkout.

## Files

```text
package_portable.py       deterministic archive, NOTICE and SHA256SUMS writer
check_portable_binary.py  ELF/PE, dependency, version and archive validator
NOTICE.linux.in           Linux notice template
NOTICE.windows.in         Windows notice template
licenses/                 verbatim third-party runtime notices
```

The Linux note covers the statically linked GCC runtime (GCC Runtime
Library Exception) and the dynamically linked system glibc. The Windows note
covers the GCC runtime exception, MinGW-w64 runtime, winpthreads MIT,
BSD-3-Clause-Clear and MinGW-w64 CRT ZPL-2.1 notices.
