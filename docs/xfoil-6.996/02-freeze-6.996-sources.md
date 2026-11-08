# U2 — Freeze the XFOIL 6.996 source baseline

## Purpose

Freeze the already accepted reduced XFOIL 6.996 upstream source baseline as an
immutable, independently verifiable source set. U2 adds repository-owned
integrity metadata and a permanent gate; it does not change any upstream
source byte.

## Scope

- Create `fortran-src/xfoil-6.996/SHA256SUMS`.
- Pin exactly the 49 upstream files recorded in
  `tools/fortran-provenance.json`.
- Declare `SHA256SUMS` as repository-owned metadata for the 6.996 version
  root.
- Extend `make check-fortran-sha` to verify both the 6.99 and 6.996 source
  manifests.
- Document the final freeze contract, the accepted subset and the deliberate
  omissions.
- Verify the license/provenance gate, release inventory, public documents,
  dependency gates and the core suite after the change.

## Explicit non-goals

- No changes to any 6.99 or 6.996 upstream source byte.
- No Fortran or C++ algorithm change.
- No C++ integration of XFOIL 6.996.
- No compiler or build-flag change.
- No rebuild, rename or replacement of either Fortran binary.
- No Golden, reference or tolerance change.
- No product-version or banner change.
- No new oracle declaration and no numerical comparison in U2.
- No extraction of the archive if the accepted manifest fingerprint still
  matches the current files.

## Accepted upstream subset

`fortran-src/xfoil-6.996/` contains exactly 49 protected upstream files:

- 1,154,891 bytes in total;
- all 49 files have mode 0644;
- all 49 files are byte-identical to the accepted U0 archive entries;
- all 49 files are listed in the provenance manifest with their SHA-256 and
  notice expectation.

The accepted source archive is `xfoil6.996.tgz` with SHA-256:

```text
0feb71b58070d8514d830fcb0b609a10a11399d362493d7b9bbfb7343dff3f23
```

U2 verifies the current files against the accepted manifest. It does not
re-extract the archive.

## Source-integrity contract

`fortran-src/xfoil-6.996/SHA256SUMS` is the authoritative repository-owned
integrity inventory for the 49 upstream files. The contract is:

- exactly 49 entries;
- one entry per upstream file in the provenance manifest;
- no entry for `SHA256SUMS` itself;
- no entry for a binary, Golden, reference or generated file;
- relative paths only;
- stable lexicographic order;
- LF line endings and a final newline;
- file mode 0644.

The manifest is metadata, not upstream content. It does not count as a 50th
upstream file and must not carry an upstream notice.

## SHA256SUMS format

Every line uses the same convention as the 6.99 manifest:

```text
<sha256>  fortran-src/xfoil-6.996/<relative-path>
```

The two spaces between the hash and the path are part of the `sha256sum`
format. Verification is performed from the repository root.

## Provenance-manifest integration

In `tools/fortran-provenance.json`, the `xfoil-6.996` block keeps its root,
version ID, 49 file paths, 49 hashes, notice expectations and notice pattern.
Only its metadata list changes from:

```json
"metadata_paths": []
```

to:

```json
"metadata_paths": ["SHA256SUMS"]
```

The complete 6.99 block and `schema_version` remain unchanged.

## Permanent gate integration

`make check-fortran-sha` verifies both source manifests, with a distinct
label for each version:

```sh
sha256sum --check fortran-src/xfoil-6.99/SHA256SUMS
sha256sum --check fortran-src/xfoil-6.996/SHA256SUMS
```

A failure in either version exits non-zero. The gate reads only the local
manifests and source files; it does not access the external archive and does
not verify the diagnostic binary.

## Deliberately omitted upstream files

The frozen subset is deliberately reduced. The following archive entries are
not part of the repository:

```text
Xfoil/src/dplot1.f
Xfoil/src/xqdes1.f
Xfoil/osrc/x.log
Xfoil/plotlib/**
Xfoil/runs/**
Xfoil/cav/**
```

The full upstream documentation and example collections and the complete
source archives are also not part of the repository. Their absence is not a
gate failure because the accepted subset is explicitly the 49 frozen files.

## Diagnostic executable status

`fortran-bin/xfoil-6.996` remains a diagnostic executable. Its SHA-256 is:

```text
000b0468d9a7f4fce42d6fac24bd14147d7d326c8a9035909c8e9f98d180f50e
```

U2 does not rebuild, rename, relink or promote it to a canonical oracle. The
new source manifest does not contain the binary.

## OSMAP status

- `osmap.f` is present in the frozen subset.
- `getosfile.c` is present in the frozen subset.
- `osmap.dat` is not in the subset.
- `osmap.dat` is not in the upstream archive.
- No 6.99 OSMAP file is copied into the 6.996 directory.
- Format compatibility remains open for a later step.

U2 creates, moves or modifies no OSMAP file.

## Acceptance criteria

- `fortran-src/xfoil-6.996/SHA256SUMS` has exactly 49 entries.
- `sha256sum --check fortran-src/xfoil-6.996/SHA256SUMS` reports 49/49 OK.
- The provenance manifest declares `SHA256SUMS` as the only 6.996 metadata
  path.
- `make check-fortran-sha` verifies both versions and exits non-zero on a
  failure.
- The license/provenance gate still reports 49 files and 23 notices for
  6.996; `SHA256SUMS` is neither a source file nor a notice file.
- All 49 upstream files, both binaries and the complete 6.99 baseline remain
  unchanged.
- `make check-release-inventory`, `make check-public-docs`,
  `make check-dependencies`, `core-link-check`, `session-link-check` and
  `make test-core` pass.
- External negative tests fail as expected when a frozen file is modified or
  removed.

## Git and publication policy

U2 creates no commit before independent acceptance. The working tree is left
unstaged for `X6996-U2-R1`. No push, force-push, fetch, tag, release or
history rewrite is permitted. All local commits remain unpushed until
cumulative XFOIL.next 1.1.0 acceptance.

## Exact next allowed step

`X6996-U2-R1-Independent-Source-Freeze-Acceptance`.
