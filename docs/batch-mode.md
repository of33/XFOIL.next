# Batch mode

The batch mode runs one non-interactive XFOIL job through the same command
interpreter, session layer, and numerical core as the interactive CLI. It
adds deterministic defaults, a fixed path base, a machine-readable result
file, structured exit codes, comments, and prefixed run artifacts.

## Invocation

```text
xfoil.next --batch FILE                     run one batch job from FILE
xfoil.next --batch -                        read the job from stdin (cwd = process cwd)
xfoil.next --batch FILE --profile FILE      load a startup profile before the commands
xfoil.next --batch FILE --prefix PREFIX     prefix for session output files
xfoil.next --batch FILE --quiet             suppress batch output on stdout
xfoil.next --defaults FILE --batch FILE     load defaults via RDEF before the commands
xfoil.next --working-directory DIR --batch FILE
                                            working directory for the job
xfoil.next --help                           show usage and exit (exit 0)
xfoil.next --version                        show version and exit (exit 0)
```

Options are order-independent. A bare positional profile cannot be combined
with `--batch`; use `--profile` or a native geometry command instead.
`--profile`, `--prefix`, `--quiet`, `--defaults`, and `--working-directory`
require `--batch`. A duplicated `--batch`, `--profile`, or `--prefix` is an
argument error. `--quiet` is idempotent.

## Job format (`.xfb`)

A batch file is a text file:

```text
XFOIL-BATCH 1
# optional comments and an optional [job] section before [commands]
[job]
defaults = xfoil.def
stop-on-error = true
stop-on-nonconvergence = false
allow-unavailable-plot-commands = true
[commands]
NACA 2412
PANE
OPER
VISC 100000
ALFA 4
QUIT
```

Format rules:

- The first non-comment, non-blank line must be exactly `XFOIL-BATCH 1`. A
  missing marker, an unknown version (for example `XFOIL-BATCH 2`), or a
  missing `[commands]` section is a format error (exit 2).
- `[commands]` is required. Everything after it, to end of file, is the
  command stream; there are no further section headers.
- The only section before `[commands]` is `[job]`. The exact `[job]` keys in
  version 1 are `defaults`, `stop-on-error`, `stop-on-nonconvergence`, and
  `allow-unavailable-plot-commands`. There is no `prefix`, `profile`, or
  `quiet` key; those are command-line options only. An unknown or duplicate
  key is an error (exit 2).
- A comment line is a line whose first non-blank character is `#`. Comments
  are removed before execution, including inside the command stream (no
  native command starts with `#`).
- Blank lines in the command stream are passed through unchanged and keep
  their canonical menu meaning (an empty entry returns to the enclosing
  menu). Blank lines in the section zone are insignificant.
- Command text is handed to the native interpreter unchanged (only a trailing
  CR is removed). There is no second command grammar, no shell interpolation,
  no includes, no variables, no loops, and no conditions in version 1.
- Encoding is ASCII-compatible UTF-8. LF and CRLF are accepted. A UTF-8 BOM
  is tolerated only at the very start. Limits: 1 MiB file size, 1024
  characters per line, 10000 command lines; exceeding a limit is a format
  error (exit 2).

All native XFOIL commands are allowed in `[commands]`, including menu
changes, submenus, and prompt chains (a prompt consumes the following line
exactly as in an interactive session).

## Defaults and geometry

- There is no implicit startup `xfoil.def` in batch mode, and no default
  profile is ever loaded.
- Defaults are loaded only explicitly, via `--defaults FILE` or the
  `defaults =` key, through the existing `RDEF` path. Using both is an error
  (exit 2).
- Geometry is established only by `--profile FILE` or by native geometry
  commands in the command stream (`LOAD`, `NACA`, `INTE`, `INTX`, `PCOP`,
  `ASET`). A job that ends without established geometry is a batch semantics
  error (exit 4, stderr `No airfoil geometry established.`); the transcript
  and `results.dat` header are still written.

## Working directory

By default the working directory of the run is the directory containing the
batch file. `--batch -` uses the process working directory.
`--working-directory DIR` overrides it (relative to the caller's working
directory; it must exist, otherwise exit 3). All relative paths in the job
and its commands resolve against this directory.

## Output files and prefix

The runner writes exactly two of its own files in the effective working
directory:

- `transcript.txt` — the full canonical transcript;
- `results.dat` — the structured result log.

With `--prefix PREFIX` these become `PREFIXtranscript.txt` and
`PREFIXresults.dat`. The prefix transform for native output files is
`directory/name.ext` -> `directory/PREFIXname.ext` (also for absolute paths);
an empty file name stays empty; the transform is applied exactly once. A
prefix must match `[A-Za-z0-9._-]*`, be at most 128 bytes, and contain no
path separator; an invalid prefix is an argument error (exit 2).

If a runner output file already exists (after prefixing) before the first
command, the run aborts with exit 3 (`Batch output file exists: <path>`).
Version 1 never overwrites implicitly and there is no `--force`.

The prefix applies to these output commands: `SAVE`, `PSAV`, `USAV`, `ISAV`,
`WDEF`, `CPWR`, `DUMP`, `DMP`, the `PACC` polar save and dump files, `PWRT`,
`CAMB WRTT` / `CAMB WRTC`, and `MDES DUMP`. It is deliberately not applied to
input/read paths (`LOAD`, `INTE`, `INTX`, `RDEF`, `PGET`, `RGET`, `CAMB RDAC`,
`MDES READ`, `--profile`, `--defaults`, the batch file) or to read-modify-
write commands such as `MSAV`.

## Transcript and `--quiet`

Without `--quiet`, stdout bytes equal the transcript file bytes. With
`--quiet`, stdout is empty (banner, menus, and prompts included) while the
transcript file still holds the full bytes; stderr diagnostics and the exit
code are unchanged. Batch format and invocation diagnostics go to stderr and
do not appear in the transcript.

## Error and non-convergence policy

```text
stop-on-error              default true   unknown command (exit 4) and solver/core
                                          error (exit 5) end the job; false continues
                                          after the canonical message
stop-on-nonconvergence     default false  non-convergence is a domain result; true ends
                                          the job at the first non-converged point (exit 6)
allow-unavailable-plot-    default true   plot/cursor commands report the canonical
commands                                  unavailable message and continue; false makes
                                          them fatal (exit 4)
```

Non-convergence is not a process error by default. The exit code comes from
the first event classified as fatal; canonical interpreter aborts
(`xfoil.def`/`WDEF`/`RDEF` I/O) keep exit 2 and end immediately.

## Exit codes

```text
0  success (QUIT or EOF, standard policy)
1  unreadable startup profile (--profile)
2  argument or format error; duplicate defaults; invalid prefix; canonical
   RDEF/WDEF/def I/O abort
3  runner file or working-directory error (batch file, defaults file,
   existing output file, missing --working-directory)
4  batch semantics error: unknown command (default fatal), plot command when
   allow-unavailable-plot-commands = false, or no geometry established
5  solver/core error (default fatal)
6  non-convergence, only when stop-on-nonconvergence = true
```

## `results.dat`

`results.dat` is whitespace-separated, not CSV, with `#` comment lines. The
header is:

```text
# XFOIL_BATCH_RESULTS 1
# product XFOIL.next 1.1.0
# index alpha cl cd cdp cm re mach ncrit_top ncrit_bottom xtr_top xtr_bottom converged iterations
```

Numbers use `%.6f`; booleans are `0`/`1`; non-finite values print as
`nan`/`inf`/`-inf`; line endings are LF. A non-converged completed point is
recorded with `converged 0`. There is exactly one data line per completed
operating point, in execution order, with no sorting or de-duplication.

Settings and geometry are recorded as comment metadata:

- `# settings ...` lines carry the monitored configuration (re, mach, matyp,
  retyp, viscous, iter, ncrit_*, xtrip_*, vaccel, waklen, idamp, and the full
  BLPAR set). An initial block is always written; later blocks appear only
  when the snapshot changes.
- `# geometry name="..." source="..." points=N` lines record the active
  geometry (`source` is `profile:<path>` or `command:<command>`); a geometry
  change starts a new block.

## Parallelism

One job is one session in one process. Jobs in different directories do not
collide. Parallel jobs in the same directory are separated by `--prefix`.
The runner creates no temporary files of its own.

## Examples

External profile:

```text
XFOIL-BATCH 1
[commands]
LOAD sample/CLARK_Y.dat
PANE
OPER
VISC 100000
ASEQ 0 14.5 0.5

QUIT
```

Internal NACA:

```text
XFOIL-BATCH 1
[commands]
NACA 4415
PANE
OPER
VISC 100000
ASEQ 0 14.5 0.5

QUIT
```

Quiet run (stdout empty, transcript and results still written):

```sh
xfoil.next --batch job.xfb --quiet
```

Prefixed run (writes `study_transcript.txt`, `study_results.dat`, and
prefixed native outputs):

```sh
xfoil.next --batch job.xfb --prefix study_
```
