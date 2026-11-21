// XFOIL.next
//
// Copyright (C) 2026 Olaf Frommann
// https://github.com/of33/XFOIL.next
//
// XFOIL.next is based on XFOIL 6.99 by Mark Drela and
// Harold Youngren.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//
// SPDX-License-Identifier: GPL-3.0-or-later

// File: src/gui/model/GuiResultSummary.cpp
// Direct, exact, interpolated and sampled summary semantics over owning
// GUI points. No solver call and no reconstruction of CDF or Cp/Cf.

#include "model/GuiResultSummary.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace xfoil::gui {
namespace {

constexpr double kAlphaZeroTolerance = 1.0e-9;
constexpr double kLiftZeroTolerance = 1.0e-9;
constexpr double kMinimumAlphaSpan = 1.0e-12;

struct ScalarPoint {
  std::size_t sourceIndex{};
  double alpha{};
  double cl{};
  double cd{};
  double cm{};
  double efficiency{};
  bool hasCd{false};
  bool hasEfficiency{false};
};

bool finite(double value) {
  return std::isfinite(value);
}

std::optional<double> finiteOptional(double value) {
  return finite(value) ? std::optional<double>{value} : std::nullopt;
}

std::vector<ScalarPoint> eligiblePoints(
    const std::vector<GuiPointEntry>& points) {
  std::vector<ScalarPoint> result;
  for (const GuiPointEntry& point : points) {
    if (!point.enabled || !point.converged) {
      continue;
    }
    const std::optional<double> cl = guiPointCl(point);
    const std::optional<double> cd = guiPointCd(point);
    const std::optional<double> cm = guiPointCm(point);
    if (!cl.has_value() || !cm.has_value() || !finite(point.alpha) ||
        !finite(*cl) || !finite(*cm)) {
      continue;
    }
    ScalarPoint scalar;
    scalar.sourceIndex = point.sourceIndex;
    scalar.alpha = point.alpha;
    scalar.cl = *cl;
    scalar.cm = *cm;
    if (cd.has_value() && finite(*cd)) {
      scalar.cd = *cd;
      scalar.hasCd = true;
      if (*cd > 0.0 && finite(*cl / *cd)) {
        scalar.efficiency = *cl / *cd;
        scalar.hasEfficiency = true;
      }
    }
    result.push_back(scalar);
  }
  std::sort(result.begin(), result.end(),
            [](const ScalarPoint& left, const ScalarPoint& right) {
              if (left.alpha != right.alpha) {
                return left.alpha < right.alpha;
              }
              return left.sourceIndex < right.sourceIndex;
            });
  return result;
}

std::optional<double> interpolate(double x, double x1, double y1,
                                  double x2, double y2) {
  const double span = x2 - x1;
  if (!finite(span) || std::abs(span) <= kMinimumAlphaSpan) {
    return std::nullopt;
  }
  const double t = (x - x1) / span;
  const double value = y1 + t * (y2 - y1);
  return finite(value) ? std::optional<double>{value} : std::nullopt;
}

std::optional<double> clAtAlphaZero(
    const std::vector<ScalarPoint>& points, GuiResultProvenance& provenance) {
  provenance = GuiResultProvenance::unavailable;
  std::optional<ScalarPoint> exact;
  for (const ScalarPoint& point : points) {
    if (std::abs(point.alpha) <= kAlphaZeroTolerance &&
        (!exact.has_value() ||
         std::abs(point.alpha) < std::abs(exact->alpha) ||
         (std::abs(point.alpha) == std::abs(exact->alpha) &&
          point.sourceIndex < exact->sourceIndex))) {
      exact = point;
    }
  }
  if (exact.has_value()) {
    provenance = GuiResultProvenance::exact;
    return exact->cl;
  }

  std::optional<double> best;
  double bestSpan = std::numeric_limits<double>::infinity();
  for (std::size_t index = 1; index < points.size(); ++index) {
    const ScalarPoint& left = points[index - 1];
    const ScalarPoint& right = points[index];
    if (left.alpha >= 0.0 || right.alpha <= 0.0) {
      continue;
    }
    const double span = right.alpha - left.alpha;
    const std::optional<double> value =
        interpolate(0.0, left.alpha, left.cl, right.alpha, right.cl);
    if (!value.has_value()) {
      continue;
    }
    if (span < bestSpan) {
      bestSpan = span;
      best = value;
    }
  }
  if (best.has_value()) {
    provenance = GuiResultProvenance::interpolated;
  }
  return best;
}

std::optional<double> minimumDrag(const std::vector<ScalarPoint>& points) {
  std::optional<double> best;
  for (const ScalarPoint& point : points) {
    if (!point.hasCd || !(point.cd >= 0.0)) {
      continue;
    }
    if (!best.has_value() || point.cd < *best) {
      best = point.cd;
    }
  }
  return best;
}

struct ZeroLiftResult {
  std::optional<double> alpha;
  std::optional<double> cm;
  GuiResultProvenance provenance{GuiResultProvenance::unavailable};
};

ZeroLiftResult alphaAtZeroLift(const std::vector<ScalarPoint>& points) {
  ZeroLiftResult result;
  std::optional<ScalarPoint> exact;
  for (const ScalarPoint& point : points) {
    if (std::abs(point.cl) <= kLiftZeroTolerance &&
        (!exact.has_value() ||
         std::abs(point.alpha) < std::abs(exact->alpha) ||
         (std::abs(point.alpha) == std::abs(exact->alpha) &&
          point.sourceIndex < exact->sourceIndex))) {
      exact = point;
    }
  }
  if (exact.has_value()) {
    result.alpha = exact->alpha;
    result.cm = exact->cm;
    result.provenance = GuiResultProvenance::exact;
    return result;
  }

  std::optional<double> bestAlpha;
  std::optional<double> bestCm;
  double bestMagnitude = std::numeric_limits<double>::infinity();
  for (std::size_t index = 1; index < points.size(); ++index) {
    const ScalarPoint& left = points[index - 1];
    const ScalarPoint& right = points[index];
    if (!((left.cl < 0.0 && right.cl > 0.0) ||
          (left.cl > 0.0 && right.cl < 0.0))) {
      continue;
    }
    const double denominator = right.cl - left.cl;
    if (!finite(denominator) ||
        std::abs(denominator) <= kLiftZeroTolerance) {
      continue;
    }
    const double t = -left.cl / denominator;
    if (!finite(t) || t < 0.0 || t > 1.0) {
      continue;
    }
    const double alpha = left.alpha + t * (right.alpha - left.alpha);
    const double cm = left.cm + t * (right.cm - left.cm);
    if (!finite(alpha) || !finite(cm)) {
      continue;
    }
    if (std::abs(alpha) < bestMagnitude) {
      bestMagnitude = std::abs(alpha);
      bestAlpha = alpha;
      bestCm = cm;
    }
  }
  if (bestAlpha.has_value()) {
    result.alpha = bestAlpha;
    result.cm = bestCm;
    result.provenance = GuiResultProvenance::interpolated;
  }
  return result;
}

struct Vertex {
  double x{};
  double y{};
};

std::optional<Vertex> concaveVertex(double x1, double y1, double x2,
                                    double y2, double x3, double y3) {
  if (!(x1 < x2 && x2 < x3) ||
      x2 - x1 <= kMinimumAlphaSpan ||
      x3 - x2 <= kMinimumAlphaSpan) {
    return std::nullopt;
  }
  const double firstSlope = (y2 - y1) / (x2 - x1);
  const double secondSlope = (y3 - y2) / (x3 - x2);
  const double a = (secondSlope - firstSlope) / (x3 - x1);
  const double b = firstSlope - a * (x1 + x2);
  const double c = y1 - a * x1 * x1 - b * x1;
  if (!finite(a) || !finite(b) || !finite(c) || !(a < 0.0) ||
      std::abs(a) <= 1.0e-18) {
    return std::nullopt;
  }
  const double x = -b / (2.0 * a);
  const double y = a * x * x + b * x + c;
  if (!finite(x) || !finite(y) || x <= x1 || x >= x3) {
    return std::nullopt;
  }
  return Vertex{x, y};
}

struct MaximumResult {
  std::optional<double> value;
  GuiResultProvenance provenance{GuiResultProvenance::unavailable};
};

MaximumResult maximum(
    const std::vector<ScalarPoint>& points,
    bool useEfficiency) {
  MaximumResult result;
  std::optional<double> sampled;
  for (const ScalarPoint& point : points) {
    const std::optional<double> value =
        useEfficiency
            ? (point.hasEfficiency
                   ? std::optional<double>{point.efficiency}
                   : std::nullopt)
            : std::optional<double>{point.cl};
    if (!value.has_value() || !finite(*value)) {
      continue;
    }
    if (!sampled.has_value() || *value > *sampled) {
      sampled = *value;
    }
  }
  if (!sampled.has_value()) {
    return result;
  }

  std::optional<Vertex> bracket;
  for (std::size_t index = 1; index + 1 < points.size(); ++index) {
    const ScalarPoint& left = points[index - 1];
    const ScalarPoint& middle = points[index];
    const ScalarPoint& right = points[index + 1];
    if (useEfficiency &&
        (!left.hasEfficiency || !middle.hasEfficiency ||
         !right.hasEfficiency)) {
      continue;
    }
    const double leftValue = useEfficiency ? left.efficiency : left.cl;
    const double middleValue = useEfficiency ? middle.efficiency : middle.cl;
    const double rightValue = useEfficiency ? right.efficiency : right.cl;
    if (!(left.alpha < middle.alpha && middle.alpha < right.alpha) ||
        !(leftValue < middleValue && rightValue < middleValue)) {
      continue;
    }
    const std::optional<Vertex> candidate =
        concaveVertex(left.alpha, leftValue, middle.alpha, middleValue,
                      right.alpha, rightValue);
    if (!candidate.has_value() || candidate->y < middleValue) {
      continue;
    }
    if (!bracket.has_value() || candidate->y > bracket->y) {
      bracket = candidate;
    }
  }

  if (bracket.has_value() && bracket->y >= *sampled) {
    result.value = bracket->y;
    result.provenance = GuiResultProvenance::interpolated;
    return result;
  }
  result.value = sampled;
  result.provenance = GuiResultProvenance::sampledProvisional;
  return result;
}

}  // namespace

std::optional<double> guiPointCd(const GuiPointEntry& point) {
  if (point.operatingPoint.has_value()) {
    const auto value =
        finiteOptional(point.operatingPoint->dragCoefficient);
    if (value.has_value()) {
      return value;
    }
  }
  return finiteOptional(point.polar.dragCoefficient);
}

std::optional<double> guiPointCdf(const GuiPointEntry& point) {
  if (point.operatingPoint.has_value() &&
      point.operatingPoint->dragCoefficientFriction.has_value()) {
    const auto value = finiteOptional(
        *point.operatingPoint->dragCoefficientFriction);
    if (value.has_value()) {
      return value;
    }
  }
  if (point.polar.dragCoefficientFriction.has_value()) {
    return finiteOptional(*point.polar.dragCoefficientFriction);
  }
  return std::nullopt;
}

std::optional<double> guiPointCdp(const GuiPointEntry& point) {
  if (point.operatingPoint.has_value() &&
      point.operatingPoint->dragCoefficientPressure.has_value()) {
    const auto value = finiteOptional(
        *point.operatingPoint->dragCoefficientPressure);
    if (value.has_value()) {
      return value;
    }
  }
  if (point.polar.dragCoefficientPressure.has_value()) {
    return finiteOptional(*point.polar.dragCoefficientPressure);
  }
  return std::nullopt;
}

std::optional<double> guiPointCl(const GuiPointEntry& point) {
  if (point.operatingPoint.has_value()) {
    const auto value =
        finiteOptional(point.operatingPoint->liftCoefficient);
    if (value.has_value()) {
      return value;
    }
  }
  return finiteOptional(point.polar.liftCoefficient);
}

std::optional<double> guiPointCm(const GuiPointEntry& point) {
  if (point.operatingPoint.has_value()) {
    const auto value =
        finiteOptional(point.operatingPoint->momentCoefficient);
    if (value.has_value()) {
      return value;
    }
  }
  return finiteOptional(point.polar.momentCoefficient);
}

std::optional<double> guiPointEfficiency(const GuiPointEntry& point) {
  const std::optional<double> cl = guiPointCl(point);
  const std::optional<double> cd = guiPointCd(point);
  if (!cl.has_value() || !cd.has_value() || !finite(*cl) ||
      !finite(*cd) || !(*cd > 0.0)) {
    return std::nullopt;
  }
  const double value = *cl / *cd;
  return finite(value) ? std::optional<double>{value} : std::nullopt;
}

GuiResultSummary computeGuiResultSummary(
    const std::vector<GuiPointEntry>& points,
    const std::optional<GuiPointEntry>& selected) {
  GuiResultSummary summary;
  if (selected.has_value()) {
    summary.cd = guiPointCd(*selected);
    summary.cdf = guiPointCdf(*selected);
    summary.cdp = guiPointCdp(*selected);
    summary.cl = guiPointCl(*selected);
    summary.cm = guiPointCm(*selected);
    summary.efficiency = guiPointEfficiency(*selected);
  }

  const std::vector<ScalarPoint> eligible = eligiblePoints(points);
  GuiResultProvenance cl0Provenance = GuiResultProvenance::unavailable;
  summary.cl0 = clAtAlphaZero(eligible, cl0Provenance);
  summary.cd0 = minimumDrag(eligible);
  const ZeroLiftResult zeroLift = alphaAtZeroLift(eligible);
  summary.alpha0 = zeroLift.alpha;
  summary.cm0 = zeroLift.cm;

  const MaximumResult clMaximum = maximum(eligible, false);
  summary.clMaximum = clMaximum.value;
  summary.clMaximumProvenance = clMaximum.provenance;
  summary.clMaximumProvisional =
      clMaximum.provenance == GuiResultProvenance::sampledProvisional;

  const MaximumResult efficiencyMaximum = maximum(eligible, true);
  summary.efficiencyMaximum = efficiencyMaximum.value;
  summary.efficiencyMaximumProvenance = efficiencyMaximum.provenance;
  summary.efficiencyMaximumProvisional =
      efficiencyMaximum.provenance ==
      GuiResultProvenance::sampledProvisional;
  return summary;
}

std::string formatResultValue(const std::optional<double>& value) {
  if (!value.has_value() || !finite(*value)) {
    return "n/a";
  }
  const double printable = *value == 0.0 ? 0.0 : *value;
  std::ostringstream stream;
  stream.imbue(std::locale::classic());
  stream << std::fixed << std::setprecision(6) << printable;
  return stream.str();
}

}  // namespace xfoil::gui

// End: src/gui/model/GuiResultSummary.cpp
