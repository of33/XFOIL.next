# XFOIL 6.996 — S18 text-only console contract report

Status: PASS — `G-TEXT-OFS` decision recorded. S18 is released; S18
implementation has not started.

## Starting state

- HEAD: `bf019d8d2afcd81b27e5f47ee1aa7552fd9e7d26`
  (`Record X6996-S17b-R1 focused acceptance`).
- Parent: `146ce0849401edd9160d94045d208f4fafdf0b1a`.
- Worktree clean, staged 0, untracked 0, ahead 34 behind 0.
- Tag `v1.0.0` unchanged.
- No G-TEXT-OFS or S18 commit existed at the start.
- No push, fetch, tag or release.

## Gate decision

```text
G-TEXT-OFS: REMOVE-NON-UPSTREAM-OFS-OUTPUT
X6996-G-TEXT-OFS: PASS
```

Decision basis: authoritative project-owner provenance and product decision.

The inviscid `OFS:` output at label 333 is a local, project-owner extension.
It is not part of the original XFOIL contract and has no compatibility status
to preserve. XFOIL.next 1.1.0 follows the original 6.996 behavior without this
additional output.

The decision introduces no legacy, version or opt-in mode. Only the affected
`OFS:` output path may be removed. Tests or goldens may be adjusted only for
that removal. Canonical 6.99 references and `tests/fortran-reference/**`
remain unchanged. Fortran sources, binaries and patches remain unchanged.

No external oracle measurement was invented or required for this decision.

## U6 confirmation

The U6 gate subject is the inviscid `OFS:` text decision. U6 assigns it to
`S18`. The decision therefore releases S18.

U6 names no further gate for S18. Its remaining dependencies are `S06` and
`S09`, both accepted.

S18 contains additional defined work beyond removing the `OFS:` output:

- `CLf`/`CDp` console lines;
- `xtcam` text widths;
- prompts;
- the non-plot `xtcam` text portion of G12.

The S18 contract remains text-only. It changes no numeric solver behavior,
session semantics, file format or default.

## Scope and boundaries

- Gate commit: documentation only.
- S18 implementation: not part of this gate pass.
- S18 commit boundary: one later local implementation commit.
- `G-EXPORT` remains open; S14/SVG/DXF remain blocked.
- No S19/S20/S21/S22 work.
- No product-version, banner or tag change.

## Verification

Documentation gates:

```text
check-fortran-sha:       PASS
check-license-headers:   PASS
check-release-inventory: PASS
check-public-docs:       PASS
git diff --check:        PASS
```

No full Linux suite and no Windows process were run for this
documentation-only gate. Windows processes for S18: 0.

## Next step

Implement U6 slice `S18` under the accepted text-only console contract:
remove the project-specific `OFS:` output and align the remaining defined
text behavior. Keep `G-EXPORT` open and S14/SVG/DXF blocked.
