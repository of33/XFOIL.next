# S03 - geometry normalization and robust LE/bend primitives

Status: PASS - `X6996-S03` accepted.

## Slice contract from U6

- Slice ID: `S03`.
- U3 group: G03 geometry normalization.
- Scope: `NORM` chord-vector normalization and shift/scale;
  `LEFIND`/`XLFIND` robustness; `CANG` ATAN2; `GEOPAR` consistency.
- Fortran routines: `xgeom.f`.
- C++ area: core geometry; mapping is fixed in this pre-analysis.
- Dependency: `S01`.
- Risk: high.
- Oracle cases: `H-S03-NORM`; `a02`, `a03`, `a09`, `a11`.
- 6.99 regressions: `a02`, `a03`, `a11`; `a09` is the existing CANG
  product-path reference.
- Expected files: geometry C++ and focused harness/tests; no Fortran
  change.
- Explicit non-goals: no sharp-TE commands, no defaults, no `SQTE`/`TECIRC`,
  no command registration, no plot port.
- Commit boundary: one local commit for geometry primitives and tests.

## Pre-analysis: Fortran changes

External comparison:
`.audit-xfoil-next/xfoil-6.996/s03/xgeom.diff`.

### LEFIND

6.996 adds a small-N guard and changes the first-guess bracket:

```fortran
IF(N .LT. 5) I = N/2

DXM = X(I) - X(I-1)
DYM = Y(I) - Y(I-1)
DXP = X(I+1) - X(I)
DYP = Y(I+1) - Y(I)
DOTM = DXTE*DXM + DYTE*DYM
DOTP = DXTE*DXP + DYTE*DYP
IF(DOTM .GT. 0.0 .AND. DOTP .LE. 0.0) GO TO 11
```

The Newton formula is unchanged. The change prevents the first guess from
starting after the true crossing when the previous chord dot product is not
positive.

### XLFIND

6.996 adds:

```fortran
IF(RESS .EQ. 0.0) GO TO 21
```

with the canonical fallback `SLE = S(I)` and the existing warning. No formula
change otherwise.

### NORM

6.996 changes the signature and normalization basis:

```fortran
SUBROUTINE NORM(X,XP,Y,YP,S,N,XSHIFT,YSHIFT,SCALE)
XTE = 0.5*(X(1) + X(N))
YTE = 0.5*(Y(1) + Y(N))
XLE = SEVAL(SLE,X,XP,S,N)
YLE = SEVAL(SLE,Y,YP,S,N)
CHORD = SQRT((XTE-XLE)**2 + (YTE-YLE)**2)
XSHIFT = -XLE
YSHIFT = -YLE
SCALE = 1.0/CHORD
X(I) = (X(I)+XSHIFT)*SCALE
Y(I) = (Y(I)+YSHIFT)*SCALE
S(I) = S(I)*SCALE
```

6.99 used the x extents (`XMAX - XMIN`) and did not shift Y to the LE. The
6.996 contract is therefore invariant under rotation/translation of the input
contour and returns unit chord from LE to TE midpoint. `XP`/`YP` remain the
pre-scaling spline coefficients.

### GEOPAR

`GEOPAR` already uses the LE-TE chord vector in both baselines. The
normalization must feed it a contour whose chord length is 1.0. The 6.996
`F7.3 -> F8.4` display-width change is text-only and is not part of S03; text
contracts remain with S18.

### CANG

6.996 replaces `ASIN(CROSSP)` with the quadrant-preserving form:

```fortran
CRS = DX1*DY2 - DY1*DX2
DOT = DX1*DX2 + DY1*DY2
ANGL = ATAN2(CRS,DOT)*(180.0/3.1415926)
```

The forward differences are `X(I)-X(I-1)` and `X(I+1)-X(I)`. Angles beyond
+-90 degrees are retained.

## C++ mapping

| Fortran | Canonical C++ | Header | Existing tests | S03 change |
|---|---|---|---|---|
| `LEFIND` | `core::geometry::lefind` | `include/xfoil/core/geometry/LeadingEdge.h` | `GeometryLefindN0012`, `GeometryLefindN2412`, LE pipeline | yes |
| `LEFIND` | `geometry::findLeadingEdge` | `include/xfoil/geometry/GeometryAlgorithms.h` | `FindLeadingEdge*` | yes, mirror |
| `XLFIND` | `core::geometry::xlfind` | `include/xfoil/core/geometry/LeadingEdge.h` | `GeometryXlfindN0012` | zero-RESS guard |
| `XLFIND` | `geometry::findLeftmostPoint` | `include/xfoil/geometry/GeometryAlgorithms.h` | `FindLeftmostPoint*` | zero-RESS guard |
| `NORM` | `core::geometry::norm` | `include/xfoil/core/geometry/GeometryParameters.h` | `GeometryNormN2412` | chord-vector normalization |
| `GEOPAR` | `core::geometry::geopar` | `include/xfoil/core/geometry/GeometryParameters.h` | `GeometryGeopar*` | consistency test only |
| `CANG` | `core::geometry::cang` | `include/xfoil/core/geometry/Transformations.h` | `GeometryCangN0012` | ATAN2 |
| `CANG` | `geometry::findMaximumCornerAngle` | `include/xfoil/geometry/GeometryAlgorithms.h` | `MaximumCornerAngle*` | ATAN2 mirror |

No new spline/geometry implementation is introduced. The 0-based
`core::geometry` implementation is the product path; the existing 1-based
`xfoil::geometry` mirror is kept consistent for its tests and legacy API.

## Focused verification

External H-S03 harness (only in the S03 audit directory) uses:

- a rotated/translated ellipse for `LEFIND` + 6.99/6.996 `NORM` + `GEOPAR`;
- a four-point contour for the 6.996 small-N `LEFIND` guard;
- a four-point polyline with a 135-degree corner for `CANG` ATAN2.

The 6.996 NORM result for the rotated ellipse must be chord length 1.0, LE at
the origin, and the 6.996 CANG result must be 135 degrees rather than the
6.99 `ASIN` result of 45 degrees.

Additional focused C++ tests cover: rotated NORM + GEOPAR consistency,
LEFIND N=4, XLFIND zero second derivative fallback, CANG quadrant handling,
legacy mirror equivalents, and the existing too-few-points/error contracts.

## Acceptance

- Focused S03 tests and `H-S03-NORM` pass.
- `test-core`, `test-unit`, integration, regression and batch pass.
- GCC 13 Debug/Release and Clang 18 Debug/Release pass with `-Werror`.
- ASan/UBSan and Valgrind reach the geometry boundary tests.
- `test-reference` and `test-reference-determinism` remain `236/236`;
  harnesses remain `16/16`.
- Existing `a02`, `a03`, `a09`, `a11` contracts remain green.
- Windows: one focused process with the highest platform-specific risk after
  Linux acceptance; no full Windows matrix in this slice.
- Canonical 6.99 reference artifacts unchanged.
- Two versioned 6.996 S03 fixtures added.
- One authorized XFOIL.next CLI product-golden line changed.
- No tolerances changed.

## Commit boundary

One local commit:

```text
Integrate XFOIL 6.996 geometry normalization
```

No push, no tag, no release.
