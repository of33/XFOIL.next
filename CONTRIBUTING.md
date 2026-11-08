# Contributing to XFOIL.next

Thank you for your interest in contributing. This document describes the
development workflow, the language policy, and the quality gates that every
change must satisfy.

## Language

New public documentation, user-facing and maintainer-facing comments, and
test diagnostics are written in English.

Canonical Fortran-compatible output is not translated when doing so would
change the established byte contract. This includes CLI prompts and
messages, file-format text, golden output, and Fortran reference output.
Quote those byte-for-byte.

## Development workflow

1. Build and run the test suite before changing anything, so you have a
   known-good baseline:

   ```sh
   make test
   ```

2. Make your change in small, reviewable steps.
3. Run the smallest sufficient check for the change, then the full gate set
   before opening a pull request (see below).
4. Open a pull request that describes the change, the motivation, and how it
   was verified.

The build system is a single `Makefile`. Use `make help` for the target
list. Build with GCC or Clang; use a separate build root per compiler, for
example:

```sh
make debug CXX=clang++ BUILDROOT=build/clang
```

Both compilers must build cleanly with `-Werror`. Do not add warning
suppressions.

## Required checks

Before a change is ready for review, all of the following must pass:

```sh
make test                       # full C++ suite (core, unit, integration, regression, batch)
make test-core                  # core suite
make test-batch                 # native batch suite
make check-dependencies         # dependency direction and forbidden-state gates
make check-fortran-sha          # Fortran source integrity
make test-reference             # Fortran reference verification
make test-reference-determinism # Fortran reference determinism
make test-harnesses             # Fortran harness rebuild/check (requires gfortran)
make check-license-headers      # GPL-3.0-or-later source header gate
make check-release-inventory    # release inventory / hygiene gate
```

Run the suite against both GCC and Clang, in debug, release, and sanitizer
builds, and run the representative Valgrind paths:

```sh
make sanitize && make test-sanitize
make test-valgrind
```

There are no expected failing tests. A pull request must leave the standard
test suite fully green.

## Numerical changes

The Fortran sources under `fortran-src/xfoil-6.99/` are the numerical authority, and
`fortran-bin/xfoil` is the executable reference oracle.

- Prove a numerical change against Fortran first. Do not change formulas,
  constants, floating-point operation order, loop or summation order,
  tolerances, iteration limits, or convergence criteria without a separately
  proven defect and a Fortran-backed justification.
- Do not raise a tolerance to hide a difference.
- Do not regenerate reference values from current C++ output. Reference data
  comes from the Fortran oracle only.
- The reference regeneration mode of the Fortran reference tool is used only
  after explicit domain authorization. Verification and determinism modes do
  not modify references.

## Golden files

- Do not change an existing golden file without an explicit, separately
  documented contract decision and review.
- A golden update requires a determinism proof: the same output over three
  consecutive runs, byte-identical, and cross-toolchain byte-identical
  between GCC and Clang.
- The only maintainer golden-recapture path is the native batch goldens:

  ```sh
  make update-golden CONFIRM_GOLDEN_UPDATE=yes
  ```

  This guard is mandatory; without the confirmation variable the target
  aborts without writing. It runs three byte-identical runs per case and
  checks that each case exits 0 before writing.
- The native batch goldens live under `tests/golden/batch/` (cases `clark_y`,
  `naca2312`, `naca4415`, `naca0012`; each with a `.xfb` job, a transcript,
  `results.dat`, and an exit-code file). After a batch golden update, review
  and update `tests/golden/metadata.md`.

## Bug reports

For a bug, include:

- the exact command line or batch job that reproduces it;
- the input profile or a minimal reproducer;
- the expected and observed output;
- the compiler, build mode, and platform;
- the relevant gate output.

Reproduce in an isolated working directory so that runtime files do not
interfere.

## Plotting

Do not assume a plotting backend exists or is planned. Plot and cursor
commands are recognised and report that plotting is not available in this
build. Do not invent non-canonical replacement grammar for cursor commands.

## License headers

Every hand-written project source carries the project GPL-3.0-or-later header
with an `SPDX-License-Identifier` declaration. The header gate enforces this
and verifies that the original Fortran sources under `fortran-src/xfoil-6.99/` remain
unmodified. Run `make check-license-headers` after adding or editing files.
