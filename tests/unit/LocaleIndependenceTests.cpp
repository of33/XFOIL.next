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

// File: tests/unit/LocaleIndependenceTests.cpp
// G6c locale contract: PolarText, DAT/profile parsing and numeric formatting
// must not depend on the process LC_NUMERIC value and must not mutate it.

#include <clocale>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

#include "TestSupport.h"
#include "app/DatFileReader.h"
#include "io/NumericText.h"
#include "xfoil/PolarText.h"

namespace {

using xfoil::app::CoordinateFileFormat;
using xfoil::app::DatFileReader;
using xfoil::app::DatReadError;

std::string currentNumericLocale() {
  const char* current = std::setlocale(LC_NUMERIC, nullptr);
  return current != nullptr ? current : "C";
}

class NumericLocaleScope {
 public:
  explicit NumericLocaleScope(std::string_view locale)
      : saved_(currentNumericLocale()) {
    active_ = std::setlocale(LC_NUMERIC, std::string(locale).c_str()) != nullptr;
  }

  ~NumericLocaleScope() {
    std::setlocale(LC_NUMERIC, saved_.c_str());
  }

  NumericLocaleScope(const NumericLocaleScope&) = delete;
  NumericLocaleScope& operator=(const NumericLocaleScope&) = delete;

  [[nodiscard]] bool active() const noexcept { return active_; }

 private:
  std::string saved_;
  bool active_{false};
};

std::string availableGermanLocale() {
  for (const char* candidate :
       {"de_DE.UTF-8", "de_DE.utf8", "German_Germany.1252", "de-DE"}) {
    if (std::setlocale(LC_NUMERIC, candidate) != nullptr) {
      return candidate;
    }
  }
  return {};
}

std::string readFixture(const char* name) {
  std::ifstream stream(std::string("tests/data/xfoil-6.996/s11/") + name,
                       std::ios::binary);
  CHECK(stream.good());
  // No istreambuf_iterator: GCC -O3 (-Wnull-dereference) cannot prove
  // the buffer pointer of a failed stream to be non-zero.
  std::ostringstream buffer;
  buffer << stream.rdbuf();
  return buffer.str();
}

struct PolarSnapshot {
  bool ok{false};
  std::string bytes;
  std::size_t points{};
  std::size_t sideRows{};
  std::size_t elementRows{};
  std::string errorCode;
  std::string errorMessage;
};

PolarSnapshot polarSnapshot(const std::string& text) {
  PolarSnapshot snapshot;
  const auto decoded = xfoil::decodePolarText(text);
  snapshot.ok = decoded.isOk();
  if (decoded.isErr()) {
    snapshot.errorCode = decoded.error().code;
    snapshot.errorMessage = decoded.error().message;
    return snapshot;
  }
  snapshot.points = decoded.value().points.size();
  snapshot.sideRows = decoded.value().sideData.size();
  snapshot.elementRows = decoded.value().elementData.size();
  const auto encoded = xfoil::encodePolarText(decoded.value());
  if (encoded.isErr()) {
    snapshot.ok = false;
    snapshot.errorCode = encoded.error().code;
    snapshot.errorMessage = encoded.error().message;
    return snapshot;
  }
  snapshot.bytes = encoded.value();
  return snapshot;
}

void writeText(const std::filesystem::path& path, const std::string& text) {
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  stream << text;
  CHECK(stream.good());
}

void checkDatContract(const std::filesystem::path& directory) {
  const auto pointFile = directory / "point.dat";
  writeText(pointFile,
            "locale\n"
            "0.000 0.000\n"
            "1.250 0.250\n"
            "0.500 -0.125\n");
  const auto point = DatFileReader::read(pointFile);
  CHECK(point.success());
  CHECK_EQ(point.airfoil.coordinates.size(), std::size_t{3});
  CHECK_EQ(point.airfoil.coordinates[1].x, 1.25);
  CHECK_EQ(point.airfoil.coordinates[1].y, 0.25);

  const auto generic = directory / "generic.dat";
  writeText(generic,
            "0.000 0.000\n"
            "1.250 0.250\n"
            "0.500 -0.125\n");
  const auto genericResult = DatFileReader::readCoordinateFile(generic, 0);
  CHECK(genericResult.success());
  CHECK(genericResult.format == CoordinateFileFormat::generic);
  CHECK_EQ(genericResult.coordinates.size(), std::size_t{3});
  CHECK_EQ(genericResult.coordinates[1].x, 1.25);

  const auto labeled = directory / "labeled.dat";
  writeText(labeled,
            "labeled\n"
            "0.000 0.000\n"
            "1.250 0.250\n"
            "0.500 -0.125\n");
  const auto labeledResult = DatFileReader::readCoordinateFile(labeled, 0);
  CHECK(labeledResult.success());
  CHECK(labeledResult.format == CoordinateFileFormat::labeled);
  CHECK_EQ(labeledResult.name, std::string{"labeled"});
  CHECK_EQ(labeledResult.coordinates.size(), std::size_t{3});

  const auto mses = directory / "mses.dat";
  writeText(mses,
            "MSES\n"
            "1.0 2.0 3.0 4.0\n"
            "0.000 0.000\n"
            "1.250 0.250\n"
            "0.500 -0.125\n"
            "999.0 999.0\n");
  const auto msesResult = DatFileReader::readCoordinateFile(mses, 1);
  CHECK(msesResult.success());
  CHECK(msesResult.format == CoordinateFileFormat::msesMulti);
  CHECK_EQ(msesResult.coordinates.size(), std::size_t{3});
  CHECK_EQ(msesResult.coordinates[1].x, 1.25);

  const auto crlf = directory / "crlf.dat";
  writeText(crlf,
            "crlf\r\n"
            "0.000 0.000\r\n"
            "1.250 0.250\r\n"
            "0.500 -0.125\r\n");
  const auto crlfResult = DatFileReader::readCoordinateFile(crlf, 0);
  CHECK(crlfResult.success());
  CHECK_EQ(crlfResult.coordinates.size(), std::size_t{3});
  CHECK_EQ(crlfResult.coordinates[1].y, 0.25);

  const auto invalid = directory / "invalid.dat";
  writeText(invalid,
            "invalid\n"
            "0.000 0.000\n"
            "1.250 x\n"
            "0.500 -0.125\n");
  const auto invalidResult = DatFileReader::readCoordinateFile(invalid, 0);
  CHECK(invalidResult.error == DatReadError::invalidCoordinate);
}

}  // namespace

TEST(unit, G6cPolarTextIsLocaleIndependentAndDoesNotMutateLocale) {
  const std::string original = currentNumericLocale();
  const std::string baseText = readFixture("canonical-base.pol");
  const std::string elementText = readFixture("canonical-elem4.pol");
  const std::string invalidText = readFixture("truncated-row.pol");

  PolarSnapshot baseC;
  PolarSnapshot elementC;
  PolarSnapshot invalidC;
  {
    NumericLocaleScope scope("C");
    CHECK(scope.active());
    const std::string before = currentNumericLocale();
    baseC = polarSnapshot(baseText);
    elementC = polarSnapshot(elementText);
    invalidC = polarSnapshot(invalidText);
    CHECK_EQ(currentNumericLocale(), before);
    CHECK(baseC.ok);
    CHECK(baseC.points > 0);
    CHECK(baseC.sideRows == baseC.points);
    CHECK(elementC.elementRows == elementC.points);
    CHECK_FALSE(invalidC.ok);
  }

  const std::string german = availableGermanLocale();
  if (german.empty()) {
    std::fprintf(stderr,
                 "G6c locale test: no German LC_NUMERIC locale available; "
                 "non-C check skipped\n");
    std::setlocale(LC_NUMERIC, original.c_str());
    return;
  }
  {
    NumericLocaleScope scope(german);
    CHECK(scope.active());
    const std::string before = currentNumericLocale();
    const PolarSnapshot baseDe = polarSnapshot(baseText);
    const PolarSnapshot elementDe = polarSnapshot(elementText);
    const PolarSnapshot invalidDe = polarSnapshot(invalidText);
    CHECK_EQ(currentNumericLocale(), before);
    CHECK(baseDe.ok);
    CHECK_EQ(baseDe.bytes, baseC.bytes);
    CHECK_EQ(baseDe.points, baseC.points);
    CHECK_EQ(baseDe.sideRows, baseC.sideRows);
    CHECK_EQ(elementDe.bytes, elementC.bytes);
    CHECK_EQ(elementDe.elementRows, elementC.elementRows);
    CHECK_EQ(invalidDe.errorCode, invalidC.errorCode);
    CHECK_EQ(invalidDe.errorMessage, invalidC.errorMessage);
  }
  std::setlocale(LC_NUMERIC, original.c_str());
}

TEST(unit, G6cNumericFormattingAndDatParsingIgnoreLocale) {
  const std::string original = currentNumericLocale();
  {
    NumericLocaleScope scope("C");
    CHECK_EQ(xfoil::io::formatFixed(1.25, 2), std::string{"1.25"});
    CHECK_EQ(xfoil::io::formatScientific(125.0, 2),
             std::string{"1.25e+02"});
    double parsed = 0.0;
    CHECK(xfoil::io::parseReal("1.25D0", parsed));
    CHECK_EQ(parsed, 1.25);
  }
  const std::string german = availableGermanLocale();
  if (german.empty()) {
    std::fprintf(stderr,
                 "G6c locale test: no German LC_NUMERIC locale available; "
                 "non-C check skipped\n");
    std::setlocale(LC_NUMERIC, original.c_str());
    return;
  }
  {
    NumericLocaleScope scope(german);
    const std::string before = currentNumericLocale();
    CHECK_EQ(xfoil::io::formatFixed(1.25, 2), std::string{"1.25"});
    CHECK_EQ(xfoil::io::formatScientific(125.0, 2),
             std::string{"1.25e+02"});
    double parsed = 0.0;
    CHECK(xfoil::io::parseReal("1.25D0", parsed));
    CHECK_EQ(parsed, 1.25);

    testing::ScopedTempDirectory directory("locale-dat");
    checkDatContract(directory.path());
    CHECK_EQ(currentNumericLocale(), before);
  }
  std::setlocale(LC_NUMERIC, original.c_str());
}

TEST(unit, G6cConcurrentTextParsingDoesNotChangeLocale) {
  const std::string original = currentNumericLocale();
  const std::string german = availableGermanLocale();
  if (german.empty()) {
    std::fprintf(stderr,
                 "G6c locale test: no German LC_NUMERIC locale available; "
                 "parallel check skipped\n");
    std::setlocale(LC_NUMERIC, original.c_str());
    return;
  }
  {
    NumericLocaleScope scope(german);
    const std::string before = currentNumericLocale();
    const std::string baseText = readFixture("canonical-base.pol");
    testing::ScopedTempDirectory directory("locale-parallel");
    const auto pointFile = directory.path() / "point.dat";
    writeText(pointFile,
              "parallel\n"
              "0.000 0.000\n"
              "1.250 0.250\n"
              "0.500 -0.125\n");

    int polarFailures = 0;
    int datFailures = 0;
    std::thread polarThread([&polarFailures, &baseText]() {
      for (int index = 0; index < 200; ++index) {
        const auto decoded = xfoil::decodePolarText(baseText);
        if (decoded.isErr()) {
          ++polarFailures;
          continue;
        }
        const auto encoded = xfoil::encodePolarText(decoded.value());
        if (encoded.isErr() ||
            encoded.value().find("1,000") != std::string::npos) {
          ++polarFailures;
        }
      }
    });
    std::thread datThread([&datFailures, &pointFile]() {
      for (int index = 0; index < 200; ++index) {
        const auto result = DatFileReader::read(pointFile);
        if (!result.success() || result.airfoil.coordinates.size() != 3 ||
            result.airfoil.coordinates[1].x != 1.25) {
          ++datFailures;
        }
      }
    });
    polarThread.join();
    datThread.join();
    CHECK_EQ(polarFailures, 0);
    CHECK_EQ(datFailures, 0);
    CHECK_EQ(currentNumericLocale(), before);
  }
  std::setlocale(LC_NUMERIC, original.c_str());
}

// End: tests/unit/LocaleIndependenceTests.cpp
