# XFOIL 6.996 — cumulative final acceptance

Status: PASS — `X6996-CUMULATIVE` and `X6996-1.1.0-RC-ACCEPTANCE` accepted.

Acceptance markers:

```text
X6996-CUMULATIVE: PASS
X6996-1.1.0-RC-ACCEPTANCE: PASS
```

Final integrated source state before this acceptance record:

```text
HEAD d3927e7639921353d6766428d1c74fc1431a37d5
subject "Treat Windows batch transcript length as path noise"
```

This document decides the cumulative XFOIL 6.996 integration scope. It does
not push, tag, release or change the product version. It is followed by one
documentation-only acceptance commit.

## Scope

The accepted scope is the cumulative XFOIL 6.996 integration through S22,
G-EXPORT, S14 DXF, S16-DXF and the cumulative acceptance corrections. The
scope excludes:

- `NIMP-01`: NTS, LPPORT and NSPL are not ported;
- `NIMP-02`: SVG is not ported;
- `G-PLOT-HEADLESS`: open and deferred; non-blocking for this acceptance per
  U6;
- product version 1.1.0 and its release preparation, which remain a separate
  product-identity/release pass.

The accepted product remains XFOIL.next 1.0.0. The hidden `DXF` command is
active, `SVG` is absent, and the post-S18 text contract has no project-specific
`OFS:` record.

## Gate status

| Gate | Decision |
|---|---|
| `G-POLAR-SAVE` | `PRESERVE-CORRECT-EXISTING-FORMAT` |
| `G-DEFAULTS` | `ACCEPT` |
| `G-TEXT-OFS` | `REMOVE-NON-UPSTREAM-OFS-OUTPUT` |
| `G-NACA-BUFFER` | `ACCEPT-XFOIL-6996` |
| `G-BL-SIDEFILE` | `PRESERVE-EXISTING-BLDUMP-AND-ADD-VERSIONED-BLDUMP3` |
| `G-OSMAP` | `PROVEN-6.99-COMPATIBILITY` |
| `G-EXPORT` | `ACCEPT-DXF-ONLY` |
| `NIMP-01` | NTS, LPPORT, NSPL not ported |
| `NIMP-02` | SVG not ported |
| `G-PLOT-HEADLESS` | open, deferred, non-blocking per U6 |

## Linux acceptance

Canonical Linux baseline: Ubuntu 24.04, GCC/G++ 13.3.0, C++20.

| Check | Result |
|---|---|
| GCC 13.3 Debug, full suite | 2121/2121 |
| GCC 13.3 Release, full suite | 2121/2121 |
| GCC 16.2 Debug, full suite | 2121/2121 |
| GCC 16.2 Release, full suite | 2121/2121 |
| Clang 18.1.3 Debug, full suite | 2121/2121 |
| Clang 18.1.3 Release, full suite | 2121/2121 |
| ASan/UBSan, full suite | 2121/2121 |
| Core suite | 465/465 |
| Unit group | 1769/1769 |
| Integration group | 297/297 |
| Regression group | 5/5 |
| Batch group | 47/47 |
| Reference verification | 236/236 |
| Reference determinism | 236/236 |
| Harness families | 16/16 |
| Valgrind | PASS |
| Permanent gates | PASS |

The GCC 16.2 core suite was also 465/465. Permanent gates cover dependency
scope, release inventory, license headers, public documentation, Fortran source
integrity and both core/session link checks.

## Windows acceptance

Accepted Windows baseline:

- Windows 11, build `10.0.26100.9711`;
- MSYS2 UCRT64;
- GCC/G++ 16.1.0 Rev5;
- GNU Make 4.4.1;
- Bash 5.3.9.

The accepted run used an exact clean source export of commit `d3927e7`, with no
Git worktree. Export identity:

```text
commit d3927e7639921353d6766428d1c74fc1431a37d5
tree   71c671898f5132b338ae4446be9e673cc10bbb2b
zip    xfoil-next-d3927e7-clean.zip
sha256 b3c34788471f988ec7900793b05f658060545cefd9d3536cb2345632009d9cde
```

| Check | Result |
|---|---|
| Debug build | PASS |
| Release build | PASS |
| Core r1/r2/r3 | 465/465 each |
| Unit | 1769/1769 |
| Integration | 297/297 |
| Regression r1/r2/r3 | 5/5 each |
| Batch | 47/47 |
| Release full suite | 2121/2121 |
| PACC diagnostic | 1/1 |
| Binary contract | PASS |

The binary contract confirms `xfoil.next.exe` in debug and release, version
`XFOIL.next 1.0.0`, and absence of `xfoil.exe`. Windows evidence run:

```text
run-20261113-231448-6321
```

Earlier Windows runs with a dirty worktree or with a non-exact source copy were
rejected and are not acceptance evidence.

## Pre-acceptance corrections

The following test/tooling commits are part of the integrated source state but
are not product changes. They were made before this documentation-only
acceptance record.

| Commit | Purpose |
|---|---|
| `b07d594` | Link the app support sources required by core test fixtures; update Windows expected totals |
| `fe9d057` | Avoid GCC 16 null-dereference warning in the S12 malformed-dump test |
| `0a79426` | Harden Windows evidence capture and correct the PACC diagnostic filter |
| `40987b0` | Correct the Windows batch contract and normalize CRLF before removing the injected RDEF block |
| `d3927e7` | Treat Windows batch transcript length as platform path noise |

No product source, Fortran source, reference, golden or DXF fixture changed in
these corrections.

## Protected data

```text
fortran-bin/xfoil
f5779d6954d0b8fdc152e0e0633f18732e00b7b69228d561d629bd524bba598c

fortran-bin/xfoil-6.996
000b0468d9a7f4fce42d6fac24bd14147d7d326c8a9035909c8e9f98d180f50e
```

No files under `tests/golden`, `tests/fortran-reference`, `tests/data`,
`fortran-src` or `fortran-bin` changed between the S16-DXF commit `1e59139`
and final accepted HEAD `d3927e7`.

## Version and release boundary

```text
Product version: 1.0.0
v1.0.0 tag object: dc7305b057286574cd137cb6f1af170b74650ec5
v1.0.0 target:     58bca7d17f847e1aef6b1c8d3d4e167f40f69945
origin/main:       58bca7d17f847e1aef6b1c8d3d4e167f40f69945
local main:        49 commits ahead, 0 behind
```

No push, fetch, amend, reset, rebase, tag or release was performed. The 1.1.0
version bump and release preparation remain a separate pass.

## Decision

The cumulative XFOIL 6.996 integration scope is accepted on Linux and Windows.
The final acceptance record is allowed to contain only this document and its
report. No product, test, fixture, golden, reference, build or tooling change
is allowed in the acceptance-record commit.

## Next allowed step

Create the documentation-only final acceptance commit and stop there. Do not
push, tag or release. Product version 1.1.0 is a separate follow-up decision.
