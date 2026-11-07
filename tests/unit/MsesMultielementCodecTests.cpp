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

// file: tests/unit/MsesMultielementCodecTests.cpp
// unit tests of the MSES multi-element text codec (MSAVE
// xfoil.f 1430-1544). Reference values come from the generated
// Fortran artifacts tests/fortran-reference/cases/top-file-export/.

#include "TestSupport.h"
#include "xfoil/io/MsesMultielementCodec.h"

#include <fstream>
#include <sstream>
#include <string>

namespace {

std::string readBytes(const std::string& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    return {};
  }
  std::ostringstream buffer;
  buffer << stream.rdbuf();
  return buffer.str();
}

}  // namespace

TEST(unit, MsesCodecDecodeTwoElements) {
  const std::string text =
      "NACA 2412\n"
      " -2.0  3.0  -2.5  3.5\n"
      "    1.000000      0.1260000E-02\n"
      "    0.000000       0.000000    \n"
      " 999.0  999.0\n"
      "    0.5000000      0.5000000E-01\n";
  const auto decoded = xfoil::io::decodeMsesMultielement(text);
  CHECK_EQ(static_cast<int>(decoded.status),
           static_cast<int>(xfoil::io::MsesDecodeStatus::ok));
  CHECK_EQ(decoded.file.name, std::string("NACA 2412"));
  CHECK_EQ(decoded.file.ispars, std::string(" -2.0  3.0  -2.5  3.5"));
  CHECK_EQ(static_cast<int>(decoded.file.elements.size()), 2);
  CHECK_EQ(static_cast<int>(decoded.file.elements[0].x.size()), 2);
  CHECK_EQ(static_cast<int>(decoded.file.elements[1].x.size()), 1);
  CHECK_EQ(decoded.residualElement, 2);
}

TEST(unit, MsesCodecResidualElementAfterFiveSentinels) {
  std::string text = "NAME\nISPARS\n";
  for (int element = 0; element < 5; ++element) {
    text += " 0.0 0.0\n 999.0 999.0\n";
  }
  const auto decoded = xfoil::io::decodeMsesMultielement(text);
  CHECK_EQ(static_cast<int>(decoded.status),
           static_cast<int>(xfoil::io::MsesDecodeStatus::ok));
  CHECK_EQ(static_cast<int>(decoded.file.elements.size()), 5);
  CHECK_EQ(decoded.residualElement, 6);
}

TEST(unit, MsesCodecSixthElementIsIgnoredLikeFortran) {
  // Fortran reads at most NEX=5 elements; a sixth element is
  // not read (no overflow).
  std::string text = "NAME\nISPARS\n";
  for (int element = 0; element < 6; ++element) {
    text += " 0.0 0.0\n 999.0 999.0\n";
  }
  const auto decoded = xfoil::io::decodeMsesMultielement(text);
  CHECK_EQ(static_cast<int>(decoded.status),
           static_cast<int>(xfoil::io::MsesDecodeStatus::ok));
  CHECK_EQ(static_cast<int>(decoded.file.elements.size()), 5);
  CHECK_EQ(decoded.residualElement, 6);
}

TEST(unit, MsesCodecOverflowWithinOneElement) {
  // An element without the 999.0 marker within the capacity
  // 2*IQX+1 = 741 pairs canonically triggers STOP 'LOAD: Array overflow'.
  std::string text = "NAME\nISPARS\n";
  for (int pair = 0; pair < 742; ++pair) {
    text += " 0.0 0.0\n";
  }
  const auto decoded = xfoil::io::decodeMsesMultielement(text);
  CHECK_EQ(static_cast<int>(decoded.status),
           static_cast<int>(xfoil::io::MsesDecodeStatus::overflow));
}

TEST(unit, MsesCodecReadErrorOnMissingHeader) {
  const auto decoded = xfoil::io::decodeMsesMultielement("NAME\n");
  CHECK_EQ(static_cast<int>(decoded.status),
           static_cast<int>(xfoil::io::MsesDecodeStatus::readError));
}

TEST(unit, MsesCodecEncodeMatchesFortranArtifact) {
  const std::string expected = readBytes(
      "tests/fortran-reference/cases/top-file-export/"
      "msav-multielement-replace/generated/multi.mses");
  if (expected.empty()) {
    testing::reportFailure(__FILE__, __LINE__,
                          "reference artifact not readable");
    return;
  }
  const auto decoded = xfoil::io::decodeMsesMultielement(expected);
  CHECK_EQ(static_cast<int>(decoded.status),
           static_cast<int>(xfoil::io::MsesDecodeStatus::ok));
  // Re-encoding the decoded Fortran file must reproduce the file up to
  // the canonically truncated file tail: the Fortran run shortened the
  // file after the replace, so the reference itself is already the
  // encoded state.
  CHECK_EQ(xfoil::io::msesMultielementText(decoded.file), expected);
}

// End: tests/unit/MsesMultielementCodecTests.cpp
