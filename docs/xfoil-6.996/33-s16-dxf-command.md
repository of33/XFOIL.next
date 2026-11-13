# XFOIL 6.996 — S16 DXF command follow-up

Status: PASS — `X6996-S16-DXF` and `X6996-S16-DXF-R1` accepted. This
follow-up activates only the hidden top-level `DXF` command and its file
wiring; the S14 encoder and format remain unchanged.

Slice: `S16-DXF`, the G-EXPORT follow-up to the accepted S14 DXF R12 encoder.
Scope is activation of the hidden top-level `DXF` command and its file wiring
only. The encoder, format and fixture bytes remain owned by S14.

## Gate and S14 basis

```text
G-EXPORT: ACCEPT-DXF-ONLY
X6996-G-EXPORT: PASS
X6996-S14: PASS
X6996-S14-R1: PASS
```

G-EXPORT commit: `8ce54d569b299fb6ad2d712372f07cd0d5eae766`

S14 commit and S16-DXF start HEAD:
`9632832d148559f02b7100be8907e2a82694dce7`

S14 parent: `8ce54d569b299fb6ad2d712372f07cd0d5eae766`

## S14 / S16 boundary

S14 owns:

- the DXF R12 byte format;
- `dxfR12TextChecked`/`dxfR12Text`;
- extents, entities, precision, LF and unterminated EOF;
- the oracle fixtures under `tests/data/xfoil-6.996/s14/`.

S16-DXF owns only:

- the hidden top-level `DXF` command;
- inline filename and prompt;
- overwrite handling;
- invalid-path and EOF behavior;
- exactly one `OutputPathPolicy` application at the open point;
- calling the S14 encoder and writing the file.

S16-DXF does not change the S14 encoder or fixture bytes. It adds no help
entry, no SVG path, no plot backend and no general export framework. The
accepted BLOU part of S16 is unchanged.

## Upstream contract

The 6.996 top-level dispatcher at `xfoil.f:185-190` adds hidden `SVG` and
`DXF` commands. Only `DXF` is in scope. `DXFOUT` is at
`xfoil.f:1710-1829`.

Accepted command behavior:

| Area | Contract |
|---|---|
| Visibility | hidden top-level command; no help entry |
| Inline filename | trimmed; used verbatim; no extension added |
| No inline filename | ASKS `Enter output filename`; no prefix default |
| Prompt answer | trimmed at both ends; blank is an invalid filename |
| Data source | current panel/contour from the session |
| Encoder | accepted S14 DXF encoder only |
| Path policy | applied exactly once before existence/open |
| Existing target | `Output file exists.  Overwrite?  Y` |
| Overwrite default | blank/default writes/truncates |
| Overwrite decline | first answer character `N`/`n`; file unchanged |
| Decline message | `DXF file not written.` |
| Invalid/unopenable path | `Bad filename.` then `DXF file not written.`; non-fatal |
| EOF at prompt | controlled session end; no file |
| Success message | none |
| State | no geometry, panel, analysis, polar or policy mutation |
| Bytes | S14 LF-only output with unterminated `EOF` |

## C++ implementation

New focused app files:

- `src/app/cli/CoreDxfHandlers.h`
- `src/app/cli/CoreDxfHandlers.cpp`

The handler resolves the logical filename, applies the existing
`OutputPathPolicy` once, checks existence/directory status, handles the
overwrite answer, obtains the current panel contour through the session API,
calls `io::dxfR12TextChecked` and writes the returned bytes in binary mode.

`src/app/cli/CommandInterpreter.cpp/.h` add only:

- top-level dispatch for `DXF`;
- the prompt-answer state machine calling the handler;
- no main-menu text change.

`Makefile` registers the new app source, the focused unit/integration tests
and the focused Valgrind filters. The S14 encoder source, API and fixtures are
not changed.

## Tests

Focused unit prefix: `unit.DxfCommandS16`

- exact S14 bytes from an inline filename;
- missing extension remains missing;
- prompt without a prefix default and with trimmed answer;
- blank prompt answer is an invalid filename;
- `N`/`n` decline keeps the existing file;
- blank/default and other answers overwrite;
- `OutputPathPolicy` is applied once;
- invalid path is non-fatal and leaves no file;
- empty contour uses the S14 skeleton.

Focused integration prefix: `integration.DxfCommandS16`

- inline command writes exact NACA 0012 S14 bytes;
- prompt path writes the same bytes;
- loaded file prefix does not become a prompt default;
- no automatic extension;
- overwrite default replaces the file;
- `N`/`n` decline preserves the file;
- invalid path is non-fatal and the session continues;
- EOF at the prompt writes no file;
- one `OutputPathPolicy` transformation;
- repeated output is deterministic and session state is unchanged;
- DXF is hidden from help and SVG remains unknown.

No new DXF fixture is added. The S14 fixtures are reused unchanged.

## Explicit non-goals

- No change to the S14 encoder or its fixture bytes.
- No SVG writer, command or fixture.
- No DXF help entry.
- No automatic `.dxf` extension.
- No prefix default at the prompt.
- No unit or geometry transformation.
- No plot dependency.
- No general export abstraction.
- No change to SAVE, PSAV, MSAV, WDEF, DUMP, DMP, BLOU, BLDUMP/BLDUMP2/
  BLDUMP3, polar text or polar dump.
- No Fortran source, binary or patch change.
- No change to `tests/fortran-reference/**`.
- No product version, banner or tolerance change.
- No Windows golden or full Windows suite.

## Acceptance conditions

- Focused unit and integration tests are green.
- Full Linux matrix, reference verify/determinism, harness families,
  focused Valgrind and permanent gates are green.
- Exactly one focused Windows process runs
  `integration.DxfCommandS16` with 11/11 tests and 0 failures.
- R1 confirms the command contract, S14 byte reuse, protected state and
  publication freeze.

## Commit boundary

One follow-up implementation commit after all acceptance conditions are green:

`Activate XFOIL 6.996 DXF command`

The existing S16 BLOU PASS markers remain unchanged.
