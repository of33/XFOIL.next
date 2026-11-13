# XFOIL 6.996 — S14 DXF R12 exporter report

Status: PASS — `X6996-S14` and `X6996-S14-R1` accepted. Linux matrix,
focused Windows process, fixture checks and R1 are complete.

## Starting state

- G-EXPORT commit status: already existed; no duplicate and no amend.
- S14 start HEAD: `8ce54d569b299fb6ad2d712372f07cd0d5eae766`
  (`Decide XFOIL 6.996 export contract`).
- G-EXPORT parent: `732b92517e891b881f6c524f6a031df96841d147`.
- Worktree clean, staged 0, untracked 0, ahead 42 behind 0 at S14 start.
- Tag `v1.0.0` unchanged at
  `dc7305b057286574cd137cb6f1af170b74650ec5`.
- No S14 DXF commit or prior S14 document existed.
- No push, fetch, amend, tag or release was performed.

## Gate decision

```text
G-EXPORT: ACCEPT-DXF-ONLY
X6996-G-EXPORT: PASS
```

The gate commit contains only the G-EXPORT document pair. It accepts the DXF
R12 format and leaves SVG excluded as `NIMP-02`. The hidden top-level `DXF`
command remains a separate S16 follow-up.

## S14 / S16 boundary

S14 implements only the DXF R12 encoder and format. It contains:

- no `DXF` command registration;
- no command help;
- no filename prompt;
- no overwrite handling;
- no invalid-path or EOF command behavior;
- no `OutputPathPolicy` integration;
- no file writing;
- no SVG code, fixture or test.

S16 will later own the hidden top-level command, filename/prompt, overwrite,
error/EOF behavior and path policy. S14 does not pre-implement that wiring.

## Upstream contract

`DXFOUT` at `fortran-src/xfoil-6.996/xfoil.f:1710-1829` writes the current
contour as DXF R12 (`AC1009`). The accepted contract is:

- `N` nodes produce `N-1` `LINE` entities with no closing segment;
- Y is unchanged and there is no unit scale;
- the layer is `Layer 1`;
- extents are the contour min/max expanded by
  `0.1 * max(Xrange,Yrange)` on each side;
- header values use four decimals, coordinates use six;
- output uses LF and ends with an unterminated `EOF`;
- the filename is not part of the file content;
- an empty contour writes a valid zero-extent skeleton without entities.

## C++ mapping

New files:

- `include/xfoil/io/DxfExport.h`
- `src/io/DxfExport.cpp`

The API is a pure text projection:

```text
dxfR12TextChecked(std::span<const double> x, std::span<const double> y)
dxfR12Text(std::span<const double> x, std::span<const double> y)
```

`dxfR12TextChecked` rejects unequal array lengths with `sizeMismatch` and
non-finite coordinates with `invalidArgument`. `dxfR12Text` returns an empty
string on validation failure. Neither function opens a file, prompts, mutates
session state or modifies the input arrays.

Fixed-point output uses `std::to_chars`, which is independent of the process
numeric locale. The encoder writes LF explicitly and appends the final `EOF`
without a newline.

The new source is registered in `IO_SRCS`; the focused tests are registered
in `TEST_SRCS` and the Valgrind filter list. No existing IO writer or command
is changed.

## Fixtures

New fixtures under `tests/data/xfoil-6.996/s14/`:

| Fixture | Size | SHA-256 |
|---|---:|---|
| `dxf-naca0012-240.dxf` | 16013 | `a0c0d13cdf9de48bc01cced1299282ce146a217794c041631c26960037e5eaed` |
| `dxf-naca2412-240.dxf` | 16009 | `f6bae1a2d674c3eb3b4f36de16583604a5a4c3befadc9584480d94271c4c3edf` |
| `dxf-nacs0012-240.dxf` | 16013 | `9be1f3eac93159fbbc125c11007de63caca5578f992b7bdb362a0c684dd61482` |
| `dxf-empty.dxf` | 239 | `818684425bb64f551a01a01a669e1c8945a06dd1d6309b1480e21db177c254c2` |

`metadata.txt` records the stock-6.996 oracle identity, command class, sizes,
hashes, record/entity counts and the empty-contour property. The `.dxf`
fixtures are marked `-whitespace` in `.gitattributes` because DXF group codes
such as ` 0 ` intentionally end with a space.

The NACA 0012 fixture was checked as:

```text
size: 16013 bytes
LINE entities: 239
text records: 2900
LF characters: 2899
final EOF: unterminated
extents: -0.1000,-0.1600 to 1.1000,0.1600
first line: (1.000000,0.001260) -> (0.996321,0.001775)
last line:  (0.996321,-0.001775) -> (1.000000,-0.001260)
```

The integration test creates the 240-node panel contour through the accepted
NACA/NACS and PANGEN session path, not through a DXF command.

## Tests

Focused unit tests:

```text
unit.DxfExportS14TriangleMatchesCanonicalBytes
unit.DxfExportS14EmptyContourMatchesOracleSkeleton
unit.DxfExportS14PreservesInputAndRepeats
unit.DxfExportS14RejectsInvalidContours
unit.DxfExportS14UsesClassicLocale
```

Focused integration tests:

```text
integration.DxfExportS14OracleBytes
integration.DxfExportS14EmptyContourAndLFContract
```

The unit tests cover exact triangular output, header/section order, extents,
`N-1` entities, no closing segment, Y preservation, precision, LF, final
unterminated EOF, empty contour, input immutability, determinism, locale
independence and controlled rejection of length/non-finite errors.

The integration tests cover exact NACA 0012, NACA 2412, NACS 0012 and empty
oracle bytes, 239 entities, no CR bytes, final unterminated EOF, deterministic
repetition and input preservation.

## Linux acceptance

```text
Full suite, GCC 13.3 Debug:       2101/2101
Full suite, GCC 13.3 Release:     2101/2101
Full suite, Clang 18.1.3 Debug:   2101/2101
Full suite, Clang 18.1.3 Release: 2101/2101
Full suite, ASan/UBSan:           2101/2101
Unit group:                        1760/1760
Integration group:                  286/286
S14 focused unit:                     5/5
S14 focused integration:              2/2
Reference verification:             236/236
Reference determinism:              236/236
Harness families:                    16/16
Focused Valgrind:                   PASS
Permanent gates:                    PASS
Existing product goldens changed:       0
```

The focused S14 Valgrind filters are part of the established
`make test-valgrind` target. The complete target and its core/session link
checks passed without definite leaks.

## Windows status

Exactly one focused process ran on the accepted Windows 11 / MSYS2 UCRT64
baseline:

```text
integration.DxfExportS14
[ RUN      ] integration.DxfExportS14OracleBytes
[       OK ] integration.DxfExportS14OracleBytes
[ RUN      ] integration.DxfExportS14EmptyContourAndLFContract
[       OK ] integration.DxfExportS14EmptyContourAndLFContract
0 failure(s) in 2 test(s)
```

Result: `2/2`, 0 failures. Additional Windows processes: 0. No full Windows
suite and no Windows golden were run.

## R1 — focused acceptance

R1 was completed after the focused Windows process and before the S14 commit.
It confirms:

- S14 start HEAD is the G-EXPORT commit
  `8ce54d569b299fb6ad2d712372f07cd0d5eae766`, parent
  `732b92517e891b881f6c524f6a031df96841d147`.
- No prior S14 DXF commit existed; no duplicate was created.
- The implementation contains only the pure DXF R12 encoder and format.
- The hidden top-level `DXF` command is not registered.
- No filename, prompt, overwrite, error/EOF or `OutputPathPolicy` wiring is
  included.
- No SVG writer, fixture or test exists; SVG remains `NIMP-02`.
- No plot backend or general export framework is included.
- Existing writers and commands are unchanged.
- The DXF R12 header, sections, entity order and trailer match the stock
  6.996 oracle.
- `N` nodes produce `N-1` entities and no closing segment is added.
- Y remains unchanged; extents and four/six-decimal formatting match.
- Output uses LF only and ends with an unterminated `EOF`.
- The empty-contour artifact is 239 bytes with no entities and zero extents.
- NACA 0012, NACA 2412, NACS 0012 and empty fixture hashes match.
- Output is deterministic and independent of the filename.
- The Linux matrix is green.
- Exactly one focused Windows process is green; no second process was run.
- No existing product golden, canonical 6.99 reference, Fortran source,
  binary, patch, product version, banner or tolerance was changed.
- No private absolute path is present in the S14 documents.
- No push, fetch, amend, tag or release was performed.

## Protected state

- Fortran sources, binaries and patches: unchanged.
- `tests/fortran-reference/**`: unchanged.
- Existing product goldens: unchanged.
- Canonical 6.99 references: unchanged.
- SAVE/PSAV/MSAV/WDEF/DUMP/DMP/BLOU/BLDUMP/BLDUMP2/BLDUMP3/polar paths:
  unchanged.
- SVG: not implemented; `NIMP-02` remains.
- DXF command: not implemented.
- Plot backend: not touched; `G-PLOT-HEADLESS` remains open and deferred.
- Product version and banner: unchanged; `v1.0.0` unchanged.
- No push, fetch, amend, tag or release.

## Git status

At S14 start:

```text
Worktree: clean
Staged: 0
Untracked: 0
Ahead/behind: 42/0
S14 start HEAD: 8ce54d569b299fb6ad2d712372f07cd0d5eae766
```

The final S14 commit hash is reported by the outer task because it cannot be
embedded in the same commit.

## Next step

Implement the S16 DXF command follow-up only after S14 is committed: hidden
top-level `DXF`, filename/prompt, overwrite, error/EOF behavior and
`OutputPathPolicy`, with no help entry and no SVG activation. Do not implement
that follow-up in this slice.

## Final decision

X6996-S14: PASS
X6996-S14-R1: PASS
