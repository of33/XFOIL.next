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

// File: src/gui/plots/ComparisonStyle.h
// Shared comparison-polar colours and pen styles for plots and the manager.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <QColor>
#include <Qt>

namespace xfoil::gui {

inline constexpr std::size_t kMaxComparisonStyles = 8;
inline constexpr int kComparisonStyleIndexRole = Qt::UserRole + 1;

inline constexpr std::array<QColor, kMaxComparisonStyles>
    kComparisonColors = {
        QColor(213, 94, 0),     // vermillion, distinct from current blue
        QColor(0, 114, 178),    // blue
        QColor(0, 158, 115),    // bluish green
        QColor(204, 121, 167),  // reddish purple
        QColor(86, 180, 233),   // sky blue
        QColor(0, 0, 0),        // black
        QColor(230, 159, 0),    // orange-yellow
        QColor(140, 86, 75),    // brown
};

inline constexpr std::array<Qt::PenStyle, kMaxComparisonStyles>
    kComparisonStyles = {
        Qt::DashLine,      Qt::DotLine,    Qt::DashDotLine,
        Qt::DashDotDotLine, Qt::SolidLine, Qt::DashLine,
        Qt::DotLine,       Qt::DashDotLine,
};

[[nodiscard]]
inline std::size_t comparisonStyleIndexForId(std::uint64_t id) {
  return id == 0 ? 0 : static_cast<std::size_t>((id - 1) % kMaxComparisonStyles);
}

[[nodiscard]]
inline QColor comparisonColor(std::size_t index) {
  return kComparisonColors[index % kMaxComparisonStyles];
}

[[nodiscard]]
inline Qt::PenStyle comparisonPenStyle(std::size_t index) {
  return kComparisonStyles[index % kMaxComparisonStyles];
}

}  // namespace xfoil::gui

// End: src/gui/plots/ComparisonStyle.h
