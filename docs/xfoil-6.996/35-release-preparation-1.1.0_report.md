# XFOIL.next 1.1.0 — local release preparation report

Status: PASS — preparation only; not published.

Marker:

```text
XFOILNEXT-1.1.0-RELEASE-PREP: PASS
```

Technical context:

```text
Start HEAD:                 f177618f6b4781494d688a8a07d5279cd043ae43
Technical test basis:       d3927e7639921353d6766428d1c74fc1431a37d5
Cumulative acceptance:      11dcb3ad6c4cf6be33167ab4fcacf06bd0e0477c
Acceptance documentation:   f177618f6b4781494d688a8a07d5279cd043ae43
Product version:            1.1.0
Product name:               XFOIL.next
POSIX binary:               xfoil.next
Windows binary:             xfoil.next.exe
```

> **Post-publication provenance errata (2026-11-14).** The XFOIL 6.99 source
> tree was later restored to the original upstream state and 21 source-based
> harness references were corrected for historical local `OFS:` output.
> Statements in this historical report about `fortran-src/**` or 6.99
> reference data remaining unchanged describe the pre-correction basis. See
> [the 6.99 provenance correction](36-xfoil-699-source-provenance-correction_report.md).

This is a local release-preparation record. It does not create a tag, push,
publish or otherwise release the product. Version 1.0.0 remains the published
tag.

## 1. Release scope

The 1.1.0 release identifies the accepted cumulative XFOIL 6.996 integration.
The technical matrix was accepted on `d3927e7`; release preparation changes
only version metadata, version-dependent banner/CI contract expectations and
current release documentation.

Explicit non-goals remain:

- SVG: `NIMP-02`;
- NTS, LPPORT, NSPL: `NIMP-01`;
- headless plot backend: `G-PLOT-HEADLESS`, open and deferred.

The release does not claim that all graphical 6.996 functionality is
integrated.

## 2. Version locations

| Path | Before | After | Reason |
|---|---|---|---|
| `src/app/ProductInfo.h` | `1.0.0` | `1.1.0` | Canonical product version source |
| `tests/unit/ProductInfoTests.cpp` | `1.0.0` | `1.1.0` | Product identity contract |
| `tools/windows/build-and-test-msys2.sh` | `1.0.0` | `1.1.0` | Binary-contract expectation |
| `.github/workflows/windows-diagnostic.yml` | `1.0.0` | `1.1.0` | Hosted binary-contract expectation |
| `tests/golden/cli/*.stdout.txt` | `1.0.0` | `1.1.0` | Targeted product banner goldens |
| `tests/golden/batch/*_transcript.txt` | `1.0.0` | `1.1.0` | Targeted product banner goldens |
| `tests/golden/batch/*_results.dat` | `1.0.0` | `1.1.0` | Targeted product metadata goldens |
| `tests/golden/metadata.md` | `1.0.0` | `1.1.0` | Current golden documentation |
| `README.md` | `1.0.0` | `1.1.0` | Current public version and counts |
| `VALIDATION.md` | `1.0.0` | `1.1.0` | Current validation baseline and counts |
| `docs/cli.md` | `1.0.0` | `1.1.0` | Current banner example |
| `docs/batch-mode.md` | `1.0.0` | `1.1.0` | Current results metadata example |
| `docs/building-and-testing.md` | `1.0.0` | `1.1.0` | Current test counts |
| `CHANGELOG.md` | no 1.1.0 entry | 1.1.0 entry | Accepted integration scope |

Historical 1.0.0 release reports, slice states, `v1.0.0` references and
Fortran oracle banners remain unchanged.

## 3. Release inventory and build metadata

`tools/check-release-inventory.py` has no product-version field. Its contract
is file presence, protected hashes, runtime-artifact hygiene and file modes.
No release-inventory script or manifest required a version edit.

The Makefile has no product-version literal. The product version is read from
`src/app/ProductInfo.h` by the banner, `--version`, batch metadata and the
tests.

The Windows diagnostic workflow counts were brought to the accepted cumulative
totals: core 465, unit 1769, integration 297, release full 2121, and the PACC
filter was aligned with the current test name.

## 4. Targeted banner-golden changes

The product version is part of the canonical CLI banner and the native batch
metadata. The following goldens were changed only in the exact version token;
no golden was regenerated.

Cause for every row: product version `1.0.0` -> `1.1.0`.

### CLI stdout banner goldens

| Path | Old SHA-256 | New SHA-256 | Diff |
|---|---|---|---|
| `tests/golden/cli/file-inte-pcop-quit.stdout.txt` | `5b360efd9a9ca0c820ede43a0c35cd97ebf22af0305c29cbf4238d1b25237d43` | `6c2a433e2b76fbe4beaf53670bfed89c0510a63cb88895d273845062289e10cf` | 1 line |
| `tests/golden/cli/file-load-clark-y-quit.stdout.txt` | `00cd3ae17ecfc7f9a71f39c51708f73914ba08bd7642ee9b81cd61ab60d225cb` | `17c279b750a4ac5bbb93b4f84e5b874f984cd20d9c8129c9690aea638ad09153` | 1 line |
| `tests/golden/cli/file-naca-save-load-quit.stdout.txt` | `009fd9bb275a7727bb9866cb727a74f65b1b6a59efc0cb44429a257199a9b3d7` | `29d07ca4cac150e5bed6ceaca436db7fcd8b53700f60c290333328fdc0cf1d76` | 1 line |
| `tests/golden/cli/file-name-ninc-xycm-quit.stdout.txt` | `cdf49a4cf8ba6be95a9d4540ec267a40b3191b55da3479b006ee7b4b202649f9` | `260331289ff7fb32b05b19d54c617d96930bc9874ba7e6158d5854c51956a110` | 1 line |
| `tests/golden/cli/file-norm-reve-deli-quit.stdout.txt` | `33751df0b30660689f4adb54a71b46102d876047a374a461ae43f9853fa23826` | `946efe861c60a36488e77c3740b853028ed1fee13598db1be3efed9b71607e72` | 1 line |
| `tests/golden/cli/help-quit.stdout.txt` | `ce3693cc58ea4928a7f0b39c0b1949fa91b4c6b1132a9f32a2f92df8de9ba9a5` | `10ad170af129d551549746a8fc519c27752a7ecc78c9a7a31f547f52e85ce2a6` | 1 line |
| `tests/golden/cli/oper-help.stdout.txt` | `3ea568e817c15c326574c21b560dd513168efc9e9bb6f0ce9613aa810e8de11e` | `2d084165e2b5e79ca95a8e0f07d20a049da690e0d992bde0cdf68d24c891b7c2` | 1 line |
| `tests/golden/cli/oper-inviscid-alpha.stdout.txt` | `7e8cc3d13392e6d333e827d223039330ad46c8cd2423c4d583fb48a31e1ea5f3` | `c766fcb295e32d4bb9cb72bec5c5dc39aef06d494f0e3307beab1b757c9ea837` | 1 line |
| `tests/golden/cli/oper-sequence.stdout.txt` | `7e8cc3d13392e6d333e827d223039330ad46c8cd2423c4d583fb48a31e1ea5f3` | `c766fcb295e32d4bb9cb72bec5c5dc39aef06d494f0e3307beab1b757c9ea837` | 1 line |
| `tests/golden/cli/oper-viscous-alpha.stdout.txt` | `263c18fb8c9e5433ccbf58f292695590aab24da86db45667b8b3d6c6e0d61b24` | `7488c63a1be98febd7c196befc8a366ab5f0dccf7b01459f662ebaec89f59e5c` | 1 line |
| `tests/golden/cli/pane-loaded-quit.stdout.txt` | `fb247428d0212e76149567a77593fa8fad2ab62ad8b1bf09f3e6c441b1497b2f` | `00d8380bd8f88f01437479d0242033820507772fd7019b7599ae7f33336aed28` | 1 line |
| `tests/golden/cli/plot-unavailable-quit.stdout.txt` | `3fb4ddda2221aec3d6e845252ffa0cf32f26122580097ddc6cda4b7d6aa3b907` | `c7e12aa6ef9bcb877e0a4dea84b81d41b5cb480a5e15c6144e5dd0cc6487381e` | 1 line |
| `tests/golden/cli/ppar-change-apply.stdout.txt` | `154d012ce4288596c810e4c7c2887fc415f8ecb9ec95093f39e782b5cf8aab49` | `4f26cd0f91c48905240eebfba6816672de0f7319ec979330dadef7256184acd4` | 1 line |
| `tests/golden/cli/ppar-plot-unavailable.stdout.txt` | `05bea6ba4089b3f8078608e49f03438c5b36bf388fe37fd1c3f3a8cf18d87ada` | `8ddeb0470814b916d6d5f285ebdf027eab954544a98599a4f5a8c4a29cb00490` | 1 line |
| `tests/golden/cli/ppar-show-return.stdout.txt` | `c72bd20cbcd6888f43b56a119c39f6aec10a5050228f5f9a3c9c24b15cbd6992` | `5a18245fdbfa2ee58801a138b1b630e35a28a1673bb79ee930725b25171c38f3` | 1 line |
| `tests/golden/cli/startup-quit.stdout.txt` | `3e99c27389dd92a9a6cab9a67226ebdd7ebf7f724e74987393184b9c03cf0815` | `5bfa922e394bff3e63dcee3051827d3aa3200668b1cc0c953f0b7d62c492aeb7` | 1 line |
| `tests/golden/cli/unknown-quit.stdout.txt` | `c7663d94e5b7e58a08a943c22d9f8c7c988d96550e41abfeffc463197f5f4f23` | `c3587fcb85b17883db0c2d5bbb6272f4c5cd6b24e4a8a803941ee707b7812db9` | 1 line |
| `tests/golden/cli/vpar-show.stdout.txt` | `04fdabdefd6b8bb9399a4f6d13685bb7d03ca0742e19dab09946b1e63125e7a8` | `2bf3af0f8b114c851f568650c6fce2c193bd20c49c9cc2312a64765f219a42f0` | 1 line |

### Native batch transcript banner goldens

| Path | Old SHA-256 | New SHA-256 | Diff |
|---|---|---|---|
| `tests/golden/batch/clark_y_transcript.txt` | `7d59cf46281b5eca647ef3a5ecd16ff276f49429b74a78bff44e5543b172bd3e` | `60cd5e6606d1a4362dc2de022b4ec84ea4a26a9084ef3435867b88465f0ee3ee` | 1 line |
| `tests/golden/batch/naca0012_transcript.txt` | `78ee40a93e1daf3e2af4ee430b74d9e3934ccdc184bc93312b5dd2e7bb7ae5d8` | `c44295ee66d0f873e65490e9baeb55d98c5925b7849a8868416be8d475376ff7` | 1 line |
| `tests/golden/batch/naca2312_transcript.txt` | `20ba089d03dbafe195d26bab13f5e377fbdcfbba52b5e78bb219fe719666d328` | `c48e763280c95989d0df7aa5ec2c49abfb62c8d859a966679fb10c7833e6428c` | 1 line |
| `tests/golden/batch/naca4415_transcript.txt` | `645a34e9ec54a6ace9c15393cc1205e75a2b4c409e193afb172f2e07ef06289a` | `6ca2eb19fd25fcf9ea59305b10d824a3005695f8cf1e4a92f42280b6e383904d` | 1 line |

### Native batch results metadata goldens

| Path | Old SHA-256 | New SHA-256 | Diff |
|---|---|---|---|
| `tests/golden/batch/clark_y_results.dat` | `a6bb5ffda566ebddcb5f19605da9a48d4320d01907904c0e8e8f71dabd472325` | `9258a05bc72cc260785a9cc1f0ca3366936ada19cd213d97863a11ed68a64b8d` | 1 line |
| `tests/golden/batch/naca0012_results.dat` | `5b69428ec762c894e9dfc7775181257d0618f87ffc7eee0fd50c169638c0c800` | `c08b61c32e12356e094ed3a911d4ef0aa78ce90c13c9b1f47b4331da4de0f11c` | 1 line |
| `tests/golden/batch/naca2312_results.dat` | `8941a4f17d1c5fba0c8a7cc9098603caaabdbcf7b99bbf8122c8c4cce6054fab` | `369920c46e514e37f15a4420e0178119b908f99f25c3eb561d4c1d5c706b28d3` | 1 line |
| `tests/golden/batch/naca4415_results.dat` | `457145ff2338745a4cc9e9ad713292e9baac7263d340967db43244b34c356e95` | `ade6a6be8212dadb4344d234453d60692a7a234287d1336a7b582fe1b91ddb9b` | 1 line |

## 5. Linux release-preparation checks

The accepted cumulative numerical matrix was not rerun in full. The required
release-preparation checks on the versioned source were:

```text
make clean
make test
make release build/release/xfoil_tests
build/release/xfoil_tests
```

Results:

```text
GCC 13.3 Debug full suite:     2121/2121
GCC 13.3 Release full suite:   2121/2121
Debug --version:               XFOIL.next 1.1.0
Release --version:             XFOIL.next 1.1.0
Debug xfoil.next present:      yes
Release xfoil.next present:    yes
xfoil.exe absent:              yes
```

Permanent gates:

```text
Fortran source integrity:      PASS
License headers:               PASS
Release inventory:             PASS
Public documentation:          PASS
Dependency scopes:             PASS
```

Logs remain in the external audit area
`xfoil-6.996/release-preparation-1.1.0/logs/`.

## 6. Windows focused binary-contract process

The repository has no focused Windows binary-contract mode; the existing
Windows evidence script always starts the complete matrix. Per instruction,
the complete matrix was not rerun.

Exactly one manually authorized UCRT64 process was run on an exact export of
the uncommitted release-preparation source. It only built Debug and Release
and checked the binary contract. It did not run tests, harnesses, references,
batch cases, golden regeneration or Git commands.

Accepted baseline observed:

```text
Windows:       Windows 11 build 10.0.26100
MSYS2:         UCRT64
GCC/G++:       16.1.0 Rev5
GNU Make:      4.4.1
```

Result:

```text
Debug build:                         PASS
Release build:                       PASS
Debug xfoil.next.exe present:        yes
Release xfoil.next.exe present:      yes
xfoil.exe absent:                    yes, Debug and Release
Debug --version:                     XFOIL.next 1.1.0
Release --version:                   XFOIL.next 1.1.0
Full Windows matrix:                 not rerun; accepted on d3927e7
Windows goldens added:               none
Windows tolerances added:            none
XFOILNEXT-1.1.0-WINDOWS-BINARY-CONTRACT: PASS
```

Windows log: external audit area
`xfoil-6.996/release-preparation-1.1.0/07-windows-binary-contract.log`.

## 7. Export identity

The exact source export used for the Windows process was built from start HEAD
`f177618` plus the full uncommitted release-preparation diff:

```text
Base HEAD:                f177618f6b4781494d688a8a07d5279cd043ae43
Base tree:                5bd2922f851ca91b207c7317e8486642bc172693
Uncommitted diff SHA-256: 61a6cdb4c76f4966599e226980ad942293e8813a349282fae073905fae381fcb
Exact source archive:     75276fa762c2b9234241f18248986676de59f034843ed55195122963012811d0
Transfer bundle:          3d7468b97de1531b698a9442c5625410327bb29082f52570779149178c319250
```

The export contained no build artifacts, Git metadata or unrelated untracked
files. Every tracked file in the export was byte-identical to the working tree
at export time. After the Windows process only this report was added; all
product, build, test, version, golden and binary-contract files remain
identical to the exported source.

## 8. Protected data

Unchanged:

- `fortran-src/**`;
- `fortran-bin/xfoil` and `fortran-bin/xfoil-6.996`;
- `patches/**`;
- `tests/fortran-reference/**`;
- canonical 6.99 references and fixtures;
- existing Windows golden policy and tolerances.

Protected binary hashes:

```text
f5779d6954d0b8fdc152e0e0633f18732e00b7b69228d561d629bd524bba598c  fortran-bin/xfoil
000b0468d9a7f4fce42d6fac24bd14147d7d326c8a9035909c8e9f98d180f50e  fortran-bin/xfoil-6.996
```

## 9. Git and publication state

```text
Start HEAD:                 f177618f6b4781494d688a8a07d5279cd043ae43
Previous acceptance:        51 commits ahead of origin/main
Product version:            1.1.0 in the release-preparation worktree
Tag v1.0.0:                 unchanged
Tag v1.1.0:                 absent
Push/fetch/amend/rebase:    none
Release/publication:        none
```

The release-preparation commit is local. Its creation is the only remaining
step in this task.

## 10. Final decision

The local 1.1.0 release-preparation state is complete and green. The accepted
numerical basis is unchanged, the versioned binaries report
`XFOIL.next 1.1.0`, and no publication has occurred.

```text
XFOILNEXT-1.1.0-RELEASE-PREP: PASS
```

## 11. Next possible task

The next separately authorized task is:

```text
XFOIL.next 1.1.0 publication authorization
```

It has not started. Before publication, the owner must decide whether and when
to push, whether to tag `v1.1.0`, and whether to create a GitHub release. This
report does not authorize any of those actions.
