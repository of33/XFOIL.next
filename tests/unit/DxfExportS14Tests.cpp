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

// File: tests/unit/DxfExportS14Tests.cpp
// Pure DXF R12 encoder contract of S14. No command, prompt, path policy or
// file wiring is exercised here.

#include "TestSupport.h"

#include "xfoil/io/DxfExport.h"

#include <clocale>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string readBytes(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    return {};
  }
  std::ostringstream buffer;
  buffer << stream.rdbuf();
  return buffer.str();
}

std::size_t countSubstring(const std::string& text,
                           const std::string& needle) {
  std::size_t count = 0;
  std::size_t position = 0;
  while ((position = text.find(needle, position)) != std::string::npos) {
    ++count;
    position += needle.size();
  }
  return count;
}

const std::vector<double> kTriangleX{0.0, 1.0, 0.0};
const std::vector<double> kTriangleY{0.0, 0.0, 1.0};

const std::string kTriangleExpected =
    "999\n"
    "\"DXF R12 Output\" (www.mydxf.blogspot.com)\n"
    " 0 \n"
    "SECTION\n"
    " 2 \n"
    "HEADER\n"
    " 9 \n"
    "$ACADVER\n"
    " 1 \n"
    "AC1009\n"
    " 9 \n"
    "$EXTMIN\n"
    " 10 \n"
    "    -0.1000\n"
    " 20 \n"
    "    -0.1000\n"
    " 9 \n"
    "$EXTMAX\n"
    " 10 \n"
    "     1.1000\n"
    " 20 \n"
    "     1.1000\n"
    " 0 \n"
    "ENDSEC\n"
    " 0 \n"
    "SECTION\n"
    " 2 \n"
    "ENTITIES\n"
    "0\n"
    "LINE\n"
    "8\n"
    "Layer 1\n"
    "10\n"
    "0.000000\n"
    "20\n"
    "0.000000\n"
    "11\n"
    "1.000000\n"
    "21\n"
    "0.000000\n"
    "0\n"
    "LINE\n"
    "8\n"
    "Layer 1\n"
    "10\n"
    "1.000000\n"
    "20\n"
    "0.000000\n"
    "11\n"
    "0.000000\n"
    "21\n"
    "1.000000\n"
    " 0 \n"
    "ENDSEC\n"
    " 0 \n"
    "EOF";

}  // namespace

TEST(unit, DxfExportS14TriangleMatchesCanonicalBytes) {
  const auto encoded =
      xfoil::io::dxfR12TextChecked(kTriangleX, kTriangleY);
  CHECK(encoded.isOk());
  CHECK_EQ(encoded.value(), kTriangleExpected);
  CHECK_EQ(countSubstring(encoded.value(), "\nLINE\n"), std::size_t{2});
  CHECK(encoded.value().find('\r') == std::string::npos);
  CHECK_EQ(encoded.value().back(), 'F');
  CHECK(encoded.value().rfind("EOF", 0) == std::string::npos);
  CHECK_EQ(encoded.value().rfind("EOF"), encoded.value().size() - 3);
  CHECK(encoded.value().find("\n11\n0.000000\n21\n1.000000\n 0 \n") !=
        std::string::npos);
}

TEST(unit, DxfExportS14EmptyContourMatchesOracleSkeleton) {
  const std::vector<double> empty;
  const auto encoded = xfoil::io::dxfR12TextChecked(empty, empty);
  CHECK(encoded.isOk());
  CHECK_EQ(encoded.value().size(), std::size_t{239});
  CHECK_EQ(countSubstring(encoded.value(), "\nLINE\n"), std::size_t{0});
  CHECK(encoded.value().find("$EXTMIN\n 10 \n     0.0000\n") !=
        std::string::npos);
  CHECK(encoded.value().find("$EXTMAX\n 10 \n     0.0000\n") !=
        std::string::npos);

  const std::string expected = readBytes(
      std::filesystem::path("tests/data/xfoil-6.996/s14/dxf-empty.dxf"));
  CHECK(!expected.empty());
  CHECK_EQ(encoded.value(), expected);
}

TEST(unit, DxfExportS14PreservesInputAndRepeats) {
  std::vector<double> x = kTriangleX;
  std::vector<double> y = kTriangleY;
  const auto first = xfoil::io::dxfR12TextChecked(x, y);
  const auto second = xfoil::io::dxfR12TextChecked(x, y);
  CHECK(first.isOk());
  CHECK(second.isOk());
  CHECK_EQ(first.value(), second.value());
  CHECK_EQ(x.size(), kTriangleX.size());
  CHECK_EQ(y.size(), kTriangleY.size());
  for (std::size_t i = 0; i < x.size(); ++i) {
    CHECK_EQ(x[i], kTriangleX[i]);
    CHECK_EQ(y[i], kTriangleY[i]);
  }
}

TEST(unit, DxfExportS14RejectsInvalidContours) {
  std::vector<double> x{0.0, 1.0};
  std::vector<double> shortY{0.0};
  const auto mismatch = xfoil::io::dxfR12TextChecked(x, shortY);
  CHECK(mismatch.isErr());
  CHECK(mismatch.error().code == xfoil::core::ErrorCode::sizeMismatch);
  CHECK_EQ(xfoil::io::dxfR12Text(x, shortY), std::string{});

  std::vector<double> nanY{0.0, std::numeric_limits<double>::quiet_NaN()};
  const auto nan = xfoil::io::dxfR12TextChecked(x, nanY);
  CHECK(nan.isErr());
  CHECK(nan.error().code == xfoil::core::ErrorCode::invalidArgument);

  std::vector<double> infY{0.0, std::numeric_limits<double>::infinity()};
  const auto inf = xfoil::io::dxfR12TextChecked(x, infY);
  CHECK(inf.isErr());
  CHECK(inf.error().code == xfoil::core::ErrorCode::invalidArgument);
}

TEST(unit, DxfExportS14UsesClassicLocale) {
  const char* const saved = std::setlocale(LC_NUMERIC, nullptr);
  const std::string savedLocale = saved != nullptr ? saved : "";
  const char* const german = std::setlocale(LC_NUMERIC, "de_DE.utf8");
  if (german != nullptr) {
    const auto encoded =
        xfoil::io::dxfR12TextChecked(kTriangleX, kTriangleY);
    CHECK(encoded.isOk());
    CHECK_EQ(encoded.value(), kTriangleExpected);
    CHECK(encoded.value().find("0,000000") == std::string::npos);
  }
  std::setlocale(LC_NUMERIC, savedLocale.c_str());
}

// End: tests/unit/DxfExportS14Tests.cpp
