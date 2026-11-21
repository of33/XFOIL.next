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

// File: src/gui/plots/ResultPlotView.h
// Selected-point Cp/Cf distribution view. It accepts owning public
// distributions only and performs no solve, interpolation or reconstruction.

#pragma once

#include <cstddef>

#include <QString>
#include <QWidget>

#include "xfoil/AnalysisData.h"
#include "xfoil/SkinFriction.h"

class QCPGraph;
class QCPItemStraightLine;
class QCPItemText;
class QCustomPlot;

namespace xfoil::gui {

class InteractivePlotController;

class ResultPlotView : public QWidget {
 public:
  explicit ResultPlotView(QWidget* parent = nullptr);
  ~ResultPlotView() override;

  ResultPlotView(const ResultPlotView&) = delete;
  ResultPlotView& operator=(const ResultPlotView&) = delete;

  void setAxisTitles(const QString& xTitle, const QString& yTitle);
  void setCpDistribution(const xfoil::SurfaceDistribution& distribution);
  void setCfDistribution(
      const xfoil::SkinFrictionDistribution& distribution);
  void clear();
  void setMessage(const QString& message);
  void fitVisibleData();

  [[nodiscard]]
  QString messageText() const;
  [[nodiscard]]
  bool sampleMarkersEnabled() const noexcept {
    return sampleMarkersEnabled_;
  }
  void setSampleMarkersEnabled(bool enabled);
  [[nodiscard]]
  bool zeroLineVisible() const noexcept {
    return zeroLineVisible_;
  }
  [[nodiscard]]
  std::size_t upperPointCount() const noexcept;
  [[nodiscard]]
  std::size_t lowerPointCount() const noexcept;

 private:
  void applyPresentation();
  void applySampleMarkers();
  void clearData();
  void setDistributionData(const std::vector<double>& upperX,
                           const std::vector<double>& upperY,
                           const std::vector<double>& lowerX,
                           const std::vector<double>& lowerY);
  void updateZeroLine(double lower, double upper, bool visible);

  QCustomPlot* plot_{};
  QCPGraph* upperGraph_{};
  QCPGraph* lowerGraph_{};
  QCPItemStraightLine* zeroLine_{};
  QCPItemText* messageItem_{};
  InteractivePlotController* interaction_{};
  QString xTitle_;
  QString yTitle_;
  QString message_;
  bool sampleMarkersEnabled_{false};
  bool zeroLineVisible_{false};
};

}  // namespace xfoil::gui

// End: src/gui/plots/ResultPlotView.h
