# XFOIL 6.996 — S18 text-only console contract

Status: RELEASED — `G-TEXT-OFS` is decided and S18 may be implemented.
S18 implementation has not started.

Slice: `S18` of U6, group G09. Scope is the accepted text-only console
contract: the project-specific inviscid `OFS:` output, `CLf`/`CDp` lines,
`xtcam` widths and prompts. No numeric behavior is changed.

## Gate decision

```text
G-TEXT-OFS: REMOVE-NON-UPSTREAM-OFS-OUTPUT
X6996-G-TEXT-OFS: PASS
```

Decision basis: authoritative project-owner provenance and product decision.

The `OFS:` output at label 333 is a local project-owner extension, not an
original XFOIL function. It has no compatibility contract to preserve.
XFOIL.next 1.1.0 follows the original 6.996 behavior at this point.

No legacy mode, version mode or opt-in compatibility option is introduced.
Only the affected `OFS:` path may be removed. Canonical 6.99 references remain
unchanged. `tests/fortran-reference/**` remains unchanged. No Fortran source,
binary or patch is changed.

`G-EXPORT` remains open. S14/SVG/DXF remain blocked.

## S18 contract from U6

- U6 slice: `S18`.
- U6 groups: G09 command/session text and the non-plot `xtcam` text portion
  of G12.
- Fortran reference paths: `xfoil.f`, `xoper.f`, `xtcam.f`.
- C++ area: app text/console.
- Dependencies: `S06`, `S09` and `G-TEXT-OFS`. `S06` and `S09` are accepted;
  the gate is now decided.
- Scope: accepted text-only console behavior for the `OFS:` decision,
  `CLf`/`CDp` lines, `xtcam` widths and prompts.
- No numeric solver, session, format or default change.
- Accepted text oracle cases: `a02`, `a03`, `a04`, `a05`, `a16`, `a17` and
  `H-S18-TEXT`; relevant existing text regressions.
- No global golden refresh and no numeric golden migration.
- Commit boundary: one local S18 implementation commit after this gate.

## Expected S18 work

- Remove the project-specific `OFS:` console output along the affected
  analysis path.
- Align the remaining text-only console behavior with the accepted 6.996
  contract for `CLf`/`CDp` lines, `xtcam` widths and prompts.
- Update only tests or goldens that are directly affected by the removed
  `OFS:` output.
- Keep canonical 6.99 references and all protected Fortran material unchanged.

The later implementation must confirm the exact C++ call sites and test
targets in its own pre-analysis. This gate document does not pre-implement
S18.

## Acceptance plan

- Focused `H-S18-TEXT` and the named text cases.
- `test-core`, `test-regression` and the relevant text suite.
- Reference checks where text parity is affected.
- Linux Debug/Release.
- Focused Windows text process for the final S18 text contract.
- No numeric golden refresh and no Windows golden.

## Non-goals

- No S19/S20/S21/S22 implementation.
- No S14/SVG/DXF or `G-EXPORT` change.
- No Fortran, binary or patch change.
- No product-version, banner or release-tag change.
- No global tolerance or existing reference migration.
