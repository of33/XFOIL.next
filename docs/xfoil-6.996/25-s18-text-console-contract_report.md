# XFOIL 6.996 — S18 text-only console contract report

Status: PASS — `X6996-S18` and `X6996-S18-R1` accepted. Implementation,
Linux matrix, focused Windows process and R1 documentation check are green.

## Starting state

- HEAD: `bf019d8d2afcd81b27e5f47ee1aa7552fd9e7d26`
  (`Record X6996-S17b-R1 focused acceptance`).
- Parent: `146ce0849401edd9160d94045d208f4fafdf0b1a`.
- Worktree clean, staged 0, untracked 0, ahead 34 behind 0.
- Tag `v1.0.0` unchanged.
- No G-TEXT-OFS or S18 commit existed at the start.
- No push, fetch, tag or release.

## Gate decision

```text
G-TEXT-OFS: REMOVE-NON-UPSTREAM-OFS-OUTPUT
X6996-G-TEXT-OFS: PASS
```

Decision basis: authoritative project-owner provenance and product decision.

The inviscid `OFS:` output at label 333 is a local, project-owner extension.
It is not part of the original XFOIL contract and has no compatibility status
to preserve. XFOIL.next 1.1.0 follows the original 6.996 behavior without this
additional output.

The decision introduces no legacy, version or opt-in mode. Only the affected
`OFS:` output path may be removed. Tests or goldens may be adjusted only for
that removal. Canonical 6.99 references and `tests/fortran-reference/**`
remain unchanged. Fortran sources, binaries and patches remain unchanged.

No external oracle measurement was invented or required for this decision.

## U6 confirmation

The U6 gate subject is the inviscid `OFS:` text decision. U6 assigns it to
`S18`. The decision therefore releases S18.

U6 names no further gate for S18. Its remaining dependencies are `S06` and
`S09`, both accepted.

S18 contains additional defined work beyond removing the `OFS:` output:

- `CLf`/`CDp` console lines;
- `xtcam` text widths;
- prompts;
- the non-plot `xtcam` text portion of G12.

The S18 contract remains text-only. It changes no numeric solver behavior,
session semantics, file format or default.

## Scope and boundaries

- Gate commit: documentation only.
- S18 implementation: not part of this gate pass.
- S18 commit boundary: one later local implementation commit.
- `G-EXPORT` remains open; S14/SVG/DXF remain blocked.
- No S19/S20/S21/S22 work.
- No product-version, banner or tag change.

## Verification

Documentation gates:

```text
check-fortran-sha:       PASS
check-license-headers:   PASS
check-release-inventory: PASS
check-public-docs:       PASS
git diff --check:        PASS
```

No full Linux suite and no Windows process were run for this
documentation-only gate. Windows processes for S18: 0.

## Next step

Implement U6 slice `S18` under the accepted text-only console contract:
remove the project-specific `OFS:` output and align the remaining defined
text behavior. Keep `G-EXPORT` open and S14/SVG/DXF blocked.

## Text-only console implementation — X6996-S18

S18 start HEAD: `a0b03d72f8ab7551d607788d8b1394f5f89280ad`
(`Decide XFOIL 6.996 text offset contract`). The G-TEXT-OFS gate commit was
already present; no duplicate gate commit was created.

### U6 scope

`S18` owns the text-only console contract: removal of the project-specific
`OFS:` record, `CLf`/`CDp` format-2020 fields, `xtcam` text widths, the
assigned prompts/help text and the non-plot `xtcam` portion of G12. No numeric
solver, session, file-format or default behavior is changed.

### Text contract matrix

| Text path | 6.99/local behavior | 6.996 target | S18 action |
|---|---|---|---|
| inviscid `OFS:` | project-specific label-333 record | absent | removed |
| `CLf` | absent from format 2020 | appended `F9.5` field | aligned |
| `CDp` | first `CDp` is `Cd-CDF`; no trailing fields | first field unchanged, then `CLf`, `CDP`, `CDP+CDF` | aligned |
| `xtcam` TCBUF/TCSCAL | `F8.4` / `F7.3` | `F10.6` / `F8.4` | aligned |
| `xtcam` TCSET/HIPNT | `F8.4` / `F7.3` | unchanged | retained |
| OPER help | ASET old wording; no blank before VELS/NAME | new ASET wording; blank records | aligned |
| PACC capacity text | `array limit` | `array limit NPX` | aligned |
| G12 | plot and text portions mixed | non-plot text only | xtcam text only |

The exact prompt literals are unchanged. The S18 prompt alignment is limited
to the OPER help records and the PACC capacity message. No global prompt
migration is included.

### C++ mapping and implementation

- `CoreOperHandlers` no longer emits the label-333 OFS record.
- `CoreInverseDesignHandlers` no longer emits OFS during QDES/SPECAL entry.
- `ViscalIterationRecord` carries the per-iteration `CLF` and `CDP` values
  already produced by `CDCALC`/`CLCALC`; no numeric value is recomputed.
- `emitViscalIterationLoads` appends the exact 6.996 fields after the existing
  `CDp = Cd-CDF` field.
- `emitTcbufPlain`/`emitTcbufReport` use the widened TCBUF/TCSCAL fields;
  `emitTcbufLegacyReport` keeps the unchanged TCSET/HIPNT fields.
- `printOperHelp` carries the 6.996 ASET wording and blank records.
- The PACC capacity message carries the 6.996 `NPX` wording.
- No solver, session, format or default code was changed.

Legacy 6.99 oracle tests project both sides through the documented
`normalizeS18LegacyText`/`normalizeS18ThicknessCamber` test helpers so the
canonical `tests/fortran-reference/**` material remains unchanged. New
`unit.ConsoleTextS18` and `integration.ConsoleTextS18` tests assert the exact
new text separately.

### Tests

New exact formatter tests:

- `unit.ConsoleTextS18ViscalLoadsAppendsClfAndCdp`
- `unit.ConsoleTextS18TcbufUsesWidenedFields`
- `unit.ConsoleTextS18TsetKeepsLegacyFields`

New integration coverage:

- `integration.ConsoleTextS18InviscidAlphaOmitsOfs`
- `integration.ConsoleTextS18ViscousLoadsAppendClfAndCdp`
- `integration.ConsoleTextS18QdesEntryOmitsOfs`
- `integration.ConsoleTextS18OperHelpMatchesTextContract`
- `integration.ConsoleTextS18XtcamReportUsesWidenedFields`

Focused result: unit `3/3`, integration `5/5`, 0 failures.

### Golden and fixture inventory

No new fixture was added. The existing batch `*_results.dat`, dump and polar
artifacts are unchanged. Only transcript/stdout text files directly affected
by the accepted S18 text contract were migrated.

| Path | Case | Old SHA-256 | New SHA-256 | +/- | Oracle basis | Cause |
|---|---|---|---|---|---|---|
| `tests/data/xfoil-6.996/s15/regression/polar-cseq-head-naca0012/stdout.raw.txt` | CSEQ head parity | `a2ccb8f5fe3808784f7ff6b196d37f369a7f67b87ec61aee0051ed17c465938e` | `d3325207864f446f3f63ef3ec9b33bca4e082e4aa8f188b9e11b479954dd5050` | +0/-12 | G-TEXT-OFS / 6.996 format 2020 | OFS and appended loads |
| `tests/data/xfoil-6.996/s15/regression/polar-wake2-alfa6-naca0012/stdout.raw.txt` | Wake-2 parity | `644cb0e9d218d43f136a8a13dbb06e1683862ea5651b04779c5616daf66adb82` | `5bfc04195bbdb79ab5c73076841fb80d7d24b6232eee610dcca4690ea5998357` | +13/-17 | G-TEXT-OFS / 6.996 format 2020 | OFS and appended loads |
| `tests/golden/batch/clark_y_transcript.txt` | batch Clark-Y | `0320bbbe0e8a62248f851eddae34145b360adc6d448491b1c6126da1a91b5d14` | `7d59cf46281b5eca647ef3a5ecd16ff276f49429b74a78bff44e5543b172bd3e` | +210/-270 | 6.996 text contract | OFS / loads |
| `tests/golden/batch/naca0012_transcript.txt` | batch NACA 0012 | `eff36989da51dd3587c065bff8eabb227af99c0a1f20411be8492a05ba96ffa4` | `78ee40a93e1daf3e2af4ee430b74d9e3934ccdc184bc93312b5dd2e7bb7ae5d8` | +599/-657 | 6.996 text contract | OFS / loads |
| `tests/golden/batch/naca2312_transcript.txt` | batch NACA 2312 | `9a41a0213e1df7623409562a1e281dc0994d6d56847399f0daf05c0fbe926187` | `20ba089d03dbafe195d26bab13f5e377fbdcfbba52b5e78bb219fe719666d328` | +180/-240 | 6.996 text contract | OFS / loads |
| `tests/golden/batch/naca4415_transcript.txt` | batch NACA 4415 | `9a264c9f09420fd4e2488883b5b9bb6518549f4f0daf3b8ea5a1b440ecc3604d` | `645a34e9ec54a6ace9c15393cc1205e75a2b4c409e193afb172f2e07ef06289a` | +161/-221 | 6.996 text contract | OFS / loads |
| `tests/golden/cli/oper-help.stdout.txt` | OPER help | `ce5b204b628e383ce1cfff69a404145fa8dc34f2226760f454350246f8a51439` | `3ea568e817c15c326574c21b560dd513168efc9e9bb6f0ce9613aa810e8de11e` | +3/-1 | 6.996 help text | ASET / blank records |
| `tests/golden/cli/oper-inviscid-alpha.stdout.txt` | inviscid alpha | `508f136c88eec47ebe80463e242cac48242db83cfc176ba9539c9ee503b2008b` | `7e8cc3d13392e6d333e827d223039330ad46c8cd2423c4d583fb48a31e1ea5f3` | +0/-2 | G-TEXT-OFS | OFS removed |
| `tests/golden/cli/oper-sequence.stdout.txt` | inviscid sequence | `254aef830c95960eea65fc9bff415893cd1d9ab46d870cdac4715fa4a09d275d` | `7e8cc3d13392e6d333e827d223039330ad46c8cd2423c4d583fb48a31e1ea5f3` | +0/-6 | G-TEXT-OFS | OFS removed |
| `tests/golden/cli/oper-viscous-alpha.stdout.txt` | viscous alpha | `7211749747f8fd94576b77487946a26dc5255305dc7b02373ce07520a1d54190` | `263c18fb8c9e5433ccbf58f292695590aab24da86db45667b8b3d6c6e0d61b24` | +5/-7 | 6.996 format 2020 | OFS / appended loads |

### Linux and gate results

```text
Full suite:                  2078/2078
Reference:                    236/236
Determinism:                  236/236
Harness families:              16/16
S18 focused unit:               3/3
S18 focused integration:        5/5
GCC 13.3 Debug:                 PASS
GCC 13.3 Release:               PASS
Clang 18.1.3 Debug:             PASS
Clang 18.1.3 Release:           PASS
ASan/UBSan focused:             PASS
Focused Valgrind:               PASS
Permanent gates:                PASS
Existing product goldens:       10 migrated and documented
Existing 6.99 references:           0 changed
```

### Windows minimal policy and R1

Exactly one focused Windows process is required after the green Linux matrix:

```text
integration.ConsoleTextS18
5/5, 0 failures
```

Exactly one focused Windows process was run:

```text
integration.ConsoleTextS18
5/5, 0 failures
```

Additional Windows processes: 0. Full Windows suite: not run. Windows
goldens: none.

### Protected state and publication freeze

- Fortran sources, binaries and patches unchanged.
- `tests/fortran-reference/**` unchanged.
- Product version and banner unchanged; `v1.0.0` unchanged.
- No global tolerance change.
- No push, fetch, amend, tag or release.

## Focused R1 acceptance — X6996-S18-R1

R1 was performed before the single S18 integration commit. Gate commit:
`a0b03d72f8ab7551d607788d8b1394f5f89280ad`.

It confirms:

- S18 is strictly text-related.
- The project-specific `OFS:` output was removed under the authoritative
  project-owner decision `G-TEXT-OFS: REMOVE-NON-UPSTREAM-OFS-OUTPUT`.
- `CLf`/`CDp`, `xtcam`, OPER help and the PACC text match the documented S18
  contract.
- Only the non-plot G12/`xtcam` text portion is included.
- No numeric solver, session, file-format, default or plot change is included.
- Ten existing product goldens were migrated and individually documented.
- No canonical 6.99 reference, Fortran source, binary or patch was changed.
- The Linux matrix is green.
- Exactly one Windows process is green; no further Windows process was run.
- No push, fetch, amend, tag or release was performed.

## Final decision

X6996-S18: PASS
X6996-S18-R1: PASS
