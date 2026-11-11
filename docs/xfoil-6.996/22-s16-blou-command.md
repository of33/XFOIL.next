# XFOIL 6.996 — S16 BLOU command acceptance

Status: PASS — `X6996-S16` accepted. Implementation, Linux matrix,
permanent gates and the single focused Windows process are green.

Slice: `S16` BLOU command only. SVG/DXF remain blocked by `G-EXPORT`.
`S13` owns the BLDUMP3 format; S16 owns command activation.

## Contract

`BLOU` is a hidden 6.996 OPER command. It writes the BLDUMP3 artifact and
does not appear in the OPER help.

| Input state | Filename | Output |
|---|---|---|
| inline argument | argument used verbatim; no extension rewriting | BLDUMP3 file, overwrite/truncate |
| no argument, loaded-file prefix | prompt ` Enter filename:  <prefix>.bl`; blank accepts default | BLDUMP3 file |
| no argument, no prefix | prompt `Enter filename   s>  `; name required | BLDUMP3 file |
| no valid BL solution | selected filename | canonical header-only BLDUMP3 artifact |
| existing target | selected filename | truncated and overwritten without an overwrite prompt |
| unreadable/invalid target | selected filename | controlled export failure, exit 2, no partial file |
| EOF at filename prompt | none | controlled session end, no file |
| valid BL solution | selected filename | exact S13 20-column BLDUMP3 bytes |

BLOU prints no success message. It creates no automatic `:00.bl` sidefile.
Repeated runs are deterministic. The command does not mutate geometry,
paneling, operating-point, polar or BL state.

## Command matrix

| Probe | Menu | Argument/prompt | Expected action |
|---|---|---|---|
| `BLOU out.bl` | OPER | inline | write `out.bl` verbatim |
| `BLOU produced.txt` | OPER | inline | write `produced.txt`; do not append `.bl` |
| `BLOU` | OPER | loaded prefix | default `<prefix>.bl` with padded prompt |
| `BLOU` | OPER | no prefix | `Enter filename   s>  ` prompt |
| `BLOU` | OPER | blank with prefix | accept default |
| `BLOU` | OPER | EOF | no file, controlled end |
| `BLOU` | OPER, no BL | any | header-only canonical BLDUMP3 file |
| `BLOU bad/path` | OPER | invalid path | export failure, exit 2 |
| `?` | OPER | help | no BLOU entry; existing DUMP entry unchanged |

## Upstream mapping

- `fortran-src/xfoil-6.996/xoper.f:1685-1686` dispatches `BLOU` to
  `BLDUMP3(COMARG)`.
- `BLDUMP3` is in `xoper.f:2290-2502`; the OPER help has no BLOU entry.
- 6.99 has no BLOU command.
- The writer emits two 20-column header lines before station loops. With
  no stations the stock artifact is the two header lines plus one blank
  line (491 bytes).
- Stock filename handling uses inline `COMARG`, else the SETFILE prefix
  default, else the ASKS prompt. `STATUS='UNKNOWN'` overwrites/truncates.
- Stock open failure aborts with exit 2 and no file; stock EOF at the
  filename prompt aborts with exit 2. The product command keeps the
  existing controlled session-end convention for interactive EOF.

## Implementation boundary

- Register only `BLOU` in the existing OPER dispatcher.
- Add one handler using the existing export-filename and file-write path.
- Reuse the S13 BLDUMP3 writer for valid BL input.
- Expose the canonical two-line BLDUMP3 header so the command can produce
  the stock header-only artifact without duplicating the format.
- Keep `boundaryLayerDump3TextChecked` validation unchanged; incomplete
  station data is still rejected by the checked writer.
- Do not change DUMP, DMP, CPWR, help text, defaults, product version or
  banner.
- Do not add SVG/DXF, a general export framework or an automatic sidefile.

## Oracle

Stock-6.996 oracle cases are under external `xfoil-6.996/s16/`.
Committed fixtures are under `tests/data/xfoil-6.996/s16/`.

| Fixture | Size | SHA-256 |
|---|---:|---|
| `blou-clark-y-re1e6-alfa4.bl` | 42704 | `04d81ba85130f109ef5d06f260a318f52e648bac9de7e792f1112d7047138c5d` |
| `blou-no-stations.bl` | 491 | `f53e0956302c5cc3e153dc7b61a7874fcd5bf881083f88494b6c733b59cb7f94` |

The viscous fixture is identical to the accepted S13 BLDUMP3 a28
artifact. The no-station fixture contains the two canonical header lines
and the stock trailing blank line.

## Acceptance gates

Linux:

```text
Full suite:          2054/2054
Reference:            236/236
Determinism:          236/236
Harness families:       16/16
S16 focused unit:        2/2
S16 focused integration: 10/10
Permanent gates:       PASS
Existing goldens changed: 0
```

Exactly one focused Windows process confirmed:

```text
integration.BlouCommandS16
10/10, 0 failures
```

The final commit is local only. No push, fetch, tag, release or amend is
part of S16.

## Non-goals

- No SVG/DXF.
- No BLDUMP/BLDUMP2/DUMP/DMP changes.
- No new BL numerics.
- No automatic `:00.bl` sidefile.
- No general export architecture.
- No global numeric tolerance or golden regeneration.
- No S13 format rewrite.
