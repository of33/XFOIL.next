# Command-line interface

The CLI binary is `xfoil.next` (`xfoil.next.exe` on MSYS2/Windows). It
provides the native interactive XFOIL command interface.

## Starting

```sh
xfoil.next                 # interactive session, no startup profile
xfoil.next profile.dat     # interactive session, loads profile.dat before input
xfoil.next < commands.txt  # drive the session from a redirected command file
```

There is no command-file argument; stdin redirection or a pipe is how a
command file is supplied. The only positional argument is the optional
startup profile.

On start the program prints the product banner, then attempts to read
`xfoil.def` from the current directory, then loads the optional startup
profile, then enters the menu loop. The banner is:

```text
 ===================================================
 XFOIL.next Version 1.0.0
 Copyright (C) 2026 Olaf Frommann

 Based upon XFOIL Version 6.99
 Copyright (C) 2000   Mark Drela, Harold Youngren

 This software comes with ABSOLUTELY NO WARRANTY,
   subject to the GNU General Public License.

 Caveat computor
 ===================================================
```

The top-level prompt is ` XFOIL   c>`.

## Menus

The interface is menu-driven. Entering a menu command prints that menu's
command list. The menus are:

- **TOP** (top level): program control, airfoil file input/output
  (`SAVE`, `PSAV`, `ISAV`, `MSAV`, `LOAD`, `NACA`, `INTE`, `NORM`, `HALF`),
  name handling (`NAME`, `NINC`, `XYCM`), structural display (`BEND`),
  paneling (`PCOP`, `PANE`, `.PPAR`), settings files (`WDEF`, `RDEF`), and
  entry to the submenus `.OPER`, `.MDES`, `.QDES`, `.GDES`, `.PLOP`.
- **OPER** (direct operating points): mode and freestream control (`Visc`,
  `Re`, `Mach`, `Type`, `ITER`, `INIT`), boundary-layer parameters (`.VPAR`),
  operating points (`Alfa`, `CLI`, `Cl`, `ASeq`, `CSeq`, `!` redo), polar
  handling (`Pacc`, `PGET`, `PWRT`, `PSUM`, `PLIS`, `PDEL`, `PSOR`, `PREM`,
  `PNAM`, `RGET`, `RDEL`), file exports (`DUMP`, `CPWR`), reports (`FMOM`,
  `FNEW`, `VELS`, `CPMN`), and name handling.
- **VPAR**: boundary-layer parameters (`N`, `XTR`, `VACC`, and related).
- **GDES / CAMB**: geometry design (camber/thickness editing, `OVER`, `EXEC`,
  flap, file I/O).
- **MDES / QDES**: inverse design (complex-mapping and surface-speed design).
- **PLOP**: plotting options (see the plotting boundary below).

Command names are case-insensitive. A blank line returns to the enclosing
menu (at the top level it redisplays the top menu). `?` prints help for the
current menu. `Q` / `QUIT` exits.

## File paths

Relative paths used by file commands (`LOAD`, `SAVE`, `WDEF`, `RDEF`,
`PACC`, `CPWR`, `DUMP`, and the startup `xfoil.def`) are resolved against
the process working directory. The `PREFIX` session value is only a default
for export prompts; it is not a directory.

## Settings file `xfoil.def`

`xfoil.def` is the canonical settings file: 14 positional lines holding 45
scalar values, of which 18 are productive solver-state values and 27 are
plot/display values. `WDEF` writes exactly those 14 lines; `RDEF` re-reads
them through the same path as the startup read. Lines after line 14 are
ignored. A truncated file (EOF before line 14) is a fatal read error.

## Plotting and cursor boundary

Plot and cursor commands are recognised but have no backend. Each reports
exactly:

```text
Plotting is not available in this build.
```

Session state is unchanged and the current menu is kept. No non-canonical
replacement grammar is provided for cursor commands.

## Unrecognised commands

An unknown command echoes the command and prints the canonical message,
keeping the current state. The literal message text is:

```text
 command not recognized.  Type a "?" for list
```

(The command is first echoed in a 4-character field, then this literal text
follows; for example `FOO` prints ` FOO  command not recognized.  Type a "?"
for list`.)

## EOF and exit codes

End of input ends the session in a controlled way with exit code 0, the same
as `QUIT`. (Some Fortran prompt paths abort at EOF with a runtime error and
backtrace; the C++ build ends cleanly instead — see
[known differences](known-differences.md).)

Interactive exit codes:

```text
0  success (QUIT or EOF)
1  unreadable startup profile file
2  command-line argument error, and canonical abort paths
   (startup xfoil.def EOF, WDEF open error, RDEF EOF)
```

## Batch mode

For non-interactive execution, see [batch mode](batch-mode.md). The batch
mode reuses this same interpreter and session layer.
