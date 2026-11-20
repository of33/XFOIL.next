# Building and testing

XFOIL.next builds with a single `Makefile`. The default target is a release
build. Run `make` from the repository root; tests resolve `sample/` paths
relative to the root.

## Requirements

- Linux x86-64 (validated platform).
- GNU Make.
- A C++20 compiler: GCC or Clang. Validated with GCC 13.3 and Clang 18.1.
- Python 3 (used by the check and harness scripts).
- `gfortran` only when rebuilding the Fortran harnesses
  (`make test-harnesses`). The Fortran reference verification and
  determinism checks use the precompiled `fortran-bin/xfoil` and do not need
  a Fortran compiler.
- `valgrind` only for `make test-valgrind`.
- Qt 6.9.x only for the optional GUI skeleton targets. Set `QT_ROOT` to the
  Qt kit prefix; the path is not built into the Makefile.

## Build targets

```text
make            release build (default; same as make all)
make debug      debug build   (-O0 -g3)
make release    release build (-O3 -DNDEBUG)
make sanitize   ASan + UBSan build (-O1 -g3)
make library    release static and shared library
make library-static
make library-shared
make library-link-check     external consumer against the static library
make test-consumers         static/shared staged-install consumer smoke
make library-demo               standalone staged-library demo (static/shared)
make test-library-demo          demo integration tests
make install    staged install (DESTDIR/PREFIX)
```

Optional GUI skeleton targets:

```text
make gui                    GUI debug archive
make gui-debug              GUI debug archive
make gui-release            GUI release archive
make test-gui               offscreen Qt tests
make gui-qcustomplot-check  QCustomPlot vendor/hash and smoke check
make gui-link-check         public-header GUI consumer link check
make clean-gui              remove build/gui
make check-gui-dependencies GUI boundary and vendoring gate
```

The GUI targets require Qt 6.9.x. Resolve it with `QT_ROOT`, for example:

```sh
QT_ROOT=/path/to/Qt/6.9.3/gcc_64 make gui
QT_ROOT=/path/to/Qt/6.9.3/gcc_64 QT_QPA_PLATFORM=offscreen make test-gui
```

The adapter also accepts a kit found through `pkg-config` or `qmake`. The
build emits generated MOC/UIC files only below `build/gui/`; debug and release
objects, and objects from different compilers or Qt kits, use separate
directories. The GUI archive is debug or release specific and contains only the
widget shell, generated UI object, QCustomPlot object and its meta-object. It
does not contain core, session or CLI objects.

This is a G1 build skeleton and embeddable widget shell, not a finished GUI
application. No GUI release package, installer or Windows GUI build is
provided. The product version remains 1.2.1.

Binaries land under `build/<mode>/`; libraries land under
`build/<mode>/lib/`. The library archive contains the core, session and
pure IO objects, but not `src/main.cpp` or the application/CLI adapters.
Static and shared objects use separate `libobj-static/` and
`libobj-shared/` roots.

## Portable runtime packaging

The portable targets produce one self-contained runtime executable per
platform:

```text
make portable-linux            Linux build + archive
make portable-windows          Windows build + archive (MSYS2 UCRT64)
make portable-package-linux    package an existing Linux build
make portable-package-windows  package an existing Windows build
make portable-check-linux      validate ELF, dependencies and archive
make portable-check-windows    validate PE, imports and archive
make portable-repro-linux      two clean Linux builds, compare bytes
make portable-repro-windows    two clean Windows builds, compare bytes
make portable-sha256sums       separate SHA256SUMS for both archives
```

The Linux build requires Docker for the digest-pinned Ubuntu 22.04 /
GCC 13.4.0 / glibc 2.35 builder. The Windows build runs on the validated
Windows 11 + MSYS2 UCRT64 + GCC/MinGW 16.1.0 Rev5 baseline. Outputs are
written below `build/portable/`:

```text
linux-x86_64/release/xfoil.next
windows-x86_64/release/xfoil.next.exe
dist/XFOIL.next-<version>-linux-x86_64.tar.xz
dist/XFOIL.next-<version>-windows-x86_64.zip
dist/XFOIL.next-<version>-SHA256SUMS
```

Each archive contains exactly `LICENSE`, `NOTICE` and the executable. The
Linux executable statically includes libstdc++/libgcc and dynamically uses
system glibc 2.35 or newer. The Windows executable statically includes the
MinGW-w64 runtime and uses the Windows UCRT. The archive version comes from
`src/app/ProductInfo.h`; packaging does not bump the version or publish a
release. Builds from a clean source archive need `XFOIL_SOURCE_COMMIT`,
`XFOIL_SOURCE_TREE`, optional `XFOIL_SOURCE_REF` and
`SOURCE_DATE_EPOCH`; the Make targets derive them from Git automatically.
See [packaging/portable](../packaging/portable/README.md) for the complete
contract and the pinned toolchain inventory.

## Test targets

```text
make test                   full C++ suite against the debug binary
make test-core              core suite
make test-unit              unit. prefix filter
make test-integration       integration. prefix filter
make test-regression        regression. prefix filter
make test-batch             native batch suite (batch. prefix)
make test-sanitize          full suite against the sanitizer binary
make test-valgrind          representative paths under Valgrind
make test-library-isolation session isolation tests
make test-library-progress progress and cancellation tests
make test-library-skin-friction skin-friction projection tests
make test-library-concurrency parallel diagnostic test
make test-library-noio      armed static/shared no-I/O proof (Linux)
make test-tsan-library      ThreadSanitizer concurrency diagnostic
make test-library-demo          standalone demo static/shared integration runs
make check-library-dependencies LIB_SRCS/public-header dependency check
make library-symbol-check   static/shared forbidden-symbol audit
```

## Library build and installation

```text
Linux static        build/<mode>/lib/libxfoil_next.a
Linux shared        build/<mode>/lib/libxfoil_next.so
Windows static      build/<mode>/lib/libxfoil_next_static.a
Windows DLL         build/<mode>/lib/xfoil_next.dll
Windows import lib  build/<mode>/lib/libxfoil_next.dll.a
```

`make install` installs the complete `include/xfoil/**` tree, the
selected library, the CLI, `LICENSE`, `NOTICE`, and
`lib/pkgconfig/xfoil-next.pc` below `DESTDIR`. The package version in the
pkg-config file is derived from `src/app/ProductInfo.h`.

`make test-consumers` performs a staged install under `build/l6-stage`,
builds `tests/consumer/LibraryConsumer.cpp` once against the static
archive and once against the shared library, and runs both. The shared
consumer is compiled with `XFOIL_NEXT_USE_SHARED`; on Windows the staged
DLL directory is added to `PATH`. No system install is required.

The [XFOIL.next Library User Manual](../XFOIL.next-Library-Manual.pdf)
documents the public API, static/shared linking, installation and the
demo in one place.

`make library-demo` builds the standalone `library-demo/` consumer once shared
and once static against the staged installation through pkg-config. It
does not include repository sources. `make test-library-demo` runs the
inviscid, viscous, optional-parameter, negative-sequence, CRLF-profile
and exit-code checks against the staged package. A direct build can use:

```sh
PKG_CONFIG_PATH="$PWD/build/l6-stage/usr/lib/pkgconfig" \
PKG_CONFIG_SYSROOT_DIR="$PWD/build/l6-stage" \
make -C library-demo LINK_MODE=shared
```

The demo application reads the supplied profile file. The library
continues to perform no file or console I/O during API calls. The
`--alpha-end` value is exclusive, and `--re 0` selects the inviscid path.
When `pkg-config` is unavailable, the demo Makefile accepts
`XFOIL_NEXT_PREFIX` (or explicit `XFOIL_NEXT_INCLUDE` and
`XFOIL_NEXT_LIBDIR`) and still links only against installed artifacts.

## Library no-I/O and isolation proof

`make test-library-isolation` runs the unit and integration isolation
tests. `make test-library-progress` runs the unit and integration progress,
callback-failure and cancellation tests. `make test-library-skin-friction`
runs the owning Cf projection, state-validity and BLDUMP-parity tests. `make test-library-concurrency`
runs the independent-session parallel diagnostic; `make test-tsan-library`
builds the selected test with ThreadSanitizer. Concurrent use of one session is not supported.

`make test-library-noio` builds an interposition shim and static/shared
proof consumers against the staged installation. The shim is armed only
around the public API window, records forbidden calls in memory, and the
consumer runs in an empty read-only directory with cwd/environment checks.
This target is Linux-only. The Windows check uses the import table,
source/dependency check, empty-directory inventory, cwd and stdout/stderr
evidence instead.

The library dependency check (`make check-library-dependencies`) verifies
the expanded `LIB_SRCS` closure and public-header boundary.
`make library-symbol-check` checks debug and release static/shared artifacts
for forbidden unresolved symbols and app/CLI exports.

## Fortran reference and integrity targets

```text
make check-fortran-sha              Fortran source integrity (SHA256SUMS)
make test-reference                 Fortran reference verification
make test-reference-determinism     Fortran reference determinism
make test-harnesses                 Fortran harness rebuild/check (needs gfortran)
```

`make test-reference` and `make test-reference-determinism` run the
precompiled `fortran-bin/xfoil`; they do not invoke `gfortran`.
`make test-harnesses` rebuilds the Fortran harnesses and compares
their output byte-for-byte against stored references; it requires `gfortran`.

## Validation targets

```text
make check-dependencies        dependency direction and forbidden-state checks
make check-license-headers     GPL-3.0-or-later source header check
make check-release-inventory   release inventory / hygiene check
make check-gui-dependencies    GUI public-header and vendoring gate
```

## Compiler and build-root contract

Both GCC and Clang must build cleanly with `-Werror`. There are no warning
suppressions. Use a separate build root per compiler so objects are never
mixed:

```sh
make debug CXX=clang++ BUILDROOT=build/clang
```

The core build area is additionally toolchain-separated under
`build/core/<toolchain>/`.

## Sanitizers and Valgrind

`make sanitize` builds with AddressSanitizer and UndefinedBehaviorSanitizer;
`make test-sanitize` runs the full suite against that binary.
`make test-valgrind` runs a representative set of API, session, solver, CLI,
and batch paths under Valgrind (sequentially, with child processes traced)
and fails on definite leaks or errors. If `valgrind` is not installed, the
target reports an error rather than silently skipping.

## Isolated test working directories

CLI and batch tests run in unique temporary working directories and restore
the process working directory afterwards, so runtime files from one test do
not collide with another. Set `XFOIL_KEEP_TEST_ARTIFACTS=1` to keep those
artifacts for inspection.

## Golden files

Golden files describe current behaviour and are regression anchors. They are
not changed casually. The only maintainer recapture path is the native batch
goldens, and it requires an explicit confirmation variable:

```sh
make update-golden CONFIRM_GOLDEN_UPDATE=yes
```

Without the confirmation the target aborts without writing. It performs
three byte-identical runs per case and checks that each case exits 0 before
writing. After an update, review and update `tests/golden/metadata.md`.

## Current counts (as of version 1.2.1)

The commands above are the durable contract; the counts below are a point-in-
time measurement and may change with new tests.

```text
core suite:        465
unit:             1868
integration:       343
regression:          5
batch:               47
full C++ suite:   2266

Fortran source integrity (SHA256SUMS): 48
Fortran reference cases:              236
Fortran harness families:              16
```

There are no expected failing tests; the standard suite is fully green.
