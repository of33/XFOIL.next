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

// file: include/xfoil/SessionTypes.h
// Neutral public session value-type header (header decoupling):
// GeometrySetError/GeometrySetResult are needed by
// both the session base layer (xfoil/session/SessionOps.h) and the
// public facade (xfoil/XFoilSession.h), so they live in this neutral
// header, so that the session base layer does not depend on its public
// facade. No dependency on XFoilSession.h, XFoil.h, SessionState.h or
// SessionOps.h.

#pragma once

#include <string>

namespace xfoil {

enum class GeometrySetError {
  none,
  emptyGeometry,
  tooManyPoints,
  panelInitializationFailed
};

struct GeometrySetResult {
  GeometrySetError error{GeometrySetError::none};
  std::string message;

  [[nodiscard]]
  bool success() const noexcept {
    return error == GeometrySetError::none;
  }
};

}  // namespace xfoil

// End: include/xfoil/SessionTypes.h
