# XFOIL.next 1.0.1 maintenance plan

## Version decision

The owner keeps the release identity `XFOIL.next 1.0.1`. No 1.0.2 release is
created. The already published tag `v1.0.1` is moved to the
harness-reference-corrected commit only after the full correction is accepted
and only with the explicit owner authorization recorded with this maintenance
work.

## Scope and chronology

The first 1.0.1 maintenance commit removed the project-specific, non-upstream
`OFS:` console output from the C++ product and updated the product version,
tests, goldens and documentation. That commit did not restore the archived
Fortran sources.

A read-only provenance audit then found that `fortran-src/xoper.f` and
`fortran-src/xgdes.f` still contained local modifications:

- `xoper.f`: the local `OF Special` block with label 333 and `write(*,333)`;
- `xgdes.f`: `CALL READR(1,XRF(1),ERROR)` instead of
  `CALL READR(2,XRF,ERROR)`.

The audit also found that current provenance documents incorrectly implied
that these files were byte-identical to upstream and that the historical
binary had been built from the restored source tree.

The owner decided to remain at 1.0.1 and authorized a source-correction commit
on `release/1.0.x`, followed by a new normal branch push and a separately
authorized replacement of the annotated tag `v1.0.1`. No branch force-push is
authorized. `main`, `v1.0.0` and the 1.1.0 preparation are outside this task.

The first GitHub Actions run on the source-correction commit showed 9 of 16
source-based harness families failing. Their references still contained the
historical local OFS blocks. The owner authorized a second, narrowly scoped
correction commit that removes only those blocks from the source-harness
references, followed by the same normal branch push and tag-replacement
procedure.

## Upstream originals

The external XFOIL 6.99 originals are read-only reference inputs:

| File | SHA-256 |
|---|---|
| external `xoper.f` | `23f0b6354d6c544b2539ee0ad01b9975838da265f59493b026c28326b69f6524` |
| external `xgdes.f` | `513d71d2eea3b69b5fdb3544ff506bdda68b2b51c2edbf3703f8ac268c4bd622` |

## Corrected repository state

- `fortran-src/xoper.f` is byte-identical to the external original:
  `23f0b6354d6c544b2539ee0ad01b9975838da265f59493b026c28326b69f6524`.
- `fortran-src/xgdes.f` is byte-identical to the external original:
  `513d71d2eea3b69b5fdb3544ff506bdda68b2b51c2edbf3703f8ac268c4bd622`.
- `fortran-src/SHA256SUMS` pins exactly these two original hashes; no other
  source entry changes.
- The local OFS block and the `READR` adjustment are absent from the archived
  sources.
- Product C++, product test logic, product goldens, canonical
  `tests/fortran-reference/**` data and the historical binary are unchanged.
- The source-based harness references were corrected separately: 21 reference
  files in 9 harness families lost only the historical local OFS blocks.

## Historical oracle binary

`fortran-bin/xfoil` remains the historical 1.0.0 reference oracle. It was
built from the earlier local source state and still contains the local OFS
output. It is not rebuilt or replaced in this correction. Its existing
reference outputs remain historical compatibility records. Its SHA-256 stays
`f5779d6954d0b8fdc152e0e0633f18732e00b7b69228d561d629bd524bba598c`.

The archived upstream sources and the historical oracle are therefore
documented separately. `fortran-bin/SHA256SUMS` and
`tests/fortran-reference/` remain unchanged.

## Source-harness reference correction

The 1.0.1 source correction made the archived sources original, but the
source-based harness references still contained the historical local OFS
blocks. The GitHub Actions run on the corrected commit showed 9 of 16 harness
families failing for exactly this reason.

The correction removes only the historical OFS blocks:

- 21 reference files in 9 harness families;
- 106 OFS blocks, 212 lines;
- every corrected reference is byte-identical to the actual output of the
  corresponding harness built from the restored sources;
- no numeric value, other text line or tolerance changed;
- historical binary references (`tests/fortran-reference/**`) and
  `fortran-bin/xfoil` are unchanged.

The detailed per-reference hashes are recorded in the companion report.

## Validation

```text
git diff --check
make check-fortran-sha
make check-license-headers
make check-dependencies
make check-release-inventory
make check-public-docs
make test
make release
build/release/xfoil_tests
make test-reference
make test-reference-determinism
make test-harnesses
```

The product version remains 1.0.1. No historical-binary reference migration
and no binary rebuild are performed. Source-harness references are corrected
only for the removed OFS blocks. Because no product, build or Windows test
file changes, the already accepted focused Windows 1.0.1 run remains valid;
no additional Windows process is started.

## Acceptance markers

```text
XFOILNEXT-1.0.1-MAINTENANCE: PASS
XFOILNEXT-1.0.1-SOURCE-CORRECTION: PASS
XFOILNEXT-1.0.1-HARNESS-REFERENCE-CORRECTION: PASS
XFOILNEXT-1.0.1-R1: PASS
```

After acceptance, the harness-reference correction commit is pushed normally
to `release/1.0.x`; the annotated tag `v1.0.1` is then replaced to point at
that commit. No GitHub release is created in this correction step.
