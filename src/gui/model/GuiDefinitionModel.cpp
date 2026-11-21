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

// File: src/gui/model/GuiDefinitionModel.cpp
// Owning G2 state transitions and syntax validation. Backend/library
// validation remains in GuiSessionBackend and xfoil::Session.

#include "model/GuiDefinitionModel.h"

#include <cmath>
#include <limits>
#include <utility>

namespace xfoil::gui {
namespace {

constexpr std::size_t maximumSequencePoints = 100000;

Error makeError(ErrorCategory category, std::string code,
                std::string message, std::string context = {}) {
  return Error::make(category, std::move(code), std::move(message),
                     std::move(context));
}

Error sequenceRangeError() {
  return makeError(ErrorCategory::invalidArgument, "sequence.invalidRange",
                   "Sequence values must be finite", "target");
}

}  // namespace

bool GuiDefinitionModel::isReady() const noexcept {
  return !error_.has_value() && geometry_.has_value() &&
         panel_.has_value() && configurationValid_ &&
         !targetError().has_value();
}

std::optional<Error> GuiDefinitionModel::targetError() const {
  if (mode_ == AnalysisMode::alphaSequence) {
    const double start = sequence_.start.value;
    const double end = sequence_.endExclusive.value;
    const double step = sequence_.step.value;
    if (!std::isfinite(start) || !std::isfinite(end) ||
        !std::isfinite(step)) {
      return sequenceRangeError();
    }
    if (step == 0.0) {
      return makeError(ErrorCategory::invalidArgument, "sequence.zeroStep",
                       "Sequence step must be non-zero", "target");
    }
    if (!((step > 0.0 && start < end) || (step < 0.0 && start > end))) {
      return makeError(ErrorCategory::invalidArgument,
                       "sequence.direction",
                       "Sequence step direction does not match start and end",
                       "target");
    }
    const long double span =
        (static_cast<long double>(end) -
         static_cast<long double>(start)) /
        static_cast<long double>(step);
    if (span <= 0.0L) {
      return sequenceRangeError();
    }
    if (std::ceil(span) >
        static_cast<long double>(maximumSequencePoints)) {
      return makeError(ErrorCategory::capacityExceeded,
                       "sequence.pointLimit",
                       "Sequence exceeds the point limit", "target");
    }
    return std::nullopt;
  }

  if (std::holds_alternative<AlphaTarget>(target_)) {
    const double alpha = std::get<AlphaTarget>(target_).alpha.value;
    if (!std::isfinite(alpha)) {
      return makeError(ErrorCategory::invalidArgument,
                       "target.nonFiniteAlpha",
                       "Alpha target must be finite", "target");
    }
  } else {
    const double cl = std::get<LiftTarget>(target_).cl;
    if (!std::isfinite(cl)) {
      return makeError(ErrorCategory::invalidArgument,
                       "target.nonFiniteLift",
                       "Lift target must be finite", "target");
    }
  }

  const bool targetMatchesMode =
      (mode_ == AnalysisMode::alphaPoint &&
       std::holds_alternative<AlphaTarget>(target_)) ||
      (mode_ == AnalysisMode::clPoint &&
       std::holds_alternative<LiftTarget>(target_));
  if (!targetMatchesMode) {
    return makeError(ErrorCategory::invalidState,
                     "target.modeMismatch",
                     "Analysis target does not match the selected mode",
                     "target");
  }
  return std::nullopt;
}

void GuiDefinitionModel::refreshState() {
  if (!error_.has_value()) {
    const std::optional<Error> validation = targetError();
    if (validation.has_value()) {
      error_ = *validation;
    }
  }
  if (error_.has_value()) {
    state_ = XFoilGuiState::error;
  } else if (!geometry_.has_value()) {
    state_ = XFoilGuiState::empty;
  } else if (!panel_.has_value()) {
    state_ = XFoilGuiState::geometry;
  } else if (configurationValid_) {
    state_ = XFoilGuiState::configured;
  } else {
    state_ = XFoilGuiState::paneled;
  }
}

void GuiDefinitionModel::acceptGeometry(
    const geometry::AirfoilGeometry& geometry) {
  if (geometry.empty()) {
    fail(makeError(ErrorCategory::invalidArgument, "gui.geometryEmpty",
                   "Airfoil geometry is empty", "geometry"));
    return;
  }
  geometry_ = geometry;
  geometrySource_ = GeometrySource::memory;
  sharpTrailingEdge_ = false;
  designation_.clear();
  panel_.reset();
  error_.reset();
  refreshState();
}

void GuiDefinitionModel::acceptNaca(
    const geometry::AirfoilGeometry& geometry, std::string designation,
    bool sharpTrailingEdge) {
  if (geometry.empty()) {
    fail(makeError(ErrorCategory::invalidArgument, "gui.geometryEmpty",
                   "Airfoil geometry is empty", "geometry"));
    return;
  }
  geometry_ = geometry;
  geometrySource_ = sharpTrailingEdge ? GeometrySource::nacs
                                      : GeometrySource::naca;
  sharpTrailingEdge_ = sharpTrailingEdge;
  designation_ = std::move(designation);
  panel_.reset();
  error_.reset();
  refreshState();
}

void GuiDefinitionModel::setPanelingOptions(
    const PanelingOptions& options) {
  panelingOptions_ = options;
  panel_.reset();
  error_.reset();
  refreshState();
}

void GuiDefinitionModel::acceptPanel(const SurfaceGeometry& panel,
                                     const PanelingOptions& options) {
  if (panel.points.empty()) {
    fail(makeError(ErrorCategory::invalidArgument, "gui.panelEmpty",
                   "Panel contour is empty", "paneling"));
    return;
  }
  panel_ = panel;
  panelingOptions_ = options;
  error_.reset();
  refreshState();
}

void GuiDefinitionModel::acceptConfiguration(
    const AnalysisConfig& configuration) {
  configuration_ = configuration;
  configurationValid_ = true;
  error_.reset();
  refreshState();
}

void GuiDefinitionModel::invalidatePanel() {
  panel_.reset();
  error_.reset();
  refreshState();
}

void GuiDefinitionModel::fail(Error error) {
  error_ = std::move(error);
  state_ = XFoilGuiState::error;
}

void GuiDefinitionModel::clearError() {
  error_.reset();
  refreshState();
}

Result<void> GuiDefinitionModel::setMode(AnalysisMode mode) {
  mode_ = mode;
  const std::optional<Error> validation = targetError();
  if (validation.has_value()) {
    error_ = *validation;
    refreshState();
    return Result<void>::fail(*validation);
  }
  error_.reset();
  refreshState();
  return Result<void>::ok();
}

Result<void> GuiDefinitionModel::setTarget(const AnalysisTarget& target) {
  target_ = target;
  mode_ = std::holds_alternative<AlphaTarget>(target)
              ? AnalysisMode::alphaPoint
              : AnalysisMode::clPoint;
  const std::optional<Error> validation = targetError();
  if (validation.has_value()) {
    error_ = *validation;
    refreshState();
    return Result<void>::fail(*validation);
  }
  error_.reset();
  refreshState();
  return Result<void>::ok();
}

Result<void> GuiDefinitionModel::setSequence(
    const AlphaSequence& sequence) {
  sequence_ = sequence;
  mode_ = AnalysisMode::alphaSequence;
  const std::optional<Error> validation = targetError();
  if (validation.has_value()) {
    error_ = *validation;
    refreshState();
    return Result<void>::fail(*validation);
  }
  error_.reset();
  refreshState();
  return Result<void>::ok();
}

Result<GuiRunSpec> GuiDefinitionModel::makeRunSpec() const {
  if (!isReady()) {
    return Result<GuiRunSpec>::fail(
        makeError(ErrorCategory::invalidState, "gui.notReady",
                  "The GUI definition is not ready to run", "workflow"));
  }

  GuiGeometryRequest geometryRequest = [&]() -> GuiGeometryRequest {
    if (geometrySource_ == GeometrySource::naca ||
        geometrySource_ == GeometrySource::nacs) {
      return GuiNacaRequest{designation_, sharpTrailingEdge_};
    }
    return *geometry_;
  }();

  GuiAnalysisRequest analysisRequest = [&]() -> GuiAnalysisRequest {
    if (mode_ == AnalysisMode::alphaSequence) {
      return sequence_;
    }
    if (mode_ == AnalysisMode::clPoint) {
      return std::get<LiftTarget>(target_);
    }
    return std::get<AlphaTarget>(target_);
  }();

  return Result<GuiRunSpec>::ok(
      GuiRunSpec(std::move(geometryRequest), panelingOptions_,
                 configuration_, std::move(analysisRequest)));
}

const char* toString(XFoilGuiState state) noexcept {
  switch (state) {
    case XFoilGuiState::empty:
      return "Empty";
    case XFoilGuiState::geometry:
      return "Geometry";
    case XFoilGuiState::paneled:
      return "Paneling";
    case XFoilGuiState::configured:
      return "Configured";
    case XFoilGuiState::running:
      return "Running";
    case XFoilGuiState::results:
      return "Results";
    case XFoilGuiState::cancelled:
      return "Cancelled";
    case XFoilGuiState::error:
      return "Error";
  }
  return "Unknown";
}

const char* toString(AnalysisMode mode) noexcept {
  switch (mode) {
    case AnalysisMode::alphaPoint:
      return "Alpha";
    case AnalysisMode::clPoint:
      return "CL";
    case AnalysisMode::alphaSequence:
      return "Alpha sequence";
  }
  return "Unknown";
}

}  // namespace xfoil::gui

// End: src/gui/model/GuiDefinitionModel.cpp
