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

// File: tests/unit/CambIoCodecTests.cpp
// Permanent checks of the pure IO codecs for CAMB
// RDAC/WRTT/WRTC against bytes verified with gfortran (Format 1X,3G15.7)
// and fortran-bin/xfoil. No session/CLI dependency.

#include "TestSupport.h"

#include "xfoil/io/AddedCamberCodec.h"
#include "xfoil/io/TextFormat.h"
#include "xfoil/io/ThicknessCamberCodec.h"

#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace xfoil;

std::string headerLine(std::string_view prefix, std::string_view name) {
  std::string field(name);
  field.resize(48, ' ');
  return std::string(prefix) + field + "\n";
}

}  // namespace

TEST(unit, AddedCamberCodecSkipsHeaderAndReadsPairs) {
  std::istringstream in("hdr line\n0.0 0.0\n0.5 0.05\n1.0 0.0\n");
  const auto data = io::decodeAddedCamberFile(in);
  CHECK(data.points.size() == 3);
  CHECK(data.points[1].x == 0.5 && data.points[1].y == 0.05);
  CHECK(!data.truncatedByParseError);
}

TEST(unit, AddedCamberCodecBlankLinesAndSplitPairs) {
  // list-directed: empty records are skipped, a pair may span
  // records, trailing tokens after a pair are ignored.
  std::istringstream in("hdr\n0.0\n\n0.0 9.9 9.9\n0.5, 0.05\n");
  const auto data = io::decodeAddedCamberFile(in);
  CHECK(data.points.size() == 2);
  CHECK(data.points[0].x == 0.0 && data.points[0].y == 0.0);
  CHECK(data.points[1].x == 0.5 && data.points[1].y == 0.05);
}

TEST(unit, AddedCamberCodecParseErrorKeepsPartialPoints) {
  std::istringstream in("hdr\n0.0 0.0\n0.5 0.05\n0.7 xx\n1.0 0.0\n");
  const auto data = io::decodeAddedCamberFile(in);
  CHECK(data.points.size() == 2);
  CHECK(data.truncatedByParseError);
}

TEST(unit, AddedCamberCodecHeaderOnlyIsEmpty) {
  std::istringstream in("header only\n");
  const auto data = io::decodeAddedCamberFile(in);
  CHECK(data.points.empty());
}

TEST(unit, AddedCamberCodecEmptyStreamIsEmpty) {
  std::istringstream in("");
  const auto data = io::decodeAddedCamberFile(in);
  CHECK(data.points.empty());
}

TEST(unit, AddedCamberCodecFortranDExponent) {
  std::istringstream in("hdr\n0.0D+00 0.0D+00\n0.5D+00 0.5D-01\n");
  const auto data = io::decodeAddedCamberFile(in);
  CHECK(data.points.size() == 2);
  CHECK(data.points[1].x == 0.5 && data.points[1].y == 0.05);
}

TEST(unit, ThicknessCodecHeaderPadsNameTo48) {
  const std::string text =
      io::thicknessDistributionText("NACA 0012", 1.0, std::vector<double>{0.0}, std::vector<double>{0.0});
  CHECK(text.substr(0, 59) ==
        headerLine("Thickness: ", "NACA 0012").substr(0, 59));
}

TEST(unit, ThicknessCodecBytesMatchGfortranFormat) {
 // gfortran Format(1X,3G15.7) reference bytes from the oracle.
  const std::string text = io::thicknessDistributionText(
      "T", 1.0, std::vector<double>{0.0, 0.5, 1.0},
      std::vector<double>{0.0, 0.1, 0.0});
  // Normalization + central difference + 1X,3G15.7 against the gfortran-
  // verified fortranG helper function.
  const auto g = [](double v) { return io::fortranG(15, 7, v); };
  const std::string expected =
      headerLine("Thickness: ", "T") +
      " " + g(0.0) + g(0.0) + g(0.2) + "\n" +
      " " + g(0.5) + g(0.1) + g(0.0) + "\n" +
      " " + g(1.0) + g(0.0) + g(-0.2) + "\n";
  CHECK(text == expected);
}

TEST(unit, CamberCodecHasNoThirdColumn) {
  const std::string text = io::camberDistributionText(
      "T", 1.0, std::vector<double>{0.0, 0.5},
      std::vector<double>{0.0, 0.1});
  const auto g = [](double v) { return io::fortranG(15, 7, v); };
  const std::string expected =
      headerLine("Camber: ", "T") +
      " " + g(0.0) + g(0.0) + "\n" +
      " " + g(0.5) + g(0.1) + "\n";
  CHECK(text == expected);
  // Lines end after the second G15.7 field (31 characters including 1X).
  const std::size_t firstData = text.find('\n', 0) + 1;
  const std::size_t secondNl = text.find('\n', firstData);
  CHECK(secondNl - firstData == 31);
}

TEST(unit, CodecNameFieldTruncatesAndPads) {
  CHECK(io::fortranNameField48("MyFoil") == "MyFoil" + std::string(42, ' '));
  CHECK(io::fortranNameField48(std::string(60, 'X')).size() == 48);
}

// end: tests/unit/CambIoCodecTests.cpp
