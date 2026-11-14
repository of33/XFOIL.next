# Building and testing

XFOIL.next builds with a single `Makefile`. The default target is a release
build. Run `make` from the repository root; tests resolve `sample/` paths
relative to the root.

## Requirements

- Linux x86-64 (validated platform).
- GNU Make.
- A C++20 compiler: GCC or Clang. Validated with GCC 13.3 and Clang 18.1.
- Python 3 (used by the gate and harness scripts).
- `gfortran` only when rebuilding the Fortran harnesses
  (`make test-harnesses`). The Fortran reference verification and
  determinism gates use the precompiled `fortran-bin/xfoil` and do not need
  a Fortran compiler.
- `valgrind` only for `make test-valgrind`.

## Build targets

```text
make            release build (default; same as make all)
make debug      debug build   (-O0 -g3)
make release    release build (-O3 -DNDEBUG)
make sanitize   ASan + UBSan build (-O1 -g3)
```

Binaries land under `build/<mode>/`.

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
```

## Fortran reference and integrity targets

```text
make check-fortran-sha              Fortran source integrity (SHA256SUMS)
make test-reference                 Fortran reference verification
make test-reference-determinism     Fortran reference determinism
make test-harnesses                 Fortran harness rebuild/check (needs gfortran)
```

`make test-reference` and `make test-reference-determinism` run the
precompiled `fortran-bin/xfoil`; they do not invoke `gfortran`.
`make test-harnesses` rebuilds the focused Fortran harnesses and compares
their output byte-for-byte against stored references; it requires `gfortran`.

## Gate targets

```text
make check-dependencies        dependency direction and forbidden-state gates
make check-license-headers     GPL-3.0-or-later source header gate
make check-release-inventory   release inventory / hygiene gate
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

## Current counts (as of version 1.0.1)

The commands above are the durable contract; the counts below are a point-in-
time measurement and may change with new tests.

```text
core suite:        414
unit:             1669
integration:       246
regression:          5
batch:              47
full C++ suite:   1970

Fortran source integrity (SHA256SUMS): 48
Fortran reference cases:              236
Fortran harness families:              16
```

There are no expected failing tests; the standard suite is fully green.
