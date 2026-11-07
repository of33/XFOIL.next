<!-- File: tests/golden/metadata.md -->

# Golden file metadata

These files record the **current** behaviour of the program. They do not
prove that the behaviour is physically correct; they are regression
anchors.

## Capture

| Field | Value |
|---|---|
| Platform | Linux x86-64 |
| Compiler | g++ (Ubuntu 13.3.0) 13.3.0 |
| Verification compiler | Ubuntu clang 18.1.3 |
| Make | GNU Make |
| Build options | Debug build: `-std=c++20 -O0 -g3` plus the project warning flags (`-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual -Wnull-dereference -Wdouble-promotion -Wformat=2 -Werror`) |
| Program | `build/debug/xfoil.next`, invoked from the repository root |

Golden text is generated with GCC and verified successfully with Clang.

## Determinism

Three consecutive runs of the debug binary produce byte-identical standard
output and standard error. The goldens generated under GCC are verified
against GCC and Clang debug and release builds.

## Files

### Interactive CLI transcripts (`tests/golden/cli/`)

Eighteen interactive cases. Each case has four files:
`<case>.stdin.txt`, `<case>.stdout.txt`, `<case>.stderr.txt`, and
`<case>.exit.txt`. The cases cover startup, help, quit, profile load/save,
NACA, INTE/PCOP, name handling, NORM/REVE/DELI, paneling (PPAR), VPAR,
OPER single points and sequences, the unrecognised-command path, and the
plot-unavailable path.

### Native batch goldens (`tests/golden/batch/`)

Four native batch cases (G1–G4):

| Case | Job fixture | Artifacts |
|---|---|---|
| G1 Clark-Y | `clark_y.xfb` (`LOAD sample/CLARK_Y.dat`) | `clark_y_transcript.txt`, `clark_y_results.dat`, `clark_y_exit_code.txt` |
| G2 NACA2312 | `naca2312.xfb` (`LOAD sample/NACA2312.dat`) | `naca2312_*` (as above) |
| G3 NACA 4415 | `naca4415.xfb` (internal `NACA 4415`) | `naca4415_*` |
| G4 NACA 0012 | `naca0012.xfb` (internal `NACA 0012`) | `naca0012_*` |

- Capture: `make update-golden CONFIRM_GOLDEN_UPDATE=yes` (three byte-identical
  runs per case before acceptance; each case exits `0`); GCC debug binary,
  cross-toolchain check with the Clang debug binary byte-identical.
- Domain parameters per case: Re = 1e5, Mach 0, ITMAX 100, Ncrit 9/9,
  Xtr 1.0/1.0, VACCEL 0.01, alpha sequence 0.0 to 14.5 step 0.5 (30 points)
  as a native ASEQ warm path. Under the native ASEQ contract all 30 points
  converge (warm start, ITMAX+5).
- Job commands follow the canonical ASKC spelling of the native router
  (`N 9` rather than `NCrit 9`; the OPER menu is closed with a blank line
  followed by `QUIT`, because the OPER menu has no native `QUIT`).
- Compared byte-for-byte: the transcript (interpreter stdout via a tee
  stream), `results.dat` (version header `# XFOIL_BATCH_RESULTS 1` and the
  product metadata line `# product XFOIL.next 1.0.0`), and the exit code.
  stderr stays empty in the successful golden cases.
- Tests: `batch.ClarkYNativeBatchMatchesGolden`,
  `batch.Naca2312NativeBatchMatchesGolden`,
  `batch.Naca4415NativeBatchMatchesGolden`,
  `batch.Naca0012NativeBatchMatchesGolden`.

## Banner

The C++ product banner is `XFOIL.next Version 1.0.0`, taken from a single
product-identity source. The 18 CLI stdout goldens and the 4 batch
transcripts carry this banner. The reference banner of the canonical Fortran
oracle (`tests/fortran-reference/cases/**`) is unchanged; the two stdout
parity tests normalise only the documented branding region with a counted
replacement (exactly one replacement expected), and all numerical and
domain stdout regions stay byte-pinned.

## Update rules

Update goldens only explicitly:

```sh
make update-golden CONFIRM_GOLDEN_UPDATE=yes
```

Without the confirmation variable the target aborts without writing. It runs
three byte-identical runs per case and checks that each case exits `0`
before writing. After an update, review and update this file.

## Status

The standard test suite is fully green; there are no expected failing tests.
Earlier non-native batch goldens and their CSV parser were removed and
replaced by the native batch goldens above; the current product has no CSV
batch contract.

<!-- End: tests/golden/metadata.md -->
