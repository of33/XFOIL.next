# XFOIL 6.996 - S09 CFT clamped-GRT derivatives report

Status: PASS - Linux F1 acceptance, Windows focused R1 acceptance and S09
acceptance passed.

## Starting state

- HEAD: `d91fb73cbd619632bfa1fd7c5379652bdc80e478` (S08).
- Parent: `304a0172d93a0ee6866d1f04e566cdca3f4600c6` (S07).
- Worktree clean before S09; staged 0; ahead 20; tag `v1.0.0`.
- No fetch, push, tag or release.
- S09 source baseline: only the `CFT` block in `xblsys.f` differs from 6.99.

## Chronology

1. The initial analysis implemented the CFT clamp, added nine focused tests,
   built the external H-S09-CFT harness and ran targeted checks. The core
   binary passed 443/443.
2. The first full suite run was **not** green: 9 failure entries in 6 existing
   targets. No golden or reference was changed at that point. The slice was
   paused and the blocking decision was documented.
3. The six targets were then explicitly released for migration to versioned
   6.996-S09 target data. The constraints were: exact six targets, no global
   golden-family change, no tolerance change, old 6.99 references preserved,
   version/banner/product unchanged.
4. New fixtures were generated from external controlled oracles, the six
   targets were migrated, and the four actually affected batch golden files
   were updated. The other batch goldens did not change.
5. F1 targeted tests and the complete Linux matrix passed. Windows focused
   acceptance then passed 9/9; only the final commit remained.

## Fortran diff

Only the `CFT` block in `xblsys.f` differs (28-line full-file diff). The
contract is documented in `15-s09-cft-clamped-grt-derivatives.md`. Key points:

- signature unchanged;
- `GRT = LOG(RT/FC)`;
- strict `GRT .LT. 3.0` clamp;
- clamp zeroes `GRT_RT` and `GRT_MSQ`;
- `CF_RT` and `CF_MSQ` use those working derivatives;
- at exactly `GRT=3` the branch is inactive;
- `CF`, `CF_HK` and all BL branch structure are otherwise unchanged.

`CFT` is invoked in turbulent/wake paths and in the turbulent-vs-laminar
selection; laminar CFT is not a separate model.

## C++ implementation

Changed product files:

- `src/core/boundary_layer_solver/BoundaryLayerCorrelations.cpp`
  (`core::boundary_layer::cft`, product path);
- `src/core/boundary_layer/BoundaryLayerCorrelations.cpp`
  (`xfoil::boundary_layer::turbulentSkinFriction`, existing public CFT alias).

The operation order follows 6.996: raw `GRT`, initialize `GRT_RT`/`GRT_MSQ`,
strict clamp branch, then the unchanged `CF`/`CF_HK` and the updated
`CF_RT`/`CF_MSQ` expressions. No tolerance, CLI, format, S08 field, version or
banner change.

## Harness and oracle evidence

### H-S09-CFT

`H-S09-CFT` external harness and data:
`/local/Olaf/Entwicklung/.audit-xfoil-next/xfoil-6.996/s09/harness/`.

It covers 20 cases: laminar/turbulent/wake, incompressible/compressible,
clear/immediate below, exact boundary, immediate/clear above, and both
6.99/6.996 builds. `cft.tsv` contains the raw values; `differences.txt`
identifies the active clamp differences. Selected values are in the plan
document.

### FMOM / VELS

The controlled oracle is external XFOIL 6.99 plus only the byte-identical
6.996 `CFT` block, with explicit old geometry `NPAN=160`, `CVPAR=1.0`,
`CTERAT=0.15`, `CTRRAT=0.2`. Two independent harness builds/runs produced
byte-identical output.

New fixtures:

- `tests/data/xfoil-6.996/s09/fmom_reference.txt`:
  `f1bac6bd8501302e15d2045ecf600e02d36b1f902a797a49dbebd9cc230b0ddb`;
- `tests/data/xfoil-6.996/s09/vels_reference.txt`:
  `237a643628c69a5b971ebb9ec4bd5291000c023941d17eb04fe73d2dc1991323`.

Only the FMOM C and VELS G values consumed by the existing tests changed;
the C++ product matches those new values exactly. The old references remain
unchanged:

- FMOM `c9ae293397ce2b04533b97bf4f0eaca1a4f8b82ec1b71027153a0e669f321686`;
- VELS `df58c3f49d771e269229775ccf45b56077633496ca6953711cc3dffb3dc1f562`.

### PACC dump

The PACC oracle is the same controlled 6.99+CFT source plus the already
separated dump workaround: `REWIND` before the new dump header and
`ACCESS='APPEND'`, with the `.pol` save filename left empty so the known 6.99
save defect cannot interfere. The dump bytes are identical to the C++ product
run of the canonical case, which still writes the `.pol`.

- old 6.99 dump: 40816 bytes,
  `9d1c61dd8964d48c459ab86e9a32657b0fd04931d69cd9631cc30e3b09e3910f`;
- new S09 dump: 40816 bytes,
  `bee8fbd6d197ae74a204f813a6a9f262b7981cc6c795a3b7313933d381063543`;
- two independent oracle runs: byte-identical;
- old/new differ in 3165 bytes, offsets 2589 through 40805;
- the C++ `.pol` result is unchanged at
  `3c767c026e864f033070e76ca5825392bb82fd507f75e0dcbc69e3dde9a1421e`.

### Full 6.996 batch-oracle limitation

A full 6.996 build with `PANE` defaults 240 and NACA `NSIDE=123` was tried for
NACA 0012. It still fails at high angle of attack (last converged alpha 12.0),
while the C++ S08+S09 product converges through 14.5. The 6.99+CFT oracle is
likewise not a complete C++ S08 baseline.

An external full 6.996 batch-oracle equality is therefore **not** claimed.
The batch goldens are C++-integrated S09 targets, not raw 6.996-oracle output.
The product differential is isolated by the clean S08 baseline: the S08
product reproduces the old batch behavior, and S09 changes only the `CFT`
implementation. This is a disclosed baseline limitation, not a tolerance
relaxation.

## Migration inventory

Six existing targets were migrated:

| Existing target | New expectation |
|---|---|
| `unit.FmomHingeLoadsViscousCpvBitAgainstS09Oracle` | `s09/fmom_reference.txt` |
| `unit.VelsVelocityViscousBitAgainstS09Oracle` | `s09/vels_reference.txt` |
| `regression.PaccDumpByteParityMatchesXfoil6996S09Target` | `s09/polar-visc-clark-y.dump` |
| `batch.ClarkYNativeBatchMatchesGolden` | updated transcript |
| `batch.Naca2312NativeBatchMatchesGolden` | updated transcript |
| `batch.Naca0012NativeBatchMatchesGolden` | updated transcript and results |

Versioned fixtures under `tests/data/xfoil-6.996/s09/`:

| File | SHA-256 |
|---|---|
| `fmom_reference.txt` | `f1bac6bd8501302e15d2045ecf600e02d36b1f902a797a49dbebd9cc230b0ddb` |
| `vels_reference.txt` | `237a643628c69a5b971ebb9ec4bd5291000c023941d17eb04fe73d2dc1991323` |
| `polar-visc-clark-y.dump` | `bee8fbd6d197ae74a204f813a6a9f262b7981cc6c795a3b7313933d381063543` |
| `metadata.txt` | `a1af0af48e20e12dbdcde5c6dc8285f6538a6023887cd6f2989af82856868b07` |

The old 6.99 files remain present and unchanged:
`tests/data/core-fmom-vels-reference/**` and
`tests/fortran-reference/cases/polar/polar-visc-clark-y/generated/**`.

### Batch golden diffs

| File | Old to new SHA-256 | Del/ins | Classification |
|---|---|---:|---|
| `clark_y_transcript.txt` | `7d7c72e2...` to `0320bbbe...` | 5/5 | intended CFT iteration-status/roundoff propagation; results unchanged |
| `naca2312_transcript.txt` | `3ab1b947...` to `9a41a021...` | 25/25 | intended small CFT residual/result changes; results unchanged |
| `naca0012_transcript.txt` | `9be339ab...` to `c9bb633e...` | 3810/777 | intended convergence-path change from CFT clamp |
| `naca0012_results.dat` | `845a90f4...` to `5c408767...` | 4/5 | intended final-result change; no NaN/Inf |

Full hashes:

- Clark Y old `7d7c72e2b198416d6fb195f65d277786ff7dcc2b2d03ef6eb5646ec5df7ba775`,
  new `0320bbbe0e8a62248f851eddae34145b360adc6d448491b1c6126da1a91b5d14`;
- NACA 2312 old `3ab1b947a943f8b9a75a21f60d6efffc72d642a24b8f6bbf4f43ea6b611640a8`,
  new `9a41a0213e1df7623409562a1e281dc0994d6d56847399f0daf05c0fbe926187`;
- NACA 0012 transcript old
  `9be339ab9947cdb0254c22fcb69ddd6004ceca179a2e9fb7566d2a782d328df0`,
  new `c9bb633efd08ef006e3a1b0d585728542523a281a744fe78cad289656f42502d`;
- NACA 0012 results old
  `845a90f42a2d33b76df3e442f3c13c9718313d64895545a7d9aef5f57216a535`,
  new `5c40876799e0f181b8aff0d2deea9e4574db91965c24b074aee9715c0507cba7`.

NACA 0012 first transcript divergence is at alpha 6.5. The first result
divergence is row 26 at alpha 12.5: old `converged=0`, 105 iterations,
`CL=0.784656`, `CD=0.105220`; new `converged=1`, 87 iterations,
`CL=0.565755`, `CD=0.140994`. The old target had a `nan` at alpha 13.5 and
ended at 14.0. New rows 26-30 are finite and converged; alpha 14.5 is added
with `CL=0.545968`, `CD=0.167263`.

No other batch golden changed.

## F1 acceptance

`X6996-S09-F1`: PASS.

- targeted: CFT 9/9, FMOM 1/1, VELS 1/1, PACC 1/1, Clark Y 1/1,
  NACA 2312 1/1, NACA 0012 1/1;
- core `/443/443`, unit `/1703/1703`, integration `/246/246`,
  regression `/5/5`, batch `/47/47`;
- full debug `/2004/2004`;
- GCC 13 release `/2004/2004`;
- Clang 18 debug `/2004/2004`;
- Clang 18 release `/2004/2004`;
- ASan/UBSan `/2004/2004`;
- Valgrind: CFT 9/9, FMOM 1/1, VELS 1/1, PACC 1/1, NACA 0012 batch 1/1,
  no definite leaks or errors;
- reference `/236/236`, reference determinism `/236/236`, harness families
  `/16/16` with the one protected reference file identical;
- gates: `check-dependencies`, `check-fortran-sha`,
  `check-release-inventory`, `check-license-headers`,
  `check-public-docs`, `core-link-check`, `session-link-check` all pass.

The first compiler-matrix invocation had no per-build CLI and produced 239
setup failures. After building the matching CLI for each build root, the same
suites were green as listed above; those were not product failures.

## R1 and Windows

`X6996-S09-R1`: PASS on 2026-11-10.

The focused Windows run

```text
./build/debug/xfoil_tests unit.CftClampedGrt
```

reported all nine tests OK and `0 failure(s) in 9 test(s)`. `X6996-S09` is
therefore PASS; the S09 commit follows this report update.

## Protected data and Git status

- `fortran-src/**`, `fortran-bin/**`, `patches/**`: unchanged.
- old `tests/data/core-fmom-vels-reference/**` and
  `tests/fortran-reference/**`: unchanged.
- only the new versioned `tests/data/xfoil-6.996/s09/**` tree is new test data.
- CLI, formats, S08 fields, product version and banner: unchanged.
- no fetch, push, tag or release.
- staged 0; worktree contains only S09 implementation, six migrated targets,
  four batch goldens, new S09 fixtures/docs.

Expected commit message after Windows PASS:
`Integrate XFOIL 6.996 clamped CFT derivatives`.
