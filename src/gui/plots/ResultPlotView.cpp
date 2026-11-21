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

// File: src/gui/plots/ResultPlotView.cpp
// QCustomPlot shell with configurable axis titles and an empty Cp
// upper/lower legend. No data is inserted.

#include "plots/ResultPlotView.h"

#include <QVBoxLayout>

#include "qcustomplot.h"

namespace xfoil::gui {

ResultPlotView::ResultPlotView(QWidget* parent) : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  plot_ = new QCustomPlot(this);
  plot_->setObjectName(QStringLiteral("plotCanvas"));
  layout->addWidget(plot_);
  clear();
}

ResultPlotView::~ResultPlotView() = default;

void ResultPlotView::setAxisTitles(const QString& xTitle,
                                   const QString& yTitle) {
  xTitle_ = xTitle;
  yTitle_ = yTitle;
  applyPresentation();
}

void ResultPlotView::setCpMode(bool upperLower) {
  if (cpMode_ == upperLower) {
    return;
  }
  cpMode_ = upperLower;
  plot_->clearGraphs();
  upperGraph_ = nullptr;
  lowerGraph_ = nullptr;
  if (cpMode_) {
    upperGraph_ = plot_->addGraph();
    upperGraph_->setName(QStringLiteral("Upper"));
    lowerGraph_ = plot_->addGraph();
    lowerGraph_->setName(QStringLiteral("Lower"));
  }
  applyPresentation();
}

void ResultPlotView::clear() {
  if (upperGraph_ != nullptr) {
    upperGraph_->data()->clear();
  }
  if (lowerGraph_ != nullptr) {
    lowerGraph_->data()->clear();
  }
  plot_->replot();
}

void ResultPlotView::applyPresentation() {
  plot_->xAxis->setLabel(xTitle_);
  plot_->yAxis->setLabel(yTitle_);
  plot_->legend->setVisible(cpMode_);
  plot_->replot();
}

}  // namespace xfoil::gui

// End: src/gui/plots/ResultPlotView.cpp
