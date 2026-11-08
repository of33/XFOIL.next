<p align="center">
<img src="XFOIL.next.png" alt="XFOIL.next logo" width="220">
</p>

<p align="center">
<strong>A modern C++ implementation of XFOIL 6.99 for aerodynamic analysis and design</strong>
</p>

<p align="center">
<a href="https://github.com/of33/XFOIL.next/actions/workflows/build-and-test.yml">
<img src="https://github.com/of33/XFOIL.next/actions/workflows/build-and-test.yml/badge.svg"
alt="Build and test">
</a>
&nbsp;&nbsp;
<a href="https://github.com/of33/XFOIL.next/releases/latest">
<img src="https://img.shields.io/github/v/release/of33/XFOIL.next?label=release"
alt="Latest stable release">
</a>
&nbsp;&nbsp;
<a href="https://doi.org/10.5281/zenodo.24046163"><img src="https://zenodo.org/badge/DOI/10.5281/zenodo.24046163.svg" alt="DOI"></a>
&nbsp;&nbsp;
<a href="LICENSE">
<img src="https://img.shields.io/badge/license-GPL--3.0--or--later-blue"
alt="GPL-3.0-or-later">
</a>
</p>

# XFOIL.next

**Version 1.0.0**

XFOIL.next implements the complete canonical non-graphical XFOIL 6.99
functionality in modern C++.

It is a C++20 port of the canonical XFOIL 6.99 Fortran implementation by
Mark Drela and Harold Youngren. The numerical behaviour is validated
directly against the original Fortran sources and the reference Fortran
binary.

## Motivation

XFOIL.next builds on the enduring aerodynamic work embodied in XFOIL 6.99.
Its purpose is to provide a modern, reliable, and understandable C++
foundation for teaching, continued development, and straightforward use,
while preserving full respect for the original authors' scientific and
engineering achievement.

The project aims to:

- offer a maintainable C++20 codebase with clear layering and a readable
  architecture suited for study and extension;
- preserve numerical continuity through a bundled Fortran reference
  corpus and systematic regression validation against the original
  implementation;
- make the tool easy to use from the command line, in batch mode, or
  through the public C++ session API;
- provide a solid base for future development without altering the
  physical and numerical foundations established by the original authors.

XFOIL 6.99 is the work of Mark Drela and Harold Youngren. The original
sources and their notices are retained unmodified under `fortran-src/`.
See [NOTICE](NOTICE) for full attribution and provenance details.

## What it provides

- the complete canonical non-graphical XFOIL 6.99 command set: airfoil and
  panel geometry, NACA generation and file import, inviscid and viscous
  analysis, boundary-layer / transition / wake / interaction numerics,
  single-point and alpha/CL sequences, polar accumulation and reference
  polars, polar save and binary dump files, file and geometry exports,
  settings via `xfoil.def` / `WDEF` / `RDEF`, GDES and CAMB, MDES and QDES,
  VPAR boundary-layer parameters, flap state and hinge moments, and the
  remaining TOP and OPER commands;
- a modern, session-based C++ API (`xfoil::XFoil`, `xfoil::XFoilSession`);
- the native interactive XFOIL command-line interface;
- a native non-interactive batch mode that reuses the same command
  interpreter, session layer, and numerical core;
- deterministic handling of unavailable plotting and cursor commands.

## What it deliberately does not provide

There is no graphical backend. X11 plotting, graphical curves, zoom,
graphical cursor interaction, and PostScript / hardcopy output are not
implemented. Pure plot and cursor commands are still recognised and report:

```text
Plotting is not available in this build.
```

The numerical, textual, and file-related parts of mixed commands are kept.
This is a port of XFOIL 6.99, not an independent re-derivation of the
solver, and it is not a bit-identical reproduction of every Fortran build
variant (see [known differences](docs/known-differences.md)).

## Quick start

Build (default target is a release build):

```sh
make            # release build, binary at build/release/xfoil.next
make debug      # debug build,  binary at build/debug/xfoil.next
```

Run the interactive CLI:

```sh
build/debug/xfoil.next                 # interactive session
build/debug/xfoil.next profile.dat     # interactive session with a startup profile
build/debug/xfoil.next < commands.txt  # drive the session from stdin
```

Run the test suites:

```sh
make test         # full C++ test suite
make test-batch   # native batch-mode suite
```

See [building and testing](docs/building-and-testing.md) for the complete
target list, compiler requirements, and validation gates.

## Platform support

**Linux is the canonical platform**: the complete Fortran reference
verification (236 canonical cases, 16 harness families), determinism,
Valgrind, and the bit-exact reference contracts are defined and run
there.

Supported Windows environment: **Windows 11 with MSYS2 UCRT64**. In an
UCRT64 shell, install the toolchain (`pacman -S mingw-w64-ucrt-x86_64-gcc
mingw-w64-ucrt-x86_64-gcc-fortran make python`) and build with the same
Makefile; the product binary is `build/debug/xfoil.next.exe` (or
`build/release/xfoil.next.exe`). Confirmed on Windows 11 + MSYS2 UCRT64
with GCC/MinGW 16.1.0 Rev5: the Debug and Release builds are clean under `-Werror`, and
`xfoil.next.exe` starts directly in the UCRT64 console; from PowerShell
or `cmd.exe` it also runs when `<MSYS2>\ucrt64\bin` is on `PATH` (MSYS2
runtime DLLs). A standalone binary without the MSYS2 runtime is not part
of the contract, and the Fortran reference harnesses are exercised on
Linux only. Platform-dependent numeric test deviations are handled by
documented per-test portable contracts in `tests/TestSupport.h` and
`tests/DumpParitySupport.h`; the Linux bit-exact and byte-exact
contracts are unchanged. The reproducible MSYS2 evidence run is
described in [Windows](#windows).

## Windows

### Supported environment

Validated on **Windows 11 + MSYS2 UCRT64 + GCC/MinGW 16.1.0 Rev5**
(the accepted Windows 1.0.0 baseline).

- MSVC is not supported and not promised;
- Cygwin is not supported and not promised;
- there is **no standalone binary without the MSYS2 runtime** and no
  bundled runtime; the product binary links the UCRT64 runtime DLLs,
  so `<MSYS2>\ucrt64\bin` must be on `PATH` when starting
  `xfoil.next.exe` from `cmd.exe` or PowerShell.

### Variable MSYS2 installation path

`C:\msys64` is only the usual default install location. MSYS2 may live
on any drive and directory. The helper script
`tools\windows\xfoil-next-env.bat` prepends `<MSYS2>\ucrt64\bin` to
`PATH` and takes the location from its first argument, from
`MSYS2_ROOT`, or falls back to `C:\msys64` (a common default attempt,
not a required location). Invoke it with `call` so the change stays
visible in the calling `cmd.exe` session:

```bat
call tools\windows\xfoil-next-env.bat D:\Programme\msys64

set MSYS2_ROOT=D:\Programme\msys64
call tools\windows\xfoil-next-env.bat
```

From PowerShell, run it in a `cmd /k` session (a batch file cannot
permanently change the environment of an already running PowerShell
process after it exits):

```powershell
cmd /k "call tools\windows\xfoil-next-env.bat D:\Programme\msys64"
```

### Starting the program

The product binary is named `xfoil.next.exe` in `build\debug\` or
`build\release\`. The former name `xfoil.exe` is never produced:

```bat
build\release\xfoil.next.exe
```

### Windows-specific numerical contracts

Linux remains the canonical bit-exact and byte-exact reference
platform. Because of different compiler, runtime and floating-point
evaluation, a few internal reference values can deviate on Windows by
extremely small amounts. These deviations are **not** globally
tolerated: each one is covered by a measured, test-specific contract,
and no Windows goldens exist. Non-numeric values and structure remain
exact — NaN and infinity are rejected, structural mismatches fail, and
large final-result deviations fail; the final batch operating points
are still checked semantically and tightly. Linux runs remain strict
(bit/byte-exact) everywhere.

Current measured contract families (Windows branches only, as
implemented in `tests/TestSupport.cpp`, `tests/DumpParitySupport.h`
and `tests/regression/CseqHeadDumpParityTests.cpp`; Linux keeps its
strict comparison):

| Family                     | Absolute | Relative |
|----------------------------|----------|----------|
| VELS (internal reference)  | 1e-14    | 2e-10    |
| FMOM (internal reference)  | 5e-15    | 1e-12    |
| CPI (internal reference)   | 3e-13    | 1e-11    |
| CSEQ pins ALFA/DTOR        | 5e-14    | 1e-12    |
| CSEQ pins CL               | 1e-15    | 1e-12    |
| CSEQ pins CM               | 2e-15    | 1e-12    |
| PACC dump field cp         | 5e-12    | 5e-11    |
| PACC dump field cf         | 1e-13    | 2e-9     |
| PACC dump field ctau       | 2e-11    | 3e-10    |

Numeric text comparison uses a whitespace-token model: non-numeric
tokens are compared byte-exactly, numeric tokens under the measured
family above, and near-zero values use IEEE signed-zero semantics
(`+0.0 == -0.0`) so that sign-only differences of zero are not
misreported.

### Reproducible Windows evidence run

`tools\windows\build-and-test-msys2.bat` performs a scripted,
reproducible evidence run on MSYS2 UCRT64 using the named options:

```bat
tools\windows\build-and-test-msys2.bat ^
  --msys2-root D:\Programme\msys64 ^
  --log-dir D:\xfoil-next-audit\pub-w5-run1
```

`--msys2-root` is optional (fallback order: option, `MSYS2_ROOT`,
`C:\msys64`); `--log-dir` is required and must lie **outside** the
repository — the run refuses to write logs inside the repository and
creates a timestamped subdirectory instead, so existing logs are never
overwritten. The run records environment metadata (Windows version,
UCRT64 identity, compiler versions, commit, dirty-worktree note),
builds Debug and Release with `-Werror`, runs the full debug suites
(unit 1668, integration 245, batch 47), repeats core (414) and
regression (5) three times each from the same untouched debug build,
executes the full release suite (1968), checks the binary contract
(`xfoil.next.exe` present, `xfoil.exe` absent, `--version` output),
and performs one focused `XFOIL_DUMP_DIAG=1` PACC dump diagnostic.
It writes separate logs (`00-environment.txt` … `50-summary.txt`), a
machine-readable summary, and `SHA256SUMS.txt`.

In addition, a manual-only workflow (`windows-diagnostic.yml`,
`workflow_dispatch`) runs the same complete suite on the
GitHub-hosted `windows-2022` runner with the current MSYS2 UCRT64
toolchain (currently GCC/MinGW 16.2.0 Rev4). That runner is Windows
Server, not Windows 11, and its toolchain is not the accepted baseline;
the workflow is a diagnostic evaluation, not a release gate, and it may
fail while unresolved toolchain deviations remain. The accepted
Windows validation remains the fully documented manual Windows 11 +
MSYS2 UCRT64 + GCC/MinGW 16.1.0 Rev5 run described above; see
`VALIDATION.md` for the hosted toolchain's measured deviations. This
statement is not a promise of MSVC, Cygwin, a standalone binary or a
bundled runtime.

## Numerical validation

The Fortran sources under `fortran-src/` are the numerical authority, and
`fortran-bin/xfoil` is the executable reference oracle. The port is checked
against them with:

```sh
make check-fortran-sha              # Fortran source integrity (SHA256SUMS)
make test-reference                 # 236 canonical reference cases
make test-reference-determinism     # byte-deterministic reference reruns
make test-harnesses                 # 16 focused Fortran harness families
```

See [numerical validation](docs/numerical-validation.md) and
[Fortran reference](docs/fortran-reference.md).

## Validation baseline

Linux is the canonical platform for the bit-exact and byte-exact reference
contracts. The 1.0.0 release tree was validated with GCC 13.3, the
GCC 16.2.0 point release (side-by-side installation), and Clang 18.1.3. The Fortran
reference and harness checks use the documented system GNU Fortran
baseline.

Compiler and runtime differences can affect floating-point evaluation and
therefore the regeneration of numerical reference artifacts. Shipped
goldens and reference data must not be regenerated merely to accommodate
a different compiler. See [VALIDATION.md](VALIDATION.md) for the recorded
compiler, platform, and reference-validation matrix.

## Documentation

- [Architecture](docs/architecture.md)
- [Building and testing](docs/building-and-testing.md)
- [Command-line interface](docs/cli.md)
- [Batch mode](docs/batch-mode.md)
- [Public C++ API](docs/public-api.md)
- [Numerical validation](docs/numerical-validation.md)
- [Fortran reference](docs/fortran-reference.md)
- [Known differences](docs/known-differences.md)
- [Contributing](CONTRIBUTING.md)
- [Security](SECURITY.md)
- [Changelog](CHANGELOG.md)

## Planned Work

The following items describe intended directions rather than fixed release
commitments. Order and scope may change as technical evaluation proceeds.

- **XFOIL 6.996 integration.** Evaluate and integrate the changes
  introduced in XFOIL 6.996, applying the same reference-based numerical
  validation used for the current codebase before any merge.

- **Detailed documentation.** Documenting the code structure and program
  flow in detail including theoretical background explanations

- **Directly embeddable library distribution.** Provide a dedicated
  library package for direct integration into third-party C++ applications
  without invoking an external executable, reusing the existing core and
  session architecture behind a stable, documented interface.

- **Graphical user interface.** Develop a graphical user interface aligned
  with the existing command, session, and CLI architecture. Plotting and
  cursor interaction are not yet available (see
  [known differences](docs/known-differences.md)).

- **QFoil approach evaluation.** Evaluate selected QFoil approaches for
  possible improvements to boundary-layer prediction, subject to numerical
  validation, technical suitability, and license compatibility. No QFoil
  code is currently part of this project.

## License and provenance

XFOIL.next is licensed under the GNU General Public License, version 3 or
later (GPL-3.0-or-later); see [LICENSE](LICENSE).

XFOIL.next is based on XFOIL 6.99 by Mark Drela and Harold Youngren. The
original XFOIL 6.99 sources under `fortran-src/` retain their own notices
(GPL-2.0-or-later) and are kept unmodified. Provenance and attribution are
recorded in [NOTICE](NOTICE).
