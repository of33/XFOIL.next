# XFOIL.next 1.2.0 release preparation

Status: release preparation complete; publication is not authorized.

Marker:

```text
XFOILNEXT-1.2.0-RELEASE-PREP: PASS
```

This is the public release-preparation record for XFOIL.next 1.2.0. It records
the exact tested source basis, the version-only changes, the focused Windows
binary/packaging check and the local Git state. It does not create a tag, push
or publish a release.

## 1. Tested source basis

| Item | Value |
| --- | --- |
| Start HEAD | `27fb0227400be5430368a4d1e87d390f9e5fd2af` |
| Start tree | `9a2f01ea4c69a151b61cdedb807a1a7b79e6068d` |
| Product | XFOIL.next |
| Product version | 1.2.0 |
| Product banner | `XFOIL.next 1.2.0` |
| POSIX binary | `xfoil.next` |
| Windows binary | `xfoil.next.exe` |
| Library | `libxfoil_next` / `xfoil_next.dll` |
| Namespace | `xfoil` |
| C++ standard | C++20 |
| License | GPL-3.0-or-later |

The cumulative Linux and Windows validation basis is recorded in
[VALIDATION.md](VALIDATION.md). The release preparation changes only product
version metadata, version-dependent expectations and current release
documentation.

## 2. Version changes

| Area | Change |
| --- | --- |
| Canonical product version | `src/app/ProductInfo.h`: 1.1.0 → 1.2.0 |
| Product identity test | `tests/unit/ProductInfoTests.cpp`: 1.2.0 |
| Windows workflow | current counts and `XFOIL.next 1.2.0` expectation |
| Windows evidence script | current counts and 1.2.0 binary expectation |
| CLI banner goldens | version token only |
| Native batch transcript goldens | version token only |
| Native batch results metadata goldens | version token only |
| Golden metadata note | version token only |
| README | release identity, current counts and validation baseline |
| VALIDATION | 1.2.0 baseline and tested source basis |
| CHANGELOG | final 1.2.0 section |
| CITATION.cff | version 1.2.0, date 2026-11-18 |
| CLI/batch/public API docs | current version examples |
| Building and testing | current 1.2.0 test counts |

The product name, binary names, library artifact names, namespace, C++ standard,
license, attribution to XFOIL 6.996 and the concept DOI are unchanged.

## 3. Version-only golden changes

Twenty-six banner/batch goldens and one golden metadata note changed by the
product version token only. No numerical value, convergence path, tolerance or
other text changed.

| Path | Old SHA-256 | New SHA-256 | Diff |
| --- | --- | --- | --- |
| `tests/golden/batch/clark_y_results.dat` | `9258a05bc72cc260785a9cc1f0ca3366936ada19cd213d97863a11ed68a64b8d` | `30c1f137dc117ef3212549076110ccdda1ed49e7963d030efd45991ad25b166b` | 1 line (version token only) |
| `tests/golden/batch/clark_y_transcript.txt` | `aca9518480bbd512df118d5d525dec000037ec4f8ca233c5cef1e82ae8931f1a` | `1f3566a8a957eb793cc9b8f8c4b3cc2cdb8e7a54466c104d81c8c5d92790d409` | 1 line (version token only) |
| `tests/golden/batch/naca0012_results.dat` | `c08b61c32e12356e094ed3a911d4ef0aa78ce90c13c9b1f47b4331da4de0f11c` | `d3bf5017fd12831f0fc2a924520977acc7e310214da7620e1c25cd533bc2a2b2` | 1 line (version token only) |
| `tests/golden/batch/naca0012_transcript.txt` | `9ac05012dcec57b965b10e2da704c919c9f61c28722d19b8596700e204ebab2e` | `c2bc9ec8909c8b36027147cea25bfe47bfcfd8dc5c0bb5f333d849e2b1457d2f` | 1 line (version token only) |
| `tests/golden/batch/naca2312_results.dat` | `369920c46e514e37f15a4420e0178119b908f99f25c3eb561d4c1d5c706b28d3` | `d15ba746350b7064e1ddb2285cb5b4d31a4e0808e98f659dd2bccaea69dd742b` | 1 line (version token only) |
| `tests/golden/batch/naca2312_transcript.txt` | `d1b62bb07482a75b43a0c8313412f52e4c255c89c8eab7adf3aa73e8e7090e5c` | `32674bb8ea215e1840a251185e73aeacc3c7e18b8ccaedf51b589cd4d0735553` | 1 line (version token only) |
| `tests/golden/batch/naca4415_results.dat` | `ade6a6be8212dadb4344d234453d60692a7a234287d1336a7b582fe1b91ddb9b` | `82b08db0aa2591da7207aee6eb319de1488b3d98fe5134d891489958bad00e7e` | 1 line (version token only) |
| `tests/golden/batch/naca4415_transcript.txt` | `a1b1768ba0c9ece1297f14760e7db4a69ca8175b5f648550c640ba1e8e9a7e3e` | `bfdccc970136b0adb0bacbbe87a16bf07d8a2d5bb862be5ebb9f848a27853bb3` | 1 line (version token only) |
| `tests/golden/cli/file-inte-pcop-quit.stdout.txt` | `bac5f82ade3a634a743649eeaf235387f5897a886a5881802802efd6bca5a5ef` | `18b628cb51274842ea1bf966afdede706c086e2523ca661c1712c268b45a32a3` | 1 line (version token only) |
| `tests/golden/cli/file-load-clark-y-quit.stdout.txt` | `c5ba5c014923cec126f5642f40bde2e0114c30ad051b18620a559f5693d0e1b0` | `03d11b3485ced2c4c487d0f5a1c80ada4060b4153c68766562c7df05fd3a4171` | 1 line (version token only) |
| `tests/golden/cli/file-naca-save-load-quit.stdout.txt` | `2b5114d0aea5b407f9a9b11d897d1249c4838b13e519feee3636c450e53c1598` | `4bcf5c203c9095e1f98479d19702d7c839957bef171bf0e109fc3f68a7d40272` | 1 line (version token only) |
| `tests/golden/cli/file-name-ninc-xycm-quit.stdout.txt` | `f7420834ad1331f7ed3fde67f6d877d84875e151e5d993bcef842d54fbf8200b` | `8565461ef2554f5ff8fde4e05aee7533bacec0542d964d0b7442025890d319bf` | 1 line (version token only) |
| `tests/golden/cli/file-norm-reve-deli-quit.stdout.txt` | `337f328fb917d1b1437a71f321a647f52aa793b70f6689e40e08c94d37001dee` | `730ee60d6d4a887d4409a076aa25f8ed6b6d72365ba17076428a6215c19a0b8a` | 1 line (version token only) |
| `tests/golden/cli/help-quit.stdout.txt` | `3ebd20999369f29ee35e056521e9772f6c0be0ea78dba8cf59074fd790d3c38c` | `7d4342a12da05ec936b0d6ccb30704a7dfd4daeb48fc6732b7239af0688e5ca9` | 1 line (version token only) |
| `tests/golden/cli/oper-help.stdout.txt` | `6a7c113f7a73319ed9c14aa6a3abea9b2f10e1cae5f21a0507e91be33e864491` | `d1ecd25e28c1d67fa2f4197de85e9140678cfdfcc9bbbdad770706be69df139e` | 1 line (version token only) |
| `tests/golden/cli/oper-inviscid-alpha.stdout.txt` | `1e11968ca4565a23c38b3aed141176a223a81be549a0f054ba169636a1c3ae70` | `cf1d6b3adb6fa8bb7b9115057a04c18cf1510dc28a3184d7cba7a39a58c83096` | 1 line (version token only) |
| `tests/golden/cli/oper-sequence.stdout.txt` | `1e11968ca4565a23c38b3aed141176a223a81be549a0f054ba169636a1c3ae70` | `cf1d6b3adb6fa8bb7b9115057a04c18cf1510dc28a3184d7cba7a39a58c83096` | 1 line (version token only) |
| `tests/golden/cli/oper-viscous-alpha.stdout.txt` | `9a1e11e440ed9ab38618cb06f89d1e264c95f701da6de388e47b9272733e2379` | `1b77ba1fe1afa84181e4731844be8d4f7c1b8d574e3151b2186db9eed1b91f19` | 1 line (version token only) |
| `tests/golden/cli/pane-loaded-quit.stdout.txt` | `4df90bc6060c1a2dabd92ea16b5b0429eaf77a202026f9cdde33e2ee245af32b` | `0189b9d7457d7ea6152077e22754d64d7755ea7371fa65264349719e79009bbc` | 1 line (version token only) |
| `tests/golden/cli/plot-unavailable-quit.stdout.txt` | `fbc430e7638e0676387cd634131116c1e74edb4b565e654bcc725205b5ee589f` | `c4c8bf1d28548afaef4171a7a2267a3662570400a7294fae52e4f1f8e784136a` | 1 line (version token only) |
| `tests/golden/cli/ppar-change-apply.stdout.txt` | `f66e6d5e43895402a3e273fa015c6e4f812ad5f0bbfb2ae50d05c809e9a9085d` | `6ee248ecd2d37c9545202e74a993a60f5b5934ad7dc7bfe264712b62a8bc9272` | 1 line (version token only) |
| `tests/golden/cli/ppar-plot-unavailable.stdout.txt` | `31c438ff594e906d15c9850b2d85aa5069a7b816b12101308a0f27bba3cf17a0` | `ef4800a72febaf4ef52daf0ba6f85f95d3e477de51ae12f973fbae8797c53a4a` | 1 line (version token only) |
| `tests/golden/cli/ppar-show-return.stdout.txt` | `c0ddf53c845965eac01243129c4d696cd9889f3a1869110ffb6847746244d124` | `c4e15230162cabb9fcdd33d900d43956414040bd2cb7dc77b8644a84355b947e` | 1 line (version token only) |
| `tests/golden/cli/startup-quit.stdout.txt` | `6877a65985901cf9c45157c56e8a9203b19258404cd4d5e845b1d6e00504d100` | `0ccc5827440c83626bbd08c040149fbc6dc26f05a63d6b6c6e8f92edd2c61bc6` | 1 line (version token only) |
| `tests/golden/cli/unknown-quit.stdout.txt` | `205ceeaf2f505687c5df4381015712386e6de8be16b9483e5d8df062d0f5ebcf` | `c37e8a33dcfd936928d652d542d0f6733dfbd414adaace34d54f7263ea606dc6` | 1 line (version token only) |
| `tests/golden/cli/vpar-show.stdout.txt` | `6b6050e9a97593dbe9d56e1f5561c796f63de1a34a3e43a8d205ff28460c79cc` | `12f0c528ae50a23ec69701f3dfd865d17bcadda717f396d8a2608aa30c2b86c8` | 1 line (version token only) |
| `tests/golden/metadata.md` | `3fd07c37d33a0991e487a39b1d4951a9e9435198144d87124a128e40a3562617` | `8f109a83a5790536e23034cf1881a9e91714fa1a876a0e4279f73ea621cc2f59` | 2 lines (version token only) |

## 4. Citation metadata

| Field | Value |
| --- | --- |
| CFF schema version | `1.2.0` |
| Software version | `1.2.0` |
| Release date | `2026-11-18` |
| License | `GPL-3.0-or-later` |
| DOI | `10.5281/zenodo.24046163` (Zenodo concept DOI) |
| Repository | `https://github.com/of33/XFOIL.next` |

Title, authors, ORCID, license, repository URL, project URL and the concept DOI
are unchanged. No version-specific DOI is claimed. The CFF YAML and required
fields were validated locally.

## 5. Changelog and public documentation

`CHANGELOG.md` contains the final `1.2.0 — 2026-11-18` section covering the
public C++20 in-memory API, `Session`, `analyze`, owning results, progress and
cancellation, Cf and operating-point completeness, scalar sequence completeness,
static/shared library artifacts, installation and consumption, pkg-config,
`library-demo`, the Library User Manual, `CITATION.cff`, no-I/O, session
isolation, full CLI parity and the supported limits.

`README.md`, `VALIDATION.md`, `docs/building-and-testing.md`, `docs/cli.md`,
`docs/batch-mode.md` and `docs/public-api.md` are consistent with version 1.2.0.
The `Planned Work` section still contains only the pre-existing open topics.

## 6. Manual

The final manual is accepted unchanged:

| Item | Value |
| --- | --- |
| PDF | `XFOIL.next-Library-Manual.pdf` |
| SHA-256 | `e5c2aec673a71bf63110251f72f83f87107b9c0e17581fe53bab356ac88995f9` |
| Pages | 109 |
| Manual edition | 1.2.0 |

The LaTeX sources under `docs/xfoil.next-library/` were not changed and the PDF
was not rebuilt during release preparation.

## 7. Linux release checks

The release preparation ran on the versioned source:

```text
make clean
make test
make release
build/release/xfoil_tests
make library
make test-consumers
make test-library-demo
make library-link-check
make session-link-check
make check-fortran-sha
make check-license-headers
make check-dependencies
make check-library-dependencies
make check-release-inventory
make check-public-docs
```

Results:

```text
Debug full suite:       2266/2266
Release full suite:     2266/2266
Debug --version:        XFOIL.next 1.2.0
Release --version:      XFOIL.next 1.2.0
Static/shared consumers: PASS
library-demo:           PASS 5/5
Library link check:     PASS
Session link check:     PASS
Permanent gates:        PASS
pkg-config version:     1.2.0
```

## 8. Focused Windows check

Exactly one focused UCRT64 binary/packaging process ran on an exact export of
the uncommitted release-preparation state.

```text
Windows:          Windows 11, build 10.0.26100.9974
Environment:      MSYS2 UCRT64
Toolchain:        GCC/MinGW 16.1.0 Rev5
GNU Make:         4.4.1
Base HEAD:        27fb0227400be5430368a4d1e87d390f9e5fd2af
Base tree:        9a2f01ea4c69a151b61cdedb807a1a7b79e6068d
Export tree:      7c9815fab973b4661d47e67c2e05c5f81a13bed8
Diff SHA-256:     55520f87bdda7dca9e18181adedad17f0de96906b7243242cd3dd25811af747f
Result:           PASS
```

The process checked Debug and Release builds, `xfoil.next.exe`, the absence of
`xfoil.exe`, both `--version` outputs (`XFOIL.next 1.2.0`), static/DLL/import
artifacts, the staged pkg-config version (`1.2.0`), `CITATION.cff` version and
date, the unchanged manual PDF hash, static/shared consumers, static/shared
`library-demo` and the demo integration tests. No full Windows suite was rerun,
no Windows golden was added and no tolerance was changed. This report file was
added after the Windows check; the tracked technical and version state is
unchanged.

Evidence:

```text
external audit area: library-1.2.0/release-prep/
Windows handoff SHA-256:  8445c58a43f2f6698cb808aeae00dbf4a63c7dd32480babbda5716680acebebe
Returned logs SHA-256:    d69af9ce04f14a3a6f7a77b0c9f480dd25eea1f1ac28ca27ff558560c89953c4
```

## 9. Protected state

No change was made to solver, session, CLI or library semantics; Fortran
sources, Fortran binaries, patches, reference data, numerical tolerances or the
manual/PDF. The only product source change is the canonical version in
`src/app/ProductInfo.h`. No Windows goldens or tolerances were added.

## 10. Git and publication state

```text
Branch:                    main
Parent:                    27fb0227400be5430368a4d1e87d390f9e5fd2af
Tag v1.2.0:                absent
Push/fetch/amend/rebase:   none
Publication:               none
```

The release-preparation commit is local. Publication, tagging and archival
remain separate owner-authorized steps.

## 11. Decision

The local XFOIL.next 1.2.0 release preparation is complete and green.

```text
XFOILNEXT-1.2.0-RELEASE-PREP: PASS
```

The next task is the separate XFOIL.next 1.2.0 publication authorization.
