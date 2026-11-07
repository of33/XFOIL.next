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

// file: tests/unit/AirfoilWriterTests.cpp
// unit tests of the coordinate-file writing core. The
// expected byte sequences follow the gfortran-verified formats from
// SUBROUTINE SAVE in fortran-src/xfoil.f: FORMAT(1X,G15.7,G15.7),
// FORMAT(1X,F10.6,A,F10.6) for BSTRIP and FORMAT(1X,I12,I12).

#include "TestSupport.h"
#include "app/AirfoilWriter.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <limits>

namespace {

using xfoil::app::AirfoilOutputFormat;
using xfoil::app::AirfoilWriteOptions;
using xfoil::app::AirfoilWriteResult;
using xfoil::app::CoordinateDelimiter;

// Controlled creation of a temporary test file in its own
// unique test directory (central RAII helper). No
// fixed name under build/test-data, no manual cleanup step.
class TempWriteFile {
 public:
  explicit TempWriteFile(const std::string& fileName)
      : work_("writer-" + fileName), path_(work_.makePath(fileName)) {}

  TempWriteFile(const TempWriteFile&) = delete;
  TempWriteFile& operator=(const TempWriteFile&) = delete;

  [[nodiscard]] const std::filesystem::path& path() const noexcept {
    return path_;
  }

  [[nodiscard]] std::string contents() const {
    std::ifstream in(path_);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
  }

 private:
  testing::ScopedTempDirectory work_;
  std::filesystem::path path_;
};

std::vector<xfoil::Point2D> samplePoints() {
  return {
      xfoil::Point2D{1.0, 0.0033680},
      xfoil::Point2D{0.25, -0.5},
      xfoil::Point2D{0.0, 0.0},
      xfoil::Point2D{12345.6789, 1.23e-9},
  };
}

AirfoilWriteOptions baseOptions() {
  AirfoilWriteOptions options;
  options.name = "TEST FOIL";
  options.isesDomainParameters = " -2.0  3.0  -2.5  3.5";
  return options;
}

}  // namespace

TEST(unit, AirfoilWriterLabeledBlankDelimiterUsesG15AndF15) {
  TempWriteFile file("writer-labeled-blank.dat");
  auto options = baseOptions();
  options.format = AirfoilOutputFormat::labeled;
  options.delimiter = CoordinateDelimiter::blank;
  options.overwriteExisting = true;

  const auto result = xfoil::app::writeAirfoil(file.path(), samplePoints(),
                                               options);
  CHECK(result.success);

  const std::string text = file.contents();
  // first line: Name(1:NNAME) without a leading space.
  CHECK(text.rfind("TEST FOIL\n", 0) == 0);
  // G15.7 (verified with gfortran): 1.0 -> EE = 1, six
  // decimal places plus four reserved exponent digits;
  // 0.003368 -> EE = -2 -> decimal format.
  CHECK(text.find("   1.000000      0.3368000E-02\n") != std::string::npos);
  // 1.23e-9: EE = -9 -> decimal (E) format with a two-digit exponent.
  CHECK(text.find("  0.1230000E-08\n") != std::string::npos);
  // Exact coordinate line: 15 + 15 characters (without the leading 1X).
  const std::size_t lineStart = text.find("   1.000000 ");
  CHECK_EQ(text.substr(lineStart, 30),
           std::string("   1.000000      0.3368000E-02"));
}

TEST(unit, AirfoilWriterCommaDelimiterStripsInnerBlanks) {
  TempWriteFile file("writer-comma.dat");
  auto options = baseOptions();
  options.format = AirfoilOutputFormat::plain;
  options.delimiter = CoordinateDelimiter::comma;
  options.overwriteExisting = true;

  const auto result = xfoil::app::writeAirfoil(file.path(), samplePoints(),
                                               options);
  CHECK(result.success);

  const std::string text = file.contents();
  // F10.6, comma, F10.6 with BSTRIP (all spaces removed).
  CHECK(text.rfind("1.000000,0.003368\n", 0) == 0);
  CHECK(text.find("0.250000,-0.500000\n") != std::string::npos);
}

TEST(unit, AirfoilWriterTabDelimiterUsesTabCharacter) {
  TempWriteFile file("writer-tab.dat");
  auto options = baseOptions();
  options.format = AirfoilOutputFormat::plain;
  options.delimiter = CoordinateDelimiter::tab;
  options.overwriteExisting = true;

  const auto result = xfoil::app::writeAirfoil(file.path(), samplePoints(),
                                               options);
  CHECK(result.success);

  const std::string text = file.contents();
  CHECK(text.rfind("1.000000\t0.003368\n", 0) == 0);
}

TEST(unit, AirfoilWriterIntegerCoordinatesRoundLikeFortranInt) {
  TempWriteFile file("writer-integer.dat");
  auto options = baseOptions();
  options.format = AirfoilOutputFormat::integerCoordinates;
  options.overwriteExisting = true;

  const std::vector<xfoil::Point2D> points{
      xfoil::Point2D{0.5, -0.5},
      xfoil::Point2D{1.4, -1.4},
      xfoil::Point2D{0.0, 0.0},
  };
  const auto result =
      xfoil::app::writeAirfoil(file.path(), points, options);
  CHECK(result.success);

  const std::string text = file.contents();
  // INT(X+SIGN(0.5,X)) with format(1X,I12,I12): 0.5 -> 1, -0.5 -> -1,
  // 1.4 -> 1, -1.4 -> -1 (INT truncates toward zero); leading 1X plus
  // field width 12 right-aligned.
  CHECK_EQ(text.substr(0, 26), std::string("            1          -1\n"));
  CHECK(text.find("            1          -1\n            1          -1\n"
                  "            0           0\n") != std::string::npos);
}

TEST(unit, AirfoilWriterIsesWritesNameAndDomainLine) {
  TempWriteFile file("writer-ises.dat");
  auto options = baseOptions();
  options.format = AirfoilOutputFormat::ises;
  options.overwriteExisting = true;

  const std::vector<xfoil::Point2D> points{
      xfoil::Point2D{0.0, 0.0},
      xfoil::Point2D{1.0, 0.0},
  };
  const auto result =
      xfoil::app::writeAirfoil(file.path(), points, options);
  CHECK(result.success);

  const std::string text = file.contents();
  // Second line: ISPARS right-truncated (ISPARS(1:K)).
  CHECK(text.rfind("TEST FOIL\n -2.0  3.0  -2.5  3.5\n", 0) == 0);
}

TEST(unit, AirfoilWriterClockwiseReversesOrder) {
  TempWriteFile file("writer-clockwise.dat");
  auto options = baseOptions();
  options.format = AirfoilOutputFormat::plain;
  options.clockwise = true;
  options.overwriteExisting = true;

  const std::vector<xfoil::Point2D> points{
      xfoil::Point2D{0.0, 0.0},
      xfoil::Point2D{0.5, 0.1},
      xfoil::Point2D{1.0, 0.0},
  };
  const auto result =
      xfoil::app::writeAirfoil(file.path(), points, options);
  CHECK(result.success);

  const std::string text = file.contents();
  // G15.7: 1.0 -> "1.000000", 0.5/0.1 -> seven digits after the decimal;
  // the first pass begins with clockwise = true at the last point.
  const std::size_t first = text.find("1.000000");
  const std::size_t last = text.find("0.000000");
  CHECK(first != std::string::npos);
  CHECK(last != std::string::npos);
  CHECK(first < last);
}

TEST(unit, AirfoilWriterRefusesExistingFileWithoutConfirmation) {
  TempWriteFile file("writer-protected.dat");
  {
    std::ofstream seed(file.path());
    seed << "original content\n";
  }

  auto options = baseOptions();
  options.format = AirfoilOutputFormat::plain;
  options.overwriteExisting = false;

  const std::vector<xfoil::Point2D> points{xfoil::Point2D{0.0, 0.0}};
  const AirfoilWriteResult result =
      xfoil::app::writeAirfoil(file.path(), points, options);
  CHECK_FALSE(result.success);
  CHECK(result.existingFileNotOverwritten);
  CHECK_EQ(file.contents(), std::string("original content\n"));
}

TEST(unit, AirfoilWriterEmptyPointSetWritesHeaderOnly) {
  TempWriteFile file("writer-empty.dat");
  auto options = baseOptions();
  options.format = AirfoilOutputFormat::labeled;
  options.overwriteExisting = true;

  const std::vector<xfoil::Point2D> noPoints;
  const auto result =
      xfoil::app::writeAirfoil(file.path(), noPoints, options);
  CHECK(result.success);
  CHECK_EQ(file.contents(), std::string("TEST FOIL\n"));
}

TEST(unit, AirfoilWriterRejectsNonFiniteCoordinates) {
  TempWriteFile file("writer-non-finite.dat");

  auto options = baseOptions();
  options.format = AirfoilOutputFormat::plain;
  options.overwriteExisting = true;

  const std::vector<xfoil::Point2D> points{
    xfoil::Point2D{
      std::numeric_limits<double>::quiet_NaN(),
      0.0},
  };

  const auto result =
  xfoil::app::writeAirfoil(
    file.path(), points, options);

  CHECK_FALSE(result.success);
  CHECK_EQ(
    result.diagnostic,
    std::string{
      "Non-finite airfoil coordinate"});
  CHECK_FALSE(
    std::filesystem::exists(file.path()));
}

TEST(unit, AirfoilWriterDoesNotTruncateStaleTemporaryFile) {
  TempWriteFile file("writer-temp-collision.dat");

  const std::filesystem::path stale =
  file.path().string() + ".xfoil-tmp";

  {
    std::ofstream output(stale);
    output << "stale temporary content\n";
  }

  auto options = baseOptions();
  options.format = AirfoilOutputFormat::plain;
  options.overwriteExisting = true;

  const std::vector<xfoil::Point2D> points{
    xfoil::Point2D{0.0, 0.0},
  };

  const auto result =
  xfoil::app::writeAirfoil(
    file.path(), points, options);

  CHECK(result.success);

  std::ifstream input(stale);
  std::ostringstream contents;
  contents << input.rdbuf();

  CHECK_EQ(
    contents.str(),
           std::string{"stale temporary content\n"});

  std::error_code ignored;
  std::filesystem::remove(stale, ignored);
}

TEST(unit, AirfoilWriterReplacesExistingFileWhenConfirmed) {
  TempWriteFile file("writer-replace.dat");

  {
    std::ofstream output(file.path());
    output << "old contents\n";
  }

  auto options = baseOptions();
  options.format = AirfoilOutputFormat::plain;
  options.overwriteExisting = true;

  const std::vector<xfoil::Point2D> points{
    xfoil::Point2D{0.0, 0.0},
  };

  const auto result =
  xfoil::app::writeAirfoil(
    file.path(), points, options);

  CHECK(result.success);
  CHECK(
    file.contents().find("old contents") ==
    std::string::npos);
}

// End: tests/unit/AirfoilWriterTests.cpp
