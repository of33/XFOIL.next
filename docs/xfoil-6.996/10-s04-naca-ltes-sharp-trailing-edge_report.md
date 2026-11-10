# XFOIL 6.996 — S04 NACA LTES and sharp-trailing-edge kernel report

Status: PASS - `X6996-S04` and `X6996-S04-R1` accepted. Decision:
`X6996-S04: PASS`.

## Starting state

- HEAD before S04: `6abf12fd825ba6940cc6f189a4ec7c12e561af22`.
- Ahead/behind before S04: `0 15`.
- Tag `v1.0.0` unchanged.
- Working tree before S04: clean, 0 staged.
- Audit data: external only,
  `.audit-xfoil-next/xfoil-6.996/s04/`.

## Fortran diff and LTES contract

Compared source: `fortran-src/xfoil-6.99/naca.f` versus
`fortran-src/xfoil-6.996/naca.f`, plus the `NACA` wrapper in `xfoil.f`.
Diffs:

- `.audit-xfoil-next/xfoil-6.996/s04/naca.diff`
- `.audit-xfoil-next/xfoil-6.996/s04/xfoil.diff`

| Routine/contract | XFOIL 6.99 | XFOIL 6.996 | Effect |
|---|---|---|---|
| `NACA4` signature | `(IDES,XX,YT,YC,NSIDE,XB,YB,NB,NAME)` | `(...,NAME,LTES)` | explicit sharp-TE flag |
| `NACA5` signature | `(IDES,XX,YT,YC,NSIDE,XB,YB,NB,NAME)` | `(...,NAME,LTES)` | explicit sharp-TE flag |
| sharp-TE constant | none | `XTES = 1.00893041136514270931482193883054` | thickness argument at TE |
| thickness | `YT(XX)` | `YT(XX*XTES)` iff `LTES` | TE gap closes when true |
| camber/buffer | `YC(XX)`, buffer from `XX/YC/YT` | unchanged | no camber/order change |
| `NACA` wrapper | `NACA(IDES1)` | `NACA(IDES1,LTES)` | caller selects TE contract |
| existing `NACA` command | no flag | passes `.FALSE.` | open-TE behavior retained |
| new `NACS` command | absent | passes `.TRUE.` | kernel only in S04 |
| `NSIDE` formula | `IQX/3` | `IQX/3` | unchanged |
| NACA buffer | `370/3 -> 123/245` | `601/3 -> 200/399` | capacity side effect, not NACA formula |
| point distribution | TE-bunched `XX`, `NB=2*NSIDE-1` | unchanged | no per-NSIDE distribution change |
| 4/5-digit validity | `ITYPE`, `N543=210..250` | unchanged | same error ranges |

All normal (`LTES=.FALSE.`) outputs are byte/field-identical between the 6.99
and 6.996 harness runs for the tested cases; only the 6.996 sharp path adds
the `XTES` thickness argument.

## Scope: existing NACA versus NACS

The 6.996 wrapper uses one kernel with two callers:

- existing `NACA`: `CALL NACA(IINPUT(1),.FALSE.)`;
- new `NACS`: `CALL NACA(IINPUT(1),.TRUE.)`.

S04 prepares the `LTES=.TRUE.` kernel and `NACA(IDES1,LTES)` signature, but
does not register `NACS`. There is no `src/app/cli/**` change, no menu/help
text, no prompt/redo/bang change and no CLI golden for `NACS`.

## C++ mapping

| Fortran | C++ routine | Callers | Tests | S04 change |
|---|---|---|---|---|
| `NACA4` | `core::geometry::naca4` | `nacaAirfoil`, tests | `GeometryNaca4*` | `LTES` bool and `XTES` branch |
| `NACA5` | `core::geometry::naca5` | `nacaAirfoil`, tests | `GeometryNaca5*` | same |
| `NACA(IDES1,LTES)` | `core::geometry::nacaAirfoil` | `handleNacaCore`, tests | `GeometryNacaAirfoil*` | optional bool, existing callers keep `false` |
| `NACA` command | `handleNacaCore` | CLI | CLI/regression | unchanged, open TE |

The public parameter is named `sharpTrailingEdge`, maps directly to Fortran
`LTES`, and defaults to `false`. No second NACA implementation was added and
the numeric core has no CLI dependency.

## Normal versus sharp path

Normal: `x = XX`; formula, camber, ordering and point count are unchanged.

Sharp: `x = XX*XTES`; the thickness distribution vanishes at the TE. Camber
and buffer point positions remain based on `XX`. The normal path remains the
existing NACA command contract.

## 399-/245-point decision

`G-NACA-BUFFER` remains pending. The 399-point buffer follows from
`IQX=601` / `NSIDE=IQX/3=200`, not from the NACA formula change.

S04 therefore:

- supports `nside=200` in the kernel and verifies `NB=399`;
- keeps the existing NACA command at `NSIDE=123` / `NB=245`;
- does not activate any default, command or point-count change.

399-point product activation remains deferred to `G-NACA-BUFFER` and the
NACS/command integration (S15 or a separate explicit decision).

## Harness H-S04-NACA-LTES

Source/output:
`.audit-xfoil-next/xfoil-6.996/s04/harness/`.

Cases at `NSIDE=123`; 6.99 supplies only the normal-TE control contract.
Sharp target values come from 6.996.

| Case | N | TE gap | max thickness @ x | max camber @ x |
|---|---:|---:|---:|---:|
| NACA4 0012 normal | 245 | 2.5199999999999554e-3 | 0.12003207898370065 @ 0.29749550101842870 | -5.8980598183211441e-17 @ 0.014776720928247184 |
| NACA4 0012 sharp | 245 | -3.3306690738754695e-17 | 0.12003449863607675 @ 0.29749550101842981 | 6.9388939039072284e-17 @ 0.88083310663491843 |
| NACA4 2412 normal | 245 | 2.5199999999999554e-3 | 0.12003208369899164 @ 0.29749567771631857 | 0.019999450869281215 @ 0.40318958280910333 |
| NACA4 2412 sharp | 245 | -3.3306690738754695e-17 | 0.12003450290498287 @ 0.29749566856972637 | 0.019999452316196244 @ 0.40318958149393869 |
| NACA5 23012 normal | 245 | 2.5199999999999554e-3 | 0.12003208797899201 @ 0.29749588969381840 | 0.018381723477353389 @ 0.14621022974616818 |
| NACA5 23012 sharp | 245 | -3.3306690738754695e-17 | 0.12003450763826617 @ 0.29749509609299629 | 0.018381727916315555 @ 0.14621029441886532 |
| NACA4 0012 normal | 399 | 2.5199999999999554e-3 | 0.12003404356207915 @ 0.30088316229660095 | 1.2490009027033011e-16 @ 0.043064363686545391 |
| NACA4 0012 sharp | 399 | -3.3306690738754695e-17 | 0.12003177554550842 @ 0.29472446684914366 | 1.0755285551056204e-16 @ 0.063848605997653990 |

Selected interior point checks for the 245-point cases:

- 0012 normal: `PT_MID(61) = 0.39219291414903468, 0.058311508418170989`;
  `PT_LOWER_MID(184) = 0.38128156646177092, -0.058672581611098097`.
- 0012 sharp: `PT_MID = 0.39219291414903468, 0.058187602518426570`;
  `PT_LOWER_MID = 0.38128156646177092, -0.058564012249496103`.
- 2412 normal/sharp and 23012 normal/sharp are checked against the same
  harness output in `unit.NacaLtesContracts`.

All cases: LE `(0,0)`, TE upper `(1,YTE+)`, TE lower `(1,YTE-)`, positive
signed-area orientation, no exact endpoint duplicate, all values finite.

## Tests and current numbers

Changed/added focused tests:

- `unit.NacaLtesContracts` (NACA4/5 normal+sharp, TE gap, point count,
  selected points, thickness/camber, orientation, finiteness, 399 kernel).
- sharp invalid-designation and too-small-capacity checks added to the
  existing `GeometryNaca5IllegalDesignationFails`,
  `GeometryNaca4OutOfRangeFails` and `GeometryNacaTooSmallNsideFails`.
- existing NACA4/NACA5/NACA-Airfoil normal-path tests remain unchanged and
  green.

No tolerance was increased. All target values are from the external 6.996
harness.

## Linux matrix

Toolchain: GCC 13.3.0, Clang 18.1.3, Valgrind 3.26.0.

| Check | Result |
|---|---|
| GCC 13 Debug `make test` | 1983/1983 pass |
| GCC 13 Release | 1983/1983 pass |
| Clang 18 Debug | 1983/1983 pass |
| Clang 18 Release | 1983/1983 pass |
| `make test-core` | 423/423 pass |
| `make test-unit` | 1683/1683 pass |
| `make test-integration` | 245/245 pass |
| `make test-regression` | 5/5 pass |
| `make test-batch` | 47/47 pass |
| `make test-sanitize` (ASan/UBSan) | 1983/1983 pass |
| `make test-valgrind` | pass, no definite leaks |
| focused NACA Valgrind (5 tests) | pass, no definite leaks |
| `make test-reference` | 236/236 pass |
| `make test-reference-determinism` | 236/236 pass |
| `make test-harnesses` | 16 families, 0 failures |
| Gates | dependencies, Fortran SHA, release inventory, license, public docs, core/session link checks pass |

No `src/app/cli/**` change. Existing `a02`/`a03` contracts and all NACA
normal-path tests remain green.

## Windows focused run

Source: user-provided Windows 11 / MSYS2 UCRT64 console output for the single
focused S04 process recorded in
`.audit-xfoil-next/xfoil-6.996/s04/windows-s04-commands.txt`.

The first invocation selected no tests because only the CLI had been built.
After building the test target, the required focused process passed:

```bash
make -j4 build/debug/xfoil_tests
./build/debug/xfoil_tests unit.NacaLtesContracts
```

```text
[ RUN      ] unit.NacaLtesContracts
[       OK ] unit.NacaLtesContracts
0 failure(s) in 1 test(s)
```

The process covers NACA4/NACA5 normal+sharp, point counts, TE gap and
coordinates, selected interior points, thickness/camber maxima,
orientation/finiteness and the deferred 399-point kernel observation. No full
Windows matrix was run for S04.

## Changed paths

- `src/core/geometry_core/Naca.cpp`
- `include/xfoil/core/geometry/Naca.h`
- `tests/core/GeometryTests.cpp`
- `docs/xfoil-6.996/10-s04-naca-ltes-sharp-trailing-edge.md`
- `docs/xfoil-6.996/10-s04-naca-ltes-sharp-trailing-edge_report.md`

No change to `fortran-src/**`, `fortran-bin/**`, `patches/**`,
`tests/golden/**`, canonical `tests/fortran-reference/**`, formats,
CLI/command registration, product version or banner.

## Git state and publication

The local S04 commit is created with this accepted scope:

```text
Integrate XFOIL 6.996 NACA sharp-trailing-edge kernel
```

No push, new tag or release before the cumulative XFOIL.next 1.1.0 acceptance.

## S04 checklist

- [x] Fortran NACA4/NACA5/NACA LTES diff and contracts analyzed.
- [x] NACA/NACS scope boundary fixed; NACS not registered.
- [x] Central C++ NACA kernel extended with sharp-TE bool.
- [x] Normal NACA path remains unchanged.
- [x] 399/245 decision documented and tested as deferred.
- [x] External H-S04-NACA-LTES harness built and measured.
- [x] Focused `unit.NacaLtesContracts` and validation tests.
- [x] Linux full/compiler/sanitizer/Valgrind/reference/harness/gate evidence.
- [x] One focused Windows S04 process.
- [x] `X6996-S04-R1-Focused-NACA-LTES-Acceptance`.
- [x] Create the single local S04 commit.

## Focused acceptance - X6996-S04-R1-Focused-NACA-LTES-Acceptance

Decision: **PASS**

The focused review confirms:

- `NACA4`/`NACA5` carry the 6.996 `LTES` contract and only stretch the
  thickness argument by `XTES`; camber, x distribution, ordering and validity
  checks are unchanged;
- the normal NACA path is identical to 6.99 in the external harness and all
  existing NACA normal-path tests remain green;
- `NACA(IDES1,LTES)` is mapped as an optional `sharpTrailingEdge` bool in the
  central kernel; no second NACA implementation or CLI dependency exists;
- the existing NACA command keeps `NSIDE=123`/245 and the 399-point buffer
  remains deferred under `G-NACA-BUFFER`; the kernel verifies 399 capability;
- no `NACS` command/menu/help/prompt/redo/bang or CLI golden was registered;
- focused values come from the external 6.996 H-S04 harness and no tolerance
  was increased;
- Linux Debug/Release GCC/Clang, ASan/UBSan, Valgrind, reference,
  determinism, harnesses and gates are green;
- the single required focused Windows process passed `1/1`;
- no Fortran source/binary, patch, canonical reference, Golden, format,
  product version or banner changed.

The only remaining action is the single local S04 commit; no push, new tag or
release is permitted.

## Next action

Create the single local S04 commit:

```text
Integrate XFOIL 6.996 NACA sharp-trailing-edge kernel
```
