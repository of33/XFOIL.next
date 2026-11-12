# XFOIL 6.996 — S17b warm starts, sequences, convergence and menu behavior

Status: PASS — `X6996-S17b` and `X6996-S17b-R1` accepted. Implementation,
Linux matrix, focused Windows process and R1 documentation check are green.

Slice: `S17b` of `S17`, the second internal stage after accepted `S17a`.
Scope is limited to warm-start, sequence, convergence and OPER menu
behavior. No numeric solver kernel, file format or default is changed.

`G-EXPORT` remains open. S14, SVG and DXF remain untouched.

## U6 contract

U6 stages `S17` internally as:

- `S17a`: state/invalidation after geometry and file commands;
- `S17b`: warm starts, sequences, convergence and menu behavior.

`S17a` precedes `S17b`; separate target cases and commits are required.
No numeric kernel, format or default change belongs to either stage.

The U6 description for `S17b` is intentionally broad. The canonical 6.996
OPER path was therefore audited against the existing sequence, warm-start
and convergence tests before changing code. That audit found one concrete
menu-repeat defect; the other audited paths already match the canonical
behavior.

## Canonical behavior

Relevant 6.996 source: `xoper.f`.

- The OPER repeat branch maps `!` through `COMOLD` and `ARGOLD`
  (lines 85-96).
- `ALFA`, `CLI` and `CL` guard their input/specification block with
  `LRECALC`; `!` therefore repeats the last analysis point instead of
  re-entering the point specification.
- `ASEQ` and `CSEQ` ignore `LRECALC`; their branches rerun the complete
  sequence from the stored raw argument.
- At the end of a sequence, `COMOLD = COMAND` and `ARGOLD = COMARG`.
- `VELS` has its own `COMOLD` precedence and remains a query repeat, not
  an operating-point repeat.

## Target behavior

- `!` after `ASEQ` reruns the complete alpha sequence.
- `!` after `CSEQ` reruns the complete lift-coefficient sequence.
- The canonical aliases `AS` and `CS` behave like `ASEQ` and `CSEQ`.
- `!` after `ALFA`, `CLI` or `CL` keeps the existing continuation behavior.
- `!` after `VELS` keeps the existing VELS redo behavior.
- Sequence replies use the stored raw argument; an originally empty
  sequence argument prompts again on repeat.
- Sequence points are accumulated and summarized by the normal sequence
  handler, including polar capture and non-convergence accounting.
- No warm-start algorithm, convergence criterion, sequence point formula
  or numeric result changes.

## Implementation boundary

- Add the canonical sequence command set `ASEQ`, `AS`, `CSEQ`, `CS` to the
  OPER repeat dispatch.
- Route `!` for those commands through the already stored
  `operRepeatCommand_` and the normal OPER command dispatcher.
- Leave `ALFA`/`CLI`/`CL` on `handleOperBangCore` and `VELS` on its existing
  redo path.
- Add focused integration tests with the prefix
  `integration.OperSequenceS17b`.
- Do not change `SessionOps`, the numerical kernels, formats, defaults,
  protected upstream sources, goldens or fixtures.

## Acceptance plan

- Focused sequence and menu tests green.
- Full Linux suite green.
- Reference and determinism suites green.
- Harness families and permanent gates green.
- GCC and Clang Debug/Release green.
- ASan/UBSan and focused Valgrind green.
- Exactly one focused Windows process:
  `integration.OperSequenceS17b`, expected `3/3`.
- No full Windows suite and no Windows golden.

## Non-goals

- No S14/SVG/DXF and no `G-EXPORT` change.
- No file-format migration or golden update.
- No new numeric kernel or tolerance change.
- No product-version, banner or release-tag change.
- No push, fetch, amend, tag or release.
