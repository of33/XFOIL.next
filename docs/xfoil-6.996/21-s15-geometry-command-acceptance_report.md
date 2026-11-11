# XFOIL 6.996 — S15 geometry command acceptance report

Status: PASS — `X6996-S15` and `X6996-S15-R1` accepted.

Decision: `X6996-S15-G1: PASS`
`G-NACA-BUFFER: ACCEPT-XFOIL-6996`
`NACA buffer: 399 points, normal TE`
`NACS buffer: 399 points, sharp TE`

Command set:

```text
S15 command set: NACS, SQTE, CIRC, SYTB, XCEN, Adeg
Excluded: NSPL
```

## Starting state

- HEAD: `53b34fbc1877543b1d81ab8e3911cfedda23cbcf`
  (`Record X6996-S13-R1 focused acceptance`).
- Working tree clean, staged 0, ahead 27, `v1.0.0` unchanged at
  `dc7305b057286574cd137cb6f1af170b74650ec5`.
- No push, fetch, tag or release.
- G1 changes only the two S15 documents; the external measurement and
  impact files remain outside the repository.

## Sources and current mapping

Compared: `fortran-src/xfoil-6.99/xfoil.f`,
`fortran-src/xfoil-6.996/xfoil.f`, `fortran-src/xfoil-6.99/xgdes.f` and
`fortran-src/xfoil-6.996/xgdes.f`.

Key source facts:

- 6.99 `NACA` calls `NACA` with `NSIDE=IQX/3=123`; `IQX=370` gives
  `NB=245`.
- 6.996 `NACA` and `NACS` call the same `NACA` routine with `LTES=false`
  or `true`; `IQX=601` gives `NSIDE=200`, `NB=399`.
- 6.996 `NACS` is a hidden command in the top-level dispatcher; the 6.996
  top-level help lists `NACA` only.
- 6.996 GDES adds `SYTB`, `XCEN`, `SQTE`, `CIRC` and the plot-only `NSPL`.
  `SQTE` uses the S05 kernel; `CIRC` sets `NHCIRC=15` and ignores an inline
  argument; `SYTB` has the two-factor prompt contract; `XCEN` is a pure
  query (LEFIND/SEVAL, top/y-t/bottom print) with no buffer mutation.
- `Adeg` accepts degrees; `ARAD` accepts radians; 6.996 adds the `A` alias.
  `Adeg` uses `ROTATE`, updates APX angles, does not call SCALC, and sets
  `LGSAME=false`.
- `NSPL` only changes `NTS` and calls plot reinitialization; it is excluded
  from S15 and belongs to plot-only `NIMP-01`/`S21`.

Current C++ mapping before implementation:

| Contract | Current C++ | Gap |
|---|---|---|
| NACA kernel | `naca4`/`naca5` with explicit `nside`; `nacaAirfoil` hardcodes `nside=123` | command default must become 200 for Option A |
| NACS dispatch | none | add top-level dispatch and sharp flag |
| GDES ADEG/ARAD | dispatcher and session wrapper exist | retain; add `A` alias if in scope |
| SQTE/CIRC/SYTB kernels | `squareTrailingEdge`, `addTrailingEdgeCircle`, `scaleTopBottomY` exist | add GDES branches/session wrappers |
| XCEN | no dedicated query/handler | add query path; no mutation |
| NSPL | absent | excluded; `NIMP-01`/S21 |

## External oracle method

External audit area: `xfoil-6.996/s15/`.

Binaries:

| Role | Binary | SHA-256 |
|---|---|---|
| 6.99 canonical | `fortran-bin/xfoil` | `f5779d6954d0b8fdc152e0e0633f18732e00b7b69228d561d629bd524bba598c` |
| 6.996 stock | `fortran-bin/xfoil-6.996` | `000b0468d9a7f4fce42d6fac24bd14147d7d326c8a9035909c8e9f98d180f50e` |
| 6.996 forced `NSIDE=123` | `s15/oracle-6996-nside123/bin/xfoil` | `6700a7ef0a0038eb326cef4a1b091564c77900f9b2c7fb638eab83549feedd8e` |

The controlled 123-point binary is the frozen 6.996 source with exactly one
external measurement change:

```diff
-      NSIDE = IQX/3
+      NSIDE = 123
```

Each case ran under `LC_ALL=C` with:

```text
PLOP
g

<COMMAND> <DESIGNATION>
PCOP
PSAV buffer.dat
PANE
OPER
ALFA 4
CPWR cp.dat
CPMN

QUIT
```

`PCOP` is used only to save the buffer contour with `PSAV`; `PANE` then
creates the normal current panel. The external cases contain stdout,
stderr, exit code, buffer, Cp and environment files.

## Buffer measurements

All 15 runs exited 0. `NACA` keeps the 0.00252 normal TE gap; `NACS` gives
a zero gap. The 6.996 `PANE` default is 240 panel nodes; 6.99 uses 160.

| Case | Buffer | Panel | TE gap | max t @ x | max camber @ x | min Cp @ x |
|---|---:|---:|---:|---|---|---|
| `699-naca-0012-123` | 245 | 160 | 0.00252000 | 0.120032 @ 0.297000 | -0.000000 @ 0.015000 | -1.539900 @ 0.011100 |
| `699-naca-23012-123` | 245 | 160 | 0.00252000 | 0.120032 @ 0.297000 | 0.018382 @ 0.146000 | -1.454300 @ 0.074600 |
| `699-naca-2412-123` | 245 | 160 | 0.00252000 | 0.120032 @ 0.297000 | 0.019999 @ 0.403000 | -1.383200 @ 0.017100 |
| `6996-naca-0012-123` | 245 | 240 | 0.00252000 | 0.120032 @ 0.297500 | -0.000000 @ 0.014800 | -1.538900 @ 0.011800 |
| `6996-naca-0012-200` | 399 | 240 | 0.00252000 | 0.120034 @ 0.300900 | 0.000000 @ 0.043100 | -1.538900 @ 0.011800 |
| `6996-naca-23012-123` | 245 | 240 | 0.00252000 | 0.120032 @ 0.297500 | 0.018382 @ 0.146200 | -1.454000 @ 0.078600 |
| `6996-naca-23012-200` | 399 | 240 | 0.00252000 | 0.120034 @ 0.300900 | 0.018385 @ 0.152200 | -1.454000 @ 0.078500 |
| `6996-naca-2412-123` | 245 | 240 | 0.00252000 | 0.120032 @ 0.297500 | 0.019999 @ 0.403200 | -1.383200 @ 0.016900 |
| `6996-naca-2412-200` | 399 | 240 | 0.00252000 | 0.120034 @ 0.300900 | 0.020000 @ 0.398000 | -1.383200 @ 0.016900 |
| `6996-nacs-0012-123` | 245 | 240 | 0.00000000 | 0.120034 @ 0.297500 | 0.000000 @ 0.880800 | -1.537800 @ 0.011700 |
| `6996-nacs-0012-200` | 399 | 240 | 0.00000000 | 0.120032 @ 0.294700 | 0.000000 @ 0.063800 | -1.537800 @ 0.011700 |
| `6996-nacs-23012-123` | 245 | 240 | 0.00000000 | 0.120035 @ 0.297500 | 0.018382 @ 0.146200 | -1.459700 @ 0.078000 |
| `6996-nacs-23012-200` | 399 | 240 | 0.00000000 | 0.120032 @ 0.294700 | 0.018385 @ 0.152200 | -1.459700 @ 0.077800 |
| `6996-nacs-2412-123` | 245 | 240 | 0.00000000 | 0.120035 @ 0.297500 | 0.019999 @ 0.403200 | -1.382700 @ 0.016900 |
| `6996-nacs-2412-200` | 399 | 240 | 0.00000000 | 0.120032 @ 0.294700 | 0.020000 @ 0.398000 | -1.382700 @ 0.016900 |
### Interpretation

- For 0012, 2412 and 23012, `699-naca-*-123` and `6996-naca-*-123` have
  byte-identical buffers (`buffer_sha256` equal). The full Cp files differ
  because the 6.996 `PANE` default is 240 nodes versus 160 in 6.99; this is
  the S07 default-panel contract, not a NACA-buffer difference.
- 123 versus 200 changes the buffer count from 245 to 399. The TE gap,
  LE, camber class and general family remain unchanged; max-thickness x
  moves by at most about 0.004 in the measured cases. The final inviscid
  min-Cp changes by at most about 0.001 in these cases.
- `NACS` at 123 and 200 has zero TE gap and remains within the same shape
  family. With Option A, `CIRC` on a 399-point NACA buffer yields 429 points,
  still below `IQX=601`.
- No non-finite values, exits or capacity failures occurred in the 15
  oracle cases.

State-invalidation probes in `s15/state-probes/`:

- `NACA 0012 -> ALFA 4 -> NACA 2412 -> ALFA 4` produces a Cp file
  byte-identical to a fresh `NACA 2412` point (`9370bad76e19c6ad...`).
- `NACS 0012 -> ALFA 4 -> NACS 2412 -> ALFA 4` produces a Cp file
  byte-identical to a fresh `NACS 2412` point (`f8474c3ff3b65a58...`).
- Therefore the geometry command does not leave a stale inviscid point in
  these product paths; the subsequent operation acts on the new buffer and
  panel.

## GDES command probes

The external `s15/command-probes/` runs used `NACA 2412` (399 buffer),
`GDES`, the command, `EXEC`, then `PCOP`/`PSAV` and a fresh `PANE`/`ALFA 4`:

| Probe | Buffer before | Buffer after | Observed text/state |
|---|---:|---:|---|
| `SQTE` | 399 | 399 | same point count; TE gap changes to about 0.00251; new Cp |
| `CIRC` | 399 | 429 | 15 points per side; sharp TE; new Cp |
| `SYTB 1.1 0.9` | 399 | 399 | buffer hash changes; X unchanged in the kernel contract |
| `XCEN 0.5 0.5` | 399 | 399 | prints top/y-t/bottom; buffer hash unchanged |
| `ADEG 5` | 399 | 399 | buffer hash changes; point remains valid |

All probes exited 0. Details are in the external `command-probes/`
directories.

## Impact inventory

External matrix: `external audit area: xfoil-6.996/s15/s15-impact-matrix.tsv`.

42 inventoried paths, with classifications:

| Classification | Count |
|---|---:|
| update-to-6.996-target | 21 |
| preserve-6.99-with-explicit-setting | 7 |
| unaffected | 6 |
| new-command-test | 5 |
| make-test-independent | 2 |
| new-versioned-fixture | 1 |

The matrix includes the NACA kernel, CLI/session mapping, all explicit
245-point product expectations, NACA/GDES unit and integration tests,
targeted CLI/batch goldens, canonical 6.99 references and the new S15
fixture area. Canonical `tests/fortran-reference/**` remains unchanged;
explicit `nside=123` kernel tests remain valid and independent of the
command default.

## Decision rationale

Option A is accepted because:

- the same 6.996 kernel at forced `NSIDE=123` reproduces the 6.99 buffer
  bytes exactly; the change is a controlled point-count migration, not a
  new geometry definition;
- 399 points is within the S01 capacity and leaves room for `CIRC`;
- `NACS` sharp TE is already verified by S04 and reproduces the expected
  zero-gap family at both point counts;
- no public function signature or serialized format is broken; the affected
  surface is product text/metadata/golden content, which S15 can update
  specifically;
- keeping NACA at 245 while NACS uses 399 would create a permanent
  project-specific hybrid contract and diverge from the 6.996 target.

No global tolerance or blanket golden regeneration is permitted. Product
goldens may change only for the intended NACA buffer migration and new
commands, with each affected path listed in the impact matrix.

## Golden and fixture policy for S15

- Canonical `tests/fortran-reference/**` stays unchanged.
- Existing 6.99 NACA cases may retain explicit `NSIDE=123` kernel tests.
- Product goldens are updated only for the intended 1.1.0 command/buffer
  changes.
- New `NACS`/GDES commands receive narrowly versioned fixtures and focused
  command tests.
- No global tolerance, no blanket NACA/GDES regeneration and no silent
  compatibility switch.

## Protected state and gates

- G1 changes only the two S15 documents.
- No C++, test, fixture, golden, reference, Fortran, patch, product version,
  banner or tolerance change.
- No Linux or Windows test matrix is required for G1.
- Gates: all green — `git diff --check`, `make check-fortran-sha`,
  `make check-license-headers`, `make check-release-inventory`,
  `make check-public-docs`.
- No push, fetch, tag or release.

## Geometry command implementation — X6996-S15

The implementation commit is
`8d4164125ae1e7e3f5e3fb0f6b1462a17ef4a52f`, parent
`53b34fbc1877543b1d81ab8e3911cfedda23cbcf`. It was not amended. This R1
follow-up changes only the two S15 documents.

Implementation summary:

- `nacaAirfoil` defaults to `NSIDE=200` / `NB=399`; an explicit `nside`
  argument preserves parameterized kernel/reference tests.
- `NACS` reuses the NACA parser/handler with `LTES=true`, remains hidden
  from top-level help, and sets the sharp-TE `NACA`/`NACS` name.
- GDES adds `SQTE`, `CIRC`, `SYTB`, `XCEN` and the `A` degree alias.
  `CIRC` always uses `NHCIRC=15`; `XCEN` is a pure LEFIND/SEVAL query.
- GDES mutations remain uncommitted until `EXEC`; the existing atomic
  panel commit path is unchanged.
- `NSPL` is not implemented and is reported as an unknown GDES command.

Test and fixture changes:

- New focused tests: `unit.GeometryCommandS15*` and
  `integration.GeometryCommandS15*`.
- New S15 data under `tests/data/xfoil-6.996/s15/`:
  - six NACA/NACS 399-point `buffer.dat`/`cp.dat` cases;
  - five GDES probe `after.dat` files;
  - targeted PACC/CSEQ/Wake stdout and generated references;
  - targeted BEND and MSAV top-export references.
- Legacy tests that only use NACA as setup pin the explicit 123/245
  baseline, so their canonical 6.99 references remain meaningful.
- Product NACA/CLI/batch goldens were migrated only where the accepted
  399-point command default changes their contract. No global golden or
  tolerance update was performed.

Linux acceptance evidence:

| Check | Result |
|---|---|
| GCC 13.3 Debug full suite | 2042/2042 |
| GCC 13.3 Release full suite | 2042/2042 |
| Clang 18.1.3 Debug full suite | 2042/2042 |
| Clang 18.1.3 Release full suite | 2042/2042 |
| ASan/UBSan full suite | 2042/2042 |
| Valgrind matrix | PASS |
| Focused S15 Valgrind | 5/5 + 5/5 |
| Reference verification | 236/236 |
| Reference determinism | 236/236 |
| Harness families | 16/16 |
| Permanent gates | PASS |
| Windows focused `integration.GeometryCommandS15` | 5/5, 0 failures |

Golden migration note: the batch `naca0012`/`naca4415` transcripts and
results now contain the deterministic product behavior at the 399-point
NACA default. S15 does not integrate later-slice analysis changes, so
these files intentionally do not claim full 6.996 analysis parity; they
record the S15 product contract. The focused GDES/Cp oracle comparisons and
the 6.996 PACC/CSEQ/Wake artifact parity remain the governing S15 checks.

Windows focused acceptance passed with exactly one process:

```text
integration.GeometryCommandS15
5/5, 0 failures
```

## Targeted existing-golden migration record

The S15 implementation commit changes exactly six files that already
existed in the parent and serve as product goldens. Hashes are computed
from the parent blob and the S15 commit blob, not from the working tree.

| Path | Case | Old SHA-256 | New SHA-256 | Diff extent | Oracle evidence | Cause |
|---|---|---|---|---|---|---|
| `tests/golden/batch/naca0012_results.dat` | batch naca0012 results | `5c40876799e0f181b8aff0d2deea9e4574db91965c24b074aee9715c0507cba7` | `5b69428ec762c894e9dfc7775181257d0618f87ffc7eee0fd50c169638c0c800` | 30 added / 31 removed; 4419 -> 4312 bytes | accepted NACA NSIDE=200/NB=399 product contract; versioned fixture `tests/data/xfoil-6.996/s15/cases/6996-naca-0012-200/` | NACA command default changes from 245 to 399 buffer points; the file records the intended deterministic product text/metadata and targeted analysis consequence. |
| `tests/golden/batch/naca0012_transcript.txt` | batch naca0012 transcript | `c9bb633efd08ef006e3a1b0d585728542523a281a744fe78cad289656f42502d` | `eff36989da51dd3587c065bff8eabb227af99c0a1f20411be8492a05ba96ffa4` | 4807 added / 2424 removed; 166184 -> 268165 bytes | accepted NACA NSIDE=200/NB=399 product contract; versioned fixture `tests/data/xfoil-6.996/s15/cases/6996-naca-0012-200/` | NACA command default changes from 245 to 399 buffer points; the file records the intended deterministic product text/metadata and targeted analysis consequence. |
| `tests/golden/batch/naca4415_results.dat` | batch naca4415 results | `9b346e63c580f6ef637429d560fd9e93ea72e8eb2a06400e0977974a69838e6f` | `457145ff2338745a4cc9e9ad713292e9baac7263d340967db43244b34c356e95` | 32 added / 32 removed; 4426 -> 4427 bytes | accepted NACA NSIDE=200/NB=399 product contract; deterministic S15 batch output | NACA command default changes from 245 to 399 buffer points; the file records the intended deterministic product text/metadata and targeted analysis consequence. |
| `tests/golden/batch/naca4415_transcript.txt` | batch naca4415 transcript | `ee003297a42b04abab2b66025b6365bce584567200e49263b263e49d859c85f7` | `9a264c9f09420fd4e2488883b5b9bb6518549f4f0daf3b8ea5a1b440ecc3604d` | 652 added / 604 removed; 61220 -> 63594 bytes | accepted NACA NSIDE=200/NB=399 product contract; deterministic S15 batch output | NACA command default changes from 245 to 399 buffer points; the file records the intended deterministic product text/metadata and targeted analysis consequence. |
| `tests/golden/cli/file-inte-pcop-quit.stdout.txt` | CLI NACA 2412 -> INTE -> PCOP | `c5bc9ab99d13e7aa56d0c14d6804bfc93738c02c577fc7c3a3c941b155877333` | `5b360efd9a9ca0c820ede43a0c35cd97ebf22af0305c29cbf4238d1b25237d43` | 4 added / 4 removed; 2871 -> 2871 bytes | accepted NACA 399-point contract; external audit area: xfoil-6.996/s15/cases/6996-naca-2412-200/ | NACA command default changes from 245 to 399 buffer points; the file records the intended deterministic product text/metadata and targeted analysis consequence. |
| `tests/golden/cli/file-naca-save-load-quit.stdout.txt` | CLI NACA 2412 -> SAVE/LOAD | `68704c162500bc2860f9456139d594119d5bddc4c95f2decf8efc9dca5156ec5` | `009fd9bb275a7727bb9866cb727a74f65b1b6a59efc0cb44429a257199a9b3d7` | 4 added / 4 removed; 2624 -> 2624 bytes | accepted NACA 399-point contract; external audit area: xfoil-6.996/s15/cases/6996-naca-2412-200/ | NACA command default changes from 245 to 399 buffer points; the file records the intended deterministic product text/metadata and targeted analysis consequence. |

New files under `tests/data/xfoil-6.996/s15/**` are separately classified as
versioned S15 fixtures below and are not counted in this table.

## Versioned S15 fixture inventory

| Path | Case | SHA-256 | Oracle evidence | Purpose |
|---|---|---|---|---|
| `tests/data/xfoil-6.996/s15/cases/6996-naca-0012-200/buffer.dat` | naca-0012-200 | `a877d11c6ab48aaad124a3a71a4b373da3c49fb84bc38bc109b27ba269709f7b` | external audit area: xfoil-6.996/s15/cases/6996-naca-0012-200/ | NACA/NACS 399-point buffer oracle |
| `tests/data/xfoil-6.996/s15/cases/6996-naca-0012-200/cp.dat` | naca-0012-200 | `4497a8a6c288c8a57fdeb7d40a5b42283c53b6d7f6e93b3637cd3a5b1e2e3ee5` | external audit area: xfoil-6.996/s15/cases/6996-naca-0012-200/ | NACA/NACS inviscid Cp oracle |
| `tests/data/xfoil-6.996/s15/cases/6996-naca-23012-200/buffer.dat` | naca-23012-200 | `ae10da2fd7c72b9f194fadd1708a67cc77fef4390a5b62000d7aaf998fbd6384` | external audit area: xfoil-6.996/s15/cases/6996-naca-23012-200/ | NACA/NACS 399-point buffer oracle |
| `tests/data/xfoil-6.996/s15/cases/6996-naca-23012-200/cp.dat` | naca-23012-200 | `e489806a8b61dcac31e1442985064c6affdade3fa9713fa28b05c10994c2ba55` | external audit area: xfoil-6.996/s15/cases/6996-naca-23012-200/ | NACA/NACS inviscid Cp oracle |
| `tests/data/xfoil-6.996/s15/cases/6996-naca-2412-200/buffer.dat` | naca-2412-200 | `f214690d60e69fcb1e97c57df7e6998bdb182b275e22b59609202c089eed62d1` | external audit area: xfoil-6.996/s15/cases/6996-naca-2412-200/ | NACA/NACS 399-point buffer oracle |
| `tests/data/xfoil-6.996/s15/cases/6996-naca-2412-200/cp.dat` | naca-2412-200 | `9370bad76e19c6ad02c9be1ac35b8a818f2b60032523865cd02f22a66d71da75` | external audit area: xfoil-6.996/s15/cases/6996-naca-2412-200/ | NACA/NACS inviscid Cp oracle |
| `tests/data/xfoil-6.996/s15/cases/6996-nacs-0012-200/buffer.dat` | nacs-0012-200 | `c41159da14c78858a19d4f646648eba47fd4222d3661dde2d20c7c3827befccd` | external audit area: xfoil-6.996/s15/cases/6996-nacs-0012-200/ | NACA/NACS 399-point buffer oracle |
| `tests/data/xfoil-6.996/s15/cases/6996-nacs-0012-200/cp.dat` | nacs-0012-200 | `71a4fd8522a7e50f69834b18b2fa1ba7caa950a7af82562e3070fd716dae3468` | external audit area: xfoil-6.996/s15/cases/6996-nacs-0012-200/ | NACA/NACS inviscid Cp oracle |
| `tests/data/xfoil-6.996/s15/cases/6996-nacs-23012-200/buffer.dat` | nacs-23012-200 | `3ad2bd02604311536d470abb061e70f58a18eb54657967f88fd947eeab1f8920` | external audit area: xfoil-6.996/s15/cases/6996-nacs-23012-200/ | NACA/NACS 399-point buffer oracle |
| `tests/data/xfoil-6.996/s15/cases/6996-nacs-23012-200/cp.dat` | nacs-23012-200 | `07d10ea63245d8840fccc78e99c4538c7bc65b62b49f944f2185bfd3bc09f48c` | external audit area: xfoil-6.996/s15/cases/6996-nacs-23012-200/ | NACA/NACS inviscid Cp oracle |
| `tests/data/xfoil-6.996/s15/cases/6996-nacs-2412-200/buffer.dat` | nacs-2412-200 | `763aec0b48a45233044df5fb231343d5385d7e1567cc3841882ea671b7a01c96` | external audit area: xfoil-6.996/s15/cases/6996-nacs-2412-200/ | NACA/NACS 399-point buffer oracle |
| `tests/data/xfoil-6.996/s15/cases/6996-nacs-2412-200/cp.dat` | nacs-2412-200 | `f8474c3ff3b65a58e7cf6a37fbca33a6bd8930c9c1c71adeaf79728abb5cc457` | external audit area: xfoil-6.996/s15/cases/6996-nacs-2412-200/ | NACA/NACS inviscid Cp oracle |
| `tests/data/xfoil-6.996/s15/command-probes/adeg/after.dat` | GDES ADEG | `14d4c1f7ca80eba4488c1176964a60ccca7ccea7e73c90b4dcae010eb9e5cd1a` | external audit area: xfoil-6.996/s15/command-probes/adeg/ | ADEG post-EXEC buffer oracle |
| `tests/data/xfoil-6.996/s15/command-probes/circ/after.dat` | GDES CIRC | `94036eef0566066937a21aa9a487cb0e2d47302fe3a3be934247e13434597772` | external audit area: xfoil-6.996/s15/command-probes/circ/ | CIRC post-EXEC buffer oracle |
| `tests/data/xfoil-6.996/s15/command-probes/sqte/after.dat` | GDES SQTE | `a1e6ba8fc4e5a7a67dbeea9d4c87471255554e01eeb8d53a701086437c97bd4a` | external audit area: xfoil-6.996/s15/command-probes/sqte/ | SQTE post-EXEC buffer oracle |
| `tests/data/xfoil-6.996/s15/command-probes/sytb/after.dat` | GDES SYTB | `6940c47bff1be97bdff1c3723db72f0f141e38696409fa3883fd4ba22e5451b0` | external audit area: xfoil-6.996/s15/command-probes/sytb/ | SYTB post-EXEC buffer oracle |
| `tests/data/xfoil-6.996/s15/command-probes/xcen/after.dat` | GDES XCEN | `f214690d60e69fcb1e97c57df7e6998bdb182b275e22b59609202c089eed62d1` | external audit area: xfoil-6.996/s15/command-probes/xcen/ | XCEN post-EXEC buffer oracle |
| `tests/data/xfoil-6.996/s15/regression/polar-cseq-head-naca0012/generated/s2.dump` | polar-cseq-head-naca0012 | `09505d4b31b3e8de214d776c17898df29b57157efc89f4d0b19a34a747002e3a` | 6.996 stock/artifact and S15 product-path parity: polar-cseq-head-naca0012 | Regression generated artifact reference |
| `tests/data/xfoil-6.996/s15/regression/polar-cseq-head-naca0012/generated/s2.pol` | polar-cseq-head-naca0012 | `42e0722167854bfbdabdb9f12b41a5f75e43f896c779077b931d2b95e5d53440` | 6.996 stock/artifact and S15 product-path parity: polar-cseq-head-naca0012 | Regression generated artifact reference |
| `tests/data/xfoil-6.996/s15/regression/polar-cseq-head-naca0012/stdout.raw.txt` | polar-cseq-head-naca0012 | `a2ccb8f5fe3808784f7ff6b196d37f369a7f67b87ec61aee0051ed17c465938e` | 6.996 stock/artifact and S15 product-path parity: polar-cseq-head-naca0012 | Regression stdout reference |
| `tests/data/xfoil-6.996/s15/regression/polar-inviscid-naca0012-aseq/generated/polar-inviscid-naca0012-aseq.dump` | polar-inviscid-naca0012-aseq | `6b77488d4bbd286a4c0001af168548279a356cd6d6a6a5254792fac0f42f0a4e` | 6.996 stock/artifact and S15 product-path parity: polar-inviscid-naca0012-aseq | Regression generated artifact reference |
| `tests/data/xfoil-6.996/s15/regression/polar-inviscid-naca0012-aseq/generated/polar-inviscid-naca0012-aseq.pol` | polar-inviscid-naca0012-aseq | `e362ca938d85840b25de58d3af8b5ec715966faaccdab184153c22b30eb6ec54` | 6.996 stock/artifact and S15 product-path parity: polar-inviscid-naca0012-aseq | Regression generated artifact reference |
| `tests/data/xfoil-6.996/s15/regression/polar-wake2-alfa6-naca0012/stdout.raw.txt` | polar-wake2-alfa6-naca0012 | `644cb0e9d218d43f136a8a13dbb06e1683862ea5651b04779c5616daf66adb82` | 6.996 stock/artifact and S15 product-path parity: polar-wake2-alfa6-naca0012 | Regression stdout reference |
| `tests/data/xfoil-6.996/s15/top-file-export/bend-naca0012/stdout.raw.txt` | bend-naca0012 | `94267d7c1b9f6dc728ba1184d8ed5e5288981d2949f7abeb5a71231edf38bd4e` | 6.996 stock BEND/MSAV parity: bend-naca0012 | BEND report reference |
| `tests/data/xfoil-6.996/s15/top-file-export/bend-naca2412/stdout.raw.txt` | bend-naca2412 | `60a2bf707c7578e06480a5557bd11483499e5b144b79a8c2f06912b2b3e01685` | 6.996 stock BEND/MSAV parity: bend-naca2412 | BEND report reference |
| `tests/data/xfoil-6.996/s15/top-file-export/msav-isav-roundtrip/generated/single.mses` | msav-isav-roundtrip | `821b5de39049c1257b4519b44bbae35dbd956ed00c2b0587832be9a2c6a4678d` | 6.996 stock BEND/MSAV parity: msav-isav-roundtrip | MSAV generated artifact reference |

## X6996-S15-R1 focused acceptance record

The implementation commit is
`8d4164125ae1e7e3f5e3fb0f6b1462a17ef4a52f`; its parent is
`53b34fbc1877543b1d81ab8e3911cfedda23cbcf`. It was not amended. The R1
follow-up is documentation-only and changes only:

- `docs/xfoil-6.996/21-s15-geometry-command-acceptance.md`
- `docs/xfoil-6.996/21-s15-geometry-command-acceptance_report.md`

The already confirmed technical matrix remains valid. No additional Linux
or Windows process was run because only documentation changed. Exactly one
focused Windows process remains documented:

```text
integration.GeometryCommandS15
5/5, 0 failures
```

Confirmed results:

```text
Full suite:        2042/2042
Reference:          236/236
Determinism:        236/236
Harness families:     16/16
Windows focused:        5/5
Permanent gates:       PASS
```

No protected Fortran source, binary, patch, canonical 6.99 reference,
product version, banner or tolerance was changed. No push, fetch, tag or
release occurred.

## Final decision

X6996-S15: PASS
X6996-S15-R1: PASS
