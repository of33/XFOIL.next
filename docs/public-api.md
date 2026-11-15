# Public C++ API

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
xfoil/SequenceResult.h            owning sequence/polar result
xfoil/SessionOptions.h            PanelingOptions
xfoil/Session.h                   stateful in-memory xfoil::Session
xfoil/AnalysisRequest.h           one-shot request and Alpha/CL targets
xfoil/Analyze.h                   xfoil::analyze one-shot entry point
xfoil/CancellationToken.h         instance-scoped cancellation
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

`xfoil::Session` is the stateful, in-memory interface introduced in L3.
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
session.lastOperatingPoint();
session.activePolar();
```

`Session` is move-only and not thread-safe. Separate sessions have
isolated state. Sequences remain available through
`Session::solveAlphaSequence`.

## One-shot analysis API

`xfoil::analyze(request)` is the stateless convenience layer introduced in
L4. Every call creates a fresh call-local `xfoil::Session`, sets the
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
  // point.liftCoefficient, point.dragCoefficient, point.surface, ...
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
preserved. A cancellation returns the same structured error as the L3
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
// missingGeometry, missingConfiguration, invalidConfiguration,
// geometryTooLarge, geometryInitializationFailed,
// analysisInitializationFailed, invalidAnalysisSettings,
// compressibilityFailure, finalizationFailed
```

## Cancellation

`CancellationToken` is instance-scoped and non-copyable. Pass it to an
analysis to request cancellation from another thread; a cancelled run returns
`AnalysisStatus::cancelled`.

```cpp
xfoil::CancellationToken token;
// token.requestCancellation();   // from another thread
xfoil::AnalysisResult r = xfoil.analyzeAtAlpha(xfoil::Degrees{4.0}, token);
```

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

The product version remains 1.1.0. There is no CMake package and no stable
C++ ABI promise in this API generation. Consumers must use a compatible
C++ standard library and compiler/ABI. The pkg-config file is
`xfoil-next.pc`; `make test-consumers` builds static and shared consumers
against a staged install only and prints one `LibraryConsumer PASS` per
consumer.
