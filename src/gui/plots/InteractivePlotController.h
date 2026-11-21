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

// File: src/gui/plots/InteractivePlotController.h
// Reusable QCustomPlot interaction: left-drag pan, wheel zoom, right-click
// fit, Ctrl+right-click sample-marker toggle and click/drag distinction.
// It contains no aerodynamic or solver behavior.

#pragma once

#include <functional>

#include <QObject>
#include <QPoint>

class QCustomPlot;

namespace xfoil::gui {

class InteractivePlotController : public QObject {
 public:
  using ClickCallback = std::function<void(const QPoint&)>;
  using ActionCallback = std::function<void()>;

  InteractivePlotController(QCustomPlot* plot, bool equalAspect,
                            bool markerToggle,
                            QObject* parent = nullptr);
  ~InteractivePlotController() override;

  InteractivePlotController(const InteractivePlotController&) = delete;
  InteractivePlotController& operator=(const InteractivePlotController&) =
      delete;

  void setClickCallback(ClickCallback callback);
  void setFitCallback(ActionCallback callback);
  void setMarkerToggleCallback(ActionCallback callback);

  void setPointSelectionEnabled(bool enabled);
  [[nodiscard]]
  bool pointSelectionEnabled() const noexcept {
    return pointSelectionEnabled_;
  }

  void setSampleMarkersEnabled(bool enabled);
  [[nodiscard]]
  bool sampleMarkersEnabled() const noexcept {
    return sampleMarkersEnabled_;
  }

  [[nodiscard]]
  static int selectionRadiusPixels() noexcept;

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  void panBy(const QPoint& from, const QPoint& to);
  void zoomAt(const QPoint& position, double factor);
  void handleRightClick(const QPoint& position, bool withControl);

  QCustomPlot* plot_{};
  bool equalAspect_{false};
  bool markerToggle_{false};
  bool pointSelectionEnabled_{false};
  bool sampleMarkersEnabled_{false};
  bool leftPressed_{false};
  bool dragging_{false};
  QPoint pressPosition_;
  QPoint lastPosition_;
  ClickCallback clickCallback_;
  ActionCallback fitCallback_;
  ActionCallback markerToggleCallback_;
};

}  // namespace xfoil::gui

// End: src/gui/plots/InteractivePlotController.h
