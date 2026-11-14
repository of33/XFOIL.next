# XFOIL.next 1.0.1 maintenance report

Companion to [maintenance-1.0.1.md](maintenance-1.0.1.md).

## Status

```text
XFOILNEXT-1.0.1-MAINTENANCE: PASS
XFOILNEXT-1.0.1-SOURCE-CORRECTION: PASS
XFOILNEXT-1.0.1-HARNESS-REFERENCE-CORRECTION: PASS
XFOILNEXT-1.0.1-R1: PASS
```

The product remains `XFOIL.next 1.0.1`. The source-correction commit on
`release/1.0.x` restores the two archived XFOIL 6.99 sources and updates the
affected provenance documentation. The follow-up harness-reference correction
aligns the source-based harness references with those restored sources. The
previously published tag `v1.0.1` is moved to the harness-correction commit
only after this acceptance and only with the owner's explicit authorization.

## Basis

- Branch: `release/1.0.x`
- Base commit: `58bca7d17f847e1aef6b1c8d3d4e167f40f69945`
  (`v1.0.0^{commit}`)
- Previous 1.0.1 maintenance commit: `43c60a74e1f842449dd89fbf68a27654f68ccaaf`
- Source-correction commit: `03d52b5043b7f74199b266c92a03e721f6b4137b`
- Previous public `v1.0.1` target:
  `03d52b5043b7f74199b266c92a03e721f6b4137b`
- Product version: `1.0.1`
- `main`: unchanged at `1fbd0d5700db5dc67ecba03c2bfd9ae9c40b49e0`
- `v1.0.0`: unchanged at `58bca7d17f847e1aef6b1c8d3d4e167f40f69945`

## Source correction

The provenance audit found local modifications in the archived Fortran copy.
They are now removed exactly:

| File | Historical local SHA-256 | Restored original SHA-256 |
|---|---|---|
| `fortran-src/xoper.f` | `303191f22c055480561c2a037447ae62c859c7fa70fb21b1c8a1216ff34bba22` | `23f0b6354d6c544b2539ee0ad01b9975838da265f59493b026c28326b69f6524` |
| `fortran-src/xgdes.f` | `4d32ef2f75d2069e1708f32ba789940517c512369de82eaa242cf994792dcf16` | `513d71d2eea3b69b5fdb3544ff506bdda68b2b51c2edbf3703f8ac268c4bd622` |

The removed differences were exactly:

- `xoper.f`: local `OF Special` block with label 333 and `write(*,333)`;
- `xgdes.f`: `CALL READR(1,XRF(1),ERROR)` instead of
  `CALL READR(2,XRF,ERROR)`.

No other archived Fortran source changed. `fortran-src/SHA256SUMS` was
updated only for these two entries and verifies with
`make check-fortran-sha`.

## Historical oracle binary

`fortran-bin/xfoil` remains the historical 1.0.0 reference binary:

```text
f5779d6954d0b8fdc152e0e0633f18732e00b7b69228d561d629bd524bba598c  xfoil
```

It was built from the earlier local source state, still emits the historical
local OFS line, and is deliberately not rebuilt from the restored upstream
sources in this correction. `fortran-bin/SHA256SUMS` is unchanged. Its
archived reference outputs remain historical compatibility records and were
not migrated.

## Source-harness reference correction

The failed GitHub Actions run on `03d52b5` reported 9 of 16 harness families
failing because their source-based references still contained the historical
local OFS blocks while the harnesses now built from the restored originals.

The correction removes exactly those blocks. No runtime filtering, whitespace
normalization, tolerance change or numerical update was introduced.

- 21 source-harness reference files
- 9 affected harness families
- 106 removed OFS blocks, 212 removed lines
- every new reference equals the actual harness output byte-for-byte
- historical binary references and `fortran-bin/xfoil` remain unchanged

| Reference | OFS blocks | Old SHA-256 | New SHA-256 (= actual) |
|---|---:|---|---|
| `tests/data/core-blf-reference/blf_reference.txt` | 3 | `fd82fe29f407ff3b6efe36215aa7c820a943f78847b665355125b7df6be62477` | `972eb498d08a8640de6e1a02eed06dcb3d7b5d048975a2ed1c2d0533fb996d6d` |
| `tests/data/core-boundary-layer-reference/boundary_layer_reference.txt` | 1 | `8f4bf4a280fa8e91680bf4400ddb48e0460cc375c24528b427575c390c706f34` | `5fbce58a2e867c60a93f76189a00c74cbf11aa7005fe8e65eccdadb36daa9804` |
| `tests/data/core-c11a-state-reference/c11a_state_reference.txt` | 2 | `fbda719b48b9c5eeab0e1df2a249b779406a8d56046240d8e9993da9e1c5b34a` | `1fa570845662814b22df1e38e0a61b8a09b375fee64067323f74ac8dc1f658d9` |
| `tests/data/core-c11a-state-reference/c11a_twopoint_reference.txt` | 2 | `2f5bcc0423557cd2103179bf25ef3cce33052b7fefd72581485beba7670dbb40` | `7711fb2088973976f18d405b6b09c4f71043396a49f594342648f6b986187afd` |
| `tests/data/core-c11a-state-reference/c11a_cseq_harness_reference.txt` | 3 | `e5486729f560ad08b0235a4ebabec5ea1f127f124a5975b0e06f407dfd78a918` | `f6dfa6d503e41b1a574327a77c0e1902359f0635f13bbeb859747a96803a693a` |
| `tests/data/core-c11a-state-reference/c11a_cseq_nseqex_harness_reference.txt` | 4 | `341251df270d67279aec8299b304ccb1ac6d3d6da8506bc3cc40efb5621561e6` | `6b7f7607fe216359d0d8c0080401c2c16690fe4346479deb32e0e5d5ef2e5644` |
| `tests/data/core-c11a-state-reference/c11a_batch_stall_harness_reference.txt` | 30 | `e9ec86356b07265dd93bd2ae30437dfc98102d53f9b46f37f432e51e9ac23d28` | `0558d20decb405465c29bec64065210011ec253b076e0d91f0e6352dc71d7bb6` |
| `tests/data/core-c11a-state-reference/c11a_mrchue341_probe_reference.txt` | 30 | `462e2a5c67a402c93c2010b7738a9a7f5e4135e99e4298a7247dd57658180193` | `9419791813995ef44a87f545166a7962acd969a934d6a727e453c501a775ca6b` |
| `tests/data/core-c11a-state-reference/c11a_lrecalc_harness_reference.txt` | 1 | `6bf69205fd56c2c4a92547503d4f70e007648201f0f9e9a553e2a5d282bd6f72` | `ecda51ce7fba6459a2edc7371acc2df85d4f092a3d74fc886a8a750fb4b2d16a` |
| `tests/data/core-c11a-state-reference/c11a_init_harness_reference.txt` | 1 | `17410838a3d243df12c5e6ffb9fc0da8f5ba3baf83c45b61c52f9f736826e65f` | `cdf61794a8dcf90f7df39264d71fa95641ae8cea3ae060a984035966f89dddc7` |
| `tests/data/core-c11a-state-reference/c11a_mach_postproc_harness_reference.txt` | 2 | `f9845586ea38e784d647947d22b0aaa8bed613a0d9186f5da2ad2b1b7c95e18e` | `c2cd15414b0e173e0de8477065f138efb18e8583243bec5da6a9ce008b212b4c` |
| `tests/data/core-c11a-state-reference/c11a_mach_cp_harness_reference.txt` | 2 | `3c79bd5f23b37d153d8cd733bf96d94c3257f6f3f18039be05287f76c7c7d38a` | `c04c76ef90ce58020e080a5dc948bc64b4a585499aca0208fedcb751f8efc7b8` |
| `tests/data/core-c11a-state-reference/c11a_oper_singlepoint_harness_reference.txt` | 3 | `15a9d6f762dc52829f23c37118965f92460fb2b7522760d1bed31b7168e929ee` | `58f27a545b614ab774cb9a2aeceb13fcdd9fd4fa61cfe58f7aea47fc7bbd7c73` |
| `tests/data/core-c11a-state-reference/c11a_alfa_diag_harness_reference.txt` | 1 | `239c4bb101da598c181bd95c630cb7d00443b433e0919b02325332d7648683b5` | `28a9b483f28aed76327d043322517b228251db78c5148bb0c67041cd19cfa809` |
| `tests/data/core-cpi-reference/cpi_reference.txt` | 3 | `1efecdcd782be01fba3b0e83751a9167c1b27d7e433d99bacabfaf58af56c173` | `e2df188624bd7673060b47c6ef4d2442f5d848bfd662b4ab948f7d939b4976bd` |
| `tests/data/core-fmom-vels-reference/fmom_reference.txt` | 3 | `c9ae293397ce2b04533b97bf4f0eaca1a4f8b82ec1b71027153a0e669f321686` | `835ad404d537013dccad0f9fff0cd334f9a3524fe157d5856af0e419ad3488d2` |
| `tests/data/core-fmom-vels-reference/vels_reference.txt` | 4 | `df58c3f49d771e269229775ccf45b56077633496ca6953711cc3dffb3dc1f562` | `48d7f68e7b0441c700b65636838bca68b23598666d400d3beae424acd7315ef0` |
| `tests/data/core-inviscid-reference/inviscid_reference.txt` | 8 | `d45257c9e0d4412c51c8f6d31cda01f137feaa66dcd443e50eaf70595385ce7d` | `464d8b06c89b58776db7388991132dcdd4d2d5ec11b2d8fc53c0ba31b662b3cb` |
| `tests/data/core-spec-failure-reference/specfailure_reference.txt` | 1 | `3c9f1bef7ea29c9d117dcadd2771d6b2c5c305a48885fbfcc87c754affba0e6e` | `4a0669bf292f9f7fd44bd216abb5ef75ef5db13b7ad676e1378e656a436e9acf` |
| `tests/data/core-viscous-reference/viscous_reference.txt` | 1 | `ab2b20522405b8e99e254f8a92c0ba697fef6f22fe002fdd22aaf4f3d63a4b2d` | `12fb4fc2746627dbfbdaf665a466b1a0ce312b96605432f9cc374e23e889dcf5` |
| `tests/data/core-xycm-reference/xycm_reference.txt` | 1 | `3b83edcd06d2451b75ce7335a38db93d2f46a72acbea1207d84fb59bc77797c2` | `620f2418ad843874c084dbb56995dae0fe4c644e2a2a6b49a72106051d3b9de7` |

## Product result

The C++ OPER and QDES/SPECAL paths no longer print the non-upstream `OFS:`
line. The `specifiedPointOutput` event and the SPECAL snapshot remain
available for chronology and structured results. The product banner and batch
metadata report `1.0.1`.

No product C++, test logic, product golden, canonical
`tests/fortran-reference/**` data, Fortran binary or binary SHA manifest
changed. The source-based harness references were corrected only for the
historical OFS blocks.

## Provenance documentation

The current provenance statements were corrected:

- `NOTICE`
- `fortran-bin/README.txt`
- `docs/fortran-reference.md`
- `docs/numerical-validation.md`
- `README.md`
- this maintenance pair

The current archived sources are now described as byte-identical upstream
originals. The historical oracle is described separately, including its
earlier source state and retained OFS behavior.

## Linux validation

```text
make check-fortran-sha                 OK
make check-license-headers             OK
make check-dependencies                OK
make check-release-inventory           OK
make check-public-docs                 OK
make test                              1970/1970
make release                           OK
build/release/xfoil_tests              1970/1970
make test-reference                    236/236
make test-reference-determinism        236/236
make test-harnesses                    16/16 families
debug --version                        XFOIL.next 1.0.1
release --version                      XFOIL.next 1.0.1
```

The suite composition remains core 414, unit 1669, integration 246,
regression 5, batch 47.

## Windows validation

No new Windows process was started. The correction changes only archived
Fortran sources, source-based harness references, the source SHA manifest and
provenance/maintenance documentation. Product, build and Windows test files
are unchanged. The previously accepted focused Windows 1.0.1 run remains
valid:

```text
MINGW64_NT-10.0-26100 DESKTOP-00M0NIP
g++.exe (Rev5, Built by MSYS2 project) 16.1.0
GNU Make 4.4.1
version: XFOIL.next 1.0.1
integration.OfsRemoval101: OK
0 failure(s) in 1 test(s)
XFOILNEXT-1.0.1-R1: PASS
```

## Tag correction and release state

The owner authorized moving the annotated tag `v1.0.1` to the
harness-reference correction commit after this acceptance. The correction
commit is pushed normally to `release/1.0.x`; only `refs/tags/v1.0.1` is
force-updated. No branch force-push is authorized.

No GitHub Release is created in this step. The 1.0.1 GitHub Release remains a
separate manual action after the corrected branch and tag are confirmed.
