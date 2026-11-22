# Public C++ API

A complete user manual is available as
[XFOIL.next Library User Manual](../XFOIL.next-Library-Manual.pdf). It
describes the implemented API and build state in detail.

The public API is a small, value-typed, session-based interface under the
`xfoil` namespace. It exposes no raw pointers, no public solver arrays, no
internal flags, and no global mutable solver state.

## Public headers

```text
xfoil/XFoil.h                     facade class xfoil::XFoil
xfoil/XFoilSession.h              xfoil::XFoilSession
xfoil/Export.h                    XFOIL_API visibility/import-export macro
xfoil/AnalysisConfig.h            configuration and validation
xfoil/AnalysisResult.h            per-point result and status
xfoil/AnalysisData.h              owning operating-point and surface data
xfoil/Result.h                    Error, ErrorCategory, Result<T>
xfoil/Diagnostic.h                structured diagnostic record
xfoil/SequenceResult.h            owning compact/detailed sequence result
xfoil/SessionOptions.h            PanelingOptions
xfoil/Session.h                   stateful in-memory xfoil::Session
xfoil/AnalysisRequest.h           one-shot request and Alpha/CL targets
xfoil/Analyze.h                   xfoil::analyze one-shot entry point
xfoil/CancellationToken.h         instance-scoped cancellation
xfoil/Progress.h                  optional synchronous progress event/callback
xfoil/SkinFriction.h              owning upper/lower skin-friction distribution
xfoil/SessionTypes.h              GeometrySetResult / GeometrySetError
xfoil/Types.h                     Point2D, Vector2D, Degrees, Radians
xfoil/geometry/AirfoilGeometry.h  owning unpanelled coordinate value type
xfoil/Limits.h                    canonical capacity constants (no API surface)
```

`xfoil/OneBasedArray.h` and `xfoil/OneBasedMatrix.h` are retained utility
headers (1-based array/matrix helpers). They are not part of the solver
facade.

The session layer (`xfoil/session/...`) and the core (`xfoil/core/...`) are
internal; they are not part of the public API surface.

## `xfoil::XFoil`

A movable, non-copyable Pimpl facade that delegates to a session.

```cpp
xfoil::XFoil xfoil;

xfoil::GeometrySetResult      setGeometry(geometry::AirfoilGeometry geometry);
xfoil::ConfigValidationResult configure(const AnalysisConfig& config);

xfoil::AnalysisResult analyzeAtAlpha(Degrees alpha);
xfoil::AnalysisResult analyzeAtAlpha(Degrees alpha,
                                     const CancellationToken& cancellation);

std::vector<xfoil::AnalysisResult> analyzePolar(Degrees start,
                                                Degrees endExclusive,
                                                Degrees step);
std::vector<xfoil::AnalysisResult> analyzePolar(Degrees start,
                                                Degrees endExclusive,
                                                Degrees step,
                                                const CancellationToken& cancellation);

const geometry::AirfoilGeometry& geometry() const noexcept;
const AnalysisConfig&             config()      const noexcept;
const AnalysisResult&             lastResult()  const noexcept;
bool hasGeometry()      const noexcept;
bool hasConfiguration() const noexcept;
```

`xfoil::XFoilSession` offers the same public operations and is the session
type the facade builds on.

## Setting geometry

`xfoil::geometry::AirfoilGeometry` is an owning value type for unpanelled
coordinates. `assign` reports ordinary validation failures (too few points,
non-finite coordinate) through `GeometryAssignmentResult`, not exceptions.

```cpp
std::vector<xfoil::Point2D> pts = { /* ordered x/y pairs */ };
xfoil::geometry::AirfoilGeometry geom;
auto set = geom.assign("NACA 2412", pts);
if (!set.success()) { /* set.error, set.pointIndex, set.message */ }
```

## Configuration

`AnalysisConfig` is a plain value type. Invalid values are reported through
`ConfigValidationResult`; there is no automatic correction.

```cpp
xfoil::AnalysisConfig cfg;
cfg.reynoldsNumber = 100000.0;   // default
cfg.machNumber     = 0.0;        // default
cfg.viscous        = true;       // default
cfg.maximumIterations = 100;     // default
cfg.viscousAcceleration = 0.01;  // default
cfg.transition.nCritical = 9.0;  // default
cfg.transition.topTripX = 1.0;   // default
cfg.transition.bottomTripX = 1.0;// default
cfg.reynoldsDependency = xfoil::ReynoldsDependency::constant;
cfg.machDependency     = xfoil::MachDependency::constant;

auto ok = xfoil.configure(cfg);
if (!ok.success()) { /* ok.error, ok.message */ }
```

## Single point and polar

`analyzeAtAlpha` runs one operating point. `analyzePolar` runs a sequence
from `start` to `endExclusive` in `step` increments and returns one result
per point.

## In-memory session API

`xfoil::Session` is the stateful, in-memory interface.
It owns one isolated session state and accepts only memory objects. It
performs no file or console I/O and never terminates the host process.

```cpp
xfoil::Session session;

auto set = session.setGeometry(geom);
if (set.isErr()) { /* set.error() */ }

xfoil::PanelingOptions paneling;   // defaults: 240 nodes, canonical INIT
auto panel = session.generatePanel(paneling);
if (panel.isErr()) { /* panel.error() */ }

xfoil::AnalysisConfig config;      // the only analysis configuration source
auto configured = session.configure(config);
if (configured.isErr()) { /* configured.error() */ }

auto point = session.solveAlpha(xfoil::Degrees{4.0});
// or: auto point = session.solveCl(0.5);
```

Additional read-only projections:

```cpp
session.currentPanel();
session.currentSurfaceDistribution();
session.currentSkinFriction();
session.lastOperatingPoint();
session.activePolar();
```

`Session` is move-only and not thread-safe. Separate sessions have
isolated state. Compact scalar sequences remain available through
`Session::solveAlphaSequence`. A detailed owning alpha sequence is
available through `Session::solveDetailedAlphaSequence`.

## Operating-point drag results

`OperatingPointData` carries the scalar drag results of the current
successful point:

- `dragCoefficient`: total drag coefficient `CD` from the existing
  solver result.
- `dragCoefficientPressure`: optional pressure/form-drag coefficient
  `CDp` when the viscous VISCAL completion provides it.
- `dragCoefficientFriction`: optional friction-drag coefficient `CDF`
  from the canonical viscous state. It is copied from the solver value,
  never reconstructed as `CD - CDp`.
- `skinFriction`: optional owning `SkinFrictionDistribution` for the same
  valid viscous point.

The two optional drag fields and `skinFriction` are present together for
a successful viscous point with a valid final VISCAL state, including a
successful but unconverged point. They are absent for an inviscid point.
A cancelled or failed point returns an error instead of exposing stale
values from a previous point.

The Cf distribution uses boundary-layer stations and is separate from the
panel-node `SurfaceDistribution` Cp data. Cp and Cf are never mixed in one
vector type. Wake stations are excluded.

`xfoil::analyze(request)` returns these owning values directly, so Cf is
available after the temporary one-shot session has been destroyed.
`Session::currentSkinFriction()` remains available for stateful callers
and uses the same projection as `OperatingPointData::skinFriction`.

Compact sequence and polar points remain scalar rows. They carry `CD`,
optional `CDp`, optional friction-only `CDF`, optional XFOIL `CDv`, the
canonical iteration count when available, and the convergence status.
`SequenceResult` and `PolarPoint` do not carry Cp or Cf distributions,
boundary-layer station arrays or wake data. The full Cp/Cf detail remains
in `OperatingPointData`, in the session projections
`currentSurfaceDistribution()` and `currentSkinFriction()`, and in the
detailed sequence result described below.

For a live `Session::solveAlphaSequence` point, `PolarPoint` fills these
scalars from the point's completed solver state:

- `dragCoefficientPressure`: direct CDp value; present for a successful
  inviscid or viscous point.
- `dragCoefficientFriction`: direct canonical friction `CDF` from the final
  VISCAL state; present only for a successful viscous point. It is never
  reconstructed as `CD - CDp` and is never integrated from Cf.
- `dragCoefficientViscous`: the existing XFOIL `CDv` total viscous drag
  column, not the friction-only `CDF`.
- `iterations`: the executed point iteration count; `0` for a successful
  inviscid point and absent for failed or cancelled points.

A successful but unconverged viscous point keeps its canonical scalar
values. Cancelled and failed points leave the optional scalars empty; no
value is carried over from a previous point. Stored-polar points exposed
through `activePolar()` carry the stored CDp/CDv columns but do not carry
CDF or an iteration count because the stored polar model has no canonical
fields for them.

`PolarPoint` is a scalar sequence row, not an owning surface-detail result.
Do not expect a `surface` or `skinFriction` member on it.

The legacy `AnalysisResult` compatibility type remains unchanged and does
not gain the optional detail fields. The legacy `analyzePolar` path returns
`AnalysisResult` values as before; callers that need the compact complete
point should use `Session::solveAlphaSequence`.

## Detailed alpha sequence

`Session::solveDetailedAlphaSequence` runs the same alpha sequence path as
`solveAlphaSequence` and additionally keeps the full owning detail of every
executed point:

```cpp
struct DetailedSequencePoint {
  PolarPoint polar;
  std::optional<OperatingPointData> operatingPoint;
};

struct DetailedSequenceResult {
  std::vector<DetailedSequencePoint> points;
  SequenceStopReason stopReason{SequenceStopReason::notRun};
  std::optional<std::size_t> stoppedAtIndex;
  std::vector<Diagnostic> diagnostics;
};

xfoil::Result<xfoil::DetailedSequenceResult> result =
    session.solveDetailedAlphaSequence(sequence, nullptr, progress);
```

The two overloads match the compact method:

```cpp
Result<DetailedSequenceResult> solveDetailedAlphaSequence(
    const AlphaSequence& sequence,
    const CancellationToken* cancellation = nullptr);
Result<DetailedSequenceResult> solveDetailedAlphaSequence(
    const AlphaSequence& sequence,
    const CancellationToken* cancellation,
    const ProgressCallback& progress);
```

`points` follows the requested sequence order. `stoppedAtIndex` is the
zero-based index of the point that ended the sequence. The stop reasons are
the same as for `SequenceResult`; there is no separate stop contract.

For every point that was actually executed, `polar` contains the same
compact scalar values, diagnostics and status as the compact result. The
optional `operatingPoint` owns the full Cp surface and, for a valid viscous
point, the Cf distribution and friction coefficient `CDF`. It is present
for a successful point, including a successful but unconverged point with a
consistent final state. It is absent for a point cancelled before a valid
result, for a failed point without a consistent result, and for a point
that was not executed. No detail value is carried over from a previous
point.

An inviscid detail point has `surface` but no `skinFriction` and no
`dragCoefficientFriction`. A viscous detail point with a valid final VISCAL
state has `surface`, `skinFriction` and `dragCoefficientFriction`. The Cf
data uses boundary-layer stations, excludes wake stations, preserves the
sign and is never interpolated onto Cp/panel nodes. Cp and Cf remain
separate owning vector sets.

The detailed result is deliberately more memory-intensive than the compact
result. It is intended for GUI distribution views and detailed host
evaluation. It has no lower point limit than the existing sequence limit
and no hidden compression, decimation or interpolation. All data remains
valid after the session is changed or destroyed. The compact
`SequenceResult` remains the normal choice for scalar polar work; the
library does not choose between the two forms automatically.

The detailed method performs no second solver run, no re-solve of a
selected point, no warm-start duplication and no text/CLI parsing. It
shares the canonical sequence loop, warm start, end-point handling,
cancellation and progress events with the compact method. The callback
receives the same sequence events.

## CL sequence

`xfoil::LiftSequence` is the public lift-coefficient sequence request:

```cpp
struct LiftSequence {
  double start{};
  double endExclusive{};
  double step{};
};
```

`start` and `endExclusive` are dimensionless lift coefficients. `step` must
be non-zero and its sign must agree with `start -> endExclusive`. The end is
exclusive, matching the public alpha sequence and `analyzePolar`; an interval
with fewer than one step still executes its start point. At most 100000 points
are run. The request is an owning value type and has no file, GUI or CLI
dependency.

```cpp
xfoil::LiftSequence lift;
lift.start = 0.4;
lift.endExclusive = 0.8;
lift.step = 0.2;

auto compact = session.solveClSequence(lift, nullptr, progress);
auto detailed = session.solveDetailedClSequence(lift, nullptr, progress);
```

The corresponding signatures are:

```cpp
Result<SequenceResult> solveClSequence(
    const LiftSequence& sequence,
    const CancellationToken* cancellation = nullptr);
Result<SequenceResult> solveClSequence(
    const LiftSequence& sequence,
    const CancellationToken* cancellation,
    const ProgressCallback& progress);
Result<DetailedSequenceResult> solveDetailedClSequence(
    const LiftSequence& sequence,
    const CancellationToken* cancellation = nullptr);
Result<DetailedSequenceResult> solveDetailedClSequence(
    const LiftSequence& sequence,
    const CancellationToken* cancellation,
    const ProgressCallback& progress);
```

Both forms use the same canonical specified-CL point path as `solveCl()` and
the established public Session sequence policy. The first point starts its
SPECCL at alpha 0.0; the alpha reached by a successful point is carried to the
next point within the run. This is the library sequence policy, not the
separate CLI CSEQ OPER policy. CLI CSEQ output, warm start, NSEQEX behavior and
rounding are unchanged.

The compact result contains scalar `PolarPoint` rows. The detailed result
additionally owns `OperatingPointData` for every executed point, including the
Cp surface and, for a valid viscous point, the Cf distribution and CDF. Both
forms share the same internal sequence run; the detailed form does not run a
second solver and does not call the CLI. The two forms emit the same progress
events.

Cancellation may occur before the first point, between points or at a safe
VISCAL iteration boundary. Partial owning results are returned with
`SequenceStopReason::cancelled`; a cancelled point carries no stale Cp, Cf,
CDF or iteration value. The session remains reusable after cancellation. A
progress callback failure stops the run with
`progress.callbackFailure`. Non-converged but executed points remain present
and continue the run, as in the public alpha sequence.

## Skin-friction distribution

`xfoil/SkinFriction.h` provides owning upper/lower boundary-layer station
data for the current valid viscous point:

```cpp
#include "xfoil/SkinFriction.h"

auto cf = session.currentSkinFriction();
if (cf.isErr()) {
  // cf.error().code
} else {
  const xfoil::SkinFrictionDistribution& distribution = cf.value();
  // distribution.upper and distribution.lower are independent
  // SkinFrictionSide values with x, y, s and cf arrays.
}
```

`SkinFrictionSide` contains `side`, `x`, `y`, `s` and `cf`. All four
arrays on a side have the same length; upper and lower lengths may
differ. The station order runs from the leading-edge region toward the
trailing edge, with increasing `s`. `x` is not required to be monotonic.
Wake stations are excluded.

`cf` is dimensionless and uses the existing BLDUMP definition:

```text
cf = tau / (0.5 * qinf^2)
```

`tau` comes from the final existing VISCAL state and `qinf` from the
operating point. `qinf` is the dimensionless freestream speed scale
(Fortran `QINF`, normally unity), not the dynamic pressure; the reference
dynamic-pressure scale is `QUE = 0.5*qinf^2`. In dimensional variables
the definition is `cf = tau_w/(0.5*rho_inf*U_inf^2)`. The sign is
preserved; there is no absolute value, clamping, interpolation or
rounding. The Cf stations are boundary-layer stations, not the
Cp/panel-node discretization. The result owns all vectors and remains
valid after later session changes.

A successful but unconverged viscous point may return the last complete
VISCAL state. Check the operating point's convergence status separately.
An inviscid last point returns `skinFriction.notViscous`. A missing
analysis returns `skinFriction.noAnalyzedPoint`. A cancelled or
callback-failed point never exposes the previous point's Cf as the
current result; it returns `skinFriction.noValidViscousState`. Malformed
internal station data returns `skinFriction.inconsistentState`.
`currentSkinFriction()` is a const, non-mutating projection: it runs no
solver, initializes no boundary layer and performs no file or console
I/O. One-shot callers receive the same owning Cf distribution through
`OperatingPointData::skinFriction`. There is no Cf distribution in
`PolarPoint` or `SequenceResult`.

## One-shot analysis API

`xfoil::analyze(request)` is the stateless convenience layer. Every call creates a fresh call-local `xfoil::Session`, sets the
geometry, generates the panel, applies the configuration, runs exactly one
alpha or CL point, and returns an owning `Result<OperatingPointData>`.

```cpp
#include "xfoil/Analyze.h"
#include "xfoil/AnalysisRequest.h"

xfoil::AnalysisRequest request;
request.geometry = geom;            // owning AirfoilGeometry
request.paneling = {};              // PanelingOptions defaults
request.configuration = config;     // AnalysisConfig
request.target = xfoil::AlphaTarget{xfoil::Degrees{4.0}};
// or: request.target = xfoil::LiftTarget{0.5};

auto result = xfoil::analyze(request);
if (result.isErr()) {
  // result.error().category, result.error().code, result.error().message
} else {
  const xfoil::OperatingPointData& point = result.value();
  // point.liftCoefficient, point.dragCoefficient,
  // point.dragCoefficientPressure, point.dragCoefficientFriction,
  // point.surface, point.skinFriction, ...
}
```

The request contains exactly one target through
`std::variant<AlphaTarget, LiftTarget>`. It contains no filenames, paths,
streams, callbacks or session state. A default request has an empty
geometry and therefore fails at the geometry step.

Error order follows execution order: geometry, paneling, configuration,
target value, solver.

A non-converged but executed point is a successful value with
`ConvergenceStatus::unconverged`; diagnostics and available values are
preserved. A cancellation returns the same structured error as the
session API. `analyze` performs no file or console I/O, uses no global or
hidden session cache, and does not mutate the request.

## Result and error status

`AnalysisResult` is a copyable value type. `successful` is true when the
analysis ran to a domain conclusion (including a non-converged point);
`converged` is true only for `AnalysisStatus::converged`. Initialization,
configuration, cancellation, and compressibility failures set `successful`
to false.

```cpp
xfoil::AnalysisResult r = xfoil.analyzeAtAlpha(xfoil::Degrees{4.0});
if (r.successful && r.converged) {
  // r.liftCoefficient, r.dragCoefficient, r.momentCoefficient,
  // r.centerOfPressureX, r.iterations, r.rmsResidual, r.maximumResidual
}
// r.status is one of: notAnalyzed, converged, unconverged, cancelled,
// progressCallbackFailure, missingGeometry, missingConfiguration,
// invalidConfiguration, geometryTooLarge, geometryInitializationFailed,
// analysisInitializationFailed, invalidAnalysisSettings,
// compressibilityFailure, finalizationFailed
```

## Optional progress reporting

`xfoil/Progress.h` defines an optional, synchronous observer. It is
separate from cancellation and is not a global sink.

```cpp
#include "xfoil/Progress.h"
#include "xfoil/Session.h"

void onProgress(const xfoil::ProgressEvent& event) {
  // event.phase, event.completed, event.total, event.fraction,
  // event.sequenceIndex, event.iteration
}

xfoil::ProgressCallback progress = onProgress;

xfoil::Session session;
session.setGeometry(geometry, progress);
session.generatePanel(paneling, progress);
session.solveAlpha(xfoil::Degrees{4.0}, nullptr, progress);
```

`ProgressPhase` distinguishes geometry, paneling, initialization,
inviscid solve, viscous iteration and sequence phases. Geometry and
paneling report a coarse 0/1 and 1/1 pair. A successful inviscid solve
reports initialization and inviscid phase boundaries. Viscous iterations
are reported after each fully completed iteration with a zero-based
`iteration`, `completed == iteration + 1`, and the effective iteration
limit as `total`. Sequence points additionally carry a zero-based
`sequenceIndex`. `fraction` is present only when `total` is positive.
Nested phase fractions are local to their phase; there is no global
fraction promise.

The callback runs synchronously on the calling thread. The library starts
no worker thread. The number of callback invocations is not guaranteed.
`configure` has no progress event. A one-shot `analyze(request,
cancellation, progress)` passes the same callback through geometry,
paneling and the selected solver; it does not introduce a second event
source.

A callback may copy the event, update host state or request the separate
`CancellationToken`. Do not call the same session recursively from its
callback. If a callback throws, the exception is caught inside the
library, the operation stops at the last completed safe checkpoint and
returns `ErrorCategory::invalidArgument` with code
`progress.callbackFailure`. No transactional rollback is promised, but
the session remains reusable. Callback failure is not reported as
cancellation. Progress reporting performs no library file, console,
environment, working-directory or process operation.

## Cancellation

`CancellationToken` is instance-scoped and non-copyable. Pass it to an
analysis to request cancellation from another thread; a cancelled run returns
`AnalysisStatus::cancelled`.

```cpp
xfoil::CancellationToken token;
// token.requestCancellation();   // from another thread
xfoil::AnalysisResult r = xfoil.analyzeAtAlpha(xfoil::Degrees{4.0}, token);
```

The cancellation flag is observed before a point, between sequence points
and at safe boundaries between completed viscous iterations. A token set
during an in-flight viscous iteration takes effect after that iteration
finishes; no further iteration starts and the point is reported as
cancelled, not as a converged partial point.

## Sessions are independent

Each `XFoil` / `XFoilSession` instance owns its state. Two instances are
independent; running an analysis in one does not affect the other. There are
no global solver states.

## Threading contract

A single session instance is not thread-safe: do not call into the same
`XFoil` / `XFoilSession` from multiple threads concurrently. Use one session
per thread. The `CancellationToken` flag itself is atomic and may be observed
from another thread.

## Result lifetime

`AnalysisResult` is a value type and stays valid after further calls on the
same session. `lastResult()` returns the most recent result held by the
session.

## Minimal example

```cpp
#include <vector>
#include "xfoil/XFoil.h"
#include "xfoil/geometry/AirfoilGeometry.h"

int main() {
  std::vector<xfoil::Point2D> pts = {
    {1.0, 0.0}, {0.5, 0.06}, {0.0, 0.0}, {0.5, -0.06}, {1.0, 0.0}
  };

  xfoil::geometry::AirfoilGeometry geom;
  if (!geom.assign("symmetric", pts).success()) {
    return 1;
  }

  xfoil::XFoil xfoil;
  if (!xfoil.setGeometry(std::move(geom)).success()) {
    return 1;
  }

  xfoil::AnalysisConfig cfg;
  cfg.reynoldsNumber = 1.0e6;
  cfg.viscous = true;
  if (!xfoil.configure(cfg).success()) {
    return 1;
  }

  xfoil::AnalysisResult r = xfoil.analyzeAtAlpha(xfoil::Degrees{2.0});
  return (r.successful && r.converged) ? 0 : 1;
}
```

## Library artifacts and installation

The library build uses the same Makefile as the CLI. The application and
CLI objects are not part of the library archive.

```sh
make library                 # release static and shared library
make library-static          # static only
make library-shared          # shared/DLL only
make install DESTDIR=/tmp/stage PREFIX=/usr
```

Artifacts:

```text
Linux static          build/<mode>/lib/libxfoil_next.a
Linux shared          build/<mode>/lib/libxfoil_next.so
Windows static        build/<mode>/lib/libxfoil_next_static.a
Windows DLL           build/<mode>/lib/xfoil_next.dll
Windows import lib    build/<mode>/lib/libxfoil_next.dll.a
```

`make install` installs the complete `include/xfoil/**` tree (the public
and internal include roots), the selected static/shared library, the CLI,
`LICENSE`, `NOTICE`, and `lib/pkgconfig/xfoil-next.pc` below `DESTDIR`.
The installed include tree is physically complete because the public
headers reference session value types; the internal `xfoil/session`,
`xfoil/core` and `xfoil/io` headers are not additional public API.

A static consumer links the archive directly:

```sh
g++ -std=c++20 -I"$PREFIX/include" example.cpp \
    "$PREFIX/lib/libxfoil_next.a" -o example
```

A shared consumer links `-lxfoil_next`. On Windows, define
`XFOIL_NEXT_USE_SHARED` when compiling a DLL consumer; a static consumer
defines neither `XFOIL_NEXT_USE_SHARED` nor `XFOIL_NEXT_BUILD_SHARED`.

```sh
g++ -std=c++20 -DXFOIL_NEXT_USE_SHARED -I"$PREFIX/include" \
    example.cpp -L"$PREFIX/lib" -lxfoil_next -o example
```

The product version remains 1.2.1. There is no CMake package and no stable
C++ ABI promise in the current public API. Consumers must use a compatible
C++ standard library and compiler/ABI. The pkg-config file is
`xfoil-next.pc`; `make test-consumers` builds static and shared consumers
against a staged install only and prints one `LibraryConsumer PASS` per
consumer.

## Standalone consumer demo

`library-demo/` is a complete external consumer. It uses only installed
public headers and pkg-config metadata, reads a two-column profile file
in the application, and passes the coordinates to `xfoil::Session`
through `xfoil::geometry::AirfoilGeometry`. The analysis workflow is an
alpha sequence followed by a specified-CL point in the same session.

```sh
PKG_CONFIG_PATH=/usr/local/lib/pkgconfig make -C library-demo
xfoil-next-lib-demo \
  --airfoil library-demo/profiles/naca0012.dat \
  --re 1000000 \
  --alpha-start -2 \
  --alpha-end 8 \
  --alpha-step 1 \
  --cl 0.6
```

The required arguments are `--airfoil`, `--re`, `--alpha-start`,
`--alpha-end`, `--alpha-step`, and `--cl`. `--mach`, `--iterations`, and
`--panels` are optional and otherwise use `xfoil::AnalysisConfig{}` and
`xfoil::PanelingOptions{}` defaults. `--re 0` selects inviscid analysis;
positive `--re` selects viscous analysis. `--alpha-end` is exclusive.
The demo application owns profile file I/O; the library performs no file
or console I/O. `make test-library-demo` builds and runs the static and shared
demo against a staged install.

## No-I/O, isolation and concurrency contract

During library API calls the implementation performs no application file,
console, environment, working-directory or process operation. Construction,
analysis, projection, reset and destruction operate on memory and owned
values only. The loader and runtime initialization are outside this
contract; memory allocation is allowed. `xfoil::analyze` creates only a
call-local session and returns an owning result.

Different `Session` instances have isolated logical state. Resetting,
cancelling, failing or destroying one instance does not change another.
A single `Session` is move-only and is not thread-safe; do not call into
the same instance concurrently.

Independent sessions have passed diagnostic parallel execution and
ThreadSanitizer checks, but concurrent use of separate sessions is not part
of the supported public contract. Use one session per thread and synchronize
host access.

The Linux test targets `make test-library-isolation`,
`make test-library-progress`, `make test-library-noio` and
`make test-library-concurrency` exercise these contracts. The no-I/O proof is Linux-only; the Windows check uses
source/dependency, import-table, directory, cwd and stdout/stderr evidence
rather than libc interposition.
