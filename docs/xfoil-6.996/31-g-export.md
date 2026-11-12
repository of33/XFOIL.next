# XFOIL 6.996 — G-EXPORT gate decision

Status: PASS — the gate is decided. This document changes no product code,
test, fixture, golden, Fortran source, binary or patch.

Gate: `G-EXPORT`

Decision:

```text
G-EXPORT: ACCEPT-DXF-ONLY
X6996-G-EXPORT: PASS
```

The decision is to accept the non-graphical DXF R12 exporter and the hidden
top-level `DXF` command. The SVG exporter and the `SVG` command are excluded
from the product and remain `NIMP-02`.

## U6 gate contract

U6 defines `G-EXPORT` as the SVG/DXF product-scope gate:

- U6 options are: include non-graphical export; exclude and record `NIMP-02`;
  defer.
- The gate blocks `S14` and the SVG/DXF portion of `S16`.
- `S14` owns the exporter and file format.
- `S16` owns command activation only.
- No automatic upstream carry-over and no general export framework are
  authorized.

The gate is decided as a partial acceptance: DXF is accepted; SVG is excluded.

## Starting state after S22

- S22 commit already exists:
  `732b92517e891b881f6c524f6a031df96841d147`.
- S22 parent: `35b49cabde41dd0e86afa1a102ad2af47f05dc3d`.
- S22 contains only its document pair and both S22 PASS markers.
- HEAD at gate start is the S22 commit.
- Worktree clean, staged 0, untracked 0, ahead 41 behind 0.
- Tag `v1.0.0` remains
  `dc7305b057286574cd137cb6f1af170b74650ec5`.
- No G-EXPORT commit or prior gate document existed.
- No push, fetch, amend, tag or release was performed.

## Upstream finding

The 6.996 top-level dispatcher adds two hidden commands:

| Command | 6.99 | 6.996 | Implementation |
|---|---|---|---|
| `SVG` | absent | present | `SVGOUT`, `fortran-src/xfoil-6.996/xfoil.f:1588-1706` |
| `DXF` | absent | present | `DXFOUT`, `fortran-src/xfoil-6.996/xfoil.f:1710-1829` |

The dispatcher entries are at `xfoil.f:185-190`. Neither command appears in
the `?` help. The 6.996 `xfoil.f` SHA-256 is
`d607e1a205ed9a9620589aa52d3a46f07fe4f312bb5c657b8b99d8c6216378e9`;
the 6.99 file has no SVG/DXF path and has SHA-256
`57b14c9e0c71097ede0f7989f8d92db161265962e9de9b3f7a5320b4ef4a23fe`.

The export observations use `fortran-bin/xfoil-6.996`, SHA-256
`000b0468d9a7f4fce42d6fac24bd14147d7d326c8a9035909c8e9f98d180f50e`.
The polar-dump patch `P-6996-P1` does not affect either exporter.

## SVG finding and exclusion

SVG is rejected for product inclusion. It is non-graphical, but its observed
contract is defective and filename-dependent:

- `INT` truncation produces `width="1in"` and `height="0in"` for the tested
  NACA 0012, NACA 2412 and NACS 0012 contours. The SVG document is
  well-formed XML but has zero drawable height.
- The output embeds the selected filename in `sodipodi:docname`, so identical
  geometry does not produce identical bytes when the filename changes.
- The empty-geometry path emits `d="M"` with no drawable contour.
- EOF at the units prompt exits abnormally and leaves a 0-byte file.
- There is no established product requirement, no help entry and no 6.99
  antecedent.

The product therefore adds no `SVG` command, no SVG writer and no SVG format
claim. `NIMP-02` is narrowed to SVG only; DXF is moved out of `NIMP-02` by
this gate.

Re-evaluation requires an explicit product requirement and a corrected SVG
width/height, filename and error contract. It is not part of this exposure.

## DXF product contract

DXF is accepted as a non-graphical native export command.

### Command and filename

- `DXF` is a hidden top-level command. It is not added to the `?` help.
- `DXF filename` uses the trimmed inline argument, preserves internal spaces
  and adds no extension.
- `DXF` without an argument prompts `Enter output filename` and trims the
  answer at both ends.
- There is no loaded-prefix default and no automatic `.dxf` extension.
- A blank or whitespace-only answer is an invalid filename.
- The command reads the current panel contour, not the buffer geometry.
- No plot library, display or cursor is used.

### Path policy and overwrite

- DXF is a native output command for path-policy purposes.
- The existing `OutputPathPolicy` is applied exactly once, at the final target
  path, before the existence check, overwrite prompt and open.
- The prompt shows the logical filename; the transformed path fixes the file
  target.
- The path prefix does not change the DXF content.
- Existing target: prompt
  `Output file exists.  Overwrite?  Y`.
- Blank, default or any answer whose first character is not `N`/`n` overwrites
  and truncates the target.
- `N`/`n` as the first answer character declines; the target is unchanged and
  the command prints `DXF file not written.`
- No success message is printed.

### Errors and session behavior

- An invalid or unopenable target prints:

  ```text
  Bad filename.
  DXF file not written.
  ```

  No output file is created and the session continues with exit 0. DXF uses
  its own non-fatal `ERR=90` path and does not reuse the fatal OPER
  `writeExportFile` open-failure path.
- Interactive EOF at the filename prompt is a controlled session end with no
  file. This retains the product interactive-EOF convention; the stock binary
  aborts with exit 2.
- The command does not mutate geometry, paneling, operating-point, polar or
  boundary-layer state.
- Repeated runs with the same geometry and filename are deterministic.

### File format

The accepted format is ASCII DXF R12 with canonical LF line endings:

- Header marker `999`, comment
  `"DXF R12 Output" (www.mydxf.blogspot.com)`, then `SECTION`, `HEADER`,
  `$ACADVER`, `AC1009`, `$EXTMIN`, `$EXTMAX`.
- Bounds are the minimum and maximum of the current contour `X`/`Y`, expanded
  by `0.1 * max(Xrange, Yrange)` on each side.
- Header bounds use four decimals with one leading blank (`1X,F10.4`).
- The `ENTITIES` section contains `N-1` `LINE` entities on `Layer 1`.
- Each line uses the `10`, `20`, `11`, `21` coordinate codes in source order;
  coordinates use six decimals (`1X,F14.6`) with leading blanks stripped.
- There is no closing line from the last contour point to the first.
- There is no Y flip and no unit scale.
- The trailer is `0`, `ENDSEC`, `0`, `EOF`; the final `EOF` token has no
  trailing newline.
- The output contains no filename, timestamp or other variable metadata.

Measured 240-node contour artifacts:

| Case | Size | SHA-256 |
|---|---:|---|
| NACA 0012 | 16013 | `a0c0d13cdf9de48bc01cced1299282ce146a217794c041631c26960037e5eaed` |
| NACA 2412 | 16009 | `f6bae1a2d674c3eb3b4f36de16583604a5a4c3befadc9584480d94271c4c3edf` |
| NACS 0012 | 16013 | `9be1f3eac93159fbbc125c11007de63caca5578f992b7bdb362a0c684dd61482` |

The default NACA 0012 artifact has 239 line entities and 2900 text records;
the final `EOF` is unterminated. Inline `foil.dxf`, prompted `out.dxf` and
inline no-extension `noext` produce the same content hash.

For NACA 0012 the measured header extents are:

```text
$EXTMIN = -0.1000, -0.1600
$EXTMAX =  1.1000,  0.1600
```

With no current contour, DXF writes the empty R12 skeleton with zero extents,
no entities and the same trailer. The measured artifact is 239 bytes with
SHA-256
`818684425bb64f551a01a01a669e1c8945a06dd1d6309b1480e21db177c254c2`.

## Options evaluation

| Criterion | Accept both | Accept DXF only | Exclude both | Defer |
|---|---|---|---|---|
| Non-graphical product value | SVG value unproven | DXF is a usable exchange format | no export command | undecided |
| File validity | SVG has zero-height defect | DXF R12 valid and deterministic | no new format | no evidence decision |
| Error behavior | SVG leaves 0-byte file on units EOF | DXF error contract controlled | unchanged | no decision |
| Filename independence | SVG embeds filename | DXF content is filename-independent | unchanged | no decision |
| Product surface | adds a defective SVG command | adds one hidden command | adds nothing | blocks S14/S16 |
| Existing contracts | risks misleading SVG output | no change to polar/BL exports | fully preserved | preserves but delays |
| Maintainability | two new formats | one retained format | none | unresolved |
| S14/S16 | both activated | DXF only | `NIMP-02` both | remain blocked |

The DXF-only option is selected. SVG has no acceptable product contract, while
DXF is valid, deterministic and independent of plotting.

## Consequences

- `S14` is released for the DXF exporter and R12 file format only.
- `S16` is released for the hidden top-level `DXF` command only after `S14`.
- BLOU remains exactly as accepted in `S16`; its command and BLDUMP3 contract
  are unchanged.
- `SVG` remains unrecognized in the product.
- `NIMP-02` now covers SVG only.
- No general export framework, plot backend or automatic side file is added.
- Existing polar, dump and boundary-layer export contracts remain protected:
  `G-BL-SIDEFILE: PRESERVE-EXISTING-BLDUMP-AND-ADD-VERSIONED-BLDUMP3`.
- `G-PLOT-HEADLESS` remains open and deferred.

## S14/S16 acceptance conditions

The DXF implementation slices must cover:

- exact NACA 0012, NACA 2412 and NACS 0012 bytes after their geometry is
  available;
- the empty-contour skeleton;
- no Y flip, no unit scaling and no closing line;
- inline, prompted and no-extension filenames;
- blank and invalid filenames, invalid paths and interactive EOF;
- existing-target accept and `N`/`n` decline;
- `OutputPathPolicy` prefixing at the open point;
- repeat determinism and no session mutation;
- hidden-command behavior with unchanged help;
- 6.99 regression preservation and no SVG activation;
- Linux Debug/Release, focused ASan/UBSan and Valgrind, and one focused
  Windows run for the DXF file contract.

## Non-goals

- No S14 or S16 implementation.
- No product code, test, fixture, golden or reference change.
- No Fortran source, binary or patch change.
- No SVG product support.
- No plot backend or headless plot work.
- No general export abstraction.
- No BLOU, BLDUMP2, BLDUMP3, DUMP, DMP or polar change.
- No product-version or banner change.
- No push, fetch, amend, tag or release.

## Commit boundary

One documentation-only local commit:

`Decide XFOIL 6.996 export contract`

Only this document and its report are committed.
