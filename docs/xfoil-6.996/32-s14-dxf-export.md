# XFOIL 6.996 — S14 DXF R12 exporter

Status: PASS — `X6996-S14` and `X6996-S14-R1` accepted. This slice
implements the DXF R12 encoder and file format only; the hidden `DXF` command
remains an S16 follow-up.

Slice: `S14` of U6, group G08. Scope is the DXF R12 encoder and file format
only. The hidden top-level `DXF` command belongs to a later S16 follow-up and
is not part of this slice.

## Gate basis

```text
G-EXPORT: ACCEPT-DXF-ONLY
X6996-G-EXPORT: PASS
```

Gate commit: `8ce54d569b299fb6ad2d712372f07cd0d5eae766`

Gate parent: `732b92517e891b881f6c524f6a031df96841d147`

The gate accepts DXF only. SVG remains excluded as `NIMP-02`.

## S14 contract from U6

- U6 slice: `S14`, group G08.
- Scope: DXF exporter and R12 file format.
- Dependencies: `S03`, `S04`, `S05` accepted; `G-EXPORT` decided.
- Oracle cases: `a29`, `a30`, `H-S14-EXPORT`.
- Commit boundary: one bounded S14 implementation commit.
- No automatic upstream carry-over of the command.
- No plot backend and no general export framework.

`S16` owns command registration and file wiring only. S14 does not register
`DXF`, does not add a prompt, does not use `OutputPathPolicy` and does not
write a file.

## Upstream format

`DXFOUT` is at `fortran-src/xfoil-6.996/xfoil.f:1710-1829`. The accepted
format contract is:

| Area | Contract |
|---|---|
| Format | AutoCAD DXF R12, `AC1009` |
| Data source | current contour `X(1..N)`, `Y(1..N)` |
| Point count | `N` nodes produce `N-1` `LINE` entities |
| Contour closure | no line from the last node to the first |
| Y direction | unchanged |
| Unit | none; no unit prompt |
| Layer | `Layer 1` |
| Header | `999`, R12 comment, `HEADER`, `$ACADVER`, `$EXTMIN`, `$EXTMAX` |
| Extents | min/max of the contour plus `0.1 * max(Xrange,Yrange)` each side |
| Header precision | `1X,F10.4` |
| Coordinate precision | `1X,F14.6`, then leading blanks stripped |
| Entities | `0`, `LINE`, `8`, `Layer 1`, `10`, `20`, `11`, `21` |
| Trailer | `0`, `ENDSEC`, `0`, `EOF` |
| Newlines | canonical LF only |
| File end | final `EOF` without trailing newline |
| Empty contour | valid header/trailer, zero extents, no entities |
| Determinism | same contour produces the same bytes |
| Filename | not part of the DXF content |

## C++ implementation

New files:

- `include/xfoil/io/DxfExport.h`
- `src/io/DxfExport.cpp`

The encoder accepts two equal-length spans and returns a
`core::Result<std::string>`. It validates length equality and finite
coordinates before producing output. It does not open files, read prompts,
touch session state or change the input arrays.

Formatting uses `std::to_chars` in fixed notation, so the decimal point is
independent of the host process locale. All line breaks are literal LF
characters. The final `EOF` is appended without a newline.

`N=0` produces the canonical 239-byte zero-extent skeleton. `N=1` produces the
header/trailer without entities. No automatic closing segment is added.

The implementation is registered in `IO_SRCS`. The focused unit and
integration tests are registered in `TEST_SRCS` and in the Valgrind filter
list. No existing writer, command, help text or output format is changed.

## Oracle fixtures

New fixtures are under `tests/data/xfoil-6.996/s14/`:

| Fixture | Case | Size | SHA-256 |
|---|---|---:|---|
| `dxf-naca0012-240.dxf` | NACA 0012, 240 nodes | 16013 | `a0c0d13cdf9de48bc01cced1299282ce146a217794c041631c26960037e5eaed` |
| `dxf-naca2412-240.dxf` | NACA 2412, 240 nodes | 16009 | `f6bae1a2d674c3eb3b4f36de16583604a5a4c3befadc9584480d94271c4c3edf` |
| `dxf-nacs0012-240.dxf` | NACS 0012, 240 nodes | 16013 | `9be1f3eac93159fbbc125c11007de63caca5578f992b7bdb362a0c684dd61482` |
| `dxf-empty.dxf` | no contour | 239 | `818684425bb64f551a01a01a669e1c8945a06dd1d6309b1480e21db177c254c2` |

The NACA 0012 fixture has 239 line entities, 2900 text records and 2899 LF
characters. Its measured extents are `-0.1000,-0.1600` to `1.1000,0.1600`.
`metadata.txt` records the oracle identity and the fixture properties.

The `.dxf` fixtures are marked `-whitespace` in `.gitattributes` because DXF
group codes such as ` 0 ` intentionally end with a space. This keeps the
byte-exact oracle files unchanged without weakening the repository-wide
whitespace check.

## Explicit non-goals

- No `DXF` command registration.
- No prompt, filename, overwrite, error or `OutputPathPolicy` wiring.
- No help entry for DXF or SVG.
- No SVG writer, fixture or test.
- No plot backend.
- No general export abstraction.
- No change to SAVE, PSAV, MSAV, WDEF, DUMP, DMP, BLOU, BLDUMP/BLDUMP2/
  BLDUMP3, polar text or polar dump.
- No Fortran source, binary or patch change.
- No change to `tests/fortran-reference/**`.
- No product version, banner or tolerance change.
- No Windows golden and no full Windows suite.

## Acceptance conditions

- Focused unit tests cover header/sections, extents, entity count, no closing
  segment, Y preservation, precision, classic locale, LF, unterminated EOF,
  empty contour, input immutability, determinism and invalid input rejection.
- Focused integration tests cover exact oracle bytes, 239 entities, LF,
  unterminated EOF, empty contour, repeat determinism and no session/input
  mutation.
- Full Linux matrix, reference verify/determinism, harness families,
  focused Valgrind and permanent gates are green.
- Exactly one focused Windows process runs
  `integration.DxfExportS14` with 2/2 tests and 0 failures.
- R1 confirms the S14/S16 boundary, fixture hashes, protected state and
  publication freeze.

## Commit boundary

One S14 implementation commit after all acceptance conditions are green:

`Integrate XFOIL 6.996 DXF exporter`

The commit contains only the DXF encoder, focused tests, versioned S14
fixtures, necessary test registration and this document pair.
