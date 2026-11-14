# XFOIL 6.99 source provenance and harness-reference correction report

Status: local correction accepted; GitHub Actions pending for the pushed commit.

This report records the main-branch correction for XFOIL.next 1.1.0. It is
separate from the completed 1.0.1 line and does not create or publish a tag or
release.

## 1. Starting point and decision

```text
Correction start HEAD: 1fbd0d5700db5dc67ecba03c2bfd9ae9c40b49e0
Product version:       1.1.0
Completed 1.0.1 line:  f648e37b47d9e8db9abf43ec78568df1544c8b6f
v1.0.1 target:         f648e37b47d9e8db9abf43ec78568df1544c8b6f
```

The owner decided to restore the archived XFOIL 6.99 source tree on `main`
and to align only the source-based harness references. The historical binary
oracle, binary-based references, product code, product goldens, tolerances and
the 6.996 source tree are outside the correction.

## 2. Original sources

External originals were used read-only:

```text
xoper.f  23f0b6354d6c544b2539ee0ad01b9975838da265f59493b026c28326b69f6524
xgdes.f  513d71d2eea3b69b5fdb3544ff506bdda68b2b51c2edbf3703f8ac268c4bd622
```

Main hashes before and after:

| File | Before | After |
|---|---|---|
| `fortran-src/xfoil-6.99/xoper.f` | `303191f22c055480561c2a037447ae62c859c7fa70fb21b1c8a1216ff34bba22` | `23f0b6354d6c544b2539ee0ad01b9975838da265f59493b026c28326b69f6524` |
| `fortran-src/xfoil-6.99/xgdes.f` | `4d32ef2f75d2069e1708f32ba789940517c512369de82eaa242cf994792dcf16` | `513d71d2eea3b69b5fdb3544ff506bdda68b2b51c2edbf3703f8ac268c4bd622` |

Only the two known local differences existed before the correction:

- `xoper.f`: the `OF Special` block with label 333 and `write(*,333)`;
- `xgdes.f`: `CALL READR(1,XRF(1),ERROR)` instead of
  `CALL READR(2,XRF,ERROR)`.

Both restored files are byte-identical to the external originals. The 6.99
source manifest and the provenance inventory were updated only for these two
hashes. `fortran-src/xfoil-6.996/**` is unchanged.

## 3. Source and binary contract

```text
fortran-bin/xfoil
f5779d6954d0b8fdc152e0e0633f18732e00b7b69228d561d629bd524bba598c
```

The historical binary is unchanged. It was built from an earlier local XFOIL
6.99 source state and is not an exact rebuild of the restored archive. It
remains the historical behavior oracle for its existing binary-based
references. Source-based harness references now follow the restored sources;
the two oracle roles are documented separately.

`tests/fortran-reference/**`, `fortran-bin/xfoil`, `fortran-bin/xfoil-6.996`,
product code and product goldens are unchanged.

## 4. Source-harness references

Nine `core-*` harness families and 21 references were corrected. Every change
consists only of removing historical two-line `OFS:` blocks. No line was
added, no numerical value changed and no tolerance or runtime normalization was
introduced.

```text
Affected references: 21
OFS blocks removed:  106
Lines removed:       212
```

The new reference bytes equal the actual output of the corresponding harness
built from the restored source tree. The proof method was: capture actual
output; walk old and actual outputs in order; allow only blank/OFS two-line
deletions; require the resulting byte stream to equal actual output exactly.

| Reference | Family | OFS blocks | Lines | Old SHA-256 | New SHA-256 |
|---|---:|---:|---:|---|---|
| `tests/data/core-blf-reference/blf_reference.txt` | blf | 3 | 6 | `fd82fe29f407ff3b6efe36215aa7c820a943f78847b665355125b7df6be62477` | `972eb498d08a8640de6e1a02eed06dcb3d7b5d048975a2ed1c2d0533fb996d6d` |
| `tests/data/core-boundary-layer-reference/boundary_layer_reference.txt` | boundary-layer | 1 | 2 | `8f4bf4a280fa8e91680bf4400ddb48e0460cc375c24528b427575c390c706f34` | `5fbce58a2e867c60a93f76189a00c74cbf11aa7005fe8e65eccdadb36daa9804` |
| `tests/data/core-c11a-state-reference/c11a_alfa_diag_harness_reference.txt` | c11a-state | 1 | 2 | `239c4bb101da598c181bd95c630cb7d00443b433e0919b02325332d7648683b5` | `28a9b483f28aed76327d043322517b228251db78c5148bb0c67041cd19cfa809` |
| `tests/data/core-c11a-state-reference/c11a_batch_stall_harness_reference.txt` | c11a-state | 30 | 60 | `e9ec86356b07265dd93bd2ae30437dfc98102d53f9b46f37f432e51e9ac23d28` | `0558d20decb405465c29bec64065210011ec253b076e0d91f0e6352dc71d7bb6` |
| `tests/data/core-c11a-state-reference/c11a_cseq_harness_reference.txt` | c11a-state | 3 | 6 | `e5486729f560ad08b0235a4ebabec5ea1f127f124a5975b0e06f407dfd78a918` | `f6dfa6d503e41b1a574327a77c0e1902359f0635f13bbeb859747a96803a693a` |
| `tests/data/core-c11a-state-reference/c11a_cseq_nseqex_harness_reference.txt` | c11a-state | 4 | 8 | `341251df270d67279aec8299b304ccb1ac6d3d6da8506bc3cc40efb5621561e6` | `6b7f7607fe216359d0d8c0080401c2c16690fe4346479deb32e0e5d5ef2e5644` |
| `tests/data/core-c11a-state-reference/c11a_init_harness_reference.txt` | c11a-state | 1 | 2 | `17410838a3d243df12c5e6ffb9fc0da8f5ba3baf83c45b61c52f9f736826e65f` | `cdf61794a8dcf90f7df39264d71fa95641ae8cea3ae060a984035966f89dddc7` |
| `tests/data/core-c11a-state-reference/c11a_lrecalc_harness_reference.txt` | c11a-state | 1 | 2 | `6bf69205fd56c2c4a92547503d4f70e007648201f0f9e9a553e2a5d282bd6f72` | `ecda51ce7fba6459a2edc7371acc2df85d4f092a3d74fc886a8a750fb4b2d16a` |
| `tests/data/core-c11a-state-reference/c11a_mach_cp_harness_reference.txt` | c11a-state | 2 | 4 | `3c79bd5f23b37d153d8cd733bf96d94c3257f6f3f18039be05287f76c7c7d38a` | `c04c76ef90ce58020e080a5dc948bc64b4a585499aca0208fedcb751f8efc7b8` |
| `tests/data/core-c11a-state-reference/c11a_mach_postproc_harness_reference.txt` | c11a-state | 2 | 4 | `f9845586ea38e784d647947d22b0aaa8bed613a0d9186f5da2ad2b1b7c95e18e` | `c2cd15414b0e173e0de8477065f138efb18e8583243bec5da6a9ce008b212b4c` |
| `tests/data/core-c11a-state-reference/c11a_mrchue341_probe_reference.txt` | c11a-state | 30 | 60 | `462e2a5c67a402c93c2010b7738a9a7f5e4135e99e4298a7247dd57658180193` | `9419791813995ef44a87f545166a7962acd969a934d6a727e453c501a775ca6b` |
| `tests/data/core-c11a-state-reference/c11a_oper_singlepoint_harness_reference.txt` | c11a-state | 3 | 6 | `15a9d6f762dc52829f23c37118965f92460fb2b7522760d1bed31b7168e929ee` | `58f27a545b614ab774cb9a2aeceb13fcdd9fd4fa61cfe58f7aea47fc7bbd7c73` |
| `tests/data/core-c11a-state-reference/c11a_state_reference.txt` | c11a-state | 2 | 4 | `fbda719b48b9c5eeab0e1df2a249b779406a8d56046240d8e9993da9e1c5b34a` | `1fa570845662814b22df1e38e0a61b8a09b375fee64067323f74ac8dc1f658d9` |
| `tests/data/core-c11a-state-reference/c11a_twopoint_reference.txt` | c11a-state | 2 | 4 | `2f5bcc0423557cd2103179bf25ef3cce33052b7fefd72581485beba7670dbb40` | `7711fb2088973976f18d405b6b09c4f71043396a49f594342648f6b986187afd` |
| `tests/data/core-cpi-reference/cpi_reference.txt` | cpi | 3 | 6 | `1efecdcd782be01fba3b0e83751a9167c1b27d7e433d99bacabfaf58af56c173` | `e2df188624bd7673060b47c6ef4d2442f5d848bfd662b4ab948f7d939b4976bd` |
| `tests/data/core-fmom-vels-reference/fmom_reference.txt` | fmom-vels | 3 | 6 | `c9ae293397ce2b04533b97bf4f0eaca1a4f8b82ec1b71027153a0e669f321686` | `835ad404d537013dccad0f9fff0cd334f9a3524fe157d5856af0e419ad3488d2` |
| `tests/data/core-fmom-vels-reference/vels_reference.txt` | fmom-vels | 4 | 8 | `df58c3f49d771e269229775ccf45b56077633496ca6953711cc3dffb3dc1f562` | `48d7f68e7b0441c700b65636838bca68b23598666d400d3beae424acd7315ef0` |
| `tests/data/core-inviscid-reference/inviscid_reference.txt` | inviscid | 8 | 16 | `d45257c9e0d4412c51c8f6d31cda01f137feaa66dcd443e50eaf70595385ce7d` | `464d8b06c89b58776db7388991132dcdd4d2d5ec11b2d8fc53c0ba31b662b3cb` |
| `tests/data/core-spec-failure-reference/specfailure_reference.txt` | spec-failure | 1 | 2 | `3c9f1bef7ea29c9d117dcadd2771d6b2c5c305a48885fbfcc87c754affba0e6e` | `4a0669bf292f9f7fd44bd216abb5ef75ef5db13b7ad676e1378e656a436e9acf` |
| `tests/data/core-viscous-reference/viscous_reference.txt` | viscous | 1 | 2 | `ab2b20522405b8e99e254f8a92c0ba697fef6f22fe002fdd22aaf4f3d63a4b2d` | `12fb4fc2746627dbfbdaf665a466b1a0ce312b96605432f9cc374e23e889dcf5` |
| `tests/data/core-xycm-reference/xycm_reference.txt` | xycm-reference | 1 | 2 | `3b83edcd06d2451b75ce7335a38db93d2f46a72acbea1207d84fb59bc77797c2` | `620f2418ad843874c084dbb56995dae0fe4c644e2a2a6b49a72106051d3b9de7` |

`tests/data/xfoil-6.996/s09/fmom_reference.txt` and
`tests/data/xfoil-6.996/s09/vels_reference.txt` are historical binary/6.996
references, not source-harness references, and are unchanged.

## 5. Provenance documentation

Current contracts were corrected in:

- `NOTICE`;
- `README.md`;
- `docs/fortran-reference.md`;
- `docs/numerical-validation.md`;
- `fortran-bin/README.txt`;
- `tools/fortran-provenance.json`.

Historical reports that described the pre-correction source state received a
narrow errata note pointing to this report. Earlier reports were not rewritten.

## 6. Local validation

```text
make test-harnesses                    16/16 families
make check-fortran-sha                 XFOIL 6.99 48/48; XFOIL 6.996 49/49
make check-license-headers             PASS
make check-dependencies                PASS
make check-release-inventory           PASS
make check-public-docs                 PASS
make test                              2121/2121
make release                           PASS
make build/release/xfoil_tests         PASS
build/release/xfoil_tests              2121/2121
make test-reference                    236/236
make test-reference-determinism        236/236
make test-sanitize                     2121/2121
make test-valgrind                     PASS
make core-link-check                   PASS
make session-link-check                PASS
Debug product version                  XFOIL.next 1.1.0
Release product version                XFOIL.next 1.1.0
```

No new Windows matrix was run. The changed files are limited to archived
Fortran sources, their source manifest, source-based harness references and
documentation. No product, build, test-binary, golden or Windows-workflow file
changed, so the previously accepted focused Windows binary contract remains
valid.

## 7. R1 focused acceptance

R1 is the final focused check before the local commit:

- the diff boundary is limited to the allowed files;
- protected binary and historical reference paths are unchanged;
- no v1.1.0 tag or GitHub release exists;
- no Windows-relevant file changed;
- the prior Windows binary contract remains the valid Windows evidence;
- no amend, rebase, reset or force operation is used.

R1 result: PASS.

## 8. GitHub Actions

At commit time this report cannot contain a run result for its own commit. The
corrected `main` is pushed normally immediately after the commit. The GitHub
Actions run for exactly that commit must be `success` before any 1.1.0 tag,
release or further publication step. The result is recorded in the external
audit area `xfoil-6.996/main-provenance-correction/ci/` and in the task
completion report.

No tag, release or further commit is created by this correction task.

## 9. Publication boundary

```text
v1.0.0:  unchanged
v1.0.1:  unchanged
v1.1.0:  absent
release/1.0.x: unchanged
```

The next separately authorized task is:

```text
XFOIL.next 1.1.0 tag and GitHub Release
```

It has not started.

## Acceptance markers

XFOILNEXT-1.1.0-SOURCE-PROVENANCE-CORRECTION: PASS
XFOILNEXT-1.1.0-R1: PASS
