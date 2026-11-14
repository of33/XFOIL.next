# XFOIL.next 1.0.1 maintenance plan

## Scope

XFOIL.next 1.0.1 is a maintenance release on `release/1.0.x`, based on
`v1.0.0`. It removes the project-specific, non-upstream `OFS:` console line
from the OPER and QDES/SPECAL paths. Solver values, files, command semantics,
API and the canonical Fortran sources remain unchanged.

The release deliberately excludes all 1.1.0/S18 backports: CLf/CDp console
changes, xT/cam widths, DXF/BLOU/NACS work, help/PACC changes and 6.996
behaviour.

## Upstream evidence

The external original sources were read only:

| File | SHA-256 |
|---|---|
| `../Xfoil-6.99/src/xoper.f` | `23f0b6354d6c544b2539ee0ad01b9975838da265f59493b026c28326b69f6524` |
| `../Xfoil-6.99/src/xgdes.f` | `513d71d2eea3b69b5fdb3544ff506bdda68b2b51c2edbf3703f8ac268c4bd622` |

At `v1.0.0` the local Fortran is flat under `fortran-src/`. The only local
OFS addition is at `xoper.f:2816` (`333 format(...)` and the
`write(*,333)`); it is absent from the external original. The local `xgdes.f`
difference is a `READR` call change (`READR(2,XRF,ERROR)` to
`READR(1,XRF(1),ERROR)`), not OFS, and is not ported.

## Product changes

- Remove `emitOfsSpecial` and its OPER call site.
- Suppress the QDES/SPECAL `specifiedPointOutput` text emission while keeping
  the event and SPECAL snapshot for chronology and structured results.
- Bump the central product version from `1.0.0` to `1.0.1`.
- Update the CLI and batch banner/metadata goldens and the focused tests.
- Add `unit.OfsRemoval101` and `integration.OfsRemoval101`.
- Add this plan and the companion report.

## Protected inputs

- `fortran-src/`, `fortran-bin/`, `patches/`: unchanged.
- `tests/fortran-reference/`: unchanged. The two stdout parity tests remove
  the known non-upstream OFS block from their reference copy at test time,
  with a counted replacement; the reference files remain byte-identical.
- No Windows golden or tolerance changes.

## Validation

Linux:

```text
make check-fortran-sha
make test
make test CXX=clang++ BUILDROOT=build/clang
make release build/release/xfoil_tests
./build/release/xfoil_tests
make test-sanitize
```

Windows: exactly one focused UCRT64 process, including debug/release build,
version/OFS smoke checks and the focused binary contract. No full Windows
matrix.

## Acceptance

- Full Linux debug, debug-Clang, release and sanitizer suites green.
- One focused Windows process green.
- No protected file changed; no 1.1.0/S18 backport.
- Local maintenance commit `Prepare XFOIL.next 1.0.1 maintenance release`
  and local annotated tag `v1.0.1`; no push, fetch, tag push or release.
