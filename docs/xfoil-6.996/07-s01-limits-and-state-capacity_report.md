# S01 - limits and state capacity implementation report

Status: PASS - `X6996-S01-R1` accepted; Linux and Windows evidence complete.

## Starting state

- HEAD before S01: `54045ca794957f28ba77ddaa8e951c471b2e4b01`
  (`Plan XFOIL 6.996 functional integration`).
- `origin/main`: `58bca7d17f847e1aef6b1c8d3d4e167f40f69945`.
- Ahead/behind: `0 11`.
- Tag `v1.0.0` unchanged; no push performed.
- Working tree before S01: clean, 0 staged.
- Frozen sources: 6.99 48/48 and 6.996 49/49 green.
- `O-699` SHA-256:
  `f5779d6954d0b8fdc152e0e0633f18732e00b7b69228d561d629bd524bba598c`.
- `O-6996` SHA-256:
  `000b0468d9a7f4fce42d6fac24bd14147d7d326c8a9035909c8e9f98d180f50e`.
- P1 patched oracle SHA-256:
  `031d667f3990c326b5b90d448fbdd10327b127a4e56d249fd6fcec71b55a69ce`.

## Implementation summary

`IQX` is now 601 and `NPX` is now 20 in the canonical C++ limits. The
dependent capacities are derived centrally:

- `IQX2 = 300`;
- `IWX = 77`;
- `IBX = 2404`;
- `IZX = 678`;
- `IVX = 427`;
- `NTX = 4808`;
- surface/panel/wake/BL storage: 602/679/79/446;
- camber storage: 2405; `SNEW`: 12021; Newton work: 3607;
- overlay: 1202; input capacity: 2404;
- GETPAN/PPAR/ABCOPY/PANGEN: 595/596/600;
- mixed inverse: `IQX=601`, max `N=596`;
- MDES: 601 points; MSES: 1203 pairs per element;
- polar slots and reference slots: 20.

The NACA contract deliberately remains at the 6.99 values:
`NSIDE=123`, buffer 245. No 6.996 NACA behavior was activated.

## Capacity mappings and code points

| Capacity | 6.99 | S01 | Code point |
|---|---:|---:|---|
| `IQX` | 370 | 601 | `include/xfoil/Limits.h`, `include/xfoil/core/Limits.h` |
| `IQX2` | 185 | 300 | `include/xfoil/Limits.h` |
| `IWX` | 48 | 77 | `include/xfoil/Limits.h`, `core/Limits.h` |
| `IBX` | 1480 | 2404 | `include/xfoil/Limits.h`, `core/Limits.h` |
| `IZX` | 418 | 678 | `include/xfoil/Limits.h`, `core/Limits.h` |
| `IVX` | 283 | 427 | `include/xfoil/Limits.h`, `core/Limits.h` |
| `NPX` | 12 | 20 | `include/xfoil/core/Limits.h`, `PolarTypes.h` |
| `NTX` | 2960 | 4808 | `core/Limits.h`, `CamberSession.h`, `AddedCamberCodec.h` |
| GETPAN clamp | 364 | 595 | `PanelingTypes.h`, `PanelParameters.h` |
| ABCOPY | 365 | 596 | `PanelingTypes.h`, `Abcopy.h` |
| PANGEN upper | 369 | 600 | `PanelingTypes.h`, `Pangen.h`, `Pangen.cpp` |
| mixed inverse | 370/365 | 601/596 | `MixedInverseKernels.h` |
| MDES | 370 | 601 | `MdesInverseFileCodec.h` |
| MSES pairs | 741 | 1203 | `MsesMultielementCodec.cpp` |
| export loop | 370 | 601 | `ExportFormats.cpp` |

## Behavioral checks

- Direct PANGEN accepts 370, 371, 596..600 and rejects 601/602.
- ABCOPY accepts 596 and rejects 597 without setting the contour.
- GETPAN/PPAR clamps above 595 to 595 and emits the canonical 1200 message.
- PCOPY rejects above 596 with the four-line message and preserves the panel.
- Mixed inverse accepts the maximum `N=596` (`N+5=601`) and rejects `N=597`.
- MDES GETVOV accepts 601 pairs and truncates the 602nd.
- MSES rejects an element that exceeds 1203 pairs.
- Polar slots append through 20 and reject the 21st append without mutation.
- GDES EXEC accepts 596 buffer points and rejects 597 atomically.
- CAMB INPC accepts 1000 points (above the old 740-point block); the full
  `2*IQX=1202` path is intentionally deferred to S02's spline `NMAX` raise.
- Existing 6.99 NACA output remains 123/245.

## Test results

| Check | Result |
|---|---|
| GCC 13.3 Debug `make test` | 1970/1970 pass |
| GCC 13.3 Release `build/release/xfoil_tests` | 1970/1970 pass |
| Clang 18.1.3 Debug `make BUILDROOT=build/clang CXX=clang++ test` | 1970/1970 pass |
| Clang 18.1.3 Release `build/clang/release/xfoil_tests` | 1970/1970 pass |
| `make test-core` | 415/415 pass |
| `make test-sanitize` (ASan/UBSan) | 1970/1970 pass |
| `make test-valgrind` | representative filters plus core/session link checks pass |
| `make test-reference` | 236/236 pass |
| `make test-reference-determinism` | 236/236 pass |
| `make test-harnesses` | 16 families, 0 failures; reference data unchanged |
| `make check-dependencies` | pass (core/io/session/CLI) |
| `make check-fortran-sha` | 6.99 and 6.996 all hashes pass |
| `make check-release-inventory` | pass |
| `make check-license-headers` | pass |
| `make check-public-docs` | pass after adding the S01 documents |
| `make core-link-check` | pass |
| `make session-link-check` | pass |
| Windows 11 / MSYS2 UCRT64 Debug full suite | 1970/1970 pass |
| Windows 11 / MSYS2 UCRT64 Release build/link | pass (`build/release/xfoil.next.exe`) |
| Windows 11 / MSYS2 UCRT64 Release test suite | not built or executed |

## Deviations and retained boundaries

- S01 does not implement S02 spline `NMAX=2500`; therefore the full 1202-point
  `INPC` loop is not yet exercised end-to-end. The capacity is raised, and the
  focused test documents the current spline limit.
- Historical GDES 6.99 over-limit unit cases are adapted in the test code
  without modifying any reference file. The old 399-point EXEC case is now
  accepted; the old overflow case keeps the reference bytes except for the
  capacity token, which changes from 740 to 1202.
- No 6.99 Golden or reference file was changed.
- Windows gate self-test portability was hardened after a Windows-only
  failure:
  - `tools/check-license-headers.py` writes self-test fixtures as exact LF
    bytes, so Windows text-mode CRLF translation cannot invalidate the
    SHA-256 fixtures;
  - both gate self-tests skip only their symlink scenario when the OS lacks
    the symlink privilege (`WinError 1314`).
- Windows validation is complete for the S01 test scope: Windows 11 /
  MSYS2 UCRT64 `test-unit`, core, integration, regression, batch and release
  are green. Windows symlink-specific self-test cases are skipped only when
  the OS lacks the symlink privilege; the skip is printed explicitly as
  `selftest SKIP` and is not represented as a passed symlink assertion.
- No Windows-specific reference or Golden change was needed.

## Protected data

No changes to:

- `fortran-src/xfoil-6.99/` or `fortran-src/xfoil-6.996/`;
- `fortran-bin/xfoil` or `fortran-bin/xfoil-6.996`;
- `patches/xfoil-6.996/`;
- `tests/fortran-reference/`;
- `tests/golden/`;
- product version or banner.

## Git state

- 43 tracked files modified (41 source/test files plus the two gate tools) and these two documents.
- The single local S01 commit is created after this acceptance; no push, tag
  or release is created.
- Commit message:
  `Integrate XFOIL 6.996 limits and state capacities`.

## R1 checklist

- [x] All required capacity paths inspected and mapped.
- [x] Central capacities and derived bounds updated.
- [x] NACA legacy contract explicitly preserved.
- [x] Focused capacity and atomicity tests added/updated.
- [x] Debug/Release, GCC/Clang, sanitizer, Valgrind, reference and harness
      checks pass.
- [x] `make check-public-docs` passes after adding the S01 documents.
- [x] Record Windows category status and current logs/counts.
- [x] Provide or locate full current Windows logs and test counts.
- [x] Record `X6996-S01-R1` acceptance.
- [ ] Create the single local S01 commit.


## Focused independent acceptance - X6996-S01-R1

Decision: **PASS**

The Windows evidence was initially missing and the audit was recorded as
INCOMPLETE. The complete Windows logs were then supplied in
`.audit-xfoil-next/xfoil-6.996/s01/logs.tgz`; after review the decision is
PASS. The local S01 commit follows in the chronological close-out.

### Verified evidence

- Current worktree scope: 43 tracked files modified plus the two S01
  documents; no forbidden path changed:
  `fortran-src/**`, `fortran-bin/**`, `patches/xfoil-6.996/**`,
  `tests/golden/**`, `tests/fortran-reference/**`, `tests/data/**`.
- Protected binaries unchanged:
  - `fortran-bin/xfoil`:
    `f5779d6954d0b8fdc152e0e0633f18732e00b7b69228d561d629bd524bba598c`;
  - `fortran-bin/xfoil-6.996`:
    `000b0468d9a7f4fce42d6fac24bd14147d7d326c8a9035909c8e9f98d180f50e`.
- Compiled capacity probe against the current headers:
  `IQX=601`, `IQX2=300`, `IWX=77`, `IBX=2404`, `IZX=678`, `IVX=427`,
  `NTX=4808`, `NPX=20`; GETPAN/ABCOPY/PANGEN `595/596/600`; mixed inverse
  `601/596`; MDES `601`; MSES `1203`; polar slots `20`; SNEW/work
  `12021/3607`; overlay/input `1202/2404`; retained NACA side `123`.
- Frozen XFOIL 6.996 declarations agree with these values. Loops and
  validation boundaries map to `IQX-6`, `IQX-5`, `IQX-1`, `N+5 <= IQX`,
  `DO K=1,IQX`, and `XTMP(2*IQX,NEX)`.
- Current Linux full suite: `1970/1970` pass. Composition:
  `1670` unit, `245` integration, `5` regression, `47` batch and
  `3` core-link tests. The rise from 1968 to 1970 is two new unit tests:
  `PanelingPangenIqxBoundaryAccepted` and
  `SessionPolarCapacityBoundaryNpx`; eight existing tests were renamed or
  reworked with no net count change.
- Separate `make test-core`: `415/415` pass.
- GCC 13.3 Debug and Release: `1970/1970` each.
- Clang 18.1.3 Debug and Release: `1970/1970` each.
- ASan/UBSan `make test-sanitize`: `1970/1970` pass.
- Reference and determinism: `236/236` each; harness families `16/16`,
  zero failures, reference data unchanged.
- Valgrind representative suite plus explicitly added boundary filters
  (`PanelingPangenIqxBoundaryAccepted`, `PanelingPangenAboveCapacity`,
  `PanelingAbcopy597AboveLimit`, `SessionPolarCapacityBoundaryNpx`,
  `CoreGdesExecCapacityBoundaryIqx5`,
  `CoreGdesOverOverflowMatchesFortranExceptCapacityToken`) passes with no
  definite leaks; core/session link checks also pass under Valgrind.
- Gates: dependencies, Fortran SHA, release inventory, license headers,
  public docs, core link check and session link check pass.
- `git diff --check` is clean.

### Size and storage audit

- Persistent state stores active lengths and dynamic containers; the S01
  capacity increase does not add fixed capacity arrays to the session
  objects.
- Measured object sizes: `SessionComposition` 6976 bytes,
  `ViscousSessionState` 3424, `DesignSessionState` 888,
  `InverseSessionState` 816, `CamberSession` 360,
  `GeometryDesignBuffer` 304. All audited state types are
  nothrow-move-constructible; no capacity-sized embedded storage was found.
- The largest changed-function stack use is 9944 bytes
  (`handleGeometryDesignCore`), followed by `pangen` at 7448 bytes.
  Existing fixed work arrays remain bounded (for example 1000 doubles in
  the spline helpers and 500 doubles in the leading-edge helper).
- New bulk PANGEN buffers (`NN = 5*(N-1)+1`; 2996 values at `N=600`) and
  `SNEW` capacity 12021 are heap vectors. No unexpectedly large local
  array was introduced by S01.

### Windows close-out evidence

- Source: `.audit-xfoil-next/xfoil-6.996/s01/logs.tgz`.
- Environment: `MINGW64_NT-10.0-26100`, MSYS2, GCC 16.1.0.
- Windows Debug full suite: `1970/1970` PASS.
  The category logs comprise `test-unit` `1670/1670`,
  `test-core` `415/415` (412 unit + 3 core), `test-integration`
  `245/245`, `test-regression` `5/5`, and `test-batch` `47/47`.
- Windows Release build/link: PASS (`build/release/xfoil.next.exe`).
- Windows Release test suite: not built or executed.
- All S01 boundary tests are present and pass on Windows, including
  `PanelingPangenIqxBoundaryAccepted`,
  `PanelingAbcopy597AboveLimit`, and
  `SessionPolarCapacityBoundaryNpx`; the old 365/366/370 names are gone.
- `check-dependencies` and `check-license-headers` pass (419 files).
  Self-test output shows only `WinError 1314` symlink skips; the current
  predicate re-raises every other error.
- No Windows-specific reference or Golden change was needed.

All required evidence is present. The reviewed S01 scope has no functional
defect, and `X6996-S01-R1` is accepted as PASS.
