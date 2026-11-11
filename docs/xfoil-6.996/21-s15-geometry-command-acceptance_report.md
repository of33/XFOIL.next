# XFOIL 6.996 — S15 geometry command acceptance report

Status: PASS — implementation accepted on Linux and with the focused
Windows process.

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

Measurement directory:
`/local/Olaf/Entwicklung/.audit-xfoil-next/xfoil-6.996/s15/`

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

External matrix:

```text
/local/Olaf/Entwicklung/.audit-xfoil-next/xfoil-6.996/s15/s15-impact-matrix.tsv
```

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

The implementation remains uncommitted. `HEAD` is still
`53b34fbc1877543b1d81ab8e3911cfedda23cbcf`; the working tree contains the
S15 code, tests, targeted golden migrations, fixtures and these two
documents.

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

| Check | Result | Log |
|---|---|---|
| GCC 13.3 Debug full suite | 2042/2042 | `/tmp/opencode/s15-full-debug.log` |
| GCC 13.3 Release full suite | 2042/2042 | `/tmp/opencode/s15-test-release2.log` |
| Clang 18.1.3 Debug full suite | 2042/2042 | `/tmp/opencode/s15-test-clang-debug.log` |
| Clang 18.1.3 Release full suite | 2042/2042 | `/tmp/opencode/s15-test-clang-release.log` |
| ASan/UBSan full suite | 2042/2042 | `/tmp/opencode/s15-test-sanitize.log` |
| Valgrind matrix | PASS | `/tmp/opencode/s15-valgrind.log` |
| Focused S15 Valgrind | 5/5 + 5/5 | `/tmp/opencode/s15-valgrind-unit.log`, `/tmp/opencode/s15-valgrind-integration.log` |
| Reference verification | 236/236 | `/tmp/opencode/s15-reference.log` |
| Reference determinism | 236/236 | `/tmp/opencode/s15-determinism.log` |
| Harness families | 16/16 | `/tmp/opencode/s15-harnesses.log` |
| Permanent gates | PASS | `/tmp/opencode/s15-gates-final.log` |
| Windows focused `integration.GeometryCommandS15` | 5/5 | user-provided Windows run, 0 failures |

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

Both the Linux matrix and the Windows focused process are green. S15 is
accepted; the remaining action is the local commit
`Integrate XFOIL 6.996 geometry commands`, without push, fetch, tag or
release.
