# XFOIL 6.996 — S16 BLOU command report

Status: PASS — `X6996-S16` accepted. Implementation, Linux matrix,
permanent gates and the single focused Windows process are green.

Decision: activate only the hidden 6.996 OPER `BLOU` command on top of the
accepted S13 BLDUMP3 writer. SVG/DXF remain excluded by `G-EXPORT`.

## Starting state

- HEAD: `69e77579c114ade5b48ec01b86a3180bf1566e4d`
  (`Document XFOIL 6.996 S15 acceptance`).
- Parent: `8d4164125ae1e7e3f5e3fb0f6b1462a17ef4a52f`.
- Worktree clean, staged 0, ahead 29 behind 0.
- Tag `v1.0.0` unchanged at
  `dc7305b057286574cd137cb6f1af170b74650ec5`.
- No S16 BLOU commit existed at the start.
- No push, fetch, tag or release occurred.

## Upstream and stock findings

Compared `fortran-src/xfoil-6.99/xoper.f` and
`fortran-src/xfoil-6.996/xoper.f`, then measured the external stock-6.996
binary under `xfoil-6.996/s16/`.

- BLOU is dispatched only by 6.996 at `xoper.f:1685-1686`:
  `CALL BLDUMP3(COMARG)`.
- `BLDUMP3` is at `xoper.f:2290-2502` and is hidden from OPER help.
- The command has no success message and ignores `KDELIM`.
- Inline argument is the literal filename. Otherwise the current SETFILE
  prefix supplies `<prefix>.bl`; without a prefix the ASKS prompt is used.
- The file is opened as unknown/overwrite, so an existing target is
  truncated without confirmation.
- Stock valid CLARK Y, Re=1e6, alfa=4 output:
  - 42704 bytes, 177 lines, 20 columns;
  - SHA-256 `04d81ba85130f109ef5d06f260a318f52e648bac9de7e792f1112d7047138c5d`;
  - identical to the accepted S13 BLDUMP3 a28 fixture.
- Stock no-BL output:
  - 491 bytes, 3 lines (two header lines plus one blank line);
  - SHA-256 `f53e0956302c5cc3e153dc7b61a7874fcd5bf881083f88494b6c733b59cb7f94`.
- Stock invalid path: exit 2, no file. Stock EOF at the filename prompt:
  exit 2, no file.
- Default-prompt, no-prefix-prompt, overwrite, repeat and no-sidefile
  behavior were measured directly.

## Implementation

- `src/app/cli/CommandInterpreter.cpp`:
  - dispatch `BLOU` in OPER;
  - add `handleOperBlou` using `askExportFilename` and `writeExportFile`.
- `src/app/cli/CommandInterpreter.h`: declare the handler.
- `include/xfoil/io/ExportFormats.h` and `src/io/ExportFormats.cpp`:
  - expose `boundaryLayerDump3HeaderText()`;
  - keep the checked BLDUMP3 writer and its validation unchanged;
  - reuse the header helper in the writer so valid bytes are unchanged.
- The command uses the checked writer for valid BL input. When the checked
  writer rejects the incomplete no-station state, the command writes the
  canonical header plus the stock trailing blank line, producing the
  measured 491-byte artifact.
- BLOU stays hidden from OPER help. DUMP, DMP and CPWR paths are untouched.
- No automatic sidefile and no extension rewriting are introduced.

## Fixtures and tests

New S16 fixtures:

- `tests/data/xfoil-6.996/s16/blou-clark-y-re1e6-alfa4.bl`
- `tests/data/xfoil-6.996/s16/blou-no-stations.bl`
- `tests/data/xfoil-6.996/s16/metadata.txt`

New unit coverage:

- `unit.BlouCommandS16HeaderMatchesOracle`
- `unit.BlouCommandS16CheckedWriterRejectsIncompleteState`

New integration coverage:

- `integration.BlouCommandS16InlineNameAndBytesMatchOracle`
- `integration.BlouCommandS16DefaultPromptUsesLoadedFilePrefix`
- `integration.BlouCommandS16NoPrefixPromptAcceptsName`
- `integration.BlouCommandS16NoViscousStateWritesCanonicalHeaderOnly`
- `integration.BlouCommandS16OpenFailureAbortsWithoutFile`
- `integration.BlouCommandS16EofAtPromptEndsWithoutFile`
- `integration.BlouCommandS16OverwritesExistingTargetWithoutPrompt`
- `integration.BlouCommandS16RepeatIsDeterministic`
- `integration.BlouCommandS16DoesNotMutateAnalysisState`
- `integration.BlouCommandS16RemainsHiddenFromOperHelp`

The test source was registered in the `Makefile`. No existing golden,
canonical 6.99 reference, Fortran source, binary, patch, product version,
banner or global tolerance was changed.

## Linux and gate results

```text
Full suite:             2054/2054
Reference:               236/236
Determinism:             236/236
Harness families:           16/16
S16 focused unit:            2/2
S16 focused integration:    10/10
Dependency gates:          PASS
Release inventory:         PASS
License headers:           PASS
Public documentation:      PASS
Existing goldens changed:     0
```

The full-suite count rose from 2042 to 2054 through the 12 new S16 tests.
A direct smoke run of `build/debug/xfoil.next` produced both committed
fixtures byte-identically.

## Windows status

Exactly one focused S16 process was run on Windows:

```text
integration.BlouCommandS16
10/10, 0 failures
```

No full Windows suite and no Windows golden was run. The earlier S15
Windows process was not counted for S16.

## Commit plan

Stage only the intended S16 files explicitly and create one local commit:

```text
Integrate XFOIL 6.996 BLOU command
```

No push, fetch, amend, tag or release. The final marker will be:

```text
X6996-S16: PASS
```

## Final decision

X6996-S16: PASS
