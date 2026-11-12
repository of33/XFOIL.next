# XFOIL 6.996 — G-EXPORT gate report

Status: PASS — the gate is decided. This is a documentation-only gate record.
No product code, test, fixture, golden, Fortran source, binary or patch is
changed.

```text
G-EXPORT: ACCEPT-DXF-ONLY
X6996-G-EXPORT: PASS
```

## Starting state after S22

- S22 commit status: already existed; no duplicate and no amend.
- S22 commit:
  `732b92517e891b881f6c524f6a031df96841d147`
  (`Record X6996-S22 OSMAP compatibility verification`).
- S22 parent: `35b49cabde41dd0e86afa1a102ad2af47f05dc3d`.
- S22 contains exactly the S22 document pair; both S22 PASS markers are
  committed; protected product/test paths are absent from its diff.
- HEAD at gate start is the S22 commit.
- Worktree clean, staged 0, untracked 0, ahead 41 behind 0.
- Tag `v1.0.0` remains
  `dc7305b057286574cd137cb6f1af170b74650ec5`.
- No G-EXPORT commit or prior gate document existed.
- No push, fetch, amend, tag or release was performed.

## S22 confirmation

Part A of this task confirmed the S22 commit boundary and contents:

- exactly `30-s22-osmap-diagnostic-compatibility.md` and its report;
- markers `X6996-S22: PASS` and `X6996-S22-R1: PASS`;
- no product, test, fixture, golden, Fortran, data-copy or reference files;
- canonical `osmap.dat` hash and size unchanged;
- `osmapDP` inactive;
- no product OSMAP path or plot activation.

No S22 change was required.

## U6 gate contract

U6 defines `G-EXPORT` as the SVG/DXF product-scope gate. The options are to
include non-graphical export, exclude and record `NIMP-02`, or defer. The gate
blocks `S14` and the SVG/DXF portion of `S16`. `S14` owns the exporter and file
format; `S16` owns command activation only.

U6 explicitly forbids automatic upstream carry-over and a general export
framework. The gate is therefore about product scope and a retained contract,
not about porting every upstream output by default.

## Upstream comparison

| Item | 6.99 | 6.996 | Finding |
|---|---|---|---|
| Top-level `SVG` dispatch | absent | present | hidden command |
| Top-level `DXF` dispatch | absent | present | hidden command |
| `SVGOUT` | absent | present | defective for the tested contours |
| `DXFOUT` | absent | present | valid R12 output |
| Help entries | absent | absent | neither command is documented |

Source locations:

- dispatcher: `fortran-src/xfoil-6.996/xfoil.f:185-190`;
- `SVGOUT`: `xfoil.f:1588-1706`;
- `DXFOUT`: `xfoil.f:1710-1829`.

Protected identities:

| Artifact | SHA-256 |
|---|---|
| `fortran-src/xfoil-6.996/xfoil.f` | `d607e1a205ed9a9620589aa52d3a46f07fe4f312bb5c657b8b99d8c6216378e9` |
| `fortran-src/xfoil-6.99/xfoil.f` | `57b14c9e0c71097ede0f7989f8d92db161265962e9de9b3f7a5320b4ef4a23fe` |
| `fortran-bin/xfoil` | `f5779d6954d0b8fdc152e0e0633f18732e00b7b69228d561d629bd524bba598c` |
| `fortran-bin/xfoil-6.996` | `000b0468d9a7f4fce42d6fac24bd14147d7d326c8a9035909c8e9f98d180f50e` |

The export measurements use the unpatched 6.996 binary. The accepted
polar-dump patch `P-6996-P1` does not touch either exporter.

## Oracle method

External audit area: `xfoil-6.996/g-export/`.

The probes ran the 6.996 binary in isolated directories with NACA 0012, NACA
2412 and NACS 0012 contours. Inputs, stdout, stderr, exit codes, generated
files and file hashes were recorded. The same probes were run with the 6.99
binary where command recognition needed confirmation.

No product command, test, fixture or golden was created.

## SVG findings

### Command and file behavior

- `SVG filename` uses the inline argument verbatim after trimming; no
  extension is added.
- `SVG` without an argument prompts `Enter output filename`.
- The command always asks for units; the default is `in`.
- The output uses the current panel contour with Y sign-flipped.
- The output embeds the filename in `sodipodi:docname`.
- Existing target: overwrite prompt; `N`/`n` leaves the file unchanged.
- Invalid path prints `Bad filename.` and `SVG file not written.`, exits 0 and
  does not create a file.
- EOF at the filename prompt exits 2 with no file.
- EOF at the units prompt exits 2 and leaves a 0-byte file.

### Measured defects

| Case | Size | Finding |
|---|---:|---|
| NACA 0012 | 4943 | `width="1in"`, `height="0in"`; SHA-256 `cb0b439381cebe0d1da7a6bf24e1601a3720cdefaee3f4dd302b1d207de137e5` |
| NACA 2412 | 4945 | zero height; SHA-256 `45b224f8b1281281408fb18f305f04da361ca7bbe8c4dbb2cdc07ec31f0742d4` |
| NACS 0012 | 4943 | zero height; SHA-256 `2cb74ed8c46c745badfcbd6a967d9d8d437cc706c438afaf3d23dd9dd2f58707` |
| units `mm` | 4943 | same zero-height geometry; SHA-256 `3c829a7ff429733649798e95662aa13b185e575d24d7c3192fa69c4614cb41fe` |
| prompted filename | 4942 | filename-dependent bytes; SHA-256 `951b8a07d75a297bf8fc9290bbc6ce6393e7bf5105ff0d4cb07c791c083bd22f` |
| empty contour | 503 | `d="M"`; no drawable contour; SHA-256 `a7f426e5b7fedff364aca042e1b2e03823a3492c47a7461a69321541d8a5c2f0` |

The 6.99 binary reports `SVG command not recognized` and creates no file.

SVG is excluded from the product as `NIMP-02`. The tested output is not an
acceptable product contract: it has no drawable height for the standard
contours, it is filename-dependent and its empty/error paths are unsafe.

## DXF findings

### Command and file behavior

- `DXF filename` uses the trimmed inline argument; no extension is added.
- `DXF` without an argument prompts `Enter output filename`; there is no
  prefix default.
- No units prompt and no plot dependency.
- Existing target: `Output file exists.  Overwrite?  Y`.
- Blank/default or a first character other than `N`/`n` overwrites.
- `N`/`n` declines and prints `DXF file not written.`; the target remains
  unchanged.
- Invalid path prints `Bad filename.` and `DXF file not written.`, exits 0 and
  creates no file.
- EOF at the filename prompt exits 2 with no file in the stock binary.
- The 6.99 binary reports `DXF command not recognized` and creates no file.

### Format measurements

The default NACA 0012 contour has 240 nodes and produces:

| Property | Value |
|---|---|
| Size | 16013 bytes |
| SHA-256 | `a0c0d13cdf9de48bc01cced1299282ce146a217794c041631c26960037e5eaed` |
| Entities | 239 `LINE` records |
| Text records | 2900 |
| Newline characters | 2899; final `EOF` is unterminated |
| Header extents | `-0.1000,-0.1600` to `1.1000,0.1600` |
| First line | `(1.000000,0.001260)` to `(0.996321,0.001775)` |
| Last line | `(0.996321,-0.001775)` to `(1.000000,-0.001260)` |
| Y direction | unchanged; no flip |
| Units | none |

Additional cases:

| Case | Size | SHA-256 |
|---|---:|---|
| NACA 2412 | 16009 | `f6bae1a2d674c3eb3b4f36de16583604a5a4c3befadc9584480d94271c4c3edf` |
| NACS 0012 | 16013 | `9be1f3eac93159fbbc125c11007de63caca5578f992b7bdb362a0c684dd61482` |
| prompted `out.dxf` | 16013 | same as NACA 0012 |
| inline no-extension `noext` | 16013 | same as NACA 0012 |
| repeated run | 16013 | same as NACA 0012 |
| overwrite accept | 16013 | same as NACA 0012 |
| empty contour | 239 | `818684425bb64f551a01a01a669e1c8945a06dd1d6309b1480e21db177c254c2` |

The empty-contour artifact contains zero extents, no entities and the normal
trailer. Inline, prompted and no-extension runs produce identical content.

### Contract implications

The existing `OutputPathPolicy` applies to DXF as a native output command:
the transformation happens once at the open point, before the existence and
overwrite checks. Prompt text keeps the logical name, and the prefix does not
change the DXF bytes.

DXF keeps its own non-fatal invalid-path path (`Bad filename.`, no file,
session continues). It must not reuse the fatal OPER export open-failure path.
Interactive EOF remains the product-controlled session end with no file; the
stock binary's exit-2 runtime abort is not adopted.

## Current C++ inventory

No SVG or DXF exporter exists in the product. Relevant existing components
are:

| Area | Current behavior |
|---|---|
| `src/app/cli/CommandInterpreter.cpp` | export filename prompts, overwrite handling and file-write helpers |
| `src/app/cli/CoreGeometryHandlers.cpp` | current-contour SAVE/PSAV filename, overwrite and output-policy handling |
| `src/io/OutputPathPolicy.cpp` | native output prefix transformation |
| `src/app/AirfoilWriter.*` | existing coordinate writers; not a DXF writer |
| `src/io/ExportFormats.*` | existing non-SVG/DXF export encoders |
| `src/app/cli/CommandInterpreter.cpp` | OPER exports use a fatal open-failure path; DXF differs |

S14 therefore adds one focused DXF input/encoder and one command path. It does
not add a general export framework and does not change existing exporters.

## Decision

XFOIL.next 1.1.0 accepts the DXF R12 exporter and the hidden top-level `DXF`
command. It does not accept or port SVG.

DXF is valid, deterministic, independent of plotting and filename-independent
in file content. SVG has a measured zero-height contract for standard airfoils
and unsafe empty/error paths, so it remains excluded as `NIMP-02`.

## S14 and S16 release

```text
S14 (G08): DXF exporter and R12 file format only
Dependencies: S03, S04, S05 accepted; G-EXPORT decided
Allowed: DXF encoder, DXF command support in S16, focused format/CLI tests
Not allowed: SVG, plot backend, general export framework, existing export changes

S16 (G09): hidden top-level DXF command only
Dependencies: S14 accepted; BLOU remains as accepted
Allowed: command registration, filename/overwrite/error wiring
Not allowed: format implementation in S16, SVG command, help entry
```

S14 and S16 are now unblocked. This gate performs no implementation.

## Protected state

- Product implementation: none.
- Product code/tests/fixtures/goldens: unchanged.
- Canonical 6.99 references: unchanged.
- Fortran sources/binaries/patches: unchanged.
- SVG: excluded; no product support.
- BLOU/BLDUMP2/BLDUMP3/DUMP/DMP/polar exports: unchanged.
- `G-BL-SIDEFILE` contract unchanged.
- Product version and banner unchanged; `v1.0.0` unchanged.
- `G-PLOT-HEADLESS` remains open and deferred.
- No push, fetch, amend, tag or release.

## Platform boundary

- DXF uses canonical LF bytes on every platform; no CRLF translation.
- The Linux measurements define the byte contract.
- S14/S16 require one focused Windows run for the DXF file behavior before the
  slice is accepted.
- No plot library, display or cursor is required.
- No cross-platform filename or unit claim is added.

## Documentation gates

The non-writing repository gates were run after this document pair was
complete:

| Gate | Result |
|---|---|
| `git diff --cached --check` after explicit staging | PASS |
| `make check-fortran-sha` | PASS |
| `make check-license-headers` | PASS |
| `make check-release-inventory` | PASS |
| `make check-public-docs` | PASS |

The two SHA manifests, license-header self-test and live check, release
inventory and public-documentation gate all completed without findings.

## Git status

At gate start:

```text
Worktree: clean
Staged: 0
Untracked: 0
Ahead/behind: 41/0
S22 commit: 732b92517e891b881f6c524f6a031df96841d147
```

The final gate commit hash is reported by the outer task because it cannot be
embedded in the same commit.

## Next step

Implement `S14` for DXF only, then `S16` for the hidden top-level `DXF`
command. Do not implement SVG, a plot backend or a general export framework.
Keep `G-PLOT-HEADLESS` open and deferred. Do not start the cumulative 1.1.0
final acceptance without a separate instruction.

## Final decision

G-EXPORT: ACCEPT-DXF-ONLY
X6996-G-EXPORT: PASS
