# XFOIL 6.996 — S17a state/invalidation after geometry and file commands

Status: PASS — `X6996-S17a` and `X6996-S17a-R1` accepted. Implementation,
Linux matrix, focused Windows process and R1 documentation check are green.

Slice: `S17a` of `S17`. Scope is limited to state transitions and
invalidation after geometry and file commands. `S17b` warm starts,
sequences, convergence and menu behavior remain out of scope.

`G-EXPORT` remains open. S14, SVG and DXF remain untouched.

## U6 contract

U6 stages `S17` internally as:

- `S17a`: state/invalidation after geometry and file commands;
- `S17b`: warm starts, sequences, convergence and menu behavior.

`S17a` precedes `S17b`; separate target cases and commits are required.
No numeric kernel, format or default change belongs to either stage.

The S17a command classes are:

- geometry replacement: `NACA`, `NACS`, `PANE`;
- buffer-only geometry: `INTE`, `INTX`, `HALF`;
- current-contour adoption: `PCOP`, GDES `EXEC`;
- GDES working-buffer mutations before `EXEC`;
- pure queries: `XCEN`, `VELS`;
- file load: `LOAD`;
- file writes/exports: `SAVE`, `PSAV`, `USAV`, `ISAV`, `MSAV`, `WDEF`,
  `CPWR`, `DUMP`, `DMP`, `BLOU`;
- error, capacity and EOF paths for the above.

No new command, file format, BLDUMP3/BLOU byte change, SVG/DXF path or
general export architecture is added.

## Upstream state model

Relevant 6.996 sources:

- `xfoil.f`: `NACA`, `INTE`, `INTX`, `HALF`, `LOAD`, `PANE`, `PCOP`,
  `SAVE`, `PANGEN` flag block;
- `xgdes.f`: GDES direct buffer edits and `ABCOPY`;
- `xoper.f`: `CPWR`, `DUMP`, `DMP`, `BLOU` export projections.

Key distinctions:

- `NACA`/`NACS` generate a new buffer and call `PANGEN`; the complete
  panel-dependent analysis state is invalidated.
- `INTE` writes only the buffer (`XB/YB/NB`, derived spline/GEOPAR values,
  name). The current panel and its analysis state remain installed until
  `PCOP` or `PANE`.
- `INTX`/`HALF` likewise write only the buffer.
- `LOAD` writes the buffer and then calls `ABCOPY`. `ABCOPY` resets the
  panel-dependent caches; `LBLINI` is reset only when the previous panel
  point count differs from the new buffer point count.
- `PCOP` and GDES `EXEC` use `ABCOPY`.
- GDES direct edits change only the working/TOP buffer and `LGSAME`; only
  `EXEC` commits the current panel and invalidates panel-dependent state.
- `XCEN`/`VELS` and export writers do not mutate the session state.
- Failed file loads and EOF before a mutation leave the previous state
  unchanged.

## State-transition matrix

| Command/path | Buffer | Current panel | Analysis/BL | Preserved |
|---|---|---|---|---|
| `NACA`/`NACS` | replaced + `PANGEN` | regenerated | invalidated | operating scalars, polar and session metadata |
| `PANE` | unchanged | regenerated | invalidated | buffer, operating scalars, polar |
| `INTE` | replaced | unchanged | unchanged | panel, mapping, BL, wake, result, inverse, polar |
| `INTX`/`HALF` | replaced | unchanged | unchanged | panel, mapping, BL, wake, result, inverse, polar |
| `LOAD` | replaced | `ABCOPY` | `ABCOPY` invalidation | operating scalars, polar, station fields; `LBLINI` only if `N==NB` |
| `PCOP`/GDES `EXEC` | unchanged | `ABCOPY` | `ABCOPY` invalidation | buffer, operating scalars, polar |
| GDES pre-`EXEC` | working/TOP buffer | unchanged | unchanged | panel, mapping, BL, wake, result, inverse, polar |
| `XCEN`/`VELS` | unchanged | unchanged | unchanged | complete session state |
| `SAVE`/`PSAV`/`USAV`/`ISAV`/`MSAV` | unchanged | unchanged | unchanged | complete session state |
| `CPWR`/`DUMP`/`DMP`/`BLOU` | unchanged | unchanged | unchanged | complete session state |
| failed `LOAD` | unchanged | unchanged | unchanged | complete session state |
| EOF at a prompt | unchanged | unchanged | unchanged | complete session state |

## Implementation boundary

- Add a public buffer-only adoption operation for `INTE`, using the
  existing buffer-revision and derived-buffer update path.
- Change `INTE` from full buffer replacement to buffer-only adoption.
- Use buffer-only adoption plus `copyBufferToCurrent` for `LOAD`, so the
  canonical `ABCOPY N!=NB` warm-start branch is evaluated on the previous
  current panel.
- Reject a `LOAD` buffer above the `ABCOPY` capacity before any mutation.
- Keep `setBufferGeometry` as the full invalidation path for `NACA` and
  other genuine buffer replacements.
- Do not change GDES kernels, `ABCOPY`, `PANGEN`, export bytes, formats,
  defaults or numeric kernels.
- Add focused unit and integration tests with the prefixes
  `unit.StateInvalidationS17a` and `integration.StateInvalidationS17a`.

## Non-goals

- No `S17b` warm-start, sequence, convergence or menu behavior.
- No S14/SVG/DXF and no `G-EXPORT` change.
- No file-format migration.
- No new BLDUMP/BLDUMP2/BLDUMP3 bytes.
- No numeric kernel or default change.
- No global tolerance or golden migration.
