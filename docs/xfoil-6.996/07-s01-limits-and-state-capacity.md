# S01 - XFOIL 6.996 limits and state capacity

Status: implemented; focused acceptance `X6996-S01-R1` pending.

## Purpose

Apply only the XFOIL 6.996 capacity change to XFOIL.next:

- `IQX` 370 -> 601;
- `NPX` 12 -> 20;
- the workspace, container and session capacities derived from `IQX`.

No other 6.996 behavior is introduced by this slice.

## Scope

- Raise the canonical C++ dimensions and all central derived capacities.
- Keep the existing XFOIL 6.99 panel, state and polar behavior for values
  that were valid before the change.
- Accept values above the old limits and below the new limits.
- Reject values above the new limits in a controlled way without partial
  mutation.
- Check affected header, container and session capacities.
- Keep all existing 6.99 reference and Golden files unchanged.

## Explicit non-goals

S01 does not:

- change the NACA point count (`NSIDE` remains 123, buffer 245);
- introduce 6.996 NACA geometry;
- implement `NPAN=240`, `CTERAT=0.3` or other new defaults;
- implement `NORM`, `LEFIND`, `CANG`, sharp-TE, `QDCALC` or BL numerics;
- add BL, polar, command or file-format fields;
- change the product version or banner;
- change the compiler baseline;
- change Fortran sources, Fortran patches or reference files;
- create a commit before `X6996-S01-R1`.

## Source and oracle baselines

| ID | Baseline | Identity |
|---|---|---|
| `S-699` | XFOIL 6.99 frozen source | 48/48 hashes green |
| `S-6996` | XFOIL 6.996 frozen source | 49/49 hashes green |
| `O-699` | `fortran-bin/xfoil` | SHA-256 `f5779d6954d0b8fdc152e0e0633f18732e00b7b69228d561d629bd524bba598c` |
| `O-6996` | `fortran-bin/xfoil-6.996` | SHA-256 `000b0468d9a7f4fce42d6fac24bd14147d7d326c8a9035909c8e9f98d180f50e` |
| `P-6996-P1` | Patched 6.996 oracle | SHA-256 `031d667f3990c326b5b90d448fbdd10327b127a4e56d249fd6fcec71b55a69ce` |

`O-699` remains the canonical reference for the existing 6.99 regression and
Golden corpus. S01 does not replace any 6.99 reference with 6.996 output.

## Canonical capacity mapping

| Fortran symbol / relation | 6.99 | 6.996 / S01 | C++ representation |
|---|---:|---:|---|
| `IQX` | 370 | 601 | `xfoil::limits::surfaceNodeDimension`, `xfoil::core::surfaceSystemCapacity` |
| `IQX2 = IQX/2` | 185 | 300 | `halfSurfaceNodeDimension` |
| `IWX = IQX/8+2` | 48 | 77 | `wakeNodeDimension`, `core::wakeCapacity` |
| `IBX = 4*IQX` | 1480 | 2404 | `bufferNodeDimension`, `core::bufferCapacity` |
| `IZX = IQX+IWX` | 418 | 678 | `panelWakeNodeDimension`, `core::panelAndWakeCapacity` |
| `IVX = IQX/2+IWX+50` | 283 | 427 | `boundaryLayerNodeDimension`, `core::boundaryLayerCapacity` |
| `NPX` | 12 | 20 | `core::polarSetCapacity`, `polar::maxPolarSlots` |
| `NTX = 2*IBX` | 2960 | 4808 | `core::camberTableCapacity`, `geometry_design::kNtx`, `io::kAddedCamberPointLimit` |
| `IBX` storage | 1480 | 2404 | `bufferNodeStorage`, `io::kGetvovPointLimit` |
| surface storage | 371 | 602 | `surfaceNodeStorage`, `mixedInverseSystemStorage` |
| panel/wake storage | 419 | 679 | `panelWakeNodeStorage` |
| wake reserve | 50 | 79 | `wakeNodeStorage` |
| BL reserve | 302 | 446 | `boundaryLayerNodeStorage` |
| camber storage | 1481 | 2405 | `camberNodeStorage` |
| `5*IBX+1` | 7401 | 12021 | `snewNodeStorage` |
| `6*IQX+1` | 2221 | 3607 | `workNodeStorage` |
| overlay `2*IQX` | 740 | 1202 | `overlayCoordinateCapacity`, `io::kGetvovPointLimit` consumers |
| GETPAN/PPAR `IQX-6` | 364 | 595 | `paneling::kMaxPanelingPanelCount`, `limits::getpanPanelLimit` |
| ABCOPY `IQX-5` | 365 | 596 | `paneling::kMaxAbcopyNodeCount`, `limits::abcopyPanelLimit` |
| PANGEN `IQX-1` | 369 | 600 | `paneling::kMaxPanelNodeCount`, `limits::pangenPanelLimit` |
| PANGEN work vectors | 370 | 601 | `Pangen.cpp` `kPanelCapacity` |
| mixed inverse `IQX` / max `N` | 370 / 365 | 601 / 596 | `inverse::kMixedIqx`, `inverse::kMixedMaxN` |
| MDES GETVOV | 370 | 601 | `io::kGetvovPointLimit` |
| MSES max pairs `2*IQX+1` | 741 | 1203 | `MsesMultielementCodec.cpp` |
| export loop `IQX` | 370 | 601 | `ExportFormats.cpp` `kIqx` |
| NACA `NSIDE` | 123 | 123 | retained 6.99 contract in `Naca.h`/`Naca.cpp` |

The NACA row is intentionally unchanged. S04 owns the 6.996 NACA point-count
change. Deriving `nacaSideLimit` blindly from the enlarged `IQX` would create
the 6.996 200/399 NACA contract as a side effect and is therefore forbidden.

## Implementation decisions

- Central dimensions are defined once in `include/xfoil/core/Limits.h` and
  mirrored by the semantic/storage constants in `include/xfoil/Limits.h`.
- Component constants are derived from the central values instead of repeating
  numeric literals:
  - paneling bounds from `surfaceSystemCapacity`;
  - mixed-inverse bounds from `surfaceSystemCapacity`;
  - MDES/Mses/export capacities from `surfaceSystemCapacity`;
  - camber/overlay/geometry design capacities from `bufferCapacity`.
- Existing reject-or-clamp contracts stay in place:
  - direct capacity APIs reject above the new bound;
  - GETPAN/PPAR keeps its documented clamp at `IQX-6 = 595`;
  - rejection paths keep the previous panel/state unchanged.
- `INPC` accepts more than the old 740-point block. The complete
  `2*IQX = 1202` path remains gated by the S02 spline `NMAX` raise; S01 tests
  the current spline boundary (1000 points) with a terminating blank line.
- The historical 6.99 GDES over-limit unit inputs are handled in the test
  harness without changing any reference file:
  - the old overflow text token `740` is compared as the new `1202`;
  - the old 399-point EXEC case is verified to be accepted instead of
    reproducing the obsolete 365-node capacity message.

## Changed areas

- Limits and capacities:
  - `include/xfoil/Limits.h`
  - `include/xfoil/core/Limits.h`
  - `include/xfoil/core/polar/PolarTypes.h`
  - `include/xfoil/core/paneling/PanelingTypes.h`
  - `src/core/paneling/Pangen.cpp`
  - `include/xfoil/core/inverse/MixedInverseKernels.h`
  - `include/xfoil/io/MdesInverseFileCodec.h`
  - `src/io/MsesMultielementCodec.cpp`
  - `src/io/ExportFormats.cpp`
  - `include/xfoil/core/geometry_design/GeometryDesignOps.h`
  - `include/xfoil/core/geometry_design/CamberSession.h`
  - `include/xfoil/io/AddedCamberCodec.h`
  - `include/xfoil/session/SessionState.h`
- Retained 6.99 NACA contract:
  - `include/xfoil/core/geometry/Naca.h`
  - `src/core/geometry_core/Naca.cpp`
- Focused tests:
  - `tests/core/FoundationTests.cpp`
  - `tests/core/CoreLinkCheck.cpp`
  - `tests/core/PanelingTests.cpp`
  - `tests/core/GeometryDesignTests.cpp`
  - `tests/core/InverseTests.cpp`
  - `tests/core/PolarTests.cpp`
  - `tests/unit/TypesAndLimitsTests.cpp`
  - `tests/unit/DatFileReaderTests.cpp`
  - `tests/unit/MdesFileCodecTests.cpp`
  - `tests/unit/MsesMultielementCodecTests.cpp`
  - `tests/unit/CoreGdesCambCapacityTests.cpp`
  - `tests/unit/CoreGdesExecAtomicityTests.cpp`
  - `tests/unit/CoreGdesMenuHandlerTests.cpp`
  - `tests/unit/CoreGeometryHandlerTests.cpp`
  - `tests/unit/SessionPolarOpsTests.cpp`
  - `tests/integration/PanelingCommandsTests.cpp`

## Focused test contract

| ID | Contract |
|---|---|
| `H-S01-IQX-580/600/601` | Values above the old boundary and inside the new capacity are accepted where the owning operation allows them. |
| `H-S01-IQX-602` | Above the new PANGEN upper bound the API/CLI rejects or clamps, without partial mutation. |
| `H-S01-NPX-12/13/19/20/21` | 12/13/19/20 polar slots are accepted; the next append at 20 is rejected and leaves the slot count unchanged. |
| `H-S01-NO-PARTIAL-MUTATION` | ABCOPY, PCOPY, PANGEN, GDES EXEC and polar append rejections preserve the previous state. |
| `H-S01-CONTAINERS` | Derived sizes in `Limits.h`, `CamberSession`, Mdes/Mses codecs and export loops match the new dimensions. |
| `H-S01-SESSION-CAPACITY` | Session polar storage and panel/state capacities reflect `IQX=601` and `NPX=20`. |

## Verification

Required and completed for the implementation:

- GCC 13.3 Debug: `make test` -> 1970/1970 tests pass.
- GCC 13.3 Release: `build/release/xfoil_tests` -> 1970/1970 tests pass.
- Clang 18.1.3 Debug: `make BUILDROOT=build/clang CXX=clang++ test` ->
  1970/1970 tests pass.
- Clang 18.1.3 Release: `build/clang/release/xfoil_tests` ->
  1970/1970 tests pass.
- Core suite: `make test-core` -> 415/415 tests pass.
- ASan/UBSan: `make test-sanitize` -> 1970/1970 tests pass.
- Valgrind: `make test-valgrind` -> representative filters and both link
  checks pass without findings.
- XFOIL 6.99 references: 236/236 verify pass.
- Reference determinism: 236/236 pass.
- Harnesses: 16 families, 0 failures; all reference data unchanged.
- Guards: `check-dependencies`, `check-fortran-sha`,
  `check-release-inventory`, `check-license-headers`, `check-public-docs`,
  `core-link-check`, `session-link-check` pass.

## Acceptance checklist

- [x] `IQX = 601` and `NPX = 20` are visible in the canonical headers.
- [x] Derived C++ capacities and bounds are consistent.
- [x] Values above the old limits and inside the new limits are accepted.
- [x] Values above the new PANGEN/ABCOPY/polar limits are rejected or clamped
      with no partial mutation.
- [x] NACA remains at the 6.99 123/245 contract.
- [x] No Fortran source, patch, reference or Golden file changed.
- [x] Linux GCC/Clang Debug/Release, sanitizers, Valgrind, references and
      harnesses pass.
- [ ] `X6996-S01-R1` acceptance recorded in the report.
- [x] Windows validation recorded in the report.
- [ ] One local commit after acceptance.

## Git policy

S01 is one local commit after `X6996-S01-R1`:

```text
Integrate XFOIL 6.996 limits and state capacities
```

No push, tag, release or history rewrite is part of S01.
