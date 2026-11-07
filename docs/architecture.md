# Architecture

XFOIL.next is organised in layers with a single, enforced dependency
direction. There is one numerical core; the interactive CLI, the public API,
and the batch mode are different entry points into the same session layer
and core.

## Layer paths

```text
public API
    -> XFoilSession
    -> SessionOps
    -> Core / IO

interactive CLI
    -> CommandInterpreter
    -> SessionComposition
    -> SessionOps
    -> Core / IO

batch mode
    -> BatchRunner
    -> CommandInterpreter
    -> SessionComposition
    -> SessionOps
    -> Core / IO
```

The batch mode reuses the interactive `CommandInterpreter`, the same
`SessionComposition`, the same `SessionOps`, and the same Core/IO paths. It
does not introduce a second command implementation, a second numerical path,
or private batch solver state.

## Dependency rule

Allowed direction:

```text
app / CLI / public API
        |
        v
core-facing application interface
        |
        v
    xfoil::core
```

The core (`include/xfoil/core/`, `src/core/`, namespace `xfoil::core`) must
not depend on the session layer, the CLI, the application layer or the
tests. In particular the core does not include the CLI
interpreter, application-specific headers, or test headers.

This rule is enforced by automated gates (`make check-dependencies`), which
check include/symbol direction, forbidden global mutable solver state, and
the Makefile source-group synchronisation.

## Sessions are independent instances

Each `XFoilSession` (and each `xfoil::XFoil` facade, which delegates to a
session) owns its own state. Two sessions are independent: running an
analysis in one session does not affect another. There are no static or
global mutable solver states. Instance-scoped values such as the viscous
acceleration and the cancellation state belong to the session, not to a
global.

A batch job is one session in one process; sessions are never reused across
jobs.

## State model

The session keeps the airfoil in distinct, well-defined states:

- **Buffer** airfoil: the raw coordinate contour (set by `LOAD`, `NACA`,
  `INTE`, `INTX`, `PCOP`, and the startup profile).
- **Current / panel** airfoil: the panelised contour produced by `PANE`
  (and `PCOP`). Panel geometry carries a revision counter so that stale
  panel data can be detected.
- Geometry-design operations (GDES/CAMB, MDES) edit the buffer and, on an
  explicit `EXEC`, commit to the current airfoil atomically.

Flap state is tracked once for the current airfoil and once for the buffer
airfoil. Flap hinge moments are reported through the polar hinge column and
`FMOM`.

Inviscid and viscous surface-pressure data are kept separately (`CPI` for
the inviscid distribution, `CPV` for the viscous distribution). The wake
length follows the canonical relation `NW = N/12 + 10*INT(WAKLEN)`. The
boundary-layer parameter set (`BLPAR`) is authoritative on the session
composition; `VACCEL`, `WAKLEN`, and `IDAMP` are session-scoped.

## Output path policy

`xfoil::io::OutputPathPolicy` is a small data type (a prefix string plus a
path transformation) injected into the session composition. It is used by the
file-writing commands so that a single `--prefix` applies exactly once to
each output file. It is non-numerical. In interactive use the prefix is empty
(identity). Input (read) paths and read-modify-write commands are not
prefixed. See [batch mode](batch-mode.md) for the full allowlist.

## Public API entry points

The modern public API (`xfoil/XFoil.h`, `xfoil/XFoilSession.h`) is the only
solver entry point. The public API is implemented through `XFoilSession`
and `SessionOps`; the `xfoil::XFoil` facade delegates to a session.

## Plotting boundary

Plot and cursor commands are recognised but have no backend. They report the
single canonical message `Plotting is not available in this build.` and leave
session state unchanged. No non-canonical cursor grammar exists.
