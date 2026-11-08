# Core polar-io reference data

These files are the canonical reference outputs of the focused Fortran
polar-io harnesses. They are produced by calling the unmodified canonical
routines from `fortran-src/xfoil-6.99/` (for example `BLDUMP`, `BLDUMP2`, `CPDUMP`,
`PLXINI`, `PLXADD`, `POLREAD`, ...) and are consumed byte-for-byte by the
C++ core tests (`tests/core/IoTests.cpp`, `tests/core/PolarTests.cpp`) and
by the harness `--check` gate.

The `.bl` files are the boundary-layer dump file-format references written
by `BLDUMP`/`BLDUMP2`; the `.cp` and `.bin` files are the `CPDUMP` and
binary-dump references of the same harness family; the `.txt` files are the
harness standard-output references.

## Regeneration and verification

Regenerate (maintainer-only, requires expert approval):

```text
tools/build-polar-io-harnesses.sh --write
```

Verify without writing (byte-compares fresh harness output against these
files; mutates nothing in the repository):

```text
tools/build-polar-io-harnesses.sh --check
```

## Rules

- These references are canonical Fortran harness outputs. They must never
  be regenerated from C++ program output.
- Any change requires expert (domain) approval and a determinism proof
  (repeated byte-identical harness runs).
- The `.bl` files are versioned on purpose: the general `*.bl` runtime rule
  in `.gitignore` carries an explicit negation for this directory so the
  reference data stays visible.

## License and provenance

These are derived reference data of the XFOIL 6.99 project. Licensing and
provenance are documented centrally in `LICENSE` and `NOTICE` at the
repository root; no per-file license header is added to reference data.
