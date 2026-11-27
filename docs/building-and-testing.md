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
- Qt 6.12.0 for the GUI targets. Set `QT_ROOT` to the Qt kit prefix; the path
  is not built into the Makefile.

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

Product executables:

```text
bin/debug/xfoil.next-cli
bin/release/xfoil.next-cli
```

On MSYS2/MinGW and native Windows the suffix is `.exe`. Test programs,
consumer/linkcheck programs, sanitizer programs, harness builds, generated
files and libraries remain under `build/`. `make clean` removes both generated
trees (`build/` and the selected `BINROOT`).

GUI targets:

```text
make gui                    GUI debug archive
make gui-debug              GUI debug archive
make gui-release            GUI release archive
make gui-app                standalone GUI debug and release binaries
make gui-app-debug          standalone GUI debug binary
make gui-app-release        standalone GUI release binary
make run-gui                run the debug standalone GUI
make test-gui               offscreen Qt tests
make gui-qcustomplot-check  QCustomPlot vendor/hash and smoke check
make gui-link-check         public-header GUI consumer link check
make clean-gui              remove build/gui
make check-gui-dependencies GUI boundary and vendoring gate
make gui-check-qt          validate QT_ROOT, Qt 6.12.0 tools, modules and kit
```

The GUI targets require Qt 6.12.0. The supported modules are Core, Gui,
Widgets, PrintSupport and Svg; the GUI test target also needs Test. The normal
and standalone build uses the shared Qt kit.

Validate a kit before building:

```sh
source scripts/setup-gui-build.sh /path/to/Qt/6.12.0/gcc_64
make gui-check-qt
```

`gui-check-qt` reports the resolved Qt root, version, qmake/moc/uic/rcc paths
and kit platform. It rejects missing tools, incomplete module/library sets,
versions other than 6.12.0 and obviously incompatible kits with corrective
messages. The equivalent manual setup is:

```sh
export QT_ROOT=/path/to/Qt/6.12.0/gcc_64
export PATH="$QT_ROOT/bin:$PATH"
export LD_LIBRARY_PATH="$QT_ROOT/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="$QT_ROOT/plugins"
make gui-check-qt
```

Build and test the standalone GUI:

```sh
make gui-app
make test-gui
make gui-link-check
```

The helper also works in the MSYS2 UCRT64 shell. The PowerShell helper
validates the same Qt kit and prepares `QT_ROOT`, `PATH` and `QT_PLUGIN_PATH`
for child processes:

```powershell
. .\scripts\setup-gui-build.ps1 -QtRoot 'C:\Qt\6.12.0\mingw_64'
```

The supported GNU Make build shell remains MSYS2 UCRT64. The standalone GUI
embeds its application icon through the Qt resource system, and the Windows
executable also carries a native multi-resolution icon resource.

The standalone targets write `bin/debug/xfoil.next` and
`bin/release/xfoil.next` (`.exe` on Windows). On Windows, build from the
MSYS2 UCRT64 shell:

```sh
source scripts/setup-gui-build.sh /path/to/Qt/6.12.0/mingw_64
make gui-app-release
```

The Windows GUI executable is not a single-file deployment. Keep the Qt
libraries, plugins and MinGW runtime next to `xfoil.next.exe`; the Windows GUI
ZIP provides this complete runtime.

The GUI is the standalone Analysis application described in the README. It
supports airfoil import, NACA/NACS geometry, paneling, operating conditions,
alpha/CL points and sequences, progress, cancellation, Cp/Cf and scalar plots,
point selection, comparison polars, reports, and exports for airfoil data and
DXF, Cp and polar data, the airfoil view, and the active result plot.

The GUI tests run offscreen and cover workflow setup, analysis execution,
plots, point selection, exports and the standalone application shell. The
supported 2.0.0 GUI build baseline is Ubuntu 24.04 with GCC 13.3.0 and Qt
6.12.0 on Linux, and Windows 11 with MSYS2 UCRT64, GCC/MinGW 16.2.0 Rev7 and
Qt 6.12.0 on Windows.

The build emits generated MOC/UIC files only below `build/gui/`; debug and
release objects, and objects from different compilers or Qt kits, use separate
directories. The GUI archive is debug or release specific and contains the
standalone application, definition model, session backend, geometry preview,
generated UI/meta-objects and QCustomPlot. Core, session and CLI objects remain
in the separate public library.

GUI package targets:

```text
make gui-appimage               Linux AppImage (Ubuntu 24.04 build host)
make gui-portable-windows       Windows GUI ZIP (MSYS2 UCRT64)
make gui-portable-check-linux   validate and run the AppImage
make gui-portable-check-windows validate the Windows ZIP
make gui-portable-repro-linux   two AppImage packaging runs
make gui-portable-repro-windows two Windows ZIP packaging runs
make test-gui-portable          packaging infrastructure tests
make check-windows-subsystems   GUI=windows, CLI=console subsystem
make test-gui-installer-windows temporary known-folder installer tests
```

The Linux AppImage uses the Qt 6.12.0 kit selected by `QT_ROOT` and pinned
AppImage tools in `APPIMAGE_TOOLS_DIR`. No AppImage tool or runtime is
downloaded. The AppImage is built and tested on Ubuntu 24.04 with GCC 13.3.0;
no older-glibc compatibility promise is made. The Windows ZIP contains the GUI
executable, Qt DLLs, plugins and MinGW runtime together in one top-level
folder. The Windows GUI executable uses the Windows GUI subsystem; the CLI
remains a console application.

Windows users have two supported modes:

- **Portable:** extract the complete ZIP and run `xfoil.next.exe` from the
  extracted folder. Keep the EXE with its Qt DLLs and plugin directories; a
  copied-only EXE is not supported.
- **Per-user installation:** run `Install XFOIL.next GUI.cmd` from the
  extracted top-level folder. The complete runtime is copied to
  `%LOCALAPPDATA%\Programs\XFOIL.next GUI`; Desktop and Start Menu shortcuts
  are created for the stable installed path. The installer needs no
  administrator rights, registry changes, network access or elevation. The
  Start Menu shortcut can be pinned manually. Re-running updates the same
  target without duplicate shortcuts.

Use **Uninstall XFOIL.next GUI** in the Start Menu, or
`Uninstall XFOIL.next GUI.cmd` in `%LOCALAPPDATA%\Programs\XFOIL.next GUI`, to
remove a per-user installation. The uninstaller validates the managed target,
marker and owned shortcuts, and leaves unrelated XFOIL files and a portable
extracted folder unchanged.

GUI package build outputs remain below `build/portable/gui/` and are not
committed. See [packaging/gui](../packaging/gui/README.md).

Product programs land under `bin/<mode>/`; intermediate objects, tests,
generated files and libraries remain under `build/<mode>/`. The default
`BINROOT` is `bin`. Alternative toolchains should use their own root, for
example `BUILDROOT=build/clang BINROOT=bin/clang`. Libraries land under
`build/<mode>/lib/`. The library archive contains the core, session and
pure IO objects, but not `src/main.cpp` or the application/CLI adapters.
Static and shared objects use separate `libobj-static/` and
`libobj-shared/` roots.

### 2.0.0 packages

The 2.0.0 distribution uses four separate package names:

```text
XFOIL.next-2.0.0-linux-x86_64.tar.xz
XFOIL.next-2.0.0-windows-x86_64.zip
XFOIL.next-GUI-2.0.0-linux-x86_64.AppImage
XFOIL.next-GUI-2.0.0-windows-x86_64.zip
```

The GUI and CLI are separate downloads. The Windows GUI ZIP contains the
executable, Qt libraries, plugins and MinGW runtime together with the optional
per-user installer. The Linux GUI AppImage and the Linux CLI package are built
and tested on Ubuntu 24.04; no older-glibc promise is made for either.
Published binary packages are available from the
[GitHub Releases page](https://github.com/of33/XFOIL.next/releases). The CLI
and GUI are separate downloads. The `XFOIL.next-2.0.0-SHA256SUMS` file
contains the SHA-256 values for the four product packages.

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

The Linux CLI is built natively on the supported Ubuntu 24.04 / GCC 13.3.0
baseline. The accepted package requires a maximum GLIBC symbol version of
2.38 and, with static libstdc++/libgcc, no GLIBCXX runtime symbol version.
No older-glibc compatibility is promised. The Windows build runs on the
supported Windows 11 + MSYS2 UCRT64 + GCC/MinGW 16.2.0 Rev7 baseline; the
1.2.1 release artifacts remain validated with GCC/MinGW 16.1.0 Rev5.
Outputs are written below `build/portable/`:

```text
linux-x86_64/release/xfoil.next-cli
windows-x86_64/release/xfoil.next-cli.exe
dist/XFOIL.next-<version>-linux-x86_64.tar.xz
dist/XFOIL.next-<version>-windows-x86_64.zip
dist/XFOIL.next-<version>-SHA256SUMS
```

Each archive contains exactly `LICENSE`, `NOTICE` and the executable. The
Linux executable statically includes libstdc++/libgcc and dynamically uses
the Ubuntu 24.04 system glibc; the accepted binary requires GLIBC 2.38
symbols and no GLIBCXX runtime symbols. No older-glibc compatibility is
promised. The Windows executable statically includes the MinGW-w64 runtime
and uses the Windows UCRT. The archive version comes from
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

## Counts at the 1.2.1 point (historical)

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
