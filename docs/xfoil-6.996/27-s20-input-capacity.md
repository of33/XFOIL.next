# XFOIL 6.996 — S20 input capacity 256/258

Status: PASS — `X6996-S20` and `X6996-S20-R1` accepted. Implementation,
Linux matrix, focused Windows process and R1 documentation check are green.

Slice: `S20` of U6, group G11. Scope is the 6.996 input-capacity change in
`userio.f`. No command grammar, solver, format or default behavior is changed.

## S20 contract from U6

- U6 slice: `S20`.
- U6 group: G11 input capacity.
- Fortran reference path: `userio.f`, routines `READI`, `READR`, `GETINT`
  and `GETFLT`.
- C++ area: application numeric parser and the shared GETFLT file parser.
- Dependencies: none.
- Scope: input buffer `80/130` to `256/258` and parse limit `128` to `256`.
- Oracle: `a32`, focused `H-S20-INPUT-256`.
- Regressions: `a01`, `a02`, `a32`.
- Commit boundary: one local S20 implementation commit.
- Test prefixes: `unit.InputCapacityS20` and
  `integration.InputCapacityS20`.

## Capacity contract

The two capacities are distinct:

| Item | 6.99 | 6.996 | Meaning |
|---|---|---|---|
| `GETINT`/`GETFLT` parse view | 128 | 256 | Maximum number of input characters considered by the inner parser |
| `GETINT`/`GETFLT` work record | 130 | 258 | 256 input characters plus the appended `" ,"` sentinel |
| `READI`/`READR` local line | 80 | 256 | Local character variable |
| `READI`/`READR` read format | `A80` | `A80` | The outer record read is still limited to 80 characters |

Characters after position 256 are ignored by `GETINT`/`GETFLT`. The sentinel
is parser-internal and does not become user text. The number grammar remains
unchanged: blanks and commas separate values, signs and decimal points are
unchanged, and `e/E/d/D` exponents remain accepted for real values.

`FORMAT(A80)` is not widened. The `READI`/`READR` outer line remains an
80-character read even though the local character variable is larger.

## Required work

- Apply the 256-character view to the 6.996 `GETINT`/`GETFLT` parser paths.
- Keep the 258-character working-record/sentinel semantics equivalent.
- Preserve the 80-character `A80` prompt reads.
- Keep the 128-character ASKC command-line buffer unchanged.
- Keep list-directed prompt paths that are not `GETINT`/`GETFLT` unchanged.
- Add focused boundary tests at 128/129, 255/256/257 and the A80 boundary.
- Add integration coverage for inline and prompt paths, beyond-128 values,
  beyond-256 truncation, blank/default behavior and the long-name `a32`
  regression.
- Record stock 6.99/6.996 oracle evidence in the external audit area
  `xfoil-6.996/s20/`.

## Acceptance plan

- Focused `H-S20-INPUT-256` unit and integration tests.
- Full Linux suite with GCC and Clang Debug/Release.
- ASan/UBSan and focused Valgrind.
- Reference verification and determinism, `236/236` each.
- Harness families, `16/16`.
- Exactly one focused Windows process:
  `integration.InputCapacityS20`, expected `6/6`, 0 failures.
- Permanent gates and a focused R1 check before commit.

## Non-goals

- No S21/S22 work.
- No S14, SVG, DXF or `G-EXPORT` work.
- No command grammar or prompt-text change.
- No solver, session, format or default change.
- No global increase of unrelated string, file-name or output capacities.
- No `A80` to 256 change.
- No product-version, banner or tolerance change.
- No Fortran source, binary, patch or canonical 6.99 reference change.
- No Windows golden and no full Windows suite.
