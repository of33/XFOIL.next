# XFOIL 6.996 — S16 DXF command follow-up report

Status: PASS — `X6996-S16-DXF` and `X6996-S16-DXF-R1` accepted. Linux
matrix, focused Windows process and R1 are complete.

## Starting state

- S14 commit status: already existed; no duplicate and no amend.
- S14 commit: `9632832d148559f02b7100be8907e2a82694dce7`
  (`Integrate XFOIL 6.996 DXF exporter`).
- S14 parent: `8ce54d569b299fb6ad2d712372f07cd0d5eae766`.
- S16-DXF start HEAD: `9632832d148559f02b7100be8907e2a82694dce7`.
- Worktree clean, staged 0, untracked 0, ahead 43 behind 0 at S16-DXF start.
- Tag `v1.0.0` unchanged at
  `dc7305b057286574cd137cb6f1af170b74650ec5`.
- No prior S16-DXF follow-up commit existed.
- No push, fetch, amend, tag or release was performed.

## S14 confirmation

The S14 commit contains only the DXF R12 encoder, focused tests, S14
fixtures, registration and documents. It contains no `DXF` command,
`CommandInterpreter` activation, SVG path or plot backend. The protected
Fortran/reference paths are absent from the diff, and both S14 PASS markers
are committed.

Fixture hashes in the S14 commit:

```text
dxf-naca0012-240.dxf  a0c0d13cdf9de48bc01cced1299282ce146a217794c041631c26960037e5eaed
dxf-naca2412-240.dxf  f6bae1a2d674c3eb3b4f36de16583604a5a4c3befadc9584480d94271c4c3edf
dxf-nacs0012-240.dxf  9be1f3eac93159fbbc125c11007de63caca5578f992b7bdb362a0c684dd61482
dxf-empty.dxf         818684425bb64f551a01a01a669e1c8945a06dd1d6309b1480e21db177c254c2
```

## Follow-up scope

This follow-up adds only the hidden top-level `DXF` command and its file
wiring:

- inline filename and prompt;
- overwrite handling;
- non-fatal invalid-path behavior;
- controlled EOF;
- one `OutputPathPolicy` transformation;
- call into the accepted S14 encoder and binary LF write.

No help line, SVG path, format change, encoder change, plot backend or general
export framework is added. The BLOU part of S16 remains unchanged.

## Command contract

```text
DXF <filename>
```

- Top-level and hidden from the main help.
- Inline filename is trimmed and used verbatim; no extension is added.
- Without an argument, the prompt is `Enter output filename` with ASKS
  spacing; there is no loaded-prefix default.
- The prompt answer is trimmed; a blank answer is an invalid filename.
- The current panel contour is encoded by the S14 encoder.
- `OutputPathPolicy` is applied once at the open point.
- An existing target prompts
  `Output file exists.  Overwrite?  Y`.
- Blank/default/other answers overwrite; first character `N`/`n` declines.
- Decline leaves the target unchanged and prints `DXF file not written.`
- An invalid/unopenable path prints `Bad filename.` and
  `DXF file not written.`; the session continues without a file.
- EOF at the prompt is a controlled session end with no file.
- No success message is printed.
- No session geometry, panel, analysis, polar or output state is mutated.

## Implementation

New files:

- `src/app/cli/CoreDxfHandlers.h`
- `src/app/cli/CoreDxfHandlers.cpp`

Changed product wiring:

- `src/app/cli/CommandInterpreter.h`: private `handleDxf` declaration;
- `src/app/cli/CommandInterpreter.cpp`: top-level `DXF` dispatch and the
  prompt-answer state machine;
- `Makefile`: app source, focused test registration and Valgrind filters.

The handler uses `io::applyOutputPathPolicy` once, then
`io::dxfR12TextChecked` on the current `session::currentPanelGeometry`.
It writes in binary mode with truncation after the overwrite decision. The S14
encoder and fixtures are unchanged.

## Tests

Focused unit: `9/9`, 0 failures.

```text
unit.DxfCommandS16InlineWritesExactS14Bytes
unit.DxfCommandS16InlineKeepsMissingExtension
unit.DxfCommandS16PromptHasNoPrefixDefaultAndTrimsAnswer
unit.DxfCommandS16PromptBlankIsBadFilename
unit.DxfCommandS16OverwriteDeclineKeepsExistingFile
unit.DxfCommandS16OverwriteDefaultAndOtherAnswerWrite
unit.DxfCommandS16OutputPathPolicyAppliedOnce
unit.DxfCommandS16BadPathIsNonFatalAndLeavesNoFile
unit.DxfCommandS16EmptyContourUsesS14Skeleton
```

Focused integration: `11/11`, 0 failures.

```text
integration.DxfCommandS16InlineWritesExactS14Bytes
integration.DxfCommandS16PromptWritesSameBytes
integration.DxfCommandS16PromptHasNoLoadedPrefixDefault
integration.DxfCommandS16KeepsMissingExtension
integration.DxfCommandS16OverwriteDefaultReplacesFile
integration.DxfCommandS16DeclineKeepsExistingFile
integration.DxfCommandS16InvalidPathIsNonFatalAndContinues
integration.DxfCommandS16EofAtPromptWritesNoFile
integration.DxfCommandS16OutputPathPolicyAppliedOnce
integration.DxfCommandS16RepeatAndSessionStateUnchanged
integration.DxfCommandS16HiddenFromHelpAndSvgUnknown
```

The tests reuse the S14 NACA 0012 and empty fixtures unchanged. No new DXF
fixture and no Windows golden were added.

## Linux acceptance

```text
Full suite, GCC 13.3 Debug:       2121/2121
Full suite, GCC 13.3 Release:     2121/2121
Full suite, Clang 18.1.3 Debug:   2121/2121
Full suite, Clang 18.1.3 Release: 2121/2121
Full suite, ASan/UBSan:           2121/2121
S16-DXF focused unit:                 9/9
S16-DXF focused integration:         11/11
Reference verification:             236/236
Reference determinism:              236/236
Harness families:                    16/16
Focused Valgrind:                   PASS
Permanent gates:                    PASS
Existing product goldens changed:       0
```

The focused Valgrind filters are part of the established
`make test-valgrind` target. The complete target and its core/session link
checks passed without definite leaks.

## Windows status

Exactly one focused process ran on the accepted Windows 11 / MSYS2 UCRT64
baseline:

```text
integration.DxfCommandS16
11/11, 0 failures
```

All eleven focused integration cases passed, including inline/prompt
filenames, missing extension, overwrite accept/decline, invalid path, EOF,
one-time path policy, repeat determinism, hidden command behavior and the
unchanged SVG rejection. Additional Windows processes: 0. No full Windows
suite and no Windows golden were run.

## R1 — focused acceptance

R1 was completed after the focused Windows process and before the S16-DXF
commit. It confirms:

- S16-DXF start HEAD is the S14 commit
  `9632832d148559f02b7100be8907e2a82694dce7`, parent
  `8ce54d569b299fb6ad2d712372f07cd0d5eae766`.
- No prior S16-DXF commit existed; no duplicate was created.
- The follow-up contains only hidden top-level `DXF` command activation and
  its file wiring.
- `DXF` is absent from the top-level help.
- Inline filenames are trimmed, used verbatim and receive no `.dxf`
  extension.
- The prompt has no loaded-prefix default; prompt answers are trimmed.
- The overwrite prompt matches the gate contract; `N`/`n` keeps the existing
  file and blank/default/other answers overwrite.
- Invalid paths and unopenable targets are non-fatal, print the canonical
  messages, leave no output file and preserve the session.
- EOF at the prompt is a controlled session end without a file.
- `OutputPathPolicy` is applied exactly once at the open point.
- The S14 encoder is the only byte producer; its source, API and fixture
  hashes are unchanged.
- The command does not mutate geometry, panel, analysis, polar or session
  output state.
- SVG remains unknown and `NIMP-02`; no SVG file is created.
- No general export framework, plot backend or help change is included.
- Existing S16 BLOU markers and behavior are unchanged.
- The Linux matrix is green.
- Exactly one focused Windows process is green; no second process was run.
- No existing product golden, canonical 6.99 reference, Fortran source,
  binary, patch, product version, banner or tolerance was changed.
- No private absolute path is present in the S16-DXF documents.
- No push, fetch, amend, tag or release was performed.

## Protected state

- S14 encoder source, API and fixture bytes: unchanged.
- Fortran sources, binaries and patches: unchanged.
- `tests/fortran-reference/**`: unchanged.
- Existing product goldens: unchanged.
- Canonical 6.99 references: unchanged.
- Existing SAVE/PSAV/MSAV/WDEF/DUMP/DMP/BLOU/BLDUMP/BLDUMP2/BLDUMP3/
  polar paths: unchanged.
- SVG: not implemented; remains `NIMP-02`.
- DXF help entry: absent.
- Plot backend: not touched; `G-PLOT-HEADLESS` remains open and deferred.
- Product version and banner: unchanged; `v1.0.0` unchanged.
- No push, fetch, amend, tag or release.

## Git status

At S16-DXF start:

```text
Worktree: clean
Staged: 0
Untracked: 0
Ahead/behind: 43/0
S16-DXF start HEAD: 9632832d148559f02b7100be8907e2a82694dce7
```

The final S16-DXF commit hash is reported by the outer task because it cannot
be embedded in the same commit.

## Next step

The G-EXPORT work is complete: S14 supplies the accepted DXF R12 encoder and
S16-DXF supplies the accepted hidden command. SVG remains `NIMP-02`.
`G-PLOT-HEADLESS` remains open and deferred. No further productive slice is
released by this follow-up; do not start the cumulative 1.1.0 final acceptance
without a separate instruction.

## Final decision

X6996-S16-DXF: PASS
X6996-S16-DXF-R1: PASS
