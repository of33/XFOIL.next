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

#pragma once

#include <filesystem>
#include <fstream>
#include <string>

#include "xfoil/io/DefaultsCodec.h"
#include "xfoil/session/SessionState.h"

namespace testing {

inline void writeXfoil699Defaults(const std::filesystem::path& path) {
  xfoil::io::DefaultsValues values = xfoil::session::initDefaultsValues();
  values.npan = 160;
  values.cterat = 0.15;
  std::ofstream out(path, std::ios::binary);
  out << xfoil::io::writeDefaults(values, 2);
}

inline void writeCommandsWithXfoil699Defaults(
    const std::filesystem::path& directory,
    const std::filesystem::path& original,
    const std::filesystem::path& derived) {
  writeXfoil699Defaults(directory / "xfoil-6.99.def");
  std::ifstream in(original, std::ios::binary);
  std::ofstream out(derived, std::ios::binary);
  out << "RDEF xfoil-6.99.def\n" << in.rdbuf();
}

inline std::string normalizeLegacyDefaultDisplay(std::string text) {
  const std::string from =
      "PANE    Set current-airfoil panel nodes ( 240 )";
  const std::string to =
      "PANE    Set current-airfoil panel nodes ( 160 )";
  std::string::size_type pos = 0;
  while ((pos = text.find(from, pos)) != std::string::npos) {
    text.replace(pos, from.size(), to);
    pos += to.size();
  }
  return text;
}

inline std::string stripInjectedDefaultsBlock(std::string text) {
  const std::string seed =
      " XFOIL   c>  \n Default parameters read in from file";
  const std::string::size_type pos = text.find(seed);
  if (pos == std::string::npos) {
    return text;
  }
  const std::string::size_type end = text.find("\n XFOIL   c>", pos);
  if (end == std::string::npos) {
    return text;
  }
  text.erase(pos, end + 1 - pos);
  return text;
}

}  // namespace testing
