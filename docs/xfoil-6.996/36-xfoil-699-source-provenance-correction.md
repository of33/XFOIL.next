# XFOIL 6.99 source provenance and harness-reference correction

Status: local correction prepared; publication hold until the pushed commit's
GitHub Actions run is green.

This plan defines the main-branch correction for the prepared XFOIL.next 1.1.0
release. It is separate from the completed 1.0.1 maintenance line.

## Starting point

```text
Correction start HEAD: 1fbd0d5700db5dc67ecba03c2bfd9ae9c40b49e0
Product version:       1.1.0
Completed 1.0.1 line:  f648e37b47d9e8db9abf43ec78568df1544c8b6f
v1.0.1 target:         f648e37b47d9e8db9abf43ec78568df1544c8b6f
```

## Decision

The archived XFOIL 6.99 source tree is restored to the original upstream
state. The historical `fortran-bin/xfoil` binary is not rebuilt or replaced.
Source authority and binary oracle are documented separately.

The source-based harness references are corrected only by removing the
historical local `OFS:` blocks. No runtime normalization, numerical edit,
tolerance change or reference regeneration is introduced.

## Source correction

Original external hashes:

```text
xoper.f  23f0b6354d6c544b2539ee0ad01b9975838da265f59493b026c28326b69f6524
xgdes.f  513d71d2eea3b69b5fdb3544ff506bdda68b2b51c2edbf3703f8ac268c4bd622
```

Only these pre-correction differences are allowed:

- `xoper.f`: the local `OF Special` block with label 333 and `write(*,333)`;
- `xgdes.f`: `CALL READR(1,XRF(1),ERROR)` instead of
  `CALL READR(2,XRF,ERROR)`.

The restored files must be byte-identical to the external originals. The
6.99 source manifest is updated only for these two entries. The 6.996 source
tree and manifest remain unchanged.

## Source and binary separation

- `fortran-src/xfoil-6.99/` is the restored source authority.
- `fortran-bin/xfoil` remains the historical binary behavior oracle for its
  existing binary-based references.
- The binary was built from an earlier local source state and is not an exact
  rebuild of the restored source tree.
- `tests/fortran-reference/**`, `fortran-bin/**`, product code, product goldens
  and tolerances remain unchanged.

## Harness-reference correction

The source-based harness references affected on main are the 21 files in the
nine `core-*` families listed in the report. Each corrected reference differs
from its predecessor only by historical two-line `OFS:` blocks:

```text
OFS blocks removed: 106
Lines removed:      212
```

The new reference bytes must equal the actual output of the corresponding
harness built from the restored sources. The historical binary references
under `tests/data/xfoil-6.996/s09/` are not source-harness references and are
not changed.

## Documentation

The correction adds this plan and its report, updates the current source and
binary provenance contracts, and adds narrow errata notes to historical
reports that described the pre-correction source state. Earlier reports are
not rewritten.

## Validation

Required local gates:

```text
make test-harnesses
make check-fortran-sha
make check-license-headers
make check-dependencies
make check-release-inventory
make check-public-docs
make test
make release
make build/release/xfoil_tests
build/release/xfoil_tests
make test-reference
make test-reference-determinism
make test-sanitize
make test-valgrind
make core-link-check
make session-link-check
```

No new Windows matrix is required because no product, build, Windows-test,
golden or Windows-workflow file changes. The accepted focused Windows binary
contract remains valid.

## Publication hold

No tag, GitHub release, force-push or maintenance-branch change is part of
this correction. After the local commit, `main` is pushed normally. The
GitHub Actions run for that exact commit must be green before any 1.1.0 tag or
release task starts.

Detailed evidence is in
`36-xfoil-699-source-provenance-correction_report.md`.
