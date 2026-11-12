# XFOIL 6.996 — S19 SMOO target correction

Status: PASS — `X6996-S19` and `X6996-S19-R1` accepted. Implementation,
Linux matrix, focused Windows process and R1 documentation check are green.

Slice: `S19` of U6, group G10. Scope is the 6.996 correction of the MDES
`SMOO` target column. No general inverse-design refactor is included.

## S19 contract from U6

- U6 slice: `S19`.
- U6 groups: G10 inverse-design behavior.
- Fortran reference path: `xmdes.f` `MDES`.
- C++ area: core inverse and CLI/session mapping.
- Dependencies: `S01` and `S03`, both accepted.
- Scope: `SMOO` `KQSP=1` target correction; no general inverse refactor.
- Oracle case: `H-S19-SMOO`; regression cases `a10` and `a16`.
- Commit boundary: one local S19 implementation commit after acceptance.
- Test prefixes: `unit.SmooTargetS19` and `integration.SmooTargetS19`.

## Upstream finding

The complete 6.99/6.996 `xmdes.f` comparison contains one S19 change:

```diff
       ELSEIF(COMAND.EQ.'SMOO') THEN
 C----- smooth Qspec within target segment
-       KQSP = KQTARG
+       KQSP = 1
        CALL SMOOQ(IQ1,IQ2,KQSP)
        CALL CNCALC(QSPEC(1,KQSP),LQSYM)
        CALL QSPCIR
```

In 6.99 the MDES `SMOO` column depends on `KQTARG`. In 6.996 it is fixed to
the first Qspec column. The `KQTARG=0` COMMON overlay on `QGAMM` is therefore
no longer the MDES `SMOO` target. `QGAMM` remains unchanged and `QSPCIR`
still refreshes the active columns after `CNCALC`.

## Expected S19 work

- Always smooth the first Qspec distribution in the MDES `SMOO` path.
- Keep `KQTARG` unchanged for state compatibility; do not use it for `SMOO`.
- Preserve the existing `CNCALC`, `QSPCIR`, `QSPINT` and report sequence.
- Keep `QGAMM` unchanged.
- Update the obsolete alias-path comments and tests.
- Add focused Windows-ready coverage under `unit.SmooTargetS19` and
  `integration.SmooTargetS19`.
- Anchor exact DUMP bytes against the stock 6.996 oracle in the external
  audit area `xfoil-6.996/s19/`.

## Acceptance plan

- Focused `H-S19-SMOO` unit and integration tests.
- `test-core` where available, `test-regression` and the full C++ suite.
- Fortran reference verification and determinism, `236/236` each.
- Harness families, `16/16`.
- Linux Debug/Release with GCC 13.3 and Clang 18.1.3.
- ASan/UBSan and focused Valgrind.
- Exactly one focused Windows process:
  `integration.SmooTargetS19`, expected `2/2`, 0 failures.
- Permanent repository gates and a focused R1 check before commit.

## Non-goals

- No S20 input-capacity change.
- No general inverse refactor.
- No QDES, MDES, MIXED, GDES or geometry redesign.
- No new command, API or file-format contract.
- No product-version, banner or tolerance change.
- No Fortran source, binary, patch or canonical 6.99 reference change.
- No Windows golden and no full Windows suite.
