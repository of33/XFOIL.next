# XFOIL 6.996 — U2 source-freeze report

Status: PASS

This report records implementation and acceptance of the immutable
XFOIL 6.996 source baseline. Detailed machine-generated logs remain in
the external audit area.

## Starting state

- HEAD: `3ab0fb5c1960f6fbf5a0388db041aefb08e11f1e`.
- `origin/main`: `58bca7d17f847e1aef6b1c8d3d4e167f40f69945`.
- Ahead/behind: `0 4` (behind 0, ahead 4).
- Working tree: clean; staged 0.
- Tag: `v1.0.0` unchanged.
- No fetch, push, tag or release was performed.

## Accepted upstream baseline

- `fortran-src/xfoil-6.996/` contains 49 protected upstream files.
- Total size: 1,154,891 bytes.
- All 49 files have mode 0644.
- All 49 files match the accepted provenance-manifest hashes.
- The sources were not re-extracted from the archive in U2.
- No upstream source byte was edited.

## New SHA-256 manifest

Created:

```text
fortran-src/xfoil-6.996/SHA256SUMS
```

Properties:

| Property | Value |
|---|---|
| Entries | 49 |
| Size | 4,847 bytes |
| Mode | 0644 |
| Line endings | LF |
| Final newline | yes |
| SHA-256 of the manifest | `d4fed0bb236f26b54befd8d7f346dabb969e6a177da26d692a4b27d424573c30` |

- Format: `<sha256>  fortran-src/xfoil-6.996/<relative-path>`.
- Order is stable and lexicographic.
- Every upstream manifest file is present exactly once.
- `SHA256SUMS` does not contain an entry for itself.
- No binary, Golden, reference or generated file is listed.
- `sha256sum --check fortran-src/xfoil-6.996/SHA256SUMS`: 49/49 OK.

## Provenance-manifest change

In `tools/fortran-provenance.json`, only the 6.996 metadata list changed:

```json
"metadata_paths": ["SHA256SUMS"]
```

Unchanged:

- `schema_version`;
- the complete 6.99 block;
- the 6.996 root and version ID;
- all 49 file paths;
- all 49 hashes;
- all notice expectations;
- the notice pattern.

No 50th upstream file was created. `SHA256SUMS` is repository-owned metadata,
not upstream content.

## Permanent gate change

`make check-fortran-sha` now verifies both source manifests with distinct
labels:

```sh
XFOIL 6.99 source integrity
sha256sum --check fortran-src/xfoil-6.99/SHA256SUMS

XFOIL 6.996 source integrity
sha256sum --check fortran-src/xfoil-6.996/SHA256SUMS
```

The gate:

- uses the existing permanent Makefile target;
- exits non-zero when either version fails;
- does not suppress errors;
- does not modify the checksum files;
- does not access the external archive;
- does not check either binary;
- introduces no compiler or build-flag change.

## Documentation

- Added `docs/xfoil-6.996/02-freeze-6.996-sources.md`.
- Added this report.
- Added a current-layout note in `docs/xfoil-6.996/README.md`: both version
  directories now carry their own repository-owned `SHA256SUMS`.
- Updated `docs/fortran-reference.md` to name both source manifests and the
  dual gate.
- Updated `docs/numerical-validation.md` to state 48 (6.99) + 49 (6.996)
  source-integrity files and the dual gate.
- Historical U0/U1 reports were not rewritten.

## Deliberate omissions

The frozen subset does not contain:

```text
Xfoil/src/dplot1.f
Xfoil/src/xqdes1.f
Xfoil/osrc/x.log
Xfoil/plotlib/**
Xfoil/runs/**
Xfoil/cav/**
```

The full upstream documentation and example collections and the complete
archives are also outside the repository. Their absence is not a gate failure
because the accepted subset is explicitly the 49 frozen files.

## Diagnostic executable

`fortran-bin/xfoil-6.996` remains diagnostic and unchanged:

```text
000b0468d9a7f4fce42d6fac24bd14147d7d326c8a9035909c8e9f98d180f50e
```

The source manifest does not list the binary. U2 did not rebuild, rename,
relink or promote it.

## OSMAP status

- `osmap.f` is present in the frozen subset.
- `getosfile.c` is present in the frozen subset.
- `osmap.dat` is not in the subset.
- `osmap.dat` is not in the upstream archive.
- No 6.99 OSMAP file was copied into the 6.996 directory.
- Format compatibility remains open for a later step.

## Negative tests

Two negative tests were run only in external temporary copies:

| Case | `sha256sum --check` | `make check-fortran-sha` |
|---|---:|---:|
| Content of `xutils.f` modified | exit 1, mismatch reported | exit 2, FAIL |
| `aread.f` removed | exit 1, missing file reported | exit 2, FAIL |

The real working tree was not modified. Both real manifests still verify.

## Gates

All executed gates exited 0:

- `make check-fortran-sha` (48/48 and 49/49 OK)
- `make check-license-headers`
  - self-test passed;
  - `xfoil-6.99: 48 files, 23 notice-bearing files`;
  - `xfoil-6.996: 49 files, 23 notice-bearing files`.
- `make check-release-inventory`
- `make check-public-docs`
- `make check-dependencies`
- `make core-link-check`
- `make session-link-check`
- `make test-core`: 414/414, 0 failures.

`SHA256SUMS` is neither an upstream source file nor a notice-bearing file.

## Protected data

| Artifact | Status |
|---|---|
| 48 XFOIL 6.99 sources | unchanged |
| `fortran-src/xfoil-6.99/SHA256SUMS` | unchanged, 48/48 OK |
| 49 XFOIL 6.996 upstream files | unchanged |
| New 6.996 `SHA256SUMS` | 49/49 OK |
| `fortran-bin/xfoil` | unchanged |
| `fortran-bin/xfoil-6.996` | unchanged |
| Golden data | unchanged |
| Reference data | unchanged |
| Tolerances | unchanged |
| Windows Goldens | unchanged |
| Product version | `1.0.0` unchanged |
| Banner | unchanged |

`fortran-bin/xfoil`:

```text
f5779d6954d0b8fdc152e0e0633f18732e00b7b69228d561d629bd524bba598c
```

## Git status

At the end of U2:

- Staged: 0.
- No commit and no push.
- Tracked modifications:
  - `Makefile`
  - `docs/fortran-reference.md`
  - `docs/numerical-validation.md`
  - `docs/xfoil-6.996/README.md`
  - `tools/fortran-provenance.json`
- Untracked:
  - `fortran-src/xfoil-6.996/SHA256SUMS`
  - `docs/xfoil-6.996/02-freeze-6.996-sources.md`
  - `docs/xfoil-6.996/02-freeze-6.996-sources_report.md`
- `git diff --check`: clean.
- `origin/main` unchanged; ahead/behind remains `0 4`.
- Tag `v1.0.0` unchanged.

## Publication policy

No push, force-push, fetch, tag, release or history rewrite. U2 does not
commit before independent acceptance. All local commits remain unpushed until
cumulative XFOIL.next 1.1.0 acceptance.

## Decision

`X6996-U2: PASS`

## Exact next allowed step

`X6996-U2-R1-Independent-Source-Freeze-Acceptance`.

## Independent acceptance — X6996-U2-R1

Decision: PASS

### Read-only proof

- Fingerprint before the report append: 2,873 entries (path, type, mode,
  size, SHA-256, symlink target), excluding `.git_custom/`, `build/` and
  `docs/Logo/`.
- Before/after comparison differs only in this report.
- No staging, commit, push, tag, release, fetch or history rewrite was
  performed.

### SHA-256 manifest

- `fortran-src/xfoil-6.996/SHA256SUMS`: 49 entries, 4,847 bytes, mode 0644.
- Manifest SHA-256:
  `d4fed0bb236f26b54befd8d7f346dabb969e6a177da26d692a4b27d424573c30`.
- `sha256sum --check`: 49/49 OK.
- Every protected upstream path appears exactly once; there is no self
  entry, no absolute path and no path outside `fortran-src/xfoil-6.996/`.
- Hashes match `tools/fortran-provenance.json`; the order is stable and
  lexicographic; LF and the final newline are present.

### Upstream sources

- All 49 upstream files are present, mode 0644, hash-correct and unchanged
  versus `HEAD`.
- 0 additional upstream files and 0 missing upstream files.
- The new `SHA256SUMS` is metadata, not a 50th upstream source.

### Provenance manifest

- Only the 6.996 `metadata_paths` changed from `[]` to `["SHA256SUMS"]`.
- Root, version ID, 49 paths, 49 hashes, notice expectations, notice pattern,
  the complete 6.99 block and `schema_version` are unchanged.

### Permanent dual SHA gate

- `make check-fortran-sha` exits 0 and prints separate version labels.
- 6.99: 48/48 OK.
- 6.996: 49/49 OK.
- The Makefile diff changes only this target; no compiler or build flag was
  touched, no error is suppressed, no archive is accessed and no binary is
  checked by this source gate.
- The external negative tests remain valid: modified content fails the check,
  removed file fails the check, and the Make gate exits non-zero.

### Documentation

- Both version directories are documented as carrying their own
  repository-owned `SHA256SUMS`.
- The 6.996 manifest is documented as covering 49 protected upstream files.
- The deliberate omissions, open `osmap.dat` question and diagnostic binary
  status are documented.
- No completed C++ integration is claimed; product version remains `1.0.0`
  and no compiler baseline changed.

### Gates and core test

All exit 0:

- `make check-fortran-sha`
- `make check-license-headers` (6.99: 48 files/23 notices; 6.996: 49
  files/23 notices)
- `make check-release-inventory`
- `make check-public-docs`
- `make check-dependencies`
- `make core-link-check`
- `make session-link-check`
- `make test-core`: 414/414, 0 failures.

No full suite or reference matrix rerun was required.

### Protected data

- `fortran-src/xfoil-6.99/**`: unchanged.
- 49 6.996 upstream files: unchanged.
- `fortran-bin/xfoil`: unchanged,
  `f5779d6954d0b8fdc152e0e0633f18732e00b7b69228d561d629bd524bba598c`.
- `fortran-bin/xfoil-6.996`: unchanged,
  `000b0468d9a7f4fce42d6fac24bd14147d7d326c8a9035909c8e9f98d180f50e`.
- Goldens, references, tolerances and Windows Goldens: unchanged.
- Product version: `1.0.0`; banner unchanged.

### Git status

- HEAD: `3ab0fb5c1960f6fbf5a0388db041aefb08e11f1e`.
- Staged: 0.
- Open paths: the five U2 tracked modifications, the new 6.996 manifest and
  the two U2 documents.
- `git diff --check`: clean.
- `origin/main`: `58bca7d17f847e1aef6b1c8d3d4e167f40f69945`; ahead/behind
  `0 4`.
- Tag `v1.0.0`: unchanged.

### Publication hold and next commit

- No push, force-push, fetch, tag, release or history rewrite.
- All local commits remain unpushed until cumulative XFOIL.next 1.1.0
  acceptance.
- Exact next allowed local commit: one path-limited U2 commit with the
  message `Freeze XFOIL 6.996 Fortran source baseline`. No push.
