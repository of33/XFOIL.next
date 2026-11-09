# XFOIL 6.996 — U6 functional-integration-plan report

Status: PASS

Initial planning status: DRAFT; superseded by this final status.

This report records the U6 planning result. It is documentation only: no C++,
Fortran, test, Golden, reference, tolerance or product-version change was made.
All implementation starts only after U6 acceptance.

## Starting state

- HEAD: `910a507a5a54a48cbb0e4581826bd31d499df1b3`.
- `origin/main`: `58bca7d17f847e1aef6b1c8d3d4e167f40f69945`.
- Ahead/behind: `0 10`.
- Tag: `v1.0.0` unchanged.
- Working tree before U6: clean; staged 0.
- Frozen sources: 6.99 48/48 and 6.996 49/49 green.
- Canonical 6.99 binary:
  `f5779d6954d0b8fdc152e0e0633f18732e00b7b69228d561d629bd524bba598c`.
- Existing unpatched 6.996 binary:
  `000b0468d9a7f4fce42d6fac24bd14147d7d326c8a9035909c8e9f98d180f50e`.
- Patched 6.996 oracle: external
  `031d667f3990c326b5b90d448fbdd10327b127a4e56d249fd6fcec71b55a69ce`,
  patch SHA-256
  `be992f7c48cee137293e15b8fe2dc169b3e5838b5f46c95475b479048cca0572`.

## Inputs used

- `03-source-difference-inventory.md` and report, including the G01-G13
  closure and R2 acceptance.
- `04-oracle-behavior-inventory.md` and report, including the 33-case corpus,
  observed differences and non-observable areas.
- `05-reproducible-oracle-build.md` and report, including the D1 and P1
  patch-overlay results.
- `tools/build-xfoil-6.996-oracle.sh`.
- `patches/xfoil-6.996/0001-rewind-polar-dump-unit-before-header.patch`.

## Planning result

The plan defines 22 implementation/decision slices, `S01` through `S22`, and
three explicit non-implementation decisions. The larger slices `S02`, `S05`
and `S17` are internally staged (`S02a`/`S02b`, `S05a`/`S05b`,
`S17a`/`S17b`) so their independent effects can use separate bounded commits:

- `NIMP-01`: plot-only state and commands, including `NTS`, `LPPORT`, `NSPL`
  and plot-only routines, with read-only shared-state justification and
  re-evaluation conditions in the plan.
- `NIMP-02`: SVG/DXF if `G-EXPORT` rejects product scope, with format/API
  ownership and geometry/export dependencies stated.
- `NIMP-03`: OSMAP data path if `G-OSMAP` cannot be resolved, with the
  `NTCALC`/`FRPLOT` producer and future reader named.

The slice decomposition keeps the required separations:

- G01 capacity is `S01`; G01 polar element metadata is `S10`; plot-only G01
  state is `NIMP-01`. BL state additions are carried by `S08`.
- G02 is `S02`.
- G03 is `S03`.
- G04 is split into `S04` (NACA/sharp-TE generation), `S05` (GDES geometry
  kernels) and `S15` (command activation).
- G05 is `S06`.
- G06 is split into `S08` (BL state and data flow), `S09` (CFT) and `S18`
  (text output only).
- G07 is `S07`.
- G08 is split into `S10` (element state), `S11` (polar text I/O), `S12`
  (polar dump), `S13` (BL dump formats), `S14` (SVG/DXF scope/export) and
  `S16` (export commands).
- G09 is split into `S15` (geometry commands), `S16` (export commands), `S17`
  (session/invalidation/warm starts/sequences) and `S18` (text-only output).
- G10 is `S19`; G11 is `S20`; G12 is `S21`; G13 is `S22`.

The full slice table in the plan contains, for every slice, scope, Fortran
routines, C++ mapping status, dependencies, risk, oracle cases, 6.99
regressions, expected files, expected change, non-goals and commit boundary.

## Dependency graph

The plan contains an explicit edge table and a Mermaid graph. Every slice
dependency exists; the graph is acyclic. The key chains are:

```text
S01 limits/state
  -> S03 geometry primitives
  -> S04 sharp-TE generation
  -> S06 QDCALC
  -> S07 defaults

S01 -> S08 BL state -> S09 CFT
S08 -> S10 polar element state -> S11 polar save / S12 polar dump
S08/S09 -> S13 BL dump -> S16 BLOU
S13/S14 -> S16 export commands
S15/S16/S08/S09/S11 -> S17 session state
```

Format/oracle gates are explicit inter-slice gates: `G-POLAR-SAVE`,
`G-DEFAULTS`, `G-EXPORT`, `G-TEXT-OFS`, `G-NACA-BUFFER`, `G-BL-SIDEFILE` and
`G-OSMAP`.

## Oracle cases

Every slice has at least one target case and at least one 6.99 regression case
in the plan. The target cases use:

- U4 cases `a01`-`a33` where applicable;
- planned focused cases `H-S01` through `H-S22` for behavior not observable
  through the CLI;
- `O-6996-P1` only as a case-scoped target contract;
- `O-6996-U` only for unpatched defect/limit reproduction;
- `T-PENDING` for polar-save and OSMAP areas awaiting gates.

The polar-dump target is separated from the polar-save column-order finding.
The patched 6.996 dump case uses the accepted P1 binary and patch contract;
the polar-save order remains an open format decision.

## Format and scope decisions

The plan defines and keeps open:

- `G-POLAR-SAVE`: accept upstream order, use a patch contract, or retain the
  existing XFOIL.next format;
- `G-DEFAULTS`: adopt or retain `NPAN=240`/`CTERAT=0.3`;
- `G-EXPORT`: include or exclude SVG/DXF; this gates only the SVG/DXF portion
  of `S14`/`S16`, while `BLOU` remains based on `S13`;
- `G-TEXT-OFS`: retain or remove the inviscid `OFS:` line;
- `G-NACA-BUFFER`: decide the 245/399-point NACA buffer behavior;
- `G-BL-SIDEFILE`: decide the automatic `:00.bl` side file;
- `G-OSMAP`: establish data provenance and format compatibility.

No format decision is silently made in U6. Polar save and OSMAP are not marked
resolved. Plot-only behavior is not promoted to product scope. SVG/DXF remain
conditional.

## First implementation step

The only released first step is:

`X6996-S01-Limits-and-State-Capacity`

It covers only `IQX` 370→601, `NPX` 12→20, derived workspace/array capacities,
bounds and controlled rejection. It excludes defaults, NACA point counts,
sharp-TE algorithms, QDCALC, BL numerics, commands and file formats. `S01a` /
`S01b` is allowed only if pre-analysis proves the two limits cannot be changed
safely together.

## Checks

- `git diff --check`: exit 0.
- `make check-fortran-sha`: 6.99 48/48 and 6.996 49/49, exit 0.
- `make check-license-headers`: exit 0.
- `make check-release-inventory`: exit 0.
- `make check-public-docs`: exit 0.
- No numeric test run was required because only documentation was added.
- The two new documents pass explicit whitespace and conflict-marker checks.
- Every G01-G13 group is assigned to a slice or an explicit non-implementation
  decision.
- Every slice dependency exists and the dependency graph is acyclic.
- Every slice has target and 6.99 regression cases in the plan.
- Defaults are not mixed with commands or formats.
- Polar dump is separate from polar-save columns.
- OSMAP is not marked resolved.

## Protected data

- 6.99 sources: 48/48 unchanged.
- 6.996 sources: 49/49 unchanged.
- `fortran-bin/xfoil`:
  `f5779d6954d0b8fdc152e0e0633f18732e00b7b69228d561d629bd524bba598c`.
- `fortran-bin/xfoil-6.996`:
  `000b0468d9a7f4fce42d6fac24bd14147d7d326c8a9035909c8e9f98d180f50e`.
- No Golden, reference, tolerance, C++ source, Fortran source, test or product
  version change.
- Product version remains `1.0.0`; banner remains `XFOIL.next`.

## Git status and publication hold

- Only the two U6 documents are untracked:
  - `docs/xfoil-6.996/06-functional-integration-plan.md`
  - `docs/xfoil-6.996/06-functional-integration-plan_report.md`
- Staged: 0.
- No commit and no push.
- HEAD `910a507a5a54a48cbb0e4581826bd31d499df1b3`; `origin/main`
  `58bca7d17f847e1aef6b1c8d3d4e167f40f69945`; ahead/behind `0 10`; tag
  `v1.0.0` unchanged.
- Publication hold remains in force: no push, force-push, fetch, tag or release
  before cumulative XFOIL.next 1.1.0 acceptance.

## Decision

Status: PASS

`X6996-U6: PASS`

## Exact next step

`X6996-U6-R1-Focused-Functional-Plan-Acceptance`.

After that focused acceptance, the allowed local commit is:

`Plan XFOIL 6.996 functional integration`

No push follows.

## Focused independent acceptance — X6996-U6-R1

Decision: PASS

- Slices: `S01`–`S22` are each defined exactly once, with no missing or
  duplicate top-level IDs. `S02`, `S05` and `S17` are explicitly staged
  internally as `S02a`/`S02b`, `S05a`/`S05b` and `S17a`/`S17b` so independent
  spline/solver, geometry and session effects are not mixed in one commit.
- Slice contracts: every slice has scope, Fortran routines or source files,
  C++ mapping or an explicit pre-analysis mapping task, dependencies, risk,
  concrete oracle cases, 6.99 regression cases, explicit non-goals, an
  acceptance-matrix row and a commit boundary.
- G01–G13: every group is assigned. G01 is split across capacity (`S01`),
  polar element state (`S10`) and `NIMP-01`; G02–G11 map to `S02`–`S20`;
  G12 maps to `S21` plus the `S18` `xtcam` text portion; G13 maps to `S22`
  and remains diagnostic until `G-OSMAP`.
- NIMP decisions: `NIMP-01`–`NIMP-03` each name the affected upstream scope,
  reason, shared state/producer and the condition under which the decision can
  be re-evaluated. They do not silently promote plot, SVG/DXF or OSMAP work.
- Dependency graph: all referenced slice IDs exist, no slice depends on itself,
  and the 22-node graph is acyclic. Format and command slices depend on their
  numerical/state producers; defaults (`S07`) depend only on state, QDCALC and
  `G-DEFAULTS`, not on format or session slices.
- Oracle and regression contracts: every slice has at least one concrete U4 or
  planned `H-Sxx` target case and at least one named 6.99 regression case.
  `O-6996-P1` is treated as an already accepted, case-scoped overlay; it is not
  a blanket canonical oracle.
- Open contracts: polar-save column order remains `G-POLAR-SAVE` and is not
  merged with the accepted polar-dump overlay; OSMAP remains unresolved under
  `G-OSMAP`; SVG/DXF remain under `G-EXPORT`; plot-only work remains
  `NIMP-01`; no compiler/toolchain migration is included; product version
  remains `1.0.0`.
- `S01` scope is confirmed as `X6996-S01-Limits-and-State-Capacity`: `IQX`
  370→601, `NPX` 12→20 and directly dependent arrays/capacities/validation
  only. It excludes defaults, NACA point counts, sharp-TE algorithms, QDCALC,
  BL numerics, commands, and all file/polar formats. Its required tests cover
  standard cases, old/new boundary values, controlled rejection, no partial
  mutation, header/container/session capacities, ASan/UBSan, Valgrind and 6.99
  regressions.
- Documentation integrity: only the two U6 documents are present in the
  worktree; staged is 0; no C++, Fortran, test, Golden, reference, tolerance or
  product-version file changed.
- Gates: `git diff --check`, `make check-public-docs`,
  `make check-release-inventory`, `make check-license-headers` and
  `make check-fortran-sha` all exit 0. The two documents pass the explicit
  whitespace/conflict-marker check.
- Git state: HEAD `910a507a5a54a48cbb0e4581826bd31d499df1b3`;
  `origin/main` `58bca7d17f847e1aef6b1c8d3d4e167f40f69945`; ahead/behind
  `0 10`; tag `v1.0.0` unchanged. No commit and no push were made in this
  audit. Publication hold remains in force.
- Allowed local commit:
  `Plan XFOIL 6.996 functional integration`, limited to:
  `docs/xfoil-6.996/06-functional-integration-plan.md` and
  `docs/xfoil-6.996/06-functional-integration-plan_report.md`.
  After the commit, `main` is eleven commits ahead of `origin/main`; no push.
- Next implementation step after the commit is exactly
  `X6996-S01-Limits-and-State-Capacity`.

`X6996-U6-R1: PASS`
