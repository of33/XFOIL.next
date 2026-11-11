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

// file: tests/integration/TopFileCommandsTests.cpp
// integration tests of the TOP file and profile commands.
// the interpreter runs with injected streams; file outputs land under
// build/test-data/. Expected messages follow the original formats from
// fortran-src/xfoil.f (LOAD, SAVE, NACA, DELI, INTE), with the formats
// additionally verified with gfortran.

#include "TestSupport.h"
#include "app/InteractiveApplication.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct SessionResult {
  int exitCode{};
  std::string outputText;
  std::string errorText;
};

SessionResult runSession(const std::string& inputText) {
  SessionResult result;
  std::istringstream input(inputText);
  std::ostringstream output;
  std::ostringstream errorOutput;

  xfoil::app::cli::InteractiveStartupOptions options;
  result.exitCode = xfoil::app::runInteractiveApplication(
      input, output, errorOutput, options);
  result.outputText = output.str();
  result.errorText = errorOutput.str();
  return result;
}

// Own, unique test working directory per test run with
// a "build/test-data" subdirectory: the checked relative path names
// stay unchanged, and the files are created
// process-isolated and removed by RAII.
class TestDataDir {
 public:
  TestDataDir() : work_("paneling", true) {
    std::error_code error;
    std::filesystem::create_directories(work_.path() / "build" / "test-data",
                                        error);
  }

  TestDataDir(const TestDataDir&) = delete;
  TestDataDir& operator=(const TestDataDir&) = delete;

 private:
  testing::ScopedTempDirectory work_;
};

std::string dataPath(const std::string& fileName) {
  return "build/test-data/" + fileName;
}

void writeDataFile(const std::string& fileName, const std::string& contents) {
  std::ofstream out(dataPath(fileName));
  out << contents;
}

std::string readDataFile(const std::string& fileName) {
  std::ifstream in(dataPath(fileName));
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

void removeDataFile(const std::string& fileName) {
  std::error_code ignored;
  std::filesystem::remove(dataPath(fileName), ignored);
}

// Simple, counterclockwise-oriented profile contour with a named line.
const char* const kLabeledProfile =
    "TEST PROFILE\n"
    "1.0 0.0\n"
    "0.75 0.05\n"
    "0.5 0.08\n"
    "0.25 0.06\n"
    "0.05 0.02\n"
    "0.0 0.0\n"
    "0.05 -0.02\n"
    "0.25 -0.06\n"
    "0.5 -0.08\n"
    "0.75 -0.05\n"
    "1.0 0.0\n";

// Same contour without a name line (plain file).
const char* const kPlainProfile =
    "1.0 0.0\n"
    "0.75 0.05\n"
    "0.5 0.08\n"
    "0.25 0.06\n"
    "0.05 0.02\n"
    "0.0 0.0\n"
    "0.05 -0.02\n"
    "0.25 -0.06\n"
    "0.5 -0.08\n"
    "0.75 -0.05\n"
    "1.0 0.0\n";

}  // namespace

TEST(integration, TopNacaSetsBufferAndNamesAirfoil) {
  TestDataDir dir;
  removeDataFile("top-naca.dat");

  const auto result =
      runSession("NACA 2412\nSAVE " + dataPath("top-naca.dat") + "\nQUIT\n");

  CHECK_EQ(result.exitCode, 0);
  // S15 NACA command contract: NSIDE=200, buffer NB=2*200-1=399.
  CHECK(result.outputText.find(" Buffer airfoil set using 399 points\n") !=
        std::string::npos);

  const std::string fileText = readDataFile("top-naca.dat");
  // name(6:9) peculiarity: "NACA 2412" with a space at position 5.
  CHECK(fileText.rfind("NACA 2412\n", 0) == 0);
  removeDataFile("top-naca.dat");
}

// S15 NACA successor group: structural tests for the accepted point count.
// NB=399 (buffer), N=240 (PANGEN default), PCOP=399 (buffer adoption).
// No exact C++ vs Fortran point-value comparison: the panel coordinates
// differ because of FCPP038 (IPFAC=3 vs 5) until the PANGEN correction
// is released. The point counts have been evidenced by FCPP009 since then.
TEST(integration, NacaBufferHas399Points) {
  for (const char* designation : {"0012", "2412", "23012"}) {
    const auto result = runSession(std::string("NACA ") + designation +
                                   "\nQUIT\n");
    CHECK_EQ(result.exitCode, 0);
    CHECK(result.outputText.find(" Buffer airfoil set using 399 points\n") !=
          std::string::npos);
  }
}

TEST(integration, NacaPangenCurrentContourHas240Points) {
  TestDataDir dir;
  removeDataFile("naca-pane.dat");
  const auto result = runSession(
      "NACA 2412\nSAVE " + dataPath("naca-pane.dat") + "\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  // SAVE stores the current (PANGEN-paneled) contour: 240 points.
  // the first line is the airfoil name, the rest are coordinate lines.
  const std::string fileText = readDataFile("naca-pane.dat");
  int dataLines = 0;
  std::istringstream stream(fileText);
  std::string line;
  bool firstLine = true;
  while (std::getline(stream, line)) {
    if (firstLine) {
      firstLine = false;
      continue;  // skip the airfoil name
    }
    if (!line.empty()) {
      ++dataLines;
    }
  }
  CHECK_EQ(dataLines, 240);
  removeDataFile("naca-pane.dat");
}

TEST(integration, NacaPcopCurrentContourHas399Points) {
  TestDataDir dir;
  removeDataFile("naca-pcop.dat");
  const auto result = runSession(
      "NACA 2412\nPCOP\nSAVE " + dataPath("naca-pcop.dat") + "\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  // PCOP takes over the 399-point buffer as the current contour.
  const std::string fileText = readDataFile("naca-pcop.dat");
  int dataLines = 0;
  std::istringstream stream(fileText);
  std::string line;
  bool firstLine = true;
  while (std::getline(stream, line)) {
    if (firstLine) {
      firstLine = false;
      continue;  // skip the airfoil name
    }
    if (!line.empty()) {
      ++dataLines;
    }
  }
  CHECK_EQ(dataLines, 399);
  removeDataFile("naca-pcop.dat");
}

TEST(integration, TopNacaRejectsUnimplementedDesignation) {
  const auto result = runSession("NACA 99999\nQUIT\n");
  CHECK(result.outputText.find(" This designation not implemented.\n") !=
        std::string::npos);
}

TEST(integration, TopNacaFiveDigitValidationMessage) {
  const auto result = runSession("NACA 23512\nQUIT\n");
  CHECK(result.outputText.find(" Illegal 5-digit designation\n") !=
        std::string::npos);
  CHECK(result.outputText.find(
            " First three digits must be 210, 220, ... 250\n") !=
        std::string::npos);
  CHECK(result.outputText.find("Buffer airfoil set") == std::string::npos);
}

TEST(integration, TopLoadLabeledPrintsFortranMessages) {
  TestDataDir dir;
  writeDataFile("top-load-labeled.dat", kLabeledProfile);

  const auto result =
      runSession("LOAD " + dataPath("top-load-labeled.dat") + "\nQUIT\n");

  CHECK(result.outputText.find(" Labeled airfoil file.  Name:  TEST PROFILE") !=
        std::string::npos);
  CHECK(result.outputText.find(
            " Number of input coordinate points:  11\n") != std::string::npos);
  CHECK(result.outputText.find(" Counterclockwise ordering\n") !=
        std::string::npos);
  CHECK(result.outputText.find(
            " Current airfoil nodes set from buffer airfoil nodes (  11 )\n") !=
        std::string::npos);
  removeDataFile("top-load-labeled.dat");
}

TEST(integration, TopLoadPlainAsksForName) {
  TestDataDir dir;
  removeDataFile("top-load-plain-save.dat");
  writeDataFile("top-load-plain.dat", kPlainProfile);

  const auto result = runSession("LOAD " + dataPath("top-load-plain.dat") +
                                 "\nPlainfoil\nSAVE " +
                                 dataPath("top-load-plain-save.dat") +
                                 "\nQUIT\n");

  CHECK(result.outputText.find(" Plain airfoil file\n") != std::string::npos);
  CHECK(result.outputText.find("Enter airfoil name   s>  ") !=
        std::string::npos);
  CHECK(readDataFile("top-load-plain-save.dat").rfind("Plainfoil\n", 0) == 0);
  removeDataFile("top-load-plain.dat");
  removeDataFile("top-load-plain-save.dat");
}

TEST(integration, TopLoadMissingFileReportsAreadErrors) {
  const auto result =
      runSession("LOAD build/test-data/does-not-exist-17d1.dat\nQUIT\n");
  CHECK(result.outputText.find(
            " File OPEN error.  Nonexistent file:  "
            "build/test-data/does-not-exist-17d1.dat  \n") !=
        std::string::npos);
  CHECK(result.outputText.find(" *** LOAD NOT COMPLETED ***\n") !=
        std::string::npos);
}

TEST(integration, TopLoadClockwiseReversesAndAnnounces) {
  TestDataDir dir;
  // Counter-clockwise order: reversed contour.
  std::string clockwise = "CLOCKWISE PROFILE\n";
  {
    const std::string source = kLabeledProfile;
    const std::size_t firstData = source.find('\n') + 1;
    std::ostringstream reversed;
    std::vector<std::string> lines;
    std::istringstream stream(source.substr(firstData));
    std::string line;
    while (std::getline(stream, line)) {
      if (!line.empty()) {
        lines.push_back(line);
      }
    }
    for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
      reversed << *it << "\n";
    }
    clockwise += reversed.str();
  }
  writeDataFile("top-load-clockwise.dat", clockwise);

  const auto result =
      runSession("LOAD " + dataPath("top-load-clockwise.dat") + "\nQUIT\n");

  CHECK(result.outputText.find(" Clockwise ordering\n") != std::string::npos);
  CHECK(result.outputText.find("Counterclockwise ordering") ==
        std::string::npos);
  removeDataFile("top-load-clockwise.dat");
}

TEST(integration, TopNormTogglesAndNormalizes) {
  TestDataDir dir;
  writeDataFile("top-norm.dat", kLabeledProfile);

  const auto toggled = runSession("NORM\nQUIT\n");
  CHECK(toggled.outputText.find(" Loaded airfoil will  be normalized\n") !=
        std::string::npos);

  const auto loaded =
      runSession("NORM\nLOAD " + dataPath("top-norm.dat") + "\nQUIT\n");
  CHECK(loaded.outputText.find(" Airfoil has been normalized\n") !=
        std::string::npos);
  // Normalization: LE at (0,0), chord 1.
  CHECK(loaded.outputText.find("  LE  x,y  =   0.00000   0.00000  |   "
                               "Chord =   1.00000\n") !=
        std::string::npos);

  const auto off = runSession("NORM\nNORM\nQUIT\n");
  CHECK(off.outputText.find(" Loaded airfoil won't be normalized\n") !=
        std::string::npos);
  removeDataFile("top-norm.dat");
}

TEST(integration, TopReveTogglesMessage) {
  const auto result = runSession("REVE\nREVE\nQUIT\n");
  CHECK(result.outputText.find(
            " Airfoil will be written in clockwise order\n") !=
        std::string::npos);
  CHECK(result.outputText.find(
            " Airfoil will be written in counterclockwise order\n") !=
        std::string::npos);
}

TEST(integration, TopDeliInlineAndTablePrompt) {
  TestDataDir dir;
  removeDataFile("top-deli-comma.dat");
  writeDataFile("top-deli-src.dat", kLabeledProfile);

  // An invalid inline value leads to the table with follow-up prompts.
  const auto result = runSession("LOAD " + dataPath("top-deli-src.dat") +
                                 "\nDELI 5\n1\nSAVE " +
                                 dataPath("top-deli-comma.dat") + "\nQUIT\n");
  CHECK(result.outputText.find("   0  blank\n   1  comma\n   2  tab\n") !=
        std::string::npos);
  CHECK(result.outputText.find("  currently, delimiter = 0\n") !=
        std::string::npos);
  CHECK(result.outputText.find("Enter new delimiter   i>  ") !=
        std::string::npos);

  const std::string fileText = readDataFile("top-deli-comma.dat");
  CHECK(fileText.find("1.000000,0.000000\n") != std::string::npos);
  removeDataFile("top-deli-src.dat");
  removeDataFile("top-deli-comma.dat");
}

TEST(integration, TopSaveOverwriteQuery) {
  TestDataDir dir;
  writeDataFile("top-overwrite.dat", "placeholder\n");
  writeDataFile("top-overwrite-src.dat", kLabeledProfile);

  const auto declined = runSession("LOAD " + dataPath("top-overwrite-src.dat") +
                                   "\nSAVE " + dataPath("top-overwrite.dat") +
                                   "\nN\nQUIT\n");
  CHECK(declined.outputText.find(" Output file exists.  Overwrite?  Y\n") !=
        std::string::npos);
  CHECK(declined.outputText.find(" Current airfoil not saved.\n") !=
        std::string::npos);
  CHECK_EQ(readDataFile("top-overwrite.dat"), std::string("placeholder\n"));

  const auto accepted = runSession("LOAD " + dataPath("top-overwrite-src.dat") +
                                   "\nSAVE " + dataPath("top-overwrite.dat") +
                                   "\n\nQUIT\n");
  CHECK(accepted.outputText.find("Current airfoil not saved") ==
        std::string::npos);
  CHECK(readDataFile("top-overwrite.dat").rfind("TEST PROFILE\n", 0) == 0);

  removeDataFile("top-overwrite.dat");
  removeDataFile("top-overwrite-src.dat");
}

TEST(integration, TopUsavIsavFormats) {
  TestDataDir dir;
  removeDataFile("top-usav.dat");
  removeDataFile("top-isav.dat");
  writeDataFile("top-isav-src.dat", kLabeledProfile);

  const auto result = runSession("LOAD " + dataPath("top-isav-src.dat") +
                                 "\nUSAV " + dataPath("top-usav.dat") +
                                 "\nISAV " + dataPath("top-isav.dat") +
                                 "\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);

  const std::string usavText = readDataFile("top-usav.dat");
  // USAV format: no name line, format(1X,I12,I12).
  CHECK(usavText.rfind("            1           0\n", 0) == 0);

  const std::string isavText = readDataFile("top-isav.dat");
  CHECK(isavText.rfind("TEST PROFILE\n -2.0  3.0  -2.5  3.5\n", 0) == 0);

  removeDataFile("top-usav.dat");
  removeDataFile("top-isav.dat");
  removeDataFile("top-isav-src.dat");
}

TEST(integration, TopPcopWithoutBufferReportsUnavailable) {
  const auto result = runSession("PCOP\nQUIT\n");
  CHECK(result.outputText.find(" ABCOPY: Buffer airfoil not available.\n") !=
        std::string::npos);
}

TEST(integration, TopXycmUpdatesDisplayedDefaults) {
  const auto result = runSession("XYCM 0.3 0.1\n?\nQUIT\n");
  CHECK(result.outputText.find(
            "   XYCM rr Change CM reference location, currently  0.30000 "
            "0.10000\n") != std::string::npos);
}

TEST(integration, TopNameAndNincUpdateSavedName) {
  TestDataDir dir;
  removeDataFile("top-name.dat");
  writeDataFile("top-name-src.dat", kLabeledProfile);

  const auto result = runSession("LOAD " + dataPath("top-name-src.dat") +
                                 "\nNAME MyFoil\nNINC\n\nSAVE " +
                                 dataPath("top-name.dat") + "\nQUIT\n");
  // NAMMOD format 1040: NAMEDEF CHARACTER*48, padded to 48.
  CHECK(result.outputText.find(
            " Enter airfoil name or <return> for default:  MyFoil [1]" +
            std::string(38, ' ') + "\n") !=
        std::string::npos);
  CHECK(readDataFile("top-name.dat").rfind("MyFoil [1]\n", 0) == 0);
  removeDataFile("top-name.dat");
  removeDataFile("top-name-src.dat");
}

TEST(integration, TopInteInterpolatesAndKeepsCurrentSeparate) {
  TestDataDir dir;
  writeDataFile("top-inte-a.dat", kLabeledProfile);

  const auto result =
      runSession("INTE\nF\n" + dataPath("top-inte-a.dat") + "\nF\n" +
                 dataPath("top-inte-a.dat") + "\n0.5\nMixed\nQUIT\n");

  CHECK(result.outputText.find("  F  disk file\n") != std::string::npos);
  CHECK(result.outputText.find(
            "  Select source of airfoil \"0\" ( F C ):  ") !=
        std::string::npos);
  CHECK(result.outputText.find(" airfoil \"0\":  TEST PROFILE") !=
        std::string::npos);
  CHECK(result.outputText.find("Specify interpolating fraction  0...1") !=
        std::string::npos);
  CHECK(result.outputText.find(
            " Result has been placed in buffer airfoil\n") !=
        std::string::npos);
  CHECK(result.outputText.find(
            " Execute PCOP or PANE to set new current airfoil\n") !=
        std::string::npos);
  removeDataFile("top-inte-a.dat");
}

TEST(integration, TopInteCurrentSourceNeedsCurrentAirfoil) {
  const auto result = runSession("INTE\nX\nQUIT\n");
  CHECK(result.outputText.find(" Invalid response\n") != std::string::npos);

  const auto noCurrent =
      runSession("INTE\nC\nZ\nQUIT\n");
  CHECK(noCurrent.outputText.find(" No current airfoil available\n") !=
        std::string::npos);
}

// INTX reaches the canonical
// source menu, HALF only decimates the buffer (399 -> 200),
// PCOP takes over only afterwards.
TEST(integration, TopIntxHalfAreProductive) {
  TestDataDir dir;
  removeDataFile("top-intx-half-pinned.dat");

  const auto guarded = runSession(
      "NACA 2412\nPCOP\nINTX\nC\nC\n0.5\n\nHALF\nPCOP\nSAVE " +
      dataPath("top-intx-half-pinned.dat") + "\nQUIT\n");
  CHECK(guarded.outputText.find("not available in this build yet") ==
        std::string::npos);
  CHECK(guarded.outputText.find("Select source of airfoil") !=
        std::string::npos);
  CHECK(guarded.outputText.find("Result has been placed in buffer") !=
        std::string::npos);
  const std::string savedText = readDataFile("top-intx-half-pinned.dat");
  std::size_t coordinateLines = 0;
  std::istringstream saved(savedText);
  std::string line;
  while (std::getline(saved, line)) {
    const std::size_t first = line.find_first_not_of(" \t");
    if (first != std::string::npos &&
        (line[first] == '-' || line[first] == '.' ||
         (line[first] >= '0' && line[first] <= '9'))) {
      ++coordinateLines;
    }
  }
  // INTX (C/C, FRAC 0.5) keeps 399 buffer points; HALF decimates to
  // 200; PCOP takes over only that contour.
  CHECK_EQ(coordinateLines, std::size_t{200});
  removeDataFile("top-intx-half-pinned.dat");
}

TEST(integration, TopLoadCoarseProfileWarnsAboutPanelAngles) {
  TestDataDir dir;
  // Hatch with 90-degree kinks: exceeds ANGTOL = 40 degrees.
  writeDataFile("top-coarse.dat",
                "COARSE\n"
                "1.0 0.0\n"
                "0.5 0.5\n"
                "0.0 0.0\n"
                "0.5 -0.5\n"
                "1.0 0.0\n");

  const auto result =
      runSession("LOAD " + dataPath("top-coarse.dat") + "\nQUIT\n");
  CHECK(result.outputText.find(
            " WARNING: Poor input coordinate distribution\n") !=
        std::string::npos);
  CHECK(result.outputText.find("          Excessive panel angle") !=
        std::string::npos);
  CHECK(result.outputText.find("          Repaneling with PANE and/or PPAR "
                               "suggested\n") != std::string::npos);
  removeDataFile("top-coarse.dat");
}

TEST(integration, TopLoadNormalizationFailurePreservesPreviousGeometry) {
  TestDataDir dir;
  removeDataFile("top-norm-preserved.dat");

  writeDataFile("top-valid-before-norm.dat", kLabeledProfile);
  writeDataFile(
    "top-degenerate-norm.dat",
    "DEGENERATE\n"
    "0.0 0.0\n"
    "0.0 0.0\n"
    "0.0 0.0\n"
    "0.0 0.0\n"
    "0.0 0.0\n");

  const auto result = runSession(
    "LOAD " + dataPath("top-valid-before-norm.dat") +
    "\nNORM\nLOAD " + dataPath("top-degenerate-norm.dat") +
    "\nSAVE " + dataPath("top-norm-preserved.dat") +
    "\nQUIT\n");

  CHECK(
    result.outputText.find("Airfoil normalization failed") !=
    std::string::npos);
  CHECK(
    result.outputText.find("*** LOAD NOT COMPLETED ***") !=
    std::string::npos);
  CHECK(
    result.outputText.find("Airfoil has been normalized") ==
    std::string::npos);

  CHECK(
    readDataFile("top-norm-preserved.dat").rfind(
      "TEST PROFILE\n", 0) == 0);

  removeDataFile("top-valid-before-norm.dat");
  removeDataFile("top-degenerate-norm.dat");
  removeDataFile("top-norm-preserved.dat");
}

// End: tests/integration/TopFileCommandsTests.cpp
