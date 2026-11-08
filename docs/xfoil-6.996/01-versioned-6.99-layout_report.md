# XFOIL 6.996 — U1 versioned XFOIL 6.99 source layout report

**Status: PASS**

This report records the implementation and acceptance evidence for the U1
structural versioning step. Detailed machine-generated logs remain in the
external audit area.

## 1. Current status

PASS — implementation complete. Independent acceptance is the next step
(`X6996-U1-R1`).

## 2. Scope

Move the 48 canonical XFOIL 6.99 source files and `SHA256SUMS` from
`fortran-src/` into `fortran-src/xfoil-6.99/`, update all path consumers, and
prove byte identity plus unchanged numerical contracts.

Out of scope: editing 6.99 source bytes, moving or editing
`fortran-src/xfoil-6.996/`, changing `fortran-bin/xfoil`, changing C++
product code, Golden data or reference data.

## 3. Changed paths

Summary:

| Category | Count |
|---|---:|
| XFOIL 6.99 source files moved | 48 |
| Integrity inventory moved and path-rewritten | 1 (`SHA256SUMS`) |
| Path consumers and current-contract documents updated | 34 |
| New U1 plan/report documents | 2 |

Moved set (old `fortran-src/<name>` → `fortran-src/xfoil-6.99/<name>`):

```text
BLPAR.INC   CIRCLE.INC  PINDEX.INC  PPLOT.INC   PXPLOT.INC
README.md   XBL.INC     XDES.INC    XFOIL.INC   aread.f
blplot.f    blu.f       dplot.f     frplot.f    getarg.f
gui.f       iopol.f     modify.f    naca.f      ntcalc.f
osmap.dat   osmap.f     osmapDP.dat osmap_DP.f  plutil.f
pntops.f    polfit.f    polplt.f    pplot.f     profil.f
pxplot.f    sort.f      spline.f    userio.f    xbl.f
xblsys.f    xfoil.f     xgdes.f     xgeom.f     xmdes.f
xoper.f     xpanel.f    xplots.f    xpol.f      xqdes.f
xsolve.f    xtcam.f     xutils.f
```

Plus `fortran-src/SHA256SUMS` → `fortran-src/xfoil-6.99/SHA256SUMS`.

Updated current-contract paths:

```text
CONTRIBUTING.md
NOTICE
README.md
Makefile
.gitattributes
fortran-bin/README.txt
docs/fortran-reference.md
docs/numerical-validation.md
docs/xfoil-6.996/README.md
sample/README.md
tests/core/InverseTests.cpp
tests/core/IoTests.cpp
tests/data/core-polar-io-reference/README.md
tests/fortran-reference/README.md
tools/check-osmap-selection.sh
tools/check-public-docs.py
tools/check-release-inventory.py
tools/fortran-provenance.json
tools/build-blf-harnesses.sh
tools/build-boundary-layer-harnesses.sh
tools/build-c11a-state-harnesses.sh
tools/build-cpi-harnesses.sh
tools/build-fmom-vels-harnesses.sh
tools/build-geometry-design-harnesses.sh
tools/build-geometry-harnesses.sh
tools/build-intx-half-harnesses.sh
tools/build-inverse-design-harnesses.sh
tools/build-inviscid-harnesses.sh
tools/build-math-harnesses.sh
tools/build-paneling-harnesses.sh
tools/build-polar-io-harnesses.sh
tools/build-spec-failure-harnesses.sh
tools/build-viscous-harnesses.sh
tools/build-xycm-reference-harnesses.sh
```

New documents:

```text
docs/xfoil-6.996/01-versioned-6.99-layout.md
docs/xfoil-6.996/01-versioned-6.99-layout_report.md
```

Not changed:

```text
fortran-src/xfoil-6.996/**          (49 files, byte-identical)
fortran-bin/xfoil                   (protected binary)
fortran-bin/xfoil-6.996             (protected binary)
tests/golden/**                     (Golden data)
tests/fortran-reference/cases/**    (reference data)
src/** and include/**               (C++ product code)
```

Historical U0 inventory documents and the protected
`fortran-src/xfoil-6.99/README.md` were not rewritten. Comment-only
provenance references in C++ and harness sources were left historical.

## 4. Commands executed

- Pre-move inventory from `tools/fortran-provenance.json`.
- `git mv` for the 48 manifest files and `SHA256SUMS`.
- Path-column rewrite of the moved `SHA256SUMS`.
- Manifest `xfoil-6.99` root update only.
- Consumer updates listed above.
- Integrity and gate runs:
  ```sh
  make check-dependencies
  make check-fortran-sha
  make check-release-inventory
  make check-license-headers
  make check-public-docs
  make core-link-check
  make session-link-check
  python3 tools/check-public-language.py
  ```
- Test and reference runs:
  ```sh
  make test-core
  make test-unit
  make test-integration
  make test-regression
  make test-batch
  make test
  make test-reference
  make test-reference-determinism
  make test-harnesses
  ```
- Post-move inventory comparison and `sha256sum --check`.
- `git diff --check` on the working tree and on the staged rename set.
- A mixed `git reset` (index only, no history change) returned the index to
  `HEAD` so that the step ends with zero staged paths.

## 5. Measurements

| Measurement | Result |
|---|---|
| Manifest 6.99 files | 48 |
| Moved source files | 48/48 |
| Pre/post path set | identical |
| Pre/post mode, size, SHA-256 | 48/48 identical, 0 mismatches |
| XFOIL 6.99 bytes (excluding `SHA256SUMS`) | 3,525,376 |
| `SHA256SUMS` entries | 48 |
| `SHA256SUMS` hash multiset before/after | identical |
| `sha256sum --check` | 48/48 OK |
| XFOIL 6.996 tree | 49 files, 1,154,891 bytes, byte-identical |
| C++ core | 414/414 |
| C++ unit | 1668/1668 |
| C++ integration | 245/245 |
| C++ regression | 5/5 |
| C++ batch | 47/47 |
| C++ full suite | 1968/1968 |
| Fortran reference | 236/236, 0 failures |
| Reference determinism | 236/236 |
| Harness families | 16/16, 0 failed |

## 6. Protected data

| Artifact | Expected SHA-256 | Observed |
|---|---|---|
| `fortran-bin/xfoil` | `f5779d6954d0b8fdc152e0e0633f18732e00b7b69228d561d629bd524bba598c` | unchanged |
| `fortran-bin/xfoil-6.996` | `000b0468d9a7f4fce42d6fac24bd14147d7d326c8a9035909c8e9f98d180f50e` | unchanged |
| `fortran-src/xfoil-6.996/Makefile_gfortran` | `ece560720547fd0ce718fa33b6bacab96b43a0fa8ee80fe21f13344cfadbfc92` | unchanged |

The moved `SHA256SUMS` keeps all 48 hashes; only the path column changes.
The 6.996 tree comparison (relative path, mode, size, SHA-256) is identical
before and after.

## 7. Gates and tests

All gates passed:

- `check-dependencies` (self-test plus core, session, CLI and full scopes).
- `check-fortran-sha`.
- `check-release-inventory`.
- `check-license-headers` (self-test 38/38; real tree 6.99 48 files,
  23 notice-bearing; 6.996 49 files, 23 notice-bearing).
- `check-public-docs` (17 documents).
- `check-public-language`.
- `core-link-check` and `session-link-check`.

All C++, reference, determinism and harness results are listed in section 5.
No Golden or reference expectation was regenerated or changed.

## 8. Git status

- HEAD `8fef06555e1bb664eeccd28107ecb0456ce4cb9b` (unchanged).
- `origin/main` `58bca7d17f847e1aef6b1c8d3d4e167f40f69945`; ahead/behind `0 3`.
- Staged: **0**.
- Working tree after the index reset: 49 deleted old paths, 34 modified
  current-contract paths, and 51 untracked files (48 moved sources,
  `fortran-src/xfoil-6.99/SHA256SUMS`, and 2 U1 documents).
- No commit, no push, no tag, no release, no fetch, no history rewrite.
- `git diff --check` is clean.

## 9. Open points

- None for U1.
- Historical path statements inside the protected 6.99 `README.md` and
  comment-only provenance references intentionally remain unchanged.
- Independent acceptance is pending.

## 10. Final decision

`X6996-U1-Versioned-XFOIL-6.99-Source-Layout: PASS`

The 48 canonical XFOIL 6.99 files and their integrity inventory now live in
`fortran-src/xfoil-6.99/`, are byte- and mode-identical to the pre-move set,
and all integrity, gate, C++, reference, determinism and harness contracts
remain green.

## 11. Exact next allowed step

`X6996-U1-R1` independent acceptance.

## Independent acceptance — X6996-U1-R1

Decision: PASS

### Audited baseline

- HEAD before the acceptance report append:
  `8fef06555e1bb664eeccd28107ecb0456ce4cb9b`.
- `origin/main`: `58bca7d17f847e1aef6b1c8d3d4e167f40f69945`; local `main`
  is ahead by 3.
- Staged: 0; tag `v1.0.0` unchanged; no other refs changed.
- Read-only fingerprint: 2,870 entries (relative path, type, mode, size,
  SHA-256, symlink target), excluding `.git_custom/`, `build/` and
  `docs/Logo/`. Before/after comparison differs only in this report.

### Target structure

- `fortran-src/` contains only `xfoil-6.99/` and `xfoil-6.996/`; no files
  remain at the top level.
- `fortran-src/xfoil-6.99/`: 49 regular files (48 protected sources plus
  `SHA256SUMS`), 0 symlinks.
- `fortran-src/xfoil-6.996/`: 49 regular files, 0 symlinks.

### Move identity

- 48/48 manifest paths were moved; every old path is absent and every new
  path is a regular file.
- 48/48 sizes, modes and SHA-256 values match both `HEAD` and the U1
  pre-move inventory.
- 48/48 notice assignments are unchanged; 0 copies, 0 missing and 0
  additional 6.99 sources.
- The protected `README.md` moved byte-identically; its internal historical
  path text was not edited.

### SHA256SUMS migration

- Old file SHA-256: `38c6503d93ac14ff88a038bf3ef0e7b116f315615f075d37c7bc59365ec08410`.
- New file SHA-256: `6e6882e37ac144d96c6cd7ab6a5dd2bfe335fc3e79b853bccbab07aa2e0b0afd`.
- 48 entries before and after; the hash column and its order are identical.
- Only the path column changed; every path is under
  `fortran-src/xfoil-6.99/`; no 6.996 reference; no entry added or removed.
- `sha256sum --check fortran-src/xfoil-6.99/SHA256SUMS`: 48/48 OK.

### Provenance manifest

- `schema_version: 1` is unchanged; version IDs and roots are unique.
- XFOIL 6.99: only `root` changed from `fortran-src` to
  `fortran-src/xfoil-6.99`. All 48 relative paths, SHA-256 values,
  `notice_expected` values, the `SHA256SUMS` metadata path and the notice
  pattern are unchanged.
- XFOIL 6.996: the entire block is semantically unchanged (49 files, root
  `fortran-src/xfoil-6.996`, empty metadata list).
- No new global count assumption and no duplicate file assignment.

### Path consumers

- 34 current-contract files were updated; a path-token reverse comparison
  against `HEAD` is exact for 33 of them.
- The single non-token difference is `docs/xfoil-6.996/README.md`, whose
  layout block now shows the achieved `xfoil-6.99/` layout, including
  `README.md` inside it.
- Active 6.99 consumers use `fortran-src/xfoil-6.99/`; active 6.996
  references remain `fortran-src/xfoil-6.996/`.
- No double path `fortran-src/xfoil-6.99/xfoil-6.996` exists.
- No harness builder or build script references 6.996 sources;
  `fortran-bin/xfoil` remains the reference binary.
- The external U1 raw inventory and independent grep were used to classify
  all path consumers.

### Obsolete path search

- Old top-level include references: 7 hits, all comment-only in C++ headers.
- Old top-level `.f` references: C++ and harness comments, gate self-test
  fixtures in `check-license-headers.py` and `check-public-language.py`, and
  two `extract-fortran-routines.py` comments; no active source or build use.
- Old `fortran-src/SHA256SUMS` references: 8 historical U0 report hits and
  one gate self-test fixture.
- `fortran-src/osmap.dat`, `fortran-src/osmapDP.dat` and
  `fortran-src/README.md`: no hits.
- No active old top-level 6.99 path remains in the Makefile, tools, tests or
  workflows.

### Build and tool changes

- Every modified build, tool or test file differs from `HEAD` only by the
  `fortran-src/` to `fortran-src/xfoil-6.99/` path token, except the intended
  layout documentation block.
- Compilers, flags, source lists, test counts, harness counts, warning
  treatment and reference normalization are unchanged.
- No new 6.996 build logic and no change to the `:00.bl` to `100.bl` special
  case.

### Gates and tests

- `check-dependencies`, `check-fortran-sha`, `check-release-inventory`,
  `check-license-headers`, `check-public-docs`, `core-link-check` and
  `session-link-check`: all exit 0.
- License gate: self-test 38/38; 6.99 has 48 files and 23 notices; 6.996 has
  49 files and 23 notices.
- `check-public-language.py --self-test` and the normal run: exit 0; still
  not a Make or CI gate.
- C++ suites: core 414, unit 1668, integration 245, regression 5, batch 47,
  full 1968.
- Reference 236/236; determinism 236/236; harnesses 16/16.
- Harness logs use `fortran-src/xfoil-6.99/` and contain no old top-level
  source or 6.996 source reference.

### Protected data

- `fortran-src/xfoil-6.996/`: 49 files, 1,154,891 bytes, 49x mode 0644,
  49/49 manifest hashes correct, unchanged versus `HEAD`.
- `fortran-bin/xfoil`:
  `f5779d6954d0b8fdc152e0e0633f18732e00b7b69228d561d629bd524bba598c`.
- `fortran-bin/xfoil-6.996`:
  `000b0468d9a7f4fce42d6fac24bd14147d7d326c8a9035909c8e9f98d180f50e`.
- `fortran-bin/xfoil-6.99` is absent; no symlinks exist under
  `fortran-bin/`.
- No Golden, reference, tolerance, Windows-Golden, product-version or banner
  change.

### Git status before report append

- HEAD `8fef06555e1bb664eeccd28107ecb0456ce4cb9b`; `origin/main`
  `58bca7d17f847e1aef6b1c8d3d4e167f40f69945`; ahead/behind `0 3`.
- Staged: 0.
- Working tree: 49 deleted old paths, 34 modified current-contract files and
  51 untracked files (48 moved sources, `SHA256SUMS` and 2 U1 documents).
- `git diff --check` is clean.

### Publication hold and next commit

- No push, force-push, fetch, tag, release or history rewrite. All local
  commits remain unpushed until cumulative XFOIL.next 1.1.0 acceptance.
- Exact next allowed local commit: one path-limited U1 commit containing only
  the U1 implementation changes and this report append, with the message
  `Version XFOIL 6.99 Fortran source layout`.
