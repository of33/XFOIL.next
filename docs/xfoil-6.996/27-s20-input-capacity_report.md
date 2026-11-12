# XFOIL 6.996 — S20 input capacity report

Status: PASS — `X6996-S20` and `X6996-S20-R1` accepted. Implementation,
Linux matrix, focused Windows process and R1 documentation check are green.

## Starting state

- HEAD: `583461fd63a1096219a370b162e5c87af4d0674c`
  (`Integrate XFOIL 6.996 SMOO target correction`).
- Parent: `2427c54241a047236b5a2dd1115dcbcd0c97def0`.
- Worktree clean, staged 0, untracked 0, ahead 37 behind 0.
- Tag `v1.0.0` unchanged at
  `dc7305b057286574cd137cb6f1af170b74650ec5`.
- No S20 commit or prior S20 work existed at the start.
- No push, fetch, tag or release.

## U6 scope and upstream diff

S20 is group G11. The complete 6.99/6.996 `userio.f` comparison changes only
the `READI`, `READR`, `GETINT` and `GETFLT` capacities:

| Area | 6.99 | 6.996 | S20 target |
|---|---|---|---|
| `GETINT`/`GETFLT` parse limit | 128 | 256 | 256-character parser view |
| `GETINT`/`GETFLT` work record | 130 | 258 | 256 plus internal `" ,"` sentinel |
| `READI`/`READR` local line | 80 | 256 | local capacity only |
| `READI`/`READR` read format | `A80` | `A80` | unchanged outer 80-character read |

No numeric grammar change is present in the upstream diff.

## A80 separation

The 80-character read is an outer `READI`/`READR` property and is retained.
The direct stock-6.996 oracle harness confirms:

| Prompt response length | `READR` result | `READI` result |
|---|---|---|
| 78 characters, value at the end | value parsed | value parsed |
| 80 spaces before value | default preserved | default preserved |
| 130 spaces before value | default preserved | default preserved |

The C++ numeric parser therefore has two explicit views:

- 256 characters for the `GETINT`/`GETFLT` contract;
- 80 characters for the product prompt call sites that use the shared
  numeric parser to map `ASKR`, `ASKI`, `READR` and `READI`.

The ASKC 128-character command-line buffer in `CommandParser` is unchanged
because 6.996 does not change ASKC. The 128-character logical defaults read
in `DefaultsCodec` is also unchanged because it is not a `GETINT`/`GETFLT`
path.

## C++ implementation

Affected paths:

- `src/app/cli/NumericArguments.cpp`
- `src/app/cli/NumericArguments.h`
- `src/io/TextFormat.cpp`
- prompt call sites in `CommandInterpreter.cpp`,
  `CoreGeometryHandlers.cpp` and `CoreInverseDesignHandlers.cpp`
- focused S20 unit and integration tests
- `Makefile` test registration
- this S20 document pair

`parseRealArguments` and `parseIntegerArguments` now default to a
256-character view and accept an explicit smaller view for A80 prompts.
`TextFormat::parseFloatList` uses the same 256-character GETFLT view.
The existing number grammar, diagnostics, sign handling, decimal point,
comma/blank separation and `e/E/d/D` exponent rules are unchanged.

No solver, session, format, default, command prompt or export behavior is
changed.

## Oracle and boundary measurements

External audit area: `xfoil-6.996/s20/`.

Direct harnesses compiled externally against each frozen `userio.f` measured:

- real token after the old 128 boundary: 6.99 ignores, 6.996 parses;
- real token ending at character 256: both the character and token are in
  the 6.996 view;
- token starting at character 257: ignored;
- integer at the 256/257 boundary: same contract;
- `D` exponent after the old 128 boundary: unchanged grammar;
- `READI`/`READR` `A80`: unchanged 80-character outer read.

The `a32` long-name case uses a 200-character name followed by `SAVE`.
Both stock binaries truncate the name to 48 characters; the file-coordinate
difference is the already accepted panel default difference, not an S20
behavior. The focused product regression checks the 48-character name header.

## Tests

Focused unit coverage:

- `unit.InputCapacityS20RealArgumentsIncludeCharacter129`
- `unit.InputCapacityS20RealArgumentsIncludeCharacter256`
- `unit.InputCapacityS20RealArgumentsIgnoreCharacter257`
- `unit.InputCapacityS20RealArgumentsRecognizeRightEdgeToken`
- `unit.InputCapacityS20RealArgumentsKeepDExponentAfterOldLimit`
- `unit.InputCapacityS20IntegerArgumentsIncludeCharacter256`
- `unit.InputCapacityS20IntegerArgumentsIgnoreCharacter257`
- `unit.InputCapacityS20PromptViewRetainsA80Limit`
- `unit.InputCapacityS20TextFormatUses256CharacterView`

Focused integration coverage:

- `integration.InputCapacityS20InlineRealBeyondOldLimit`
- `integration.InputCapacityS20InlineIntegerBeyondOldLimit`
- `integration.InputCapacityS20InlineIgnoresCharacterBeyond256`
- `integration.InputCapacityS20PromptIntegerPath`
- `integration.InputCapacityS20PromptBlankKeepsDefault`
- `integration.InputCapacityS20LongNameRegressionA32`

Focused result: unit `9/9`, integration `6/6`, 0 failures.

## Fixture and golden inventory

No new fixture was added under `tests/data/xfoil-6.996/s20/`.
No existing product golden was changed.
No canonical 6.99 reference was changed.

## Linux and gate results

```text
Full suite, GCC Debug:           2094/2094
Full suite, GCC Release:         2094/2094
Full suite, Clang Debug:         2094/2094
Full suite, Clang Release:       2094/2094
Full suite, ASan/UBSan:          2094/2094
Reference verification:            236/236
Reference determinism:             236/236
Harness families:                   16/16
S20 focused unit:                    9/9
S20 focused integration:             6/6
Valgrind representative matrix:     PASS
Valgrind focused S20 unit:           9/9
Valgrind focused S20 integration:    6/6
Permanent gates:                    PASS
Existing product goldens changed:      0
Existing 6.99 references changed:      0
```

The separate `make test-core` target still has the documented pre-existing
link-set issue also present at the starting HEAD. It is not changed by S20
and all core tests are included in the full suite.

## Windows minimal policy

Exactly one focused Windows process is required after the green Linux matrix:

```text
integration.InputCapacityS20
6/6, 0 failures
```

Exactly one focused Windows process was run:

```text
integration.InputCapacityS20
6/6, 0 failures
```

Additional Windows processes: 0. No full Windows suite and no Windows golden
are part of S20.

## Focused R1 acceptance — X6996-S20-R1

R1 was performed after the focused Windows process and before the single S20
implementation commit. It confirms:

- S20 is limited to the 6.996 input-capacity change; no command grammar,
  solver, session, format, default, export or prompt-text change is included.
- `GETINT`/`GETFLT` use the 256-character parse view with equivalent
  `" ,"` sentinel handling; characters after position 256 are ignored.
- The 80-character `READI`/`READR` `A80` outer read is preserved and the
  shared product prompt call sites that use the numeric parser pass the
  80-character view.
- The 128-character ASKC command-line buffer and the non-`GETINT`/`GETFLT`
  logical defaults read are unchanged.
- The numeric grammar, signs, decimal point, comma/blank separation and
  `e/E/d/D` exponents are unchanged.
- Boundary tests cover 128/129, 255/256/257, prompt A80 and integer/real
  paths; the stock 6.99/6.996 audit evidence is external only.
- The Linux matrix is green.
- Exactly one focused Windows process is green; no further Windows process
  was run.
- `a32` preserves the 48-character airfoil-name truncation.
- No product golden, canonical 6.99 reference, Fortran source, binary,
  patch, product version, banner or global tolerance was changed.
- No push, fetch, amend, tag or release was performed.

## Protected state and publication freeze

- Fortran sources, binaries and patches unchanged.
- `tests/fortran-reference/**` unchanged.
- Product version and banner unchanged; `v1.0.0` unchanged.
- No global tolerance change and no existing golden change.
- No push, fetch, amend, tag or release.

## Final decision

X6996-S20: PASS
X6996-S20-R1: PASS
