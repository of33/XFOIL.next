# S03 - geometry normalization and robust LE/bend primitives report

Status: PASS - `X6996-S03` and `X6996-S03-R1` accepted. Decision:
`X6996-S03: PASS`.

## Starting state

- HEAD before S03: `0ccae284d0ff2915110ef5ee971dce37f188b92c`.
- Parent: `afb559c263490a627cc1805bdddae766227badf5`.
- Ahead/behind: `0 14`.
- Tag `v1.0.0` unchanged.
- Working tree before S03: clean, 0 staged.
- Audit data: external only,
  `.audit-xfoil-next/xfoil-6.996/s03/`.

## Fortran diff

Compared source: `fortran-src/xfoil-6.99/xgeom.f` versus
`fortran-src/xfoil-6.996/xgeom.f`, diff in
`.audit-xfoil-next/xfoil-6.996/s03/xgeom.diff`.

Adopted S03 changes:

- `LEFIND`: `IF(N .LT. 5) I = N/2`; first-guess bracket
  `DOTM > 0.0 .AND. DOTP <= 0.0`.
- `XLFIND`: `IF(RESS .EQ. 0.0) GO TO 21` / `SLE = S(I)` fallback.
- `NORM`: signature `NORM(X,XP,Y,YP,S,N,XSHIFT,YSHIFT,SCALE)`; chord vector
  from LE to TE midpoint; shift LE to origin; scale by `1/CHORD`; `XP`/`YP`
  stay pre-scaling.
- `CANG`: `CRS = DX1*DY2-DY1*DX2`; `DOT = DX1*DX2+DY1*DY2`;
  `ANGL = ATAN2(CRS,DOT)*(180/3.1415926)`; doubled-point fallback uses
  `X(I+2)-X(I)`.

Deferred, not changed in S03:

- `GEOPAR` exists in 6.996 but only the `F7.3 -> F8.4` display width differs;
  text ownership remains with S18.
- `SQTE`/`TECIRC`, sharp-TE and new GDES kernels remain S04/S05.

## C++ mapping implemented

| Fortran | Canonical C++ | S03 change |
|---|---|---|
| `LEFIND` | `core::geometry::lefind` | small-N midpoint guard; new `DOTM/DOTP` bracket |
| `LEFIND` | `geometry::findLeadingEdge` | same legacy mirror |
| `XLFIND` | `core::geometry::xlfind` | zero `RESS` limit fallback |
| `XLFIND` | `geometry::findLeftmostPoint` | same legacy mirror |
| `NORM` | `core::geometry::norm` | LE-TE chord vector, LE shift, uniform scale |
| `GEOPAR` | `core::geometry::geopar` | consistency test only; no formula change |
| `CANG` | `core::geometry::cang` | ATAN2 quadrant form |
| `CANG` | `geometry::findMaximumCornerAngle` | same legacy mirror |

Headers were updated to state the 6.996 preconditions and formulas. No new
spline, geometry or tolerance implementation was introduced.

## Focused tests

Added/updated:

- `unit.GeometryNormUsesChordVectorAndShiftsLeToOrigin`
- `unit.GeometryLefindSmallNGuard`
- `unit.GeometryXlfindZeroSecondDerivativeFallsBack`
- `unit.GeometryCangUsesAtan2Quadrant`
- `unit.FindLeadingEdgeSmallNGuardUsesMidpoint`
- `unit.FindLeadingEdgeRejectsTooFewPoints` (now `pointCount = 3`)
- `unit.MaximumCornerAngleUsesAtan2Quadrant`

The new legacy CANG test deliberately uses the existing legacy `kPi`
(`3.141592654`) for its expected value; the canonical core CANG uses the
Fortran literal `3.1415926`, matching the external H-S03 oracle.

## External oracle H-S03

Harness: `.audit-xfoil-next/xfoil-6.996/s03/h_s03_harness.f`.
Build/outputs: `h-s03-699`, `h-s03-6996`, `h-s03-699.out`,
`h-s03-6996.out`, `h-s03-vs.diff`.

Measured 6.99 -> 6.996:

| Quantity | 6.99 | 6.996 |
|---|---:|---:|
| `NORM_X1` | 0.9999999999999999 | 0.9396926207859084 |
| `NORM_Y1` | 0.3639702342662024 | 0.3420201433256688 |
| `NORM_XM` | -4.7259e-16 | -4.4409e-16 |
| `NORM_SM` | 1.116773641855652 | 1.049423950339961 |
| `GEOPAR_CHORD` | 1.064177772475912 | 1.0 |
| `GEOPAR_AREA` | 0.1771580162093260 | 0.1564344650402310 |
| `CANG_IMAX` | 2 | 2 |
| `CANG_AMAX` | 45.00000076761724 | 135.0000023028518 |
| `SMALL_LEFIND_SLE` | 0.8415859839056992 | 0.8415859839056992 |

`LEFIND` on the rotated/translated ellipse is unchanged at
`1.049423950339961`. The NORM and GEOPAR changes prove the chord-vector
normalization; CANG proves quadrant preservation.

## Versioned target expectations

Authorization: `X6996-S03-F1-Minimal-Versioned-Target-Expectation-Updates`.

The canonical 6.99 corpus remains byte-for-byte untouched. New/updated
target evidence is narrowly scoped.

### Inline measured updates

| Test | Old | New | Oracle |
|---|---:|---:|---|
| `unit.PanelingPangenCornerInsertion` `SLE` | 1.0267851352691650 | 1.0267846584320068 | 6.996 LEFIND paneling harness |
| same `x[1]` | 0.9917981624603271 | 0.9898628592491150 | same |
| same `y[1]` | 0.0029480566736311 | 0.0033436061348766 | same |
| `unit.CorePparApplySmallCountN4MatchesFortran` | `87.097` at node 2, `0.1197 0.0600` | `93.403` at node 3, `0.1181 -0.0396` | 6.996 full CLI + PANGEN/CANG stack |
| `unit.QdesMenuRestCommits` | included one LEFIND warning | no LEFIND warning | 6.996 LEFIND bracket |
| `unit.QdesMenuRestTextlessCommit` | included one LEFIND warning | no LEFIND warning | 6.996 LEFIND bracket |
| `unit.QdesRestLefindEventFlag` | `leadingEdgeNotFound == true` | `false` | 6.996 LEFIND bracket |

`unit.GeometryDesignGdesOps2412Sequence` was updated after the normalization
step from the 6.996 GDES operator reference. Old -> new values:

| Field | Old 6.99 | New 6.996 |
|---|---:|---:|
| `chord` after NORM | 1.003820 | 0.9999999 |
| `thick` after NORM | 0.1044804 | 0.1040828 |
| `y[0]` after NORM | -0.08686046 | -0.08652993 |
| `thick` after MOVS | 0.1244885 | 0.1240909 |
| `y[0]` after MOVS | -0.07693861 | -0.07660808 |
| `y[61]` after MOVS | 0.008043844 | 0.008050935 |
| `s[244]` after SPLI | 22.08830 | 22.04722 |
| `xp[0]` after SPLI | -0.03531265 | -0.03524623 |
| `yp[61]` after SPLI | 0.01418035 | 0.01415332 |
| `thick` after CADD | 0.1244885 | 0.1240909 |
| `cambr` after CADD | -0.01598141 | -0.01592057 |
| `y[0]` after CADD | -0.07693861 | -0.07660808 |
| `y[last]` after CADD | -0.09811561 | -0.09778030 |
| `thick` after SYMM | 0.1244731 | 0.1240755 |
| `cambr` after SYMM | 0.3166497e-7 | 0.2607703e-7 |
| `y[0]` after SYMM | -0.07690713 | -0.07657649 |
| `y[last]` after SYMM | -0.09805054 | -0.09771511 |

No tolerance was changed.

### Versioned fixtures added

- `tests/data/core-intx-half-reference/6.996-s03/intx_half_reference.txt`:
  current accepted S03 target stack, 6.996 `xgeom` + 6.996 `spline`, 6.99
  `naca` (S04 not active). `IX I SLE1` changes from
  `0.98247685451257005251` to `0.98247685451875232943`; the affected case
  coordinates follow bit-exactly. The old 6.99 fixture remains canonical.
- `tests/data/core-gdes-menu-reference/6.996-s03/gdes-over-lnorm.segment.txt`:
  6.996 finds the overlay LE and normalizes it. Old 6.99 segment contains two
  `LEFIND:  LE point not found.` warnings and
  `Max thickness = 15.378039`, `Max camber = 7.689020`; the S03 target has no
  warning and reports thickness `0.120001`, camber `0.001189`, x values
  `-0.691`/`-0.038` under the unchanged 6.99 `F7.3` GEOPAR text width
  (`F8.4` remains S18). The raw 6.996 `-0.6912`/`-0.0381` bytes are preserved
  in the external audit fixture.

Oracle reproducibility artifacts:
`.audit-xfoil-next/xfoil-6.996/s03/target-fixtures/` with frozen-source
hashes, harness sources/binaries/outputs, full 6.996 CLI binary and
`SHA256SUMS`.

### Product golden change

Only one byte-level CLI golden line changed:

```diff
-  LE  x,y  =   0.00000  -0.00000  |   Chord =   1.00000
+  LE  x,y  =  -0.00000  -0.00000  |   Chord =   1.00000
```

in `tests/golden/cli/file-norm-reve-deli-quit.stdout.txt`. This is the
verified 6.996 NORM signed-zero direction. No unrelated transcript content was
regenerated; the full 6.996 CLI stdout is retained in the external audit.

## Linux acceptance evidence

Toolchain: GCC 13.3.0, Clang 18.1.3, Valgrind 3.26.0.

| Check | Result |
|---|---|
| GCC 13.3 Debug `make test` | 1982/1982 pass |
| GCC 13.3 Release `build/release/xfoil_tests` | 1982/1982 pass |
| Clang 18.1.3 Debug | 1982/1982 pass |
| Clang 18.1.3 Release | 1982/1982 pass |
| `make test-core` | 422/422 pass |
| `make test-unit` | 1682/1682 pass |
| `make test-integration` | 245/245 pass |
| `make test-regression` | 5/5 pass |
| `make test-batch` | 47/47 pass |
| `make test-sanitize` (ASan/UBSan) | 1982/1982 pass |
| `make test-valgrind` | pass, no definite leaks |
| focused S03 Valgrind (8 geometry tests) | pass, no definite leaks |
| `make test-reference` | 236/236 pass |
| `make test-reference-determinism` | 236/236 pass |
| `make test-harnesses` | 16 families, 0 failures |
| Gates | dependencies, Fortran SHA, release inventory, license, public docs/language, core/session link checks pass |

The canonical unchanged 6.99 reference contracts therefore remain 236/236
and determinism 236/236; harnesses remain 16/16.

## Windows evidence

Source: user-provided Windows 11 / MSYS2 UCRT64 console output for the single
focused S03 process recorded in
`.audit-xfoil-next/xfoil-6.996/s03/windows-s03-commands.txt`:

```bash
make -j4 debug
./build/debug/xfoil_tests unit.GeometryNormUsesChordVectorAndShiftsLeToOrigin
```

Result:

```text
[ RUN      ] unit.GeometryNormUsesChordVectorAndShiftsLeToOrigin
[       OK ] unit.GeometryNormUsesChordVectorAndShiftsLeToOrigin
0 failure(s) in 1 test(s)
```

The focused process covers 6.996 LEFIND, chord-vector NORM, LE-to-origin
shift/scale and GEOPAR consistency. No full Windows matrix was run for S03,
in accordance with the minimal Windows-test policy.

## Protected data and Git state

- No `fortran-src/**`, `fortran-bin/**` or `patches/**` change.
- Canonical 6.99 reference artifacts unchanged.
- Two versioned 6.996 S03 fixtures added.
- One authorized XFOIL.next CLI product-golden line changed.
- No tolerances changed.
- Product version/banner unchanged.
- The local S03 commit is created with this accepted scope; no push, fetch,
  new tag or release.

## S03 checklist

- [x] 6.99/6.996 `xgeom.f` diff analyzed.
- [x] Core and legacy mappings fixed in the pre-analysis.
- [x] `LEFIND`, `XLFIND`, `NORM`, `CANG` implemented; `GEOPAR` consistency checked.
- [x] Focused C++ tests and external H-S03 oracle.
- [x] Versioned S03 target fixtures for Intx and GDES OVER LNORM.
- [x] Minimal authorized product-golden signed-zero update.
- [x] Full Linux, compiler, sanitizer, Valgrind, reference, determinism and harness evidence.
- [x] One focused Windows S03 process.
- [x] Focused R1 acceptance.
- [x] Create the single local S03 commit.

## Focused acceptance - X6996-S03-R1-Focused-Geometry-Acceptance

Decision: **PASS**

The focused review confirms:

- `LEFIND`, `XLFIND`, `NORM` and `CANG` implement the 6.996 `xgeom.f` contracts
  in both the canonical core and the existing legacy mirror;
- `GEOPAR` needed no formula change and now consumes a unit-chord normalized
  contour consistently;
- the external H-S03 oracle and versioned Intx/GDES fixtures derive the new
  expected values from 6.996 sources, not from the C++ implementation;
- no tolerance was changed; the six inline updates, two versioned fixtures and
  the single authorized CLI golden signed-zero line are narrowly scoped;
- the canonical 6.99 Fortran-reference corpus, old Intx fixture, Fortran
  sources/binaries/patches, product version and banner are unchanged;
- Linux Debug/Release GCC/Clang, ASan/UBSan, Valgrind, reference,
  determinism, harness and gate evidence is complete;
- the single required focused Windows S03 process passed `1/1`;
- no S04/S05 sharp-TE, GDES, default, command or text-format work was pulled
  forward.

The only remaining action is the single local S03 commit; no push, tag or
release is permitted.

## Next action

Create the single local S03 commit:

```text
Integrate XFOIL 6.996 geometry normalization
```
