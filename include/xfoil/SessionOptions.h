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

// File: include/xfoil/SessionOptions.h
// Public in-memory option values shared by the stateful session API and
// the one-shot request API.

#pragma once

#include <cstddef>

namespace xfoil {

// Paneling inputs. Defaults are the canonical INIT values. panelCount is
// the requested panel-node count and must be in [2, 595]; the remaining
// fields are passed unchanged to the existing PANGEN contract.
struct PanelingOptions {
  std::size_t panelCount{240};
  double bunchingParameter{1.0};
  double teToLeDensityRatio{0.3};
  double refinementDensityRatio{0.2};
  double upperRefinementStart{1.0};
  double upperRefinementEnd{1.0};
  double lowerRefinementStart{1.0};
  double lowerRefinementEnd{1.0};
};

}  // namespace xfoil

// End: include/xfoil/SessionOptions.h
