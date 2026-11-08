# U0 — XFOIL 6.996 inventory and freeze

## Decision

```text
X6996-U0: PASS
```

The imported XFOIL 6.996 source set is byte-consistent with the local upstream
archive, the imported file modes are normalised, and the existing XFOIL 6.99
baseline is unchanged.

Resolved in the structural closure pass (U0-f1):

1. `Makefile_gfortran` was restored byte-identically from the local upstream
   archive entry `Xfoil/bin/Makefile_gfortran`.
2. All imported source files now have mode `0644`; the diagnostic executable has
   mode `0755`.
3. The redundant variants `dplot1.f` and `xqdes1.f` were removed after a
   read-only usage and routine proof.
4. The remaining 49 imported files are all byte-identical to their archive
   entries.

Resolved in the version-aware provenance gate pass (U0-G1):

5. The global fixed notice-count assumption was replaced by the explicit
   manifest `tools/fortran-provenance.json`.
6. XFOIL 6.99 and XFOIL 6.996 are checked separately as two version roots:
   48 files / 23 notice-bearing files and 49 files / 23 notice-bearing files.
7. `make check-license-headers` now passes.

Points that are explicitly **not** U0 blockers:

- The complete compiler/link command line of the diagnostic executable is not
  independently recorded. The executable is not yet canonical.
- `osmap.dat` is absent upstream and remains the subject of a later diagnostic
  phase (U4/U5).
- The plot library `plotlib/**` is deliberately not imported.
- The reduced source subset is not currently self-buildable by itself.

## Scope

This pass inventories only the already imported artifacts:

- `fortran-src/xfoil-6.996/**`
- `fortran-bin/xfoil-6.996`

It records repository state, file hashes, modes, classifications, archive
provenance, upstream build-description dependencies, `osmap.dat` status, and
static executable metadata. All audit work products are kept outside the
repository.

## Explicit non-goals

This pass does not and must not:

- edit upstream Fortran sources;
- move or rename the 6.99 baseline;
- create or change references, goldens, tolerances, or oracle contracts;
- change C++ code;
- change build paths or build targets;
- create the `xfoil-6.99` source directory or a `xfoil-6.99` symlink;
- copy or generate `osmap.dat`;
- modify the C++ OSMAP reader;
- change the product banner or product version;
- stage, commit, amend, tag, push, or rewrite history;
- perform a numerical comparison of 6.99 versus 6.996 results.

## Repository state

Captured before any change in this pass.

| Item | Value |
|---|---|
| HEAD | `58bca7d17f847e1aef6b1c8d3d4e167f40f69945` |
| Branch | `main` |
| `origin/main` | `58bca7d17f847e1aef6b1c8d3d4e167f40f69945` (same commit) |
| Tag | `v1.0.0` |
| Product version | `1.0.0` (`src/app/ProductInfo.h`) |
| Remote | `origin https://github.com/of33/XFOIL.next` |

Working-tree state at the start of the pass:

```text
 M README.md
 M XFOIL.next.png
?? docs/xfoil-6.996/
?? fortran-bin/xfoil-6.996
?? fortran-src/xfoil-6.996/
```

Notes:

- `README.md` and `XFOIL.next.png` were already modified before this pass. They
  are unrelated to U0 and were not touched.
- `docs/xfoil-6.996/README.md` is **untracked**.
- `fortran-src/xfoil-6.996/` and `fortran-bin/xfoil-6.996` are **untracked**;
  they are not committed.
- The 6.99 source baseline is still the unversioned layout at
  `fortran-src/**` top level. It has not been moved to
  `fortran-src/xfoil-6.99/`.

Protected pre-pass values (also re-measured after the pass):

| Path | Mode | SHA-256 |
|---|---:|---|
| `README.md` | 664 | `a44f6945b283569be4c1b483d6817174e255b4fd8ab6f06ab19b683279efe119` |
| `XFOIL.next.png` | 664 | `4a89a54f03601b126b764b141dd5518de13b279d1e0b5dbc0f116f1f2e3e6589` |
| `fortran-bin/xfoil` | 775 | `f5779d6954d0b8fdc152e0e0633f18732e00b7b69228d561d629bd524bba598c` |
| `fortran-src/SHA256SUMS` | 664 | `38c6503d93ac14ff88a038bf3ef0e7b116f315615f075d37c7bc59365ec08410` |

## Existing XFOIL 6.99 baseline

The 6.99 baseline was measured before and after this pass and was not modified.

| Artifact | SHA-256 |
|---|---|
| `fortran-bin/xfoil` | `f5779d6954d0b8fdc152e0e0633f18732e00b7b69228d561d629bd524bba598c` |
| `fortran-src/SHA256SUMS` | `38c6503d93ac14ff88a038bf3ef0e7b116f315615f075d37c7bc59365ec08410` |

The complete set of top-level `fortran-src/` file hashes was captured externally
before and after the pass and is unchanged. `fortran-bin/xfoil` remains at
`fortran-bin/xfoil`; no `xfoil-6.99` symlink was created.

## Imported XFOIL 6.996 source set

### Summary

| Property | Value |
|---|---|
| Root | `fortran-src/xfoil-6.996/` |
| Regular files | 49 |
| Symlinks | 0 |
| Empty files | 0 |
| Total size | 1,154,891 bytes |
| NUL-containing/binary files | 0 |
| Text encoding | ASCII (all files) |
| Line endings | LF only, all files end with a final newline |
| File modes | all regular files `0644` |

### File inventory

Every imported file is listed below. `Size` is in bytes, `Mode` is the POSIX
mode, and the SHA-256 covers the file exactly as stored in the repository.

| Repository path | Class | Size | Mode | SHA-256 | Notes |
|---|---|---:|---:|---|---|
| `fortran-src/xfoil-6.996/BLPAR.INC` | Fortran include | 165 | 644 | `4758e32600e364e0c5d0f60c5fc6bcf479ef47d7a63cdf0d52ab8a5c14d5f850` | Boundary-layer parameter common include. |
| `fortran-src/xfoil-6.996/CIRCLE.INC` | Fortran include | 1975 | 644 | `eef4b54829a98d64ab50575ba10933301b36d4f26f5c15f7fb1dc1b2eb33b7fa` | MDES circle-geometry common include. |
| `fortran-src/xfoil-6.996/Makefile_gfortran` | build description | 6628 | 644 | `ece560720547fd0ce718fa33b6bacab96b43a0fa8ee80fe21f13344cfadbfc92` | Upstream gfortran build description, byte-identical to the archive entry. |
| `fortran-src/xfoil-6.996/PINDEX.INC` | Fortran include | 2510 | 644 | `8d9c210e5e2c8899aad099bba4296ad2523b634f01d81d7c071b9bb90fac9714` | Polar storage/index common include. |
| `fortran-src/xfoil-6.996/PPLOT.INC` | Fortran include; plotting-related source | 1540 | 644 | `0e1b74eb546d618a246065856b2df54c6abee76e5d5668b81092a666d814ea50` | Include for the standalone PPLOT polar-plot program. |
| `fortran-src/xfoil-6.996/PXPLOT.INC` | Fortran include; plotting-related source | 893 | 644 | `d9ae5306f9a038c96354fc7ce922cb08aab63f9dd954ea9333a4ccc6b5ed50f9` | Include for the standalone PXPLOT dump plotter. |
| `fortran-src/xfoil-6.996/XBL.INC` | Fortran include | 3107 | 644 | `de22d7c5d036ba8c02ab935a9af7671b0c790304226b744231c815648e23ceaa` | Boundary-layer common include. |
| `fortran-src/xfoil-6.996/XDES.INC` | Fortran include | 247 | 644 | `b1cdab4b9b5b241cf616d06c77b043df601c278cb15dadc00c89eefefe8c032e` | Design common include. |
| `fortran-src/xfoil-6.996/XFOIL.INC` | Fortran include | 26421 | 644 | `4b12cccb24d07c30b58aed40124404d876750826a7ec4fcb94d82b424edb5107` | Main XFOIL common include. |
| `fortran-src/xfoil-6.996/aread.f` | Fortran source | 4333 | 644 | `3be82e776c9c92f8872f1d5c6ef5691d51a72937cc0bb91be6dcd45f47777f15` | Airfoil coordinate-file reader. |
| `fortran-src/xfoil-6.996/blplot.f` | Fortran source; plotting-related source | 64409 | 644 | `f4c20e3d5457947116016a3651b81e41ba867cca90501353f789896751d42720` | Boundary-layer variable plotting menu. |
| `fortran-src/xfoil-6.996/blu.f` | Fortran source | 2886 | 644 | `d228d1a4593593d7b5d930319f47cca9d1577348d8c0ce9edee65b55e1b1d7d2` | Standalone boundary-layer utility program. |
| `fortran-src/xfoil-6.996/dplot.f` | Fortran source; plotting-related source | 13333 | 644 | `17704c2c7da56977fec65f75473ed1c52ef514219bcbcf5ae1728175bfcad7eb` | Analytical profile plotting. |
| `fortran-src/xfoil-6.996/frplot.f` | Fortran source; plotting-related source | 2451 | 644 | `45653f7547ffc132eb0507903f4918f4779fea004099e7e228143bf8d152fe1e` | TS-wave frequency plotting using the OSMAP data. |
| `fortran-src/xfoil-6.996/frplot0.f` | Fortran source; plotting-related source | 345 | 644 | `2b86b6b25a4a30465f1113ce91144454a8aa1762b9949f748814228633d8b479` | Dummy FRPLOT stub used for the non-frequency build. |
| `fortran-src/xfoil-6.996/getarg.f` | Fortran source; platform/build helper | 110 | 644 | `67c8527c07d0232757247ebd6956ff81d5087bfe2be266e7307e3879184c7499` | Command-line argument wrapper. |
| `fortran-src/xfoil-6.996/getosfile.c` | platform/build helper | 819 | 644 | `ef601fbb6f5e0b3c4673065d027a40399c7fa783e13c1b6bfdce5142afc11b06` | C helper that reads the `OSMAP` environment variable; links to the Fortran OSMAP path. |
| `fortran-src/xfoil-6.996/gui.f` | Fortran source; plotting-related source | 1899 | 644 | `b8c4e7b8563566a60be5d4a6a25562052d2fe14a3c89b74d8111eaa994f629c9` | GUI button-box drawing helper. |
| `fortran-src/xfoil-6.996/iopol.f` | Fortran source | 24388 | 644 | `cc4ed012c3c28be9400ee77efc1de9761985a3f309932a87a91c1e2717015ac5` | Polar file read/write routines. |
| `fortran-src/xfoil-6.996/modify.f` | Fortran source | 38289 | 644 | `5fabbcd3254b06731db4cb58f1892c52740967d79ed8a200ef0979f548a58b3a` | Geometry modification routines. |
| `fortran-src/xfoil-6.996/naca.f` | Fortran source | 5556 | 644 | `4088836c43cf08900897f4527d2785be16544f75f145fdab9dcb68f6bfbab8ef` | NACA airfoil geometry generator. |
| `fortran-src/xfoil-6.996/ntcalc.f` | Fortran source | 3560 | 644 | `f47e31d970312cb4d095de9e9aea6ea305b688156f472e5c7a1bb947b24628ba` | TS-wave frequency range calculation for OSMAP. |
| `fortran-src/xfoil-6.996/osmap.f` | Fortran source | 15558 | 644 | `3c806b1d5bd62b80d2a2180b93ea2db95cb5f56c5ec7e55c07868fa20065e719` | Orr-Sommerfeld map reader/interpolator; opens an external OSMAP data file. |
| `fortran-src/xfoil-6.996/p.ftnchek` | platform/build helper | 219 | 644 | `887697b86a6489bc63b884f330cde6fe6474af29d88d5e008d80573ff9b6771f` | ftnchek static-analysis invocation for the PPLOT sources. |
| `fortran-src/xfoil-6.996/plutil.f` | Fortran source; plotting-related source | 13795 | 644 | `49f8661d848ed610046b84dc54167696021538255aa2f6dfee9315ea33a8855c` | Plot parameter utility. |
| `fortran-src/xfoil-6.996/pntops.f` | Fortran source | 10245 | 644 | `746469aeec458338842d943d8f1520788875fd27b6e1764104bab140884a4d3e` | Interactive panel/point operations. |
| `fortran-src/xfoil-6.996/polfit.f` | Fortran source; plotting-related source | 32687 | 644 | `c5f4d379ad181a3deb337edd42803b6acc062b9a4de75c77fd25d10741c2de30` | Polar plot generation. |
| `fortran-src/xfoil-6.996/polplt.f` | Fortran source; plotting-related source | 36535 | 644 | `a1b50f6d9553d367fad3d02596a16a91c842cfa3114e2a2e9fd5a9c4bc68ee7e` | Polar plotting routines. |
| `fortran-src/xfoil-6.996/pplot.f` | Fortran source; plotting-related source | 42093 | 644 | `b2d90d3129b5ff66d5ec7ff1b28fce6330c9f09a9a50dbf83f77f1ab7154234f` | Standalone polar plotting program. |
| `fortran-src/xfoil-6.996/profil.f` | Fortran source | 29244 | 644 | `69e687faafb9528b08b3714d6c1a6d17c86e13a716a3a367c0ee449b80ba815b` | Wall boundary-layer profile / slip velocity. |
| `fortran-src/xfoil-6.996/pxplot.f` | Fortran source; plotting-related source | 32771 | 644 | `bf1e5cfb8c740251ec5b061ab25b6c31ba8dae00ec2689d0dd527abc6aeb18dd` | Standalone polar-dump plotting program. |
| `fortran-src/xfoil-6.996/sort.f` | Fortran source | 5601 | 644 | `ff0578cafa7f94c4e397cb9d79d464e0a65731e5e02980c203073c4148cc42aa` | Heap-sort utility. |
| `fortran-src/xfoil-6.996/spline.f` | Fortran source | 17525 | 644 | `03bb98d9cfedc530ea878d314669b153d89dff5273a8e2e33b0c38425c2d358e` | Spline interpolation. |
| `fortran-src/xfoil-6.996/userio.f` | Fortran source | 12349 | 644 | `a239ded06483038284f458a39e955126e465918b1ae43800f3d75d1ef99c92ab` | Interactive user input/output helpers. |
| `fortran-src/xfoil-6.996/x.ftnchek` | platform/build helper | 471 | 644 | `2ebc6d28ec89e4d48da322edf23e68a08d623cec8d14fecb2962cfa1eaaabd32` | ftnchek static-analysis invocation for the XFOIL sources. |
| `fortran-src/xfoil-6.996/xbl.f` | Fortran source | 46215 | 644 | `b87f9e4e31b56071a373861863526fb860eb9fc4964b3d953a4ca989cbef1941` | Boundary-layer driver. |
| `fortran-src/xfoil-6.996/xblsys.f` | Fortran source | 78120 | 644 | `04f15d3e0371982293ecf575a02c7939af1f0071ff5fd7731411dd8f48471cb4` | Boundary-layer equations. |
| `fortran-src/xfoil-6.996/xfoil.f` | Fortran source | 76879 | 644 | `d607e1a205ed9a9620589aa52d3a46f07fe4f312bb5c657b8b99d8c6216378e9` | Main program and top-level menus. |
| `fortran-src/xfoil-6.996/xgdes.f` | Fortran source | 73148 | 644 | `0c55705ea4ecad1ec0d3262f8707763e8c9f7645e327669a3a45e7ec62bbe484` | Geometry design (GDES). |
| `fortran-src/xfoil-6.996/xgeom.f` | Fortran source | 53789 | 644 | `d237678cf3b6d2c7386dfd036fa0338a9212a9de1dd6d6ee1efa64d7698ee3c7` | Geometry and paneling support. |
| `fortran-src/xfoil-6.996/xmdes.f` | Fortran source | 54572 | 644 | `809af28934d12154fb57431fd4588906dc4580b99b0b7549b198007a7327466c` | Complex mapping design (MDES). |
| `fortran-src/xfoil-6.996/xoper.f` | Fortran source | 94724 | 644 | `339c6bfc0eb3aa18cd48cae931850125b85d8e960457e83ec957046c164871bb` | OPER operating-point and sequence driver. |
| `fortran-src/xfoil-6.996/xpanel.f` | Fortran source | 51883 | 644 | `5a0467110dd07c0bc59e09dc3d878bed03652b5e5e285bd78c017e9c6fd435eb` | Inviscid panel method. |
| `fortran-src/xfoil-6.996/xplots.f` | Fortran source; plotting-related source | 40332 | 644 | `a9a3ea88537dbea9bb90d072ae90f26d59c6756d602a90f3d45b74f960a4cf54` | XFOIL plot routines. |
| `fortran-src/xfoil-6.996/xpol.f` | Fortran source | 26277 | 644 | `048c491773bd896d7325fa4e00928f1c18551283fae6ed9f45b42f979c5e3f8f` | Polar accumulation and file I/O. |
| `fortran-src/xfoil-6.996/xqdes.f` | Fortran source | 43772 | 644 | `e3c13b712d3c5d7c901cbd521f26a189a3890f02a49a97d054c0c846f01bbda0` | QDES surface-speed design. |
| `fortran-src/xfoil-6.996/xsolve.f` | Fortran source | 13810 | 644 | `a86a8e7bbd185ec6607f27fceb823e53c61685998f4bc415d641532b66a50bfb` | Inviscid solver. |
| `fortran-src/xfoil-6.996/xtcam.f` | Fortran source | 43167 | 644 | `7bc66b481630d1aaf6f919414aff994e8aca02d5e5479ea88eccc73774e2f90e` | Thickness/camber and geometry tools. |
| `fortran-src/xfoil-6.996/xutils.f` | Fortran source | 3246 | 644 | `a73b6e3fe8f986ab2f70486ae544851900d9cf8c14332870f1e9cbbcae4bd3b9` | Geometric stretching utilities. |

### File formats and line endings

| Property | Result |
|---|---|
| Symlinks | none |
| Empty files | none |
| NUL-containing files | none |
| Binary files in the source tree | none |
| CRLF files | none |
| Mixed line endings | none |
| Files without final newline | none |

All 49 files are ASCII text with LF line endings and a final newline.

Extension distribution:

| Extension | Count |
|---|---:|
| `.f` | 37 |
| `.INC` | 8 |
| `.c` | 1 |
| `.ftnchek` | 2 |
| no extension (`Makefile_gfortran`) | 1 |

Mode distribution:

| Mode | Count |
|---|---:|
| 644 | 49 |

All imported source files are non-executable. The diagnostic executable
`fortran-bin/xfoil-6.996` has mode `0755`.

### Source classifications

Primary classification (each file has exactly one primary class):

| Primary class | Count |
|---|---:|
| Fortran source | 37 |
| Fortran include | 8 |
| build description | 1 |
| platform/build helper | 3 |
| numerical data | 0 |
| documentation | 0 |
| unknown | 0 |

Overlapping role tags:

| Role tag | Count | Meaning |
|---|---:|---|
| plotting-related source | 13 | 11 `.f` plotting routines/programs plus `PPLOT.INC` and `PXPLOT.INC` |
| platform/build helper | 4 | `getosfile.c`, `p.ftnchek`, `x.ftnchek`, and the `getarg.f` wrapper |

The 37 Fortran sources split into 26 numerical/IO/infrastructure sources and 11
plotting-related sources. The 8 includes split into 6 state/numerical includes
and 2 plotting includes.

### Removed redundant variants

Two files imported in the first pass were removed in this structural closure
pass:

```text
fortran-src/xfoil-6.996/dplot1.f
fortran-src/xfoil-6.996/xqdes1.f
```

Read-only proof before removal:

- `grep -RniE 'dplot1|xqdes1' fortran-src/xfoil-6.996` returned no references
  other than the file names themselves.
- `grep -niE 'dplot1|xqdes1' Makefile_gfortran` returned no matches.
- The Makefile object lists contain `dplot.o` and `xqdes.o` only; rules build
  `dplot.o` from `dplot.f` and `xqdes.o` from `xqdes.f`.
- The diagnostic executable contains the source-name strings `../src/dplot.f`
  and `../src/xqdes.f`, not `dplot1.f` or `xqdes1.f`.
- Routine-set comparison:
  - `dplot1.f` has `DPLOT`, `PRPLOT`; it has **no** routine that is absent from
    `dplot.f`. `dplot.f` additionally has `FBLGET`.
  - `xqdes1.f` and `xqdes.f` declare the identical routine set.
- Therefore no technical routine exists exclusively in either variant, and the
  diagnostic executable was not built from them.

Both files remain present in the upstream archive and are now recorded as
deliberately omitted (see below).

## Upstream archive provenance

A local upstream archive was found outside the repository. Only non-sensitive
identity data is published here.

| Property | Value |
|---|---|
| Archive filename | `xfoil6.996.tgz` |
| Archive SHA-256 | `0feb71b58070d8514d830fcb0b609a10a11399d362493d7b9bbfb7343dff3f23` |
| Archive size | 673,006 bytes |
| Archive format | gzip-compressed tar (`tar.gz`) |
| Archive entries | 309 total (19 directories, 290 regular files) |
| Distribution identity | XFOIL 6.996; `Xfoil/src/xfoil.f` sets `VERSION = 6.996` |

Archive composition by top-level subtree (regular files only):

| Archive subtree | File count |
|---|---:|
| `Xfoil/plotlib` | 206 |
| `Xfoil/src` | 48 |
| `Xfoil/runs` | 18 |
| `Xfoil/cav` | 7 |
| `Xfoil` (root documents) | 5 |
| `Xfoil/osrc` | 3 |
| `Xfoil/bin` | 3 |

The archive hash was re-verified before restoration. No network access was used.

## Imported-file/archive comparison

Full per-file mapping is retained externally as `archive-source-map.tsv` with
the columns `repository_path`, `archive_path`, `repository_sha256`,
`archive_sha256`, `byte_identical`.

Result: **49 of 49 remaining imported files are byte-identical to their archive
entry.** There are no differences.

The restored build description matches:

| Repository path | Archive path | SHA-256 |
|---|---|---|
| `fortran-src/xfoil-6.996/Makefile_gfortran` | `Xfoil/bin/Makefile_gfortran` | `ece560720547fd0ce718fa33b6bacab96b43a0fa8ee80fe21f13344cfadbfc92` |

`cmp` confirms the file is identical to the archive entry. No compiler option
was inserted into this upstream file.

Mapping summary:

| Imported files | Archive source |
|---|---:|
| 47 | `Xfoil/src/*` |
| 1 | `Xfoil/osrc/osmap.f` |
| 1 | `Xfoil/osrc/getosfile.c` |
| 1 | `Xfoil/bin/Makefile_gfortran` |

Deliberately omitted archive entries:

| Archive entry | Reason |
|---|---|
| `Xfoil/osrc/x.log` | Upstream LaTeX/build log; not a source, include, build description, or data dependency. |
| `Xfoil/src/dplot1.f` | Redundant older variant; no routine absent from `dplot.f`; not in any Makefile target. |
| `Xfoil/src/xqdes1.f` | Redundant alternate; routine set identical to `xqdes.f`; not in any Makefile target. |

All 48 `Xfoil/src` files were originally imported; the two removed variants are
the only omissions from `Xfoil/src`. No file named `x.log` was created.

## Upstream Makefile

The repository contains `fortran-src/xfoil-6.996/Makefile_gfortran`, restored
byte-identically from the archive. The upstream archive also contains
`Xfoil/bin/Makefile` and `Xfoil/bin/Makefile_ifort`, which are not imported.

Observed settings in the retained build description:

| Property | Value |
|---|---|
| Active Fortran compiler | `FC = gfortran` |
| Active C compiler | `CC = cc` |
| Commented alternatives | `f77`, `g77`, `fort77`, `ifort` variants |
| Fortran flags | `FFLAGS = -O $(CHK) $(DBL)`, `FFLOPT = -O $(CHK) $(DBL)` |
| `CHK` | empty |
| `DBL` | `-fdefault-real-8` |
| C flags | `CFLAGS = -O -DUNDERSCORE` |
| Linker inputs | `$(PLTOBJ) $(PLTLIB) $(FTNLIB)` |
| Plot library | `../plotlib/libPlt_gDP.a` |
| X11 library | `-L/usr/X11R6/lib -lX11` |
| Extra Fortran library | `FTNLIB =` (empty) |
| Program targets | `xfoil`, `pplot`, `pxplot` |
| Primary target | `xfoil` |
| Source directories | `SRC = ../src`, `OSRC = ../osrc` |
| Install directory | `BINDIR = /home/codes/bin/` |
| Shell | `sh` |

Source groups:

- `XFOILOBJ`: `xfoil.o xpanel.o xoper.o xtcam.o xgdes.o xqdes.o xmdes.o
  xsolve.o xbl.o xblsys.o xpol.o xplots.o pntops.o xgeom.o xutils.o modify.o
  blplot.o polplt.o aread.o naca.o spline.o plutil.o iopol.o gui.o sort.o
  dplot.o profil.o`
- `PPLOTOBJ`: `pplot.o polplt.o sort.o iopol.o`
- `PXPLOTOBJ`: `pxplot.o plutil.o gui.o`
- `XUTILOBJ`: `userio.o`
- `OSOBJ`: `frplot.o ntcalc.o osmap.o getosfile.o` (the latter assignment
  overrides the earlier `frplot0.o`)

External paths and expected data:

- `../src` and `../osrc` sibling source directories;
- `../plotlib/libPlt_gDP.a`;
- X11 at `/usr/X11R6/lib`;
- `xfoil.def` at runtime (optional; the smoke run reported it as not found);
- an external OSMAP data file via `OSMAP`/`OSFILE`;
- the upstream README additionally describes an `orrs` step to build the
  Orr-Sommerfeld database.

Operating-system assumptions: Unix/POSIX `sh`, X-Windows, X11 development
libraries, and `install -s`.

Status of the reduced source set:

| Statement | Status |
|---|---|
| Upstream build description present | yes (`Makefile_gfortran`, byte-identical) |
| Reduced public source subset currently self-buildable | **no** |
| Locally built executable available separately | yes (`fortran-bin/xfoil-6.996`) |

Reasons the reduced set is not self-buildable:

- the sibling `../plotlib` directory and `libPlt_gDP.a` are absent;
- the flattened layout has no `../src` or `../osrc` directories;
- X11 headers/libraries and the hard-coded `/usr/X11R6/lib` path are assumed;
- the `pplot` and `pxplot` targets also require the plot library;
- the external OSMAP data file is not part of the source set;
- the `orrs` database-generation step is not included.

No dummy files were created and no upstream source was changed to make the build
succeed.

### Compiler compatibility note

The successful local build of the diagnostic executable used GNU Fortran 13.3.0.
For `xgdes.f` and `blplot.f` the option

```text
-fallow-argument-mismatch
```

was required. The cause is historical use of the same Fortran routine with
incompatible scalar and array arguments. The same compatibility need already
existed for XFOIL 6.99. This adjustment belongs in a future repository-owned
build adapter, not in the unmodified upstream `Makefile_gfortran`.

No claim is made here that a complete compile/link command line was recorded.
The requirement is documented; the exact command remains unproven.

## Plotting dependencies and reduced-source build status

The archive contains 206 files under `Xfoil/plotlib`. **None of them were
imported.** The source files under `fortran-src/xfoil-6.996/` that are tagged
plotting-related are application-level plotting sources from `Xfoil/src`, not
the plot library itself.

The following application-level plotting sources are retained for now:

```text
blplot.f  dplot.f  frplot.f  frplot0.f  gui.f  plutil.f
polfit.f  polplt.f  pplot.f  pxplot.f  xplots.f
PPLOT.INC  PXPLOT.INC
```

Policy:

- `plotlib/**` remains excluded.
- These application sources are retained for direct comparison with the 6.99
  baseline.
- Their later relevance to the non-graphical C++ integration is classified in
  U3.
- Mere plotting logic is not ported into XFOIL.next.

Deliberately not imported:

- the whole `Xfoil/plotlib` plot library, including `plt_base.f`, `plt_font.f`,
  `plt_color.f`, `plt_util.f`, `ps_subs.f`, `gw_subs.f`, and the Win32/PDF
  variants;
- `Xfoil/bin/Makefile` and `Xfoil/bin/Makefile_ifort`;
- `Xfoil/cav/**`, `Xfoil/runs/**`, and the root documents `README`,
  `version_notes.txt`, `xfoil_doc.txt`, `sessions.txt`, `smoothing.txt`;
- `Xfoil/osrc/x.log`.

Assessment:

- Plot library sources are absent.
- The upstream Makefile variants other than the imported gfortran description
  are absent.
- Examples, documentation, run data, and platform helpers are absent.
- All non-redundant upstream `Xfoil/src` sources are present.
- The numerical core sources from `Xfoil/src` and `Xfoil/osrc` are present, but
  build-level completeness cannot be proven because the plot library and
  OSMAP data are absent.

The imported set is intended to contain the non-graphical numerical sources.
Completeness relative to the upstream archive remains to be established by the
source-dependency inventory.

## `osmap.dat` status

Facts from the imported sources:

- `osmap.f` implements `SUBROUTINE OSMAP`, the Orr-Sommerfeld map interpolator.
- `ntcalc.f` calls `OSMAP`.
- `frplot.f` uses the OSMAP-based frequency data for plotting.
- `getosfile.c` reads the environment variable `OSMAP`.
- The literal in `osmap.f` is:

  ```text
  DATA OSFILE / '/var/local/codes/orrs/osmap.dat' /
  ```

- A commented alternative literal is
  `/afs/athena.mit.edu/course/16/16_d0006/Codes/orrs/osmap_lx.dat`.
- `osmap.f` opens the file with `STATUS='OLD'` and reports
  `OSMAP: Orr-Sommerfeld database file not found:` on failure.
- `osmap.f` uses a lazily loaded external file; it is not embedded data.

File presence:

| Question | Answer |
|---|---|
| `osmap.dat` in the 6.996 subset? | **0** |
| Other `.dat` files in the 6.996 subset? | **0** |
| `osmap.dat` in the upstream archive? | **0** |
| `osmapDP.dat` in the upstream archive? | **0** |
| `orrs` directory in the upstream archive? | **0** |
| Does the routine expect an external file? | **Yes**, via `OSMAP` or the compiled-in `OSFILE` |

The item remains deliberately open. Next diagnostic points:

- establish the origin of the data from the external ORRS process;
- treat format compatibility with the 6.99 data as unknown;
- do not copy or generate data before U4/U5;
- keep the existing 6.99 OSMAP data and the C++ reader unchanged.

The startup/QUIT smoke run exited 0, printed the banner, and reported no OSMAP
error. This only shows that startup and immediate exit did not access or require
the file. OSMAP is loaded on first use, so this does **not** show that the file
is unnecessary during analysis. No `strace` or OSMAP runtime diagnosis was
performed in U0.

No 6.99 OSMAP data was copied, no format compatibility is claimed, no new data
was generated, and the C++ reader is unchanged.

## XFOIL 6.996 executable

### Identity and hash

| Property | Value |
|---|---|
| Path | `fortran-bin/xfoil-6.996` |
| Size | 860,704 bytes |
| Mode | 0755 |
| SHA-256 | `000b0468d9a7f4fce42d6fac24bd14147d7d326c8a9035909c8e9f98d180f50e` |
| File modification time | 2026-11-05 15:59:01 +0100 |

### Binary format

| Property | Value |
|---|---|
| Format | ELF 64-bit LSB |
| Type | PIE executable (`DYN`) |
| Architecture | x86-64 |
| Endianness | little-endian |
| Interpreter | `/lib64/ld-linux-x86-64.so.2` |
| Minimum OS | GNU/Linux 3.2.0 |
| Linking | dynamically linked |
| Stripped | not stripped |
| Debug sections | none (no `.debug*` sections) |
| Build ID | `1f07dff6b67a3bf0235e21e1ac957cbd8b5f6d4a` |
| RPATH | none |
| RUNPATH | none |
| `.comment` compiler | `GCC: (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0` |

### Dynamic dependencies

From `readelf -d` / `objdump -p` (authoritative static view):

| NEEDED library |
|---|
| `libX11.so.6` |
| `libgfortran.so.5` |
| `libm.so.6` |
| `libgcc_s.so.1` |
| `libc.so.6` |

`ldd` (supplementary) additionally resolves the transitive X11 libraries
`libxcb.so.1`, `libXau.so.6`, `libXdmcp.so.6`, `libbsd.so.0`, and `libmd.so.0`.

Plot/X11 dependency: the executable directly needs `libX11.so.6`. The plot
library is linked statically into the executable; there is no dynamic `libPlt`
dependency. Plot-related strings and symbols are present in the binary.

### Banner

Controlled smoke run: executed in an external temporary directory with a
minimal `QUIT` input, a 20-second timeout, and captured stdout/stderr. Exit
code was 0.

```text
Upstream distribution name: XFOIL 6.996
Observed executable banner: XFOIL Version 7.00
```

Exact captured banner text:

```text
 ===================================================
  XFOIL Version 7.00
  Copyright (C) 2000   Mark Drela, Harold Youngren

  This software comes with ABSOLUTELY NO WARRANTY,
    subject to the GNU General Public License.

  Caveat computor
 ===================================================
```

The source sets `VERSION = 6.996` and prints it with format `F5.2`, which rounds
to `7.00`. The executable therefore identifies itself as XFOIL 7 while the
distribution is XFOIL 6.996.

### Provenance

| Property | Value |
|---|---|
| Compiler family | GNU Fortran |
| Compiler version | 13.3.0 |
| Evidence | builder statement; ELF `.comment` identifies GCC 13.3.0 |

The only provenance statement made here is:

> Locally built from the upstream XFOIL 6.996 source distribution using its
> original build description and plotting dependencies.

Still open:

- the complete exact compile/link command is **not independently recorded**.

Status:

- diagnostic executable;
- not yet canonical;
- not yet reproducibly built from the reduced subset.

The executable is **not** claimed to be distributed by upstream, **not** claimed
to be reproducible from the reduced repository source subset, and **not** claimed
to be numerically accepted.

## 6.99 and 6.996 executable distinction

Metadata only; no numerical result comparison was performed.

| Property | `fortran-bin/xfoil` | `fortran-bin/xfoil-6.996` |
|---|---|---|
| Intended upstream baseline | 6.99 | 6.996 |
| Status | canonical | diagnostic |
| SHA-256 | `f5779d6954d0b8fdc152e0e0633f18732e00b7b69228d561d629bd524bba598c` | `000b0468d9a7f4fce42d6fac24bd14147d7d326c8a9035909c8e9f98d180f50e` |
| Size | 940,720 bytes | 860,704 bytes |
| Mode | 0775 | 0755 |
| Format | ELF 64-bit LSB PIE | ELF 64-bit LSB PIE |
| Architecture | x86-64 | x86-64 |
| Dynamic dependencies | `libX11.so.6`, `libgfortran.so.5`, `libm.so.6`, `libmvec.so.1`, `libgcc_s.so.1`, `libc.so.6` | `libX11.so.6`, `libgfortran.so.5`, `libm.so.6`, `libgcc_s.so.1`, `libc.so.6` |
| Build ID | `a979c0ed7e305c72c57983d057474c6504483349` | `1f07dff6b67a3bf0235e21e1ac957cbd8b5f6d4a` |
| `.comment` compiler | GCC 11.4.0 | GCC 13.3.0 |
| Banner | `XFOIL Version 6.99` | `XFOIL Version 7.00` |

## Protected data

Confirmed unchanged by this pass:

- all existing `fortran-src/**` 6.99 files (top-level hashes identical before
  and after);
- `fortran-src/SHA256SUMS` (SHA-256 unchanged);
- `fortran-bin/xfoil` (SHA-256 unchanged);
- the Fortran 6.99 SHA contract in `fortran-src/SHA256SUMS`;
- no golden file;
- no reference file;
- no tolerance;
- no Windows golden;
- product version remains `1.0.0`;
- `README.md` and `XFOIL.next.png` are unchanged in this pass;
- no product banner change.

## Commands executed

Representative commands, all read-only except the documented structural edits
and this document:

```text
git status --short
git diff --check
git diff --name-status
git diff --cached --name-status
git ls-files --others --exclude-standard
git log --oneline --decorate -8
git tag --list
git remote -v
git rev-parse HEAD
git rev-parse origin/main

sha256sum <protected files>
stat -c '%a %n' <protected files>
sha256sum <local archive>          # re-verified before extraction

find fortran-src/xfoil-6.996 -printf '%y\t%m\t%s\t%P\n' | LC_ALL=C sort
find fortran-src/xfoil-6.996 -type f -print0 | LC_ALL=C sort -z | xargs -0 sha256sum
find fortran-src/xfoil-6.996 -type f -printf '%m\t%P\n'
find fortran-src/xfoil-6.996 -type l
find fortran-src/xfoil-6.996 -type f -empty
grep -qP '\x00' <each file>
grep -c $'\r' <each file>
tail -c 1 <each file> | od -An -tu1
file -- <each file>

tar -tzf <local-archive>
tar -xzf <local-archive> -C <external-audit-area> ...
sha256sum <repository files> <archive entries>
cmp <archive Makefile_gfortran> <repository Makefile_gfortran>

grep -RniE 'dplot1|xqdes1' fortran-src/xfoil-6.996
grep -niE 'dplot1|xqdes1' fortran-src/xfoil-6.996/Makefile_gfortran
grep -nE '^(XFOILOBJ|PPLOTOBJ|PXPLOTOBJ|XUTILOBJ|OSOBJ|PROGS)' fortran-src/xfoil-6.996/Makefile_gfortran
grep -nEi '^[[:space:]]*(SUBROUTINE|FUNCTION|ENTRY|PROGRAM|BLOCK DATA)' <variant and primary>
strings -a fortran-bin/xfoil-6.996 | grep -iE 'dplot|xqdes'

find fortran-src/xfoil-6.996 -type f -exec chmod 0644 {} +
chmod 0755 fortran-bin/xfoil-6.996
find fortran-src/xfoil-6.996 -type f ! -perm 0644 -print

grep -RniE 'osmap|OSMAP|OPEN[[:space:]]*\(|INQUIRE[[:space:]]*\(' fortran-src/xfoil-6.996
find fortran-src/xfoil-6.996 \( -iname '*osmap*' -o -iname '*.dat' \) -printf '%P\n'
grep -nEi 'plot|x11|lib|osmap|gfortran|ifort|fort|fflags|ldflags|xfoil' fortran-src/xfoil-6.996/Makefile_gfortran

stat fortran-bin/xfoil-6.996
sha256sum fortran-bin/xfoil-6.996
file fortran-bin/xfoil-6.996
readelf -h fortran-bin/xfoil-6.996
readelf -d fortran-bin/xfoil-6.996
readelf -S fortran-bin/xfoil-6.996
readelf -p .comment fortran-bin/xfoil-6.996
readelf -n fortran-bin/xfoil-6.996
objdump -p fortran-bin/xfoil-6.996
ldd fortran-bin/xfoil-6.996
strings -a fortran-bin/xfoil-6.996
timeout 20s <binary> < input.txt   # QUIT-only smoke run, external work dir

make check-fortran-sha
make check-public-docs
make check-release-inventory
make check-license-headers
```

Hash and metadata work products are retained outside the repository under the
U0-f1 audit area, including `pre-source-sha256.txt`,
`post-source-sha256.txt`, `archive-source-map.tsv`, `table.md`, and the
pre/post protected-file records.

## Measurements

| Measurement | Value |
|---|---:|
| 6.996 source files | 49 |
| 6.996 source bytes | 1,154,891 |
| Files with mode 644 | 49 |
| Files with other modes | 0 |
| Archive entries | 309 |
| Archive regular files | 290 |
| Imported files matching archive | 49 |
| Imported files differing from archive | 0 |
| Deliberately omitted `Xfoil/src` files | 2 (`dplot1.f`, `xqdes1.f`) |
| Deliberately omitted `Xfoil/osrc` files | 1 (`x.log`) |
| `fortran-bin/xfoil-6.996` bytes | 860,704 |
| `fortran-bin/xfoil-6.996` mode | 0755 |
| NEEDED shared libraries | 5 |
| Banner smoke exit code | 0 |

## Test and gate results

| Command | Exit | Result |
|---|---:|---|
| `git diff --check` | 0 | clean |
| `make check-fortran-sha` | 0 | all listed 6.99 sources and data files OK |
| `make check-public-docs` | 0 | OK (17 public documents checked) |
| `make check-release-inventory` | 0 | OK (source modes normalised) |
| `make check-license-headers` | 0 | PASS: version-aware manifest gate green |
| `make check-dependencies` | 0 | PASS |
| `make core-link-check` | 0 | PASS |
| `make session-link-check` | 0 | PASS |

`make check-license-headers` now passes. The former global fixed notice-count
assumption was replaced by the explicit manifest
`tools/fortran-provenance.json`:

- `xfoil-6.99`: 48 files, 23 notice-bearing files;
- `xfoil-6.996`: 49 files, 23 notice-bearing files;
- each version is checked separately for the exact file inventory, SHA-256 and
  notice expectation.

The decision is no longer based on a combined total. A wrong per-version notice
assignment fails even when the combined notice count stays unchanged.

Unit and core test evidence measured in this pass:

| Command | Result |
|---|---|
| `make test-core` | 414/414 tests passed |
| `make test-unit` | 1668/1668 tests passed (includes `LicenseHeaderGateTests`) |

## Open questions

1. What exact compiler invocation and link flags produced
   `fortran-bin/xfoil-6.996`? `.comment` proves GCC/GNU Fortran 13.3.0 only.
2. How is the missing upstream `osmap.dat` expected to be produced? The archive
   has no `orrs` directory and no data file. Compatibility with the 6.99 format
   is unknown.
3. In U3, which of the retained plotting-related application sources carry
   non-graphical logic that must be compared with the 6.99 baseline.

Resolved in this pass and no longer open:

- the local patch to `Makefile_gfortran` (restored byte-identically);
- executable bits on imported source files (normalised to `0644`);
- the redundant `dplot1.f` and `xqdes1.f` variants (removed with proof);
- the missing `x.log` documentation (recorded as deliberately omitted);
- the version-aware, manifest-based `check-license-headers` provenance gate.

## Git status

After this pass, the working tree contains the pre-existing modifications, the
pre-existing untracked import, and the untracked inventory document. Nothing was
staged or committed.

```text
 M README.md
 M XFOIL.next.png
?? docs/xfoil-6.996/
?? fortran-bin/xfoil-6.996
?? fortran-src/xfoil-6.996/
```

`docs/xfoil-6.996/` contains the pre-existing untracked `README.md` and the
updated untracked `00-inventory.md`; both remain untracked.

## Exact next allowed step

U0 is now `PASS`. The next step is an independent, read-only acceptance audit:

```text
X6996-U0-R1-Independent-Inventory-and-Freeze-Acceptance
```

Only after that audit may U1 begin:

```text
X6996-U1-Versioned-XFOIL-6.99-Source-Layout
```

U1 may only move the existing 6.99 source set to `fortran-src/xfoil-6.99/` and
must prove complete byte identity plus:

```text
Reference 236/236
Determinism 236/236
Harnesses 16/16
```

No source-difference analysis and no C++ integration are allowed in U0 or U1.
The XFOIL.next banner/header change is explicitly not part of U0 or U1; it is a
separate product-identity and documentation pass to be planned once the set of
actually integrated 6.996 changes is defined.
