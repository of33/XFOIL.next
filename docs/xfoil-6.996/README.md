# XFOIL 6.996 integration strategy

## Purpose

This directory records the staged integration of the upstream XFOIL 6.996
changes into XFOIL.next.

The upstream distribution is identified as XFOIL 6.996, while the executable
banner identifies the program as XFOIL 7. These names describe the same
upstream source baseline in this integration project.

The integration must preserve the existing XFOIL 6.99 contracts until each
6.996 change has been independently classified and accepted.

## Guiding principles

The integration follows these rules:

1. Do not change the upstream source version and the compiler baseline in the
   same migration.
2. Keep the existing XFOIL 6.99 references, goldens and oracle executable
   unchanged.
3. Treat XFOIL 6.996 as a second upstream baseline until the integration is
   complete.
4. Do not overwrite 6.99 references with output produced by 6.996.
5. Do not regenerate reference artifacts merely because another compiler
   produces different floating-point bytes.
6. Integrate upstream changes in small, reviewable functional groups.
7. Keep product changes separate from source-layout and provenance changes.
8. Do not modify original upstream Fortran source bytes.
9. Do not hide large numerical or convergence changes behind tolerances.
10. Preserve the existing Windows and Linux portability contracts.

## Compiler baseline

The compiler baselines remain unchanged during the initial XFOIL 6.996
integration.

### Canonical Linux baseline

- GNU C++ 13.3.0
- GNU Fortran 13.3.0
- Ubuntu 24.04
- C++20

### Additional Linux validation

- GNU C++ 16.2.0
- Clang 18.1.3

### Accepted Windows baseline

- Windows 11
- MSYS2 UCRT64
- GCC/MinGW 16.1.0 Rev5

A compiler-baseline migration is explicitly outside the scope of the initial
6.996 integration. GNU Fortran 16.2 and newer Windows toolchains may be used
for diagnostic comparisons, but they must not redefine the canonical
references during this work.

## Upstream material policy

Only the upstream source files required to understand, build or validate the
non-graphical numerical implementation are included in the repository.

The complete upstream ZIP distributions are not included.

This avoids:

- unnecessary duplication of plotting libraries and unrelated files;
- permanent growth of the Git history;
- redistribution questions for files not needed by XFOIL.next;
- ambiguity between used source files and the complete upstream package.

External archive provenance must instead be recorded through:

- upstream release name;
- original archive filename;
- original source URL, if known;
- archive SHA-256, if available;
- extraction date;
- a manifest mapping imported files to archive paths.

The external archives may be retained in the private audit area but are not
part of the public release inventory.

## Proposed source layout

The target layout is:

```text
fortran-src/
  README.md
  xfoil-6.99/
    SHA256SUMS
    ...
  xfoil-6.996/
    SHA256SUMS
    Makefile
    ...
```

The two source directories represent separate immutable upstream baselines.

The source files under these directories must retain their upstream bytes.
Repository-specific build adapters, patches or generated files must not be
placed inside the immutable source sets.

If repository-owned build support is required, it belongs under a separate
path such as:

```text
tools/
tools/fortran/
```

or is implemented through version-aware Makefile targets.

## Current oracle executables

The current executable layout is initially retained:

```text
fortran-bin/xfoil
fortran-bin/xfoil-6.996
```

Their meanings are:

| Path | Meaning | Status |
|---|---|---|
| `fortran-bin/xfoil` | XFOIL 6.99 reference executable | canonical |
| `fortran-bin/xfoil-6.996` | XFOIL 6.996 / banner XFOIL 7 executable | diagnostic until accepted |

The existing `fortran-bin/xfoil` path is not renamed during the structural
migration. Existing scripts and the complete 6.99 validation corpus depend on
that path.

No `xfoil-6.99` symlink is introduced initially. This avoids unnecessary
cross-platform Git and Windows symlink semantics. If an explicit versioned
6.99 binary name later becomes necessary, that change requires a separate
compatibility migration.

## Oracle provenance requirements

Before `fortran-bin/xfoil-6.996` can become a canonical oracle, record:

- executable SHA-256;
- source archive identity;
- exact imported source-file hashes;
- compiler path and version;
- compiler and linker flags;
- linked plotting and runtime libraries;
- operating system and architecture;
- dynamic library dependencies;
- executable banner;
- whether the executable runs without external data files;
- build date, if recoverable;
- whether the binary can be reproduced from the retained source subset.

The binary built with the original Makefile must be described as a locally
built upstream executable, not as an executable distributed by upstream,
unless that can be independently demonstrated.

## Plotting dependencies

The reduced 6.996 source set intentionally excludes plotting sources and
plotting libraries that are not required by XFOIL.next.

The upstream Makefile may therefore be retained for provenance even if it
cannot build from the reduced public source set.

It must be documented as:

```text
upstream build description; not a supported standalone build contract for
the reduced source subset
```

Do not add dummy plotting implementations or change upstream source files
merely to make the original Makefile succeed.

A headless 6.996 oracle build may be introduced later only if its behavior can
be shown to match the accepted upstream executable for the non-graphical
cases.

## `osmap.dat` open question

The XFOIL 6.996 source archive contains the Orr–Sommerfeld reader and related
Fortran routines, but no `osmap.dat` file was found in the imported archive.

Until this is resolved:

1. do not copy the XFOIL 6.99 `osmap.dat` into the 6.996 source directory;
2. do not describe the 6.99 data file as a 6.996 upstream artifact;
3. do not generate a replacement and treat it as canonical;
4. do not change the C++ OSMAP reference data;
5. do not alter existing 6.99 OSMAP tests or hashes.

The investigation must determine:

- whether the 6.996 executable opens an external `osmap.dat`;
- whether the data is embedded or generated;
- whether another filename is used;
- whether the upstream archive accidentally omitted the file;
- whether the 6.99 data file is still expected unchanged;
- whether the existing C++ reader remains format-compatible.

Useful read-only diagnostics include:

```text
searching the 6.996 sources for OPEN/INQUIRE and filename literals
searching the executable strings
running the executable from a directory without OSMAP files
tracing file-open operations
comparing the 6.99 and 6.996 Orr–Sommerfeld routines
```

The result must be documented before any 6.996 OSMAP reference contract is
created.

## Migration phases

### U0 — Inventory and freeze

Purpose: establish exactly what has already been imported.

Tasks:

- inventory `fortran-src/xfoil-6.996/`;
- inventory `fortran-bin/xfoil-6.996`;
- record file modes, sizes and SHA-256 values;
- identify the upstream archive path for every imported source file;
- classify source, include, data and build-description files;
- confirm that no plotting source was imported unintentionally;
- record the missing `osmap.dat` issue;
- record the executable banner and dynamic dependencies;
- verify the current Git diff and release inventory.

No source movement or product change is allowed in this phase.

Deliverable:

```text
docs/refactoring/xfoil-6.996/00-inventory.md
```

Acceptance:

- complete source manifest;
- complete binary provenance statement;
- no modified 6.99 source or reference file;
- no new numerical reference data.

### U1 — Structural versioning of XFOIL 6.99

Purpose: move the existing 6.99 source baseline into its versioned directory
without changing its bytes or behavior.

Target:

```text
fortran-src/* -> fortran-src/xfoil-6.99/*
```

Tasks:

- move the existing upstream files;
- preserve every file byte and mode;
- update `SHA256SUMS`;
- update all build and validation scripts;
- update documentation and release inventory;
- keep `fortran-bin/xfoil` unchanged;
- do not yet change the 6.996 files.

Deliverable:

```text
docs/refactoring/xfoil-6.996/01-versioned-6.99-layout.md
```

Acceptance:

- every moved 6.99 file byte-identical;
- Fortran source integrity passes;
- reference verification: 236/236;
- determinism: 236/236;
- harness families: 16/16;
- full C++ suite: 1968/1968;
- no Golden or reference change;
- no product behavior change.

### U2 — Freeze the XFOIL 6.996 source baseline

Purpose: establish the imported 6.996 source set as an immutable upstream
baseline.

Tasks:

- create a source manifest;
- create `fortran-src/xfoil-6.996/SHA256SUMS`;
- identify files added, removed or omitted relative to 6.99;
- document the retained upstream Makefile;
- verify that imported source files match their archive entries;
- document the XFOIL 7 banner;
- document missing plotting dependencies and `osmap.dat`.

Deliverable:

```text
docs/refactoring/xfoil-6.996/02-freeze-6.996-sources.md
```

Acceptance:

- every imported 6.996 source file has a recorded archive origin and hash;
- no source file is edited;
- no generated file is mixed into the upstream source directory;
- no new oracle reference is declared canonical.

### U3 — Raw source-difference inventory

Purpose: determine the exact upstream changes before changing C++ code.

The comparison must distinguish:

- files identical by SHA-256;
- files changed;
- files added in 6.996;
- files removed from 6.996;
- plotting-only files absent from the reduced source set;
- include/common-block changes;
- workspace and dimension changes;
- constants and default changes;
- numerical algorithm changes;
- command and state-machine changes;
- output-format changes;
- bug fixes;
- comments or formatting only.

Raw source bytes remain untouched.

For analysis, both raw and normalized views may be produced:

```text
raw byte diff
line-ending-neutral diff
fixed-form Fortran-aware diff
comment/whitespace-reduced diagnostic diff
```

Only the raw byte comparison is authoritative. Normalized views are analysis
tools and must not replace the source files.

Deliverable:

```text
docs/refactoring/xfoil-6.996/03-source-difference-inventory.md
```

Large generated diff files should remain in the external audit area. The
public document should summarize and classify the changes.

### U4 — Oracle behavior inventory

Purpose: compare the existing 6.99 oracle with the diagnostic 6.996
executable without changing any canonical references.

Tasks:

- run both executables on a small representative smoke corpus;
- use separate working directories;
- record banners, exit codes and generated file sets;
- identify changed defaults and command behavior;
- detect changed output formats;
- investigate OSMAP file access;
- separate plotting behavior from non-graphical behavior;
- record the first behavioral divergence for each case.

Initial cases should cover:

- startup and exit;
- NACA generation;
- paneling;
- inviscid single point;
- viscous single point;
- alpha sequence;
- CL sequence;
- polar accumulation;
- boundary-layer dump;
- GDES/CAMB;
- MDES/QDES;
- defaults read/write.

Deliverable:

```text
docs/refactoring/xfoil-6.996/04-oracle-behavior-inventory.md
```

Acceptance:

- 6.99 reference corpus unchanged;
- all 6.996 results stored separately;
- no new Golden promoted without review;
- every large numerical difference classified, not tolerated globally.

### U5 — Reproducible 6.996 oracle build

Purpose: establish whether the retained 6.996 source subset can reproducibly
produce a non-graphical oracle.

Questions:

- are the plotting libraries required only for linking;
- can the original plotting libraries be treated as external build
  dependencies;
- can a headless executable be produced without changing numerical behavior;
- is the locally built `fortran-bin/xfoil-6.996` reproducible;
- what role does the missing `osmap.dat` play.

Do not modify upstream sources in place. Build adapters or link stubs, if
needed, must be repository-owned and separately reviewed.

Deliverable:

```text
docs/refactoring/xfoil-6.996/05-reproducible-oracle-build.md
```

The 6.996 executable becomes canonical only after this phase passes.

### U6 — Functional integration plan

Purpose: divide the upstream changes into reviewable C++ integration slices.

Expected groups include:

1. limits, common-state and initialization;
2. geometry and paneling;
3. inviscid analysis;
4. boundary-layer and transition numerics;
5. wake and viscous coupling;
6. inverse design;
7. OPER and sequence behavior;
8. GDES/CAMB/MDES/QDES behavior;
9. file and polar formats;
10. defaults and command text;
11. OSMAP behavior;
12. product documentation and versioning.

Deliverable:

```text
docs/refactoring/xfoil-6.996/06-functional-integration-plan.md
```

Each group must list:

- changed upstream routines;
- affected C++ files;
- affected state;
- expected numerical impact;
- required new reference cases;
- old 6.99 regression coverage;
- platform-specific risks.

### U7 and later — C++ implementation slices

Each implementation slice must:

- name the upstream routines being integrated;
- explain the behavioral change;
- add or update 6.996-specific tests;
- keep 6.99 references available for regression diagnosis;
- avoid global tolerances;
- preserve Linux byte/bit contracts where the upstream behavior is unchanged;
- use measured Windows contracts only when necessary;
- leave unrelated functional groups untouched.

No implementation slice may silently replace all 6.99 references with 6.996
outputs.

## Step-document convention

Follow-up documents in this directory should use numeric prefixes:

```text
00-inventory.md
01-versioned-6.99-layout.md
02-freeze-6.996-sources.md
03-source-difference-inventory.md
04-oracle-behavior-inventory.md
05-reproducible-oracle-build.md
06-functional-integration-plan.md
07-<first-implementation-slice>.md
08-<next-implementation-slice>.md
```

Each document must include:

1. scope;
2. source inputs;
3. changed repository paths;
4. explicit non-goals;
5. commands executed;
6. measurements;
7. protected data;
8. test results;
9. Git status;
10. decision: PASS, FAIL or INCOMPLETE;
11. exact next allowed step.

## Step report convention

Each numbered integration step has one planning document and one public
summary report:

```text
NN-step-name.md
NN-step-name_report.md
```

The report is updated throughout implementation, review and closure of the
step. It records the current status, measurements, protected contracts, test
results, Git status and final decision.

Implementation corrections and independent review results belonging to the
same numbered step are added to the same report in chronological sections.
They do not create additional public audit-report files.

Verbose logs, generated diffs, temporary checkouts, machine inventories and
other audit artifacts remain outside the public repository.

For U0 this means:

```text
00-inventory.md
00-inventory_report.md
```

For U1 this means:

```text
01-versioned-6.99-layout.md
01-versioned-6.99-layout_report.md
```

Sub-passes such as U1-F1, U1-R1 or U1-G1 are documented chronologically in
`01-versioned-6.99-layout_report.md` when they belong to the same numbered
step.

Every `*_report.md` must contain at least:

1. current status: DRAFT, PASS, FAIL or INCOMPLETE;
2. scope;
3. changed paths;
4. commands executed;
5. measurements;
6. protected data;
7. gates and tests;
8. Git status;
9. open points;
10. final decision;
11. exact next allowed step.

While a step is in progress the status may be DRAFT. After independent
acceptance the same file is brought to its final state. A report must not
contain local credentials, private absolute paths or large raw logs.

## Reference-data policy

The existing XFOIL 6.99 reference data remains immutable.

Initial 6.996 results must be stored outside the canonical 6.99 paths. A
possible future layout is:

```text
tests/fortran-reference/
  xfoil-6.99/
  xfoil-6.996/
```

That migration must not be combined with source integration. It requires a
separate structural pass proving byte identity of the moved 6.99 artifacts.

Until that migration is approved, use an external audit directory for
exploratory 6.996 outputs.

## Product-version policy

The upstream version and the XFOIL.next product version are separate.

During investigation:

```text
XFOIL.next product version: 1.0.0
upstream comparison baseline: XFOIL 6.996
upstream executable banner: XFOIL 7
```

A product version change should occur only after a defined set of 6.996
changes has been integrated and accepted. A likely product version is 1.1.0,
but that decision is outside the initial inventory and refactoring phases.

## Commit policy

The repository has already been published. All future accepted changes use
normal follow-up commits.

Forbidden:

- rewriting published history;
- replacing the public root commit;
- force-pushing;
- amending already pushed commits.

Each phase should be independently reviewed before commit and push.

### Publication hold for the 1.1.0 integration

Accepted integration steps are recorded as normal local commits. These commits
are not pushed individually. The local integration history is published only
after the complete XFOIL.next 1.1.0 scope has passed its cumulative final
acceptance.

Until that acceptance:

- `origin/main` may remain behind the local `main` branch;
- no force-push or published-history rewrite is permitted;
- the existing `v1.0.0` tag remains unchanged;
- no `v1.1.0` tag or GitHub Release is created.

## Immediate next step

The first permitted implementation step is:

```text
U0 — Inventory and freeze
```

It must not move files, change build paths, modify references or port any
6.996 algorithm.

Its purpose is only to establish the exact provenance and current state of:

```text
fortran-src/xfoil-6.996/
fortran-bin/xfoil-6.996
```

and to confirm that the existing 6.99 baseline remains unchanged.
