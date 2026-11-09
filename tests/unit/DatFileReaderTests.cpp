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

// file: tests/unit/DatFileReaderTests.cpp
// unit tests for the DAT file reader extracted from the application layer.
// the AREAD-compatible path (readCoordinateFile) limits
// the point count to the AREAD capacity (maxInputCoordinates=2404);
// the maximum successful real-point count is 1479. The domain
// boundary is additionally checked in the session layer.

#include "TestSupport.h"
#include "app/DatFileReader.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

using xfoil::app::DatFileReader;
using xfoil::app::DatReadError;
using xfoil::app::DatReadResult;

// Controlled creation of a temporary test file in its own
// unique test directory (central RAII helper). No
// fixed name under build/test-data; the file and its directory
// are reliably removed when the validity scope is left.
class TempDatFile {
 public:
  explicit TempDatFile(const std::string& fileName,
                       const std::string& contents)
      : work_("coord-" + fileName), path_(work_.makePath(fileName)) {
    std::ofstream out(path_);
    out << contents;
    created_ = static_cast<bool>(out);
  }

  TempDatFile(const TempDatFile&) = delete;
  TempDatFile& operator=(const TempDatFile&) = delete;

  [[nodiscard]] bool created() const noexcept { return created_; }
  [[nodiscard]] const std::filesystem::path& path() const noexcept {
    return path_;
  }

 private:
  testing::ScopedTempDirectory work_;
  std::filesystem::path path_;
  bool created_{false};
};

}  // namespace

TEST(unit, DatReaderReadsClarkY) {
  const DatReadResult result = DatFileReader::read("sample/CLARK_Y.dat");

  CHECK(result.success());
  CHECK(result.error == DatReadError::none);
  CHECK_EQ(result.airfoil.name, std::string("CLARK Y"));
  CHECK_EQ(result.airfoil.coordinates.size(), static_cast<std::size_t>(151));

  if (result.airfoil.coordinates.size() == 151) {
    const auto& first = result.airfoil.coordinates.front();
    const auto& last = result.airfoil.coordinates.back();
    CHECK_NEAR(first.x, 1.00000, 1e-9, 1e-9);
    CHECK_NEAR(first.y, 0.00060, 1e-9, 1e-9);
    CHECK_NEAR(last.x, 1.00000, 1e-9, 1e-9);
    CHECK_NEAR(last.y, -0.00060, 1e-9, 1e-9);
  }
}

TEST(unit, DatReaderRejectsMissingFile) {
  const DatReadResult result =
      DatFileReader::read("tests/data/does-not-exist.dat");

  CHECK_FALSE(result.success());
  CHECK(result.error == DatReadError::cannotOpen);
  CHECK_EQ(result.errorMessage, std::string("Failed to open dat file"));
}

TEST(unit, DatReaderRejectsEmptyFile) {
  const DatReadResult result = DatFileReader::read("tests/data/empty.dat");

  CHECK_FALSE(result.success());
  CHECK(result.error == DatReadError::missingName);
}

TEST(unit, DatReaderRejectsNameOnlyFile) {
  const DatReadResult result =
      DatFileReader::read("tests/data/name-only.dat");

  CHECK_FALSE(result.success());
  CHECK(result.error == DatReadError::tooFewCoordinates);
  CHECK_EQ(result.errorMessage,
           std::string("Invalid dat file: fewer than 3 coordinate points"));
}

TEST(unit, DatReaderRejectsInvalidNumber) {
  const DatReadResult result =
      DatFileReader::read("tests/data/invalid-number.dat");

  CHECK_FALSE(result.success());
  CHECK(result.error == DatReadError::invalidCoordinate);
  // name in line 1, "abc def" in line 2.
  CHECK_EQ(result.lineNumber, static_cast<std::size_t>(2));
  CHECK_EQ(result.errorMessage,
           std::string("Invalid coordinate line in dat file: abc def"));
}

TEST(unit, DatReaderReadsThreePoints) {
  const DatReadResult result =
      DatFileReader::read("tests/data/three-points.dat");

  CHECK(result.success());
  CHECK_EQ(result.airfoil.coordinates.size(), static_cast<std::size_t>(3));

  if (result.airfoil.coordinates.size() == 3) {
    CHECK_NEAR(result.airfoil.coordinates[0].x, 1.0, 1e-12, 0.0);
    CHECK_NEAR(result.airfoil.coordinates[1].x, 0.0, 1e-12, 0.0);
    CHECK_NEAR(result.airfoil.coordinates[2].x, 1.0, 1e-12, 0.0);
    CHECK_NEAR(result.airfoil.coordinates[2].y, -0.00126, 1e-12, 0.0);
  }
}

TEST(unit, DatReaderAcceptsBeyondOldCapacity) {
  // the reader itself does not know the capacity boundary: all 700 prepared
  // points must be read successfully.
  const DatReadResult result =
      DatFileReader::read("tests/data/too-many-points.dat");

  CHECK(result.success());
  CHECK_EQ(result.airfoil.coordinates.size(), static_cast<std::size_t>(700));
}

TEST(unit, DatReaderAcceptsNegativeFiniteCoordinates) {
  const TempDatFile file(
      "negative-coordinates.dat",
      "NEGATIVE POINT FOIL\n"
      "-1.25 -0.5\n"
      "0.0 0.0\n"
      "-3.5e-2 4.25\n");
  CHECK(file.created());
  if (!file.created()) {
    return;
  }

  const DatReadResult result = DatFileReader::read(file.path());
  CHECK(result.success());
  CHECK_EQ(result.airfoil.coordinates.size(), static_cast<std::size_t>(3));

  if (result.airfoil.coordinates.size() == 3) {
    CHECK_NEAR(result.airfoil.coordinates[0].x, -1.25, 1e-12, 0.0);
    CHECK_NEAR(result.airfoil.coordinates[0].y, -0.5, 1e-12, 0.0);
    CHECK_NEAR(result.airfoil.coordinates[2].x, -3.5e-2, 1e-12, 0.0);
  }
}

TEST(unit, DatReaderRejectsAdditionalColumns) {
  const TempDatFile file(
      "additional-columns.dat",
      "EXTRA COLUMN FOIL\n"
      "1.0 0.5 9.9\n"
      "0.0 0.0\n"
      "1.0 -0.5\n");
  CHECK(file.created());
  if (!file.created()) {
    return;
  }

  const DatReadResult result = DatFileReader::read(file.path());
  CHECK_FALSE(result.success());
  CHECK(result.error == DatReadError::invalidCoordinate);
  CHECK_EQ(result.lineNumber, static_cast<std::size_t>(2));
}

TEST(unit, DatReaderRejectsNanAndInfinity) {
  const TempDatFile nanFile(
      "nan-coordinate.dat",
      "NAN FOIL\n"
      "nan 0.5\n"
      "0.0 0.0\n"
      "1.0 -0.5\n");
  CHECK(nanFile.created());
  if (nanFile.created()) {
    const DatReadResult result = DatFileReader::read(nanFile.path());
    CHECK_FALSE(result.success());
    CHECK(result.error == DatReadError::invalidCoordinate);
    CHECK_EQ(result.lineNumber, static_cast<std::size_t>(2));
  }

  const TempDatFile infFile(
      "infinity-coordinate.dat",
      "INF FOIL\n"
      "1.0 0.5\n"
      "-inf 0.0\n"
      "1.0 -0.5\n");
  CHECK(infFile.created());
  if (infFile.created()) {
    const DatReadResult result = DatFileReader::read(infFile.path());
    CHECK_FALSE(result.success());
    CHECK(result.error == DatReadError::invalidCoordinate);
    CHECK_EQ(result.lineNumber, static_cast<std::size_t>(3));
  }
}

// AREAD-compatible path ------------------------------------

TEST(unit, DatReaderCoordinateFileDetectsLabeled) {
  const TempDatFile file(
      "coord-labeled.dat",
      "CLARK Y\n"
      "1.0 0.00013\n0.9 0.02\n0.5 0.06\n0.2 0.05\n0.05 0.02\n"
      "0.0 0.0\n0.05 -0.02\n0.5 -0.05\n0.9 -0.01\n1.0 0.0\n");
  CHECK(file.created());
  const auto result =
      DatFileReader::readCoordinateFile(file.path(), 0);
  CHECK(result.success());
  CHECK(result.format == xfoil::app::CoordinateFileFormat::labeled);
  CHECK_EQ(result.name, std::string("CLARK Y"));
  CHECK_EQ(result.coordinates.size(), static_cast<std::size_t>(10));
  CHECK(result.ispars.empty());
}

TEST(unit, DatReaderCoordinateFileDetectsGenericAndKeepsBothLines) {
  const TempDatFile file(
      "coord-generic.dat",
      "1.0 0.0\n0.5 0.1\n0.0 0.0\n0.5 -0.1\n1.0 0.0\n");
  CHECK(file.created());
  const auto result =
      DatFileReader::readCoordinateFile(file.path(), 0);
  CHECK(result.success());
  CHECK(result.format == xfoil::app::CoordinateFileFormat::generic);
  CHECK(result.name.empty());
  CHECK_EQ(result.coordinates.size(), static_cast<std::size_t>(5));
}

TEST(unit, DatReaderCoordinateFileSkipsCommentLines) {
  const TempDatFile file(
      "coord-comments.dat",
      "# comment first\n! name line starts with sharp, so this is plain\n"
      "# skipped\n1.0 0.0\n\n0.5 0.1\n! skipped again\n0.0 0.0\n"
      "0.5 -0.1\n1.0 0.0\n");
  CHECK(file.created());
  // First non-comment line contains two numbers -> plain file.
  const auto result =
      DatFileReader::readCoordinateFile(file.path(), 0);
  CHECK(result.success());
  CHECK(result.format == xfoil::app::CoordinateFileFormat::generic);
  CHECK_EQ(result.coordinates.size(), static_cast<std::size_t>(5));
}

TEST(unit, DatReaderCoordinateFileMsesKeepsDomainLine) {
  const TempDatFile file(
      "coord-mses.dat",
      "MSES name line\n"
      " -2.0  3.0  -2.5  3.5\n"
      "1.0 0.0\n0.5 0.1\n0.0 0.0\n0.5 -0.1\n1.0 0.0\n");
  CHECK(file.created());
  const auto result =
      DatFileReader::readCoordinateFile(file.path(), 0);
  CHECK(result.success());
  CHECK(result.format == xfoil::app::CoordinateFileFormat::msesSingle);
  CHECK_EQ(result.ispars, std::string(" -2.0  3.0  -2.5  3.5"));
  CHECK_EQ(result.coordinates.size(), static_cast<std::size_t>(5));
}

TEST(unit, DatReaderCoordinateFileRequestsElementSelection) {
  const TempDatFile file(
      "coord-multi.dat",
      "MSES multi\n"
      " -2.0  3.0  -2.5  3.5\n"
      "1.0 0.0\n0.5 0.1\n999.0 999.0\n"
      "2.0 0.0\n1.5 0.1\n");
  CHECK(file.created());
  const auto first =
      DatFileReader::readCoordinateFile(file.path(), 0);
  CHECK(first.elementSelectionRequired);
  CHECK_FALSE(first.success());

  const auto selected =
      DatFileReader::readCoordinateFile(file.path(), 2);
  CHECK(selected.success());
  CHECK(selected.format == xfoil::app::CoordinateFileFormat::msesMulti);
  CHECK_EQ(selected.coordinates.size(), static_cast<std::size_t>(2));
}

TEST(unit, DatReaderCoordinateFileMissingFileReportsCannotOpen) {
  const auto result = DatFileReader::readCoordinateFile(
      std::filesystem::path("build/test-data/no-such-17d1-file.dat"), 0);
  CHECK_FALSE(result.success());
  CHECK(result.error == DatReadError::cannotOpen);
}

TEST(unit, DatReaderCoordinateFileRejectsMissingElement) {
  const TempDatFile file(
    "coord-missing-element.dat",
    "MSES multi\n"
    " -2.0  3.0  -2.5  3.5\n"
    "1.0 0.0\n0.5 0.1\n999.0 999.0\n"
    "2.0 0.0\n1.5 0.1\n");

  CHECK(file.created());

  const auto result =
  DatFileReader::readCoordinateFile(file.path(), 3);

  CHECK_FALSE(result.success());
  CHECK(result.error == DatReadError::elementNotFound);
  CHECK(result.coordinates.empty());
}

TEST(unit, DatReaderCoordinateFileRejectsEmptyElementAfterFinalMarker) {
  const TempDatFile file(
    "coord-final-marker.dat",
    "MSES multi\n"
    " -2.0  3.0  -2.5  3.5\n"
    "1.0 0.0\n0.5 0.1\n999.0 999.0\n");

  CHECK(file.created());

  const auto result =
  DatFileReader::readCoordinateFile(file.path(), 2);

  CHECK_FALSE(result.success());
  CHECK(result.error == DatReadError::elementNotFound);
  CHECK(result.coordinates.empty());
}

// Buffer semantics: max NB=2403, capacity=2404 ----------

namespace {

std::string MakeLabeledFoil(const std::string& name, int pointCount) {
  std::string contents = name + "\n";
  for (int i = 0; i < pointCount; ++i) {
    contents += "0.5 0.1\n";
  }
  return contents;
}

}  // namespace

TEST(unit, DatReaderCoordinateFileAcceptsMaxPointCount2403) {
  // maximum successful real-point count NB = 2403 (AREAD capacity 2404).
  const TempDatFile file("coord-max-2403.dat", MakeLabeledFoil("MAX 2403", 2403));
  CHECK(file.created());
  const auto result = DatFileReader::readCoordinateFile(file.path(), 0);
  CHECK(result.success());
  CHECK_EQ(result.coordinates.size(), std::size_t{2403});
}

TEST(unit, DatReaderCoordinateFileRejects2404RealCoordinates) {
  // 2404 real coordinates without EOF/separator pair = capacity overflow.
  const TempDatFile file("coord-overflow-2404.dat",
                         MakeLabeledFoil("OVERFLOW 2404", 2404));
  CHECK(file.created());
  const auto result = DatFileReader::readCoordinateFile(file.path(), 0);
  CHECK_FALSE(result.success());
  CHECK(result.error == DatReadError::coordinateOverflow);
}

// End: tests/unit/DatFileReaderTests.cpp
