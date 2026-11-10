# S09 - CFT clamped-GRT derivatives

Status: PASS - Linux F1 acceptance, Windows focused R1 acceptance and S09
acceptance passed.

## Slice contract

`X6996-S09-CFT-Clamped-GRT-Derivatives` ports only the XFOIL 6.996 `CFT`
change:

- clamped `GRT` working value;
- corresponding `GRT_RT` / `GRT_MSQ` derivatives;
- corrected `CF_RT` / `CF_MSQ` chain rule;
- turbulent/wake use through the existing correlation;
- focused tests and versioned 6.996-S09 target values.

Not included: new BL model, S08 field changes, BLDUMP3/output, polar columns,
marching/Newton/convergence logic, CLI, product version/banner or tolerances.
No canonical Fortran file, binary or patch inventory file was changed.

## Fortran contract

The signature is unchanged:

```fortran
SUBROUTINE CFT(HK, RT, MSQ, CF, CF_HK, CF_RT, CF_MSQ)
```

| Contract | XFOIL 6.99 | XFOIL 6.996 |
|---|---|---|
| `GRT` raw | `LOG(RT/FC)` | `LOG(RT/FC)` |
| clamp | `GRT = MAX(GRT,3.0)` | `IF(GRT .LT. 3.0) GRT = 3.0` |
| comparison | `MAX` arithmetic | strict `GRT < 3.0` |
| `GRT_RT` | implicit `1/RT` in `CF_RT` | `1/RT`; set to `0` in clamp |
| `GRT_MSQ` | inline `-0.25*GM1/FC**2` | `-0.25*GM1/FC**2`; set to `0` in clamp |
| `CF` | uses clamped `GRT` | uses clamped `GRT` |
| `CF_HK` | unchanged formula | unchanged formula |
| `CF_RT` | `GEX*CFO/(FC*GRT)/RT` | `GEX*CFO/(FC*GRT)*GRT_RT` |
| `CF_MSQ` | `...*(-0.25*GM1/FC**2) - 0.25*GM1*CF/FC**2` | `...*GRT_MSQ - 0.25*GM1*CF/FC**2` |

At exactly `GRT = 3`, the strict `<` branch is not taken. The unclamped
derivatives remain active at the boundary; immediately below the boundary
both `GRT` derivatives are zero.

`CFT` is used only by the turbulent/wake BL paths and the turbulent-vs-laminar
`CF` selection; the pure laminar path uses `CFL`. No laminar, turbulent or wake
model or branch was changed beyond the `CFT` values and derivatives.

Only the `CFT` block in `fortran-src/xfoil-6.996/xblsys.f` differs from 6.99.
That remains an external diff statement, not a new repository change.

## C++ mapping

| Fortran | C++ | Change |
|---|---|---|
| `CFT` core | `core::boundary_layer::cft` in `src/core/boundary_layer_solver/BoundaryLayerCorrelations.cpp` | clamped `GRT`, `GRT_RT`, `GRT_MSQ`, corrected `CF_RT`/`CF_MSQ` |
| `CFT` public correlation alias | `xfoil::boundary_layer::turbulentSkinFriction` in `src/core/boundary_layer/BoundaryLayerCorrelations.cpp` | kept consistent with the same 6.996 contract |
| callers | `BoundaryLayerSolverKernels.cpp` (`BLVAR`, midpoint) and `WallProfileKernels.cpp` | unchanged; consume the 6.996 derivatives |

No new BL architecture or global clamp helper was introduced.

## External H-S09-CFT harness

Location outside the repository:
`/local/Olaf/Entwicklung/.audit-xfoil-next/xfoil-6.996/s09/harness/`.

`CFT` was extracted byte-faithfully from both `xblsys.f` versions and compiled
with the canonical double-precision flags. The harness covers 20 cases:

- laminar-shape control (`HK=2.2`), turbulent (`HK=1.8`) and wake
  (`HK=1.2`), incompressible and compressible;
- `GRT` clearly below, immediately below, exactly at, immediately above and
  clearly above the clamp;
- clamp branch, value and all four derivative outputs.

Evidence: `harness/cft.tsv`, `harness/differences.txt`,
`harness/699/out.txt`, `harness/6996/out.txt`.

Selected 6.996 values:

| Case | GRT raw | Branch | CF | CF_HK | CF_RT | CF_MSQ |
|---|---:|---|---:|---:|---:|---:|
| `TURB_INCOMP_BELOW` | 2.9989995 | clamped | 1.4902006698021190e-02 | -2.1058061986980445e-02 | 0 | -1.4902006698021186e-03 |
| `TURB_INCOMP_AT` | 3.0 | unclamped | 1.4902006698021190e-02 | -2.1058061986980445e-02 | -5.6848506473499555e-04 | -3.4836789400057044e-04 |
| `TURB_INCOMP_JUST_ABOVE` | 3.000000001 | unclamped | 1.4902006686602864e-02 | -2.1058061972397898e-02 | -5.6848506354155589e-04 | -3.4836789411399273e-04 |
| `TURB_INCOMP_ABOVE` | 6.0 | unclamped | 3.0267192449410637e-03 | -4.9412647012446230e-03 | -2.8776547514796056e-06 | -1.8657904604630092e-04 |
| `TURB_COMP_BELOW` | 2.9989995 | clamped | 1.4474108426541240e-02 | -2.0453397896597027e-02 | 0 | -1.3654819270321923e-03 |
| `WAKE_COMP_BELOW` | 2.9989995 | clamped | 3.4742778217684366e-02 | -4.9061618140888820e-02 | 0 | -3.4673431354974410e-03 |

The 6.99/6.996 deltas are exactly zero for the clamp-zeroed `CF_RT` and
`CF_MSQ` terms in the active branch; outside the branch the only differences
are last-bit operation-order effects.

## Oracle and fixture evidence

### FMOM / VELS

The controlled oracle is XFOIL 6.99 plus only the byte-identical 6.996 `CFT`
block, with explicit old geometry `NPAN=160`, `CVPAR=1.0`, `CTERAT=0.15`,
`CTRRAT=0.2`. It deliberately does not include S07 defaults or NACA-399.

Two independent harness builds/runs produced byte-identical references. S09
consumes only the existing cases C (FMOM) and G (VELS):

| Value | 6.99 old | 6.996-S09 new |
|---|---:|---:|
| FMOM C `HMOM` | 1.27207182642534054e-03 | 1.27207182642534184e-03 |
| FMOM C `HFX` | -1.23895351713690996e-02 | -1.23895351713691031e-02 |
| FMOM C `HFY` | 9.11934668204253396e-03 | 9.11934668204254437e-03 |
| VELS G `V` | -7.89906625580703292e-02 | -7.89906625580701904e-02 |
| VELS G `U`, `Q`, `CP` | unchanged | unchanged |

The C++ product values for these targeted cases match the new references
exactly.

### PACC dump

The PACC oracle is the same controlled 6.99+CFT source plus the already
separated polar-dump workaround: `REWIND` before the new dump header and
`ACCESS='APPEND'`, with the `.pol` save filename left empty so the known 6.99
save defect cannot interfere. The dump bytes are identical to the C++ product
run of the canonical case, which still writes the `.pol`.

- old 6.99 dump: 40816 bytes, SHA-256
  `9d1c61dd8964d48c459ab86e9a32657b0fd04931d69cd9631cc30e3b09e3910f`;
- new S09 dump: 40816 bytes, SHA-256
  `bee8fbd6d197ae74a204f813a6a9f262b7981cc6c795a3b7313933d381063543`;
- two independent oracle runs are byte-identical;
- old and new differ in 3165 bytes, from offset 2589 through 40805;
- the C++ `.pol` result remains the old 6.99 hash
  `3c767c026e864f033070e76ca5825392bb82fd507f75e0dcbc69e3dde9a1421e`.

### Full 6.996 batch-oracle limitation

The full 6.996 source contains slices not yet integrated in the C++ product.
A controlled full-6.996 build with `PANE` defaults 240 and NACA `NSIDE=123`
was tried for NACA 0012; it still fails at high angle of attack (last
converged alpha 12.0), while the C++ S08+S09 product converges through 14.5.
The 6.99+CFT oracle likewise is not a complete C++ S08 baseline.

Therefore an external full 6.996 batch-oracle equality is **not** claimed.
The batch targets are C++-integrated S09 targets. The product differential is
isolated by a clean S08 checkout: S09 changes only the `CFT` implementation,
and the S08 baseline reproduces the old batch behavior. This limitation is
repeatable evidence, not a tolerance relaxation.

## Tests

Added nine `unit.CftClampedGrt*` tests in
`tests/core/BoundaryLayerCorrelationTests.cpp`:

- below, immediately below, exactly at, immediately above, clearly above;
- laminar/turbulent/wake representative inputs;
- compressible clamp;
- finite values, zero clamp derivatives and boundary contract;
- finite-difference derivatives away from the clamp.

Core binary: `443/443` pass.

Six existing targets were migrated to versioned 6.996-S09 expectations:

| Existing target | New consumer |
|---|---|
| `unit.FmomHingeLoadsViscousCpvBitAgainstS09Oracle` | `s09/fmom_reference.txt` |
| `unit.VelsVelocityViscousBitAgainstS09Oracle` | `s09/vels_reference.txt` |
| `regression.PaccDumpByteParityMatchesXfoil6996S09Target` | `s09/polar-visc-clark-y.dump` |
| `batch.ClarkYNativeBatchMatchesGolden` | updated transcript only |
| `batch.Naca2312NativeBatchMatchesGolden` | updated transcript only |
| `batch.Naca0012NativeBatchMatchesGolden` | updated transcript and results |

The three migrated unit/regression tests still compare exact values/bytes.
The old 6.99 references remain canonical and unchanged.

## Versioned S09 fixtures

Location: `tests/data/xfoil-6.996/s09/`.

| File | Bytes | SHA-256 |
|---|---:|---|
| `fmom_reference.txt` | 4802 | `f1bac6bd8501302e15d2045ecf600e02d36b1f902a797a49dbebd9cc230b0ddb` |
| `vels_reference.txt` | 6628 | `237a643628c69a5b971ebb9ec4bd5291000c023941d17eb04fe73d2dc1991323` |
| `polar-visc-clark-y.dump` | 40816 | `bee8fbd6d197ae74a204f813a6a9f262b7981cc6c795a3b7313933d381063543` |
| `metadata.txt` | 4298 | `a1af0af48e20e12dbdcde5c6dc8285f6538a6023887cd6f2989af82856868b07` |

`metadata.txt` records source/binary/fixture hashes, old-reference hashes,
repeatability, dump workarounds and the batch-oracle limitation.

## Batch goldens

| File | Old SHA-256 | New SHA-256 | Changed lines (del/ins) | Classification |
|---|---|---|---:|---|
| `clark_y_transcript.txt` | `7d7c72e2b198416d6fb195f65d277786ff7dcc2b2d03ef6eb5646ec5df7ba775` | `0320bbbe0e8a62248f851eddae34145b360adc6d448491b1c6126da1a91b5d14` | 5/5 | intended CFT iteration-status/roundoff propagation; results unchanged |
| `naca2312_transcript.txt` | `3ab1b947a943f8b9a75a21f60d6efffc72d642a24b8f6bbf4f43ea6b611640a8` | `9a41a0213e1df7623409562a1e281dc0994d6d56847399f0daf05c0fbe926187` | 25/25 | intended small CFT residual/result changes; results unchanged |
| `naca0012_transcript.txt` | `9be339ab9947cdb0254c22fcb69ddd6004ceca179a2e9fb7566d2a782d328df0` | `c9bb633efd08ef006e3a1b0d585728542523a281a744fe78cad289656f42502d` | 3810/777 | intended convergence-path change from CFT clamp |
| `naca0012_results.dat` | `845a90f42a2d33b76df3e442f3c13c9718313d64895545a7d9aef5f57216a535` | `5c40876799e0f181b8aff0d2deea9e4574db91965c24b074aee9715c0507cba7` | 4/5 | intended final-result change from CFT clamp; no NaN/Inf |

NACA 0012 first diverges at alpha 6.5 in the transcript. The first result
divergence is row 26 at alpha 12.5: the old target had `converged=0` at 105
iterations with `CL=0.784656`, `CD=0.105220`; the new target has
`converged=1` after 87 iterations with `CL=0.565755`, `CD=0.140994`. The old
rows contained a `nan` at alpha 13.5 and ended at alpha 14.0. All new rows
26-30 are finite and converged, and the new target adds alpha 14.5 with
`CL=0.545968`, `CD=0.167263`.

No other batch golden changed.

## Acceptance

`X6996-S09-F1` is PASS:

- targeted tests: CFT 9/9, FMOM 1/1, VELS 1/1, PACC 1/1, all three batch
  targets 1/1;
- core `/443/443`, unit `/1703/1703`, integration `/246/246`,
  regression `/5/5`, batch `/47/47`;
- full debug suite `/2004/2004`;
- GCC 13 release `/2004/2004`, Clang 18 debug `/2004/2004`, Clang 18
  release `/2004/2004`, ASan/UBSan `/2004/2004`;
- Valgrind focused: CFT 9/9, FMOM 1/1, VELS 1/1, PACC 1/1, NACA 0012 batch
  1/1, no definite leaks or errors;
- reference `/236/236`, reference determinism `/236/236`, harness families
  `/16/16` with the one protected reference file identical;
- all permanent gates pass.

`X6996-S09-R1` is PASS. The focused Windows run
`./build/debug/xfoil_tests unit.CftClampedGrt` reported all nine tests OK and
`0 failure(s) in 9 test(s)`. `X6996-S09` is PASS; the S09 commit follows this
documentation update.
