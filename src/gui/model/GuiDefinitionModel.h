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

// File: src/gui/model/GuiDefinitionModel.h
// Owning definition state for the G2 input workflow. The model has no
// QWidget, QObject, Session, file path or callback dependency.

#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "model/GuiRunSpec.h"
#include "xfoil/AnalysisConfig.h"
#include "xfoil/AnalysisData.h"
#include "xfoil/AnalysisRequest.h"
#include "xfoil/Result.h"
#include "xfoil/Session.h"
#include "xfoil/SessionOptions.h"
#include "xfoil/geometry/AirfoilGeometry.h"
#include "xfoil/gui/GuiState.h"

namespace xfoil::gui {

enum class GeometrySource {
  none,
  memory,
  naca,
  nacs
};

enum class AnalysisMode {
  alphaPoint,
  clPoint,
  alphaSequence,
  clSequence
};

class GuiDefinitionModel {
 public:
  GuiDefinitionModel() = default;

  [[nodiscard]]
  GeometrySource geometrySource() const noexcept {
    return geometrySource_;
  }

  [[nodiscard]]
  const std::string& designation() const noexcept {
    return designation_;
  }

  [[nodiscard]]
  bool sharpTrailingEdge() const noexcept {
    return sharpTrailingEdge_;
  }

  [[nodiscard]]
  const std::optional<geometry::AirfoilGeometry>& geometry() const noexcept {
    return geometry_;
  }

  [[nodiscard]]
  const std::optional<SurfaceGeometry>& panel() const noexcept {
    return panel_;
  }

  [[nodiscard]]
  const PanelingOptions& panelingOptions() const noexcept {
    return panelingOptions_;
  }

  [[nodiscard]]
  const AnalysisConfig& configuration() const noexcept {
    return configuration_;
  }

  [[nodiscard]]
  AnalysisMode mode() const noexcept {
    return mode_;
  }

  [[nodiscard]]
  const AnalysisTarget& target() const noexcept {
    return target_;
  }

  [[nodiscard]]
  const AlphaSequence& sequence() const noexcept {
    return sequence_;
  }

  [[nodiscard]]
  const LiftSequence& clSequence() const noexcept {
    return clSequence_;
  }

  [[nodiscard]]
  XFoilGuiState state() const noexcept {
    return state_;
  }

  [[nodiscard]]
  const std::optional<Error>& error() const noexcept {
    return error_;
  }

  [[nodiscard]]
  bool isReady() const noexcept;

  [[nodiscard]]
  Result<GuiRunSpec> makeRunSpec() const;

  void acceptGeometry(const geometry::AirfoilGeometry& geometry);
  void acceptNaca(const geometry::AirfoilGeometry& geometry,
                  std::string designation, bool sharpTrailingEdge);
  void renameGeometry(std::string name);
  void setPanelingOptions(const PanelingOptions& options);
  void acceptPanel(const SurfaceGeometry& panel,
                   const PanelingOptions& options);
  void acceptConfiguration(const AnalysisConfig& configuration);
  void invalidatePanel();
  void fail(Error error);
  void clearError();

  [[nodiscard]]
  Result<void> setMode(AnalysisMode mode);
  [[nodiscard]]
  Result<void> setTarget(const AnalysisTarget& target);
  [[nodiscard]]
  Result<void> setSequence(const AlphaSequence& sequence);
  [[nodiscard]]
  Result<void> setClSequence(const LiftSequence& sequence);

  void reset();

 private:
  [[nodiscard]]
  std::optional<Error> targetError() const;
  void refreshState();

  GeometrySource geometrySource_{GeometrySource::none};
  bool sharpTrailingEdge_{false};
  std::string designation_;
  std::optional<geometry::AirfoilGeometry> geometry_;
  std::optional<SurfaceGeometry> panel_;
  PanelingOptions panelingOptions_{};
  AnalysisConfig configuration_{};
  bool configurationValid_{false};
  AnalysisMode mode_{AnalysisMode::alphaPoint};
  AnalysisTarget target_{AlphaTarget{Degrees{0.0}}};
  AlphaSequence sequence_{Degrees{-5.0}, Degrees{20.0}, Degrees{1.0}};
  LiftSequence clSequence_{-0.5, 1.5, 0.1};
  std::optional<Error> error_;
  XFoilGuiState state_{XFoilGuiState::empty};
};

[[nodiscard]]
const char* toString(XFoilGuiState state) noexcept;

[[nodiscard]]
const char* toString(AnalysisMode mode) noexcept;

}  // namespace xfoil::gui

// End: src/gui/model/GuiDefinitionModel.h
