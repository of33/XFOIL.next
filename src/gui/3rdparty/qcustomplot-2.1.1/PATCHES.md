# QCustomPlot 2.1.1 — AirfoilCenter local patches

Upstream release: QCustomPlot **2.1.1** (2022-11-06, official site news:
"compatibility up to Qt 6.4"). This directory contains the official,
unmodified-otherwise sources plus the minimal patches listed below.
License: GPL v3 or later (upstream headers preserved unchanged).

## Patch 1 — Qt >= 6.9 deprecation compatibility (2026-11-12)

Motivation: AirfoilCenter builds with Qt 6.9.3. QCP 2.1.1 predates the
Qt 6.9 deprecations and produced six `-Wdeprecated-declarations`
warnings. Every change is wrapped in `QT_VERSION` guards so behaviour
on older Qt builds is bit-for-bit unchanged.

| # | Location (upstream) | Deprecated call | Replacement |
|---|---------------------|-----------------|-------------|
| 1 | `QCPAxisTickerDateTime::getTickLabel` | `QDateTime::toTimeSpec(mDateTimeSpec)` | `QDateTime::toTimeZone(zone)` via helper `qcpZoneForTimeSpec` (UTC → UTC, OffsetFromUTC → offset of the reference datetime — same as `toTimeSpec` semantics, LocalTime → system zone), guard `>= 6.9` |
| 2 | `QCPAxisTickerDateTime::dateTimeToKey(const QDate&, Qt::TimeSpec)` | `QDate::startOfDay(timeSpec)` | `startOfDay(zone)` via helper `qcpZoneForDateSpec` (OffsetFromUTC → fixed zone 0, matching the old default `offsetSeconds = 0`), guard `>= 6.9` |
| 3–6 | `QCPColorScaleAxisRectPrivate::draw`, `QCPColorMap::draw`, `QCPColorMap::updateLegendIcon`, `QCPItemPixmap::updateScaledPixmap` | `QImage::mirrored(bool, bool)` | helper `qcpMirroredImage` calling `QImage::flipped(Qt::Orientations)`, guard `>= 6.5` (where `flipped` exists), otherwise old call |

Helpers are inserted once after `#include "qcustomplot.h"`.

Verification: full release builds (g++ and clang++, Qt 6.9.3) produce
zero warnings from this directory; plot-related regression harnesses
(bigplot, plotter, plotattributes, plotselectiondialog, controlpoints,
xfoil) pass.
