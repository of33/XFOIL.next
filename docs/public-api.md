# Public C++ API

The public API is a small, value-typed, session-based interface under the
`xfoil` namespace. It exposes no raw pointers, no public solver arrays, no
internal flags, and no global mutable solver state.

## Public headers

```text
xfoil/XFoil.h                     facade class xfoil::XFoil
xfoil/XFoilSession.h              xfoil::XFoilSession
xfoil/AnalysisConfig.h            configuration and validation
xfoil/AnalysisResult.h            per-point result and status
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

There is no install step and no prebuilt shared library. Compile against the
headers under `include/` and link the core, session, and IO objects that the
build produces under `build/<mode>/obj/` (the application/CLI objects and the
`main` object are not needed by the API). For example, after `make release`:

```sh
g++ -std=c++20 -Iinclude example.cpp \
    $(find build/release/obj -name '*.o' \
        -not -path '*/app/*' -not -name 'main.o') \
    -o example
```
