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
// Empty G2a result-plot shell. It owns QCustomPlot presentation only and
// deliberately carries no result data or aerodynamic mapping.

#pragma once

#include <QString>
#include <QWidget>

class QCPGraph;
class QCustomPlot;

namespace xfoil::gui {

class ResultPlotView : public QWidget {
 public:
  explicit ResultPlotView(QWidget* parent = nullptr);
  ~ResultPlotView() override;

  ResultPlotView(const ResultPlotView&) = delete;
  ResultPlotView& operator=(const ResultPlotView&) = delete;

  void setAxisTitles(const QString& xTitle, const QString& yTitle);
  void setCpMode(bool upperLower);
  void clear();

 private:
  void applyPresentation();

  QCustomPlot* plot_{};
  QCPGraph* upperGraph_{};
  QCPGraph* lowerGraph_{};
  QString xTitle_;
  QString yTitle_;
  bool cpMode_{false};
};

}  // namespace xfoil::gui

// End: src/gui/plots/ResultPlotView.h
