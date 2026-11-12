# XFOIL 6.996 — S17b warm starts, sequences, convergence and menu behavior report

Status: PASS — `X6996-S17b` and `X6996-S17b-R1` accepted. Implementation,
Linux matrix, focused Windows process and R1 documentation check are green.

## Starting state

- HEAD: `affe96f12e36db2fbb669138402a61f507024a0f`
  (`Integrate XFOIL 6.996 state invalidation`).
- Parent: `de40db120096f5ba48576431f07ddf79bcf26ff2`.
- Worktree clean, staged 0, untracked 0, ahead 32 behind 0.
- Tag `v1.0.0` unchanged at
  `dc7305b057286574cd137cb6f1af170b74650ec5`.
- No S17b commit existed at the start.
- No push, fetch, tag or release.

## U6 scope and substage boundary

`S17b` is the second internal stage of `S17`: warm starts, sequences,
convergence and menu behavior. Accepted `S17a` state/invalidation contracts
remain unchanged. S14/SVG/DXF remain blocked by open `G-EXPORT`.

The broad U6 line was resolved by auditing `xoper.f` and the existing
C++ paths before editing. The audit found one concrete divergence in the
OPER menu repeat dispatch. Existing warm-start, sequence point calculation,
non-convergence counting, polar accumulation and menu prompt paths were
already covered and are not changed.

## Upstream analysis

`xoper.f` maps a typed `!` through `COMOLD` and `ARGOLD` and sets
`LRECALC = .TRUE.`. The individual command branches then decide what the
repeat means:

- `ALFA`, `CLI` and `CL` skip their specification block when `LRECALC` is
  true and repeat the analysis of the stored point.
- `ASEQ` and `CSEQ` do not test `LRECALC`; they execute their complete
  sequence using the stored `ARGOLD`.
- Both sequence branches end with `COMOLD = COMAND` and
  `ARGOLD = COMARG`.
- `VELS` uses its own stored-command precedence and remains a query redo.

The C++ product already stored `operRepeatCommand_` for `ASEQ`/`AS` and
`CSEQ`/`CS`, but the `!` dispatch only consumed it for `VELS`. Sequences
therefore fell into the single-point continuation path instead of
rerunning the full sequence.

## Implementation

Changed files:

- `src/app/cli/CommandInterpreter.cpp`
- `tests/integration/OperSequenceTests.cpp`

The OPER repeat dispatch now recognizes the canonical sequence commands
`ASEQ`, `AS`, `CSEQ` and `CS`. For those commands it calls
`executeOperCommand` with the stored parsed command, so the normal sequence
handler performs the full rerun, including prompts when the original
argument was empty, polar accumulation, point-result events and
non-convergence accounting.

`ALFA`/`CLI`/`CL` remain on `handleOperBangCore`. `VELS` remains on its
existing `operRepeatCommand_` redo path. No session, numeric or format code
is changed.

## Stock-6.996 oracle

External audit area: `xfoil-6.996/s17b/`.

The oracle case uses:

```text
LOAD CLARK_Y.dat
OPER
PACC
<blank save filename>
<blank dump filename>
ASEQ 0 2 1
!
PWRT
stock.pol
<blank>
QUIT
```

Stock 6.996 writes six polar rows in the order `0, 1, 2, 0, 1, 2`. The
pre-fix product wrote only the first three rows. After the correction the
product writes the same six semantic rows. Product polar bytes differ from
the stock file only in the already known 6.99 product banner and polar
column spacing; no numeric row differs.

The focused tests additionally verify the same repeat contract for `CSEQ`,
`AS` and `CS`, and verify `ALFA`, `CLI`, `CL` and `VELS` retain their
existing repeat behavior.

## Tests

New integration coverage:

- `integration.OperSequenceS17bBangAfterAseqRepeatsWholeSequence`
- `integration.OperSequenceS17bBangAfterCseqRepeatsWholeSequence`
- `integration.OperSequenceS17bBangAfterAliasesRepeatsWholeSequence`

Focused result: `3/3`, 0 failures.

The tests assert six ordered sequence points after `!` and therefore cover
the complete rerun rather than a single continuation point. Existing
`integration.OperBang*` and OPER sequence tests remain green.

No new fixture, golden, canonical reference, Fortran source, binary, patch,
product version, banner or global tolerance is changed.

## Linux and gate results

```text
Full suite:                  2070/2070
Reference:                    236/236
Determinism:                  236/236
Harness families:              16/16
S17b focused integration:       3/3
GCC 13.3 Debug:                 PASS
GCC 13.3 Release:               PASS
Clang 18.1.3 Debug:             PASS
Clang 18.1.3 Release:           PASS
ASan/UBSan focused:             PASS
Focused Valgrind:               PASS
Permanent gates:                PASS
Existing goldens changed:           0
```

The full-suite count rose from 2067 to 2070 through the three new S17b
tests.

## Windows minimal policy

Exactly one focused Windows process is required after the green Linux
matrix:

```text
integration.OperSequenceS17b
3/3, 0 failures
```

Exactly one focused Windows process was run:

```text
integration.OperSequenceS17b
3/3, 0 failures
```

No full Windows suite and no Windows golden were run. The earlier S15/S16/S17a
Windows processes were not counted for S17b.

## Focused R1 acceptance — X6996-S17b-R1

R1 was performed before the single S17b integration commit. It confirms:

- S17b is limited to warm starts, sequences, convergence and OPER menu
  behavior; no numeric kernel, format or default is changed.
- The `!` repeat dispatch follows the 6.996 `COMOLD`/`ARGOLD` contract:
  `ASEQ`/`AS` and `CSEQ`/`CS` rerun the complete sequence, while
  `ALFA`/`CLI`/`CL` continue the last point and `VELS` keeps its query redo.
- Sequence reruns still use the normal sequence handler for prompts,
  polar accumulation, point events and non-convergence accounting.
- S17a state/invalidation contracts remain unchanged.
- S14/SVG/DXF and `G-EXPORT` remain untouched.
- Protected Fortran sources, binaries, patches and reference data are
  unchanged.
- The code diff is limited to 7 added lines in
  `src/app/cli/CommandInterpreter.cpp` and 42 added test lines in
  `tests/integration/OperSequenceTests.cpp`; no existing line is removed or
  changed.
- The focused Windows process is `integration.OperSequenceS17b`, `3/3`,
  0 failures.

## Protected state and publication freeze

- Fortran sources, binaries and patches unchanged.
- `tests/fortran-reference/**` unchanged.
- Product version and banner unchanged; `v1.0.0` unchanged.
- No global tolerance change and no existing golden change.
- No push, fetch, amend, tag or release.

## Final decision

X6996-S17b: PASS
X6996-S17b-R1: PASS
