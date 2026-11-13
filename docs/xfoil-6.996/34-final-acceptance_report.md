# XFOIL 6.996 — cumulative final acceptance report

Status: PASS — `X6996-CUMULATIVE` and `X6996-1.1.0-RC-ACCEPTANCE` accepted.

## 1. Current status

Cumulative final acceptance is complete for the integrated XFOIL 6.996 scope.
The accepted source state is `d3927e7` before the documentation-only acceptance
record. Linux and Windows matrices are green, protected data is unchanged, and
the repository remains local and unpublished.

## 2. Scope and evidence basis

The accepted scope covers S01 through S22, G-OSMAP, G-EXPORT, S14 DXF and
S16-DXF, with the gate decisions recorded in `34-final-acceptance.md`.

Accepted evidence:

- Linux logs: external audit area
  `xfoil-6.996/final-acceptance/logs/20261113-d3927e7/`;
- Windows logs: external audit area
  `xfoil-6.996/final-acceptance/windows-runs/d3927e7-clean/run-20261113-231448-6321/`;
- Windows source export: external audit area
  `xfoil-6.996/final-acceptance/xfoil-next-d3927e7-clean.zip`.

The Windows export was created with `git archive` from commit `d3927e7`; it
contains no Git worktree and records its commit and tree in `EXPORT-INFO.txt`.
The acceptance run reported `environment: PASS` and `Overall: PASS`.

Earlier Windows runs with a dirty Git worktree or a copied non-exact source
tree were rejected and are not acceptance evidence.

## 3. Final source state

```text
HEAD d3927e7639921353d6766428d1c74fc1431a37d5
parent 40987b09159914c5ee7389ad47e7affaa37d52b9
tree 71c671898f5132b338ae4446be9e673cc10bbb2b
subject "Treat Windows batch transcript length as path noise"
origin/main 58bca7d17f847e1aef6b1c8d3d4e167f40f69945
ahead/behind 49/0
v1.0.0 tag object dc7305b057286574cd137cb6f1af170b74650ec5
v1.0.0 target 58bca7d17f847e1aef6b1c8d3d4e167f40f69945
```

Worktree, index and untracked state were clean before this report was written.

## 4. Commands executed

Linux acceptance used the canonical targets:

```text
make clean
make test
make test-core
make test-unit
make test-integration
make test-regression
make test-batch
make release build/release/xfoil_tests
build/release/xfoil_tests
make test-sanitize
make test-reference
make test-reference-determinism
make test-harnesses
make test-valgrind
make check-dependencies
make check-release-inventory
make check-license-headers
make check-public-docs
make check-fortran-sha
make core-link-check
make session-link-check
```

GCC 16.2 used separate build roots and the same full-suite, core-suite and
group targets. Clang 18.1.3 used separate debug and release build roots.

Windows acceptance used:

```text
tools/windows/build-and-test-msys2.sh <external-log-dir> <export-dir>
```

The exact clean export and Windows evidence run are identified above.

## 5. Linux measurements

```text
GCC 13.3 Debug full suite       2121/2121
GCC 13.3 Release full suite     2121/2121
GCC 16.2 Debug full suite       2121/2121
GCC 16.2 Release full suite     2121/2121
Clang 18.1.3 Debug full suite   2121/2121
Clang 18.1.3 Release full suite 2121/2121
ASan/UBSan full suite           2121/2121
Core suite                       465/465
Unit group                      1769/1769
Integration group                297/297
Regression group                   5/5
Batch group                       47/47
Reference verification           236/236
Reference determinism            236/236
Harness families                  16/16
Valgrind                          PASS
Permanent gates                   PASS
```

No sanitizer, Valgrind, dependency, inventory, license, public-documentation or
Fortran-integrity finding remains open.

## 6. Windows measurements

```text
Windows 11 build 10.0.26100.9711
MSYS2 UCRT64
GCC/G++ 16.1.0 Rev5
GNU Make 4.4.1
Bash 5.3.9
```

```text
Debug build                       PASS
Release build                     PASS
Core r1/r2/r3                   465/465 each
Unit                            1769/1769
Integration                      297/297
Regression r1/r2/r3               5/5 each
Batch                             47/47
Release full suite              2121/2121
PACC diagnostic                    1/1
Binary contract                   PASS
Environment                       PASS
Overall                           PASS
```

The binary contract verified `xfoil.next.exe` in debug and release with
`XFOIL.next 1.0.0`, and verified that `xfoil.exe` is absent.

## 7. Windows acceptance corrections

The full Windows matrix exposed test/tooling assumptions that did not hold on
the accepted Windows toolchain. They were corrected in separate commits, not
in the final acceptance record:

| Commit | Correction |
|---|---|
| `b07d594` | Core test link set and cumulative Windows expected totals |
| `fe9d057` | GCC 16 null-dereference warning in the S12 malformed-dump test |
| `0a79426` | Windows evidence capture, dirty-tree reporting and PACC filter |
| `40987b0` | Windows batch contract based on `results.dat`; CRLF normalization before RDEF stripping |
| `d3927e7` | Windows batch transcript length treated as platform path noise |

The Windows batch contract now checks the banner, crash markers, final line and
the authoritative `results.dat`. Converged result rows are compared field-wise;
non-converged rows are checked structurally because their values are outside
the solved contract. The obsolete post-S18 `OFS:` requirement was removed.

These corrections changed no product source, Fortran source, golden, reference
or DXF fixture.

## 8. Protected data

```text
fortran-bin/xfoil
f5779d6954d0b8fdc152e0e0633f18732e00b7b69228d561d629bd524bba598c

fortran-bin/xfoil-6.996
000b0468d9a7f4fce42d6fac24bd14147d7d326c8a9035909c8e9f98d180f50e
```

S14 DXF fixture hashes remain:

```text
dxf-naca0012-240.dxf  a0c0d13cdf9de48bc01cced1299282ce146a217794c041631c26960037e5eaed
dxf-naca2412-240.dxf  f6bae1a2d674c3eb3b4f36de16583604a5a4c3befadc9584480d94271c4c3edf
dxf-nacs0012-240.dxf  9be1f3eac93159fbbc125c11007de63caca5578f992b7bdb362a0c684dd61482
dxf-empty.dxf         818684425bb64f551a01a01a669e1c8945a06dd1d6309b1480e21db177c254c2
```

No golden, reference, Fortran source or DXF fixture changed between `1e59139`
and `d3927e7`.

## 9. Git status

At the accepted source state:

```text
HEAD d3927e7
worktree clean
index clean
untracked 0
ahead 49
behind 0
```

No push, fetch, amend, reset, rebase, tag or release was performed. The final
acceptance record is documentation-only. The product version remains 1.0.0.

## 10. Open points

- `G-PLOT-HEADLESS` remains open and deferred; U6 makes it non-blocking for
  this cumulative acceptance.
- `NIMP-01` remains an explicit non-port decision for NTS, LPPORT and NSPL.
- `NIMP-02` remains an explicit non-port decision for SVG.
- Product version 1.1.0, the release inventory/version update, the release
  preparation and any tag/release decision are outside this acceptance.

## 11. Final decision

PASS. The cumulative XFOIL 6.996 integration scope is accepted on the canonical
Linux baseline, the additional GCC 16.2 and Clang 18.1.3 Linux validation, and
the accepted Windows 11 / MSYS2 UCRT64 / GCC 16.1.0 baseline.

## 12. Exact next allowed step

Create the documentation-only final acceptance commit containing this report
and `34-final-acceptance.md`. Do not push, tag or release. Stop after the
acceptance commit; 1.1.0 remains a separate follow-up.
