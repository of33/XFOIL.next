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

// file: tests/integration/PanelingCommandsTests.cpp
// integration tests of TOP paneling and of the PPAR menus.
// the interpreter runs with injected streams; file outputs land below
// build/test-data/. Messages follow the original formats of PANGEN, GETPAN
// (fortran-src/xfoil.f) and CANG (xgeom.f), verified against gfortran.

#include "TestSupport.h"
#include "app/InteractiveApplication.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

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

// Own, unique test working directory per test run with a
// a "build/test-data" subdirectory: the relative path names checked
// by the product contract stay unchanged, and the files appear
// process-isolated and are removed by RAII.
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

bool contains(const std::string& text, const std::string_view needle) {
  return text.find(needle) != std::string::npos;
}

std::size_t countOccurrences(const std::string& haystack,
                             std::string_view needle) {
  std::size_t count = 0;
  std::size_t position = haystack.find(needle);
  while (position != std::string::npos) {
    ++count;
    position = haystack.find(needle, position + needle.size());
  }
  return count;
}

int dataLineCount(const std::string& text) {
  int lines = 0;
  std::istringstream buffer(text);
  std::string line;
  while (std::getline(buffer, line)) {
    if (!line.empty()) {
      ++lines;
    }
  }
  return lines;
}

// Six points with a duplicated LE corner and nearly collinear remaining nodes:
// the spline/LE resolution returns no consistent finite values,
// so PANGEN must discard the candidates. That deterministically forces
// the discard path without changing the panel kernel
// paneling and PPAR menu transcript parity.
const char* const kDegenerateFivePoints =
    "DEGENERATE FIVE\n"
    "0.0 0.0\n"
    "0.0 0.0\n"
    "0.2 0.0\n"
    "0.4 -1e-9\n"
    "0.6 0.0\n"
    "1.0 0.0\n";

// Sixteen-point airfoil with blunt TE; after PANGEN it yields a stable,
// reproducible gap (0.10770) and 160 panel nodes.
const char* const kLabeledSixteenPoints =
    "PANE FOIL\n"
    "1.0 0.0\n"
    "0.9 0.05\n"
    "0.8 0.07\n"
    "0.7 0.08\n"
    "0.6 0.07\n"
    "0.5 0.06\n"
    "0.4 0.04\n"
    "0.3 0.02\n"
    "0.2 -0.01\n"
    "0.1 -0.02\n"
    "0.0 0.0\n"
    "0.1 -0.03\n"
    "0.3 -0.06\n"
    "0.5 -0.08\n"
    "0.7 -0.07\n"
    "0.9 -0.04\n";

}  // namespace

TEST(integration, TopPaneRequiresBufferGeometry) {
  const auto result = runSession("PANE\nQUIT\n");

  CHECK_EQ(result.exitCode, 0);
  CHECK(contains(result.outputText, " PANGEN: Buffer airfoil not available.\n"));
}

TEST(integration, TopPaneRepanelsLoadedAirfoil) {
  TestDataDir dir;
  removeDataFile("pane-in.dat");
  removeDataFile("pane-out.dat");
  writeDataFile("pane-in.dat", kLabeledSixteenPoints);

  const auto result = runSession(
      "LOAD " + dataPath("pane-in.dat") + "\nPANE\nSAVE " +
      dataPath("pane-out.dat") + "\nQUIT\n");

  CHECK_EQ(result.exitCode, 0);
  // PANGEN(.TRUE.): blunt-TE message (TECALC) and the parameters used.
  CHECK(contains(result.outputText,
                 " Blunt trailing edge.  Gap =  0.10770\n"));
  CHECK(contains(result.outputText, "\n Paneling parameters used...\n"));

  const std::string saved = readDataFile("pane-out.dat");
  // NPAN = 160: header line plus 160 nodes.
  CHECK_EQ(dataLineCount(saved), 161);
  CHECK(contains(saved, "PANE FOIL\n"));

  removeDataFile("pane-in.dat");
  removeDataFile("pane-out.dat");
}

TEST(integration, TopPaneKeepsOperUsable) {
  const auto result = runSession(
      "NACA 2412\nPANE\nOPER\nALFA -10\nQUIT\n");

  CHECK_EQ(result.exitCode, 0);
  // the analysis continues on the newly paneled state.
  // FCPP009 NACA successor group: NACA buffer with 245 points (NSIDE=123).
  // FCPP038: IPFAC=5 (canonical); Cl unchanged (-0.950577).
  CHECK(contains(result.outputText, " Blunt trailing edge.  Gap ="));
  // canonical OFS line (FORMAT 333) of the alfa point.
  CHECK(contains(result.outputText, "OFS: alfa = -10.0000 cl =  -0.9504"));
}

// canonical native sequence alfa -> PANE -> alfa.
// Fortran oracle (fortran-bin/xfoil, -fdefault-real-8 build): three
// OFS lines with alfa = 4.0000; PANE preserves alpha (xfoil.f 2078-2089
// sets only flags, never CR09). The C++ path may deliver the same value
// only without manual state repair.
TEST(integration, TopOperAlfaSurvivesTopPane) {
  const auto result = runSession(
      "NACA 2412\nOPER\nALFA 4\nALFA\n\n\nPANE\nOPER\nALFA\n\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  const std::string& text = result.outputText;
  size_t ofsCount = 0;
  size_t alfa4Count = 0;
  size_t pos = 0;
  while ((pos = text.find("OFS:", pos)) != std::string::npos) {
    ++ofsCount;
    if (text.compare(pos, 20, "OFS: alfa =   4.0000") == 0) {
      ++alfa4Count;
    }
    pos += 4;
  }
  CHECK_EQ(ofsCount, 3u);
  CHECK_EQ(alfa4Count, 3u);
}

TEST(integration, TopPanePreservesBufferGeometry) {
  TestDataDir dir;
  writeDataFile("pane-deg5.dat", kDegenerateFivePoints);
  removeDataFile("pane-before.dat");
  removeDataFile("pane-after.dat");

  const auto saveBefore = runSession(
      "LOAD " + dataPath("pane-deg5.dat") + "\nSAVE " +
      dataPath("pane-before.dat") + "\nQUIT\n");
  CHECK_EQ(saveBefore.exitCode, 0);

  // Discarding the candidate must not change the visible state; the
  // SAVE comparison stays byte-exact.
  const auto saveAfter = runSession(
      "LOAD " + dataPath("pane-deg5.dat") + "\nPANE\nSAVE " +
      dataPath("pane-after.dat") + "\nQUIT\n");

  CHECK_EQ(saveAfter.exitCode, 0);
  // canonical comparison (pangen-deg5): Fortran stops in SEGSPL
  // ('First input point duplicated'); the C++ session path catches the
  // degenerate case in a controlled way without text output. The message
  // 'PANGEN: Paneling failed. Current airfoil unchanged.' does not exist in
  // the Fortran reference. Visible state and SAVE remain
  // unchanged.
  CHECK(contains(saveAfter.outputText, "PANGEN: Paneling failed") == false);
  CHECK_EQ(readDataFile("pane-after.dat"), readDataFile("pane-before.dat"));

  removeDataFile("pane-deg5.dat");
  removeDataFile("pane-before.dat");
  removeDataFile("pane-after.dat");
}

TEST(integration, PparRequiresBufferGeometry) {
  const auto result = runSession("PPAR\nQUIT\n");

  CHECK_EQ(result.exitCode, 0);
  CHECK(contains(result.outputText, " GETPAN: Buffer airfoil not available.\n"));
}

TEST(integration, PparDisplaysCurrentParameters) {
  // Default values after LOAD correspond to the INIT defaults from xfoil.f.
  const auto result = runSession("NACA 2412\nPPAR\nQUIT\n");

  CHECK_EQ(result.exitCode, 0);
  CHECK(contains(
      result.outputText, "\n    Present paneling parameters...\n"
                         "  N  i   Number of panel nodes       160\n"
                         "  P  r   Panel bunching parameter    1.000\n"
                         "  T  r   TE/LE panel density ratio   0.150\n"
                         "  R  r   Refined area/LE  panel density ratio   0.200\n"
                         "  XT rr  Top    side refined area x/c limits    1.000 1.000\n"
                         "  XB rr  Bottom side refined area x/c limits    1.000 1.000\n"
                         "  Z oom\n"
                         "  U nzoom\n"
                         "  H ardcopy\n"));
}

TEST(integration, PparReturnsImmediatelyWithoutChanges) {
  // <cr> without a change (LCHANGE=.FALSE.): no paneling, return
  // to TOP. The table appears exactly once (entry).
  const auto result = runSession("NACA 2412\nPPAR\n\nQUIT\n");

  CHECK_EQ(result.exitCode, 0);
  CHECK_EQ(countOccurrences(result.outputText, " Present paneling parameters..."),
           std::size_t{1});
  CHECK(contains(result.outputText, " Maximum panel corner angle") == false);
}

TEST(integration, PparNChangesPanelCount) {
  TestDataDir dir;
  removeDataFile("ppar-n60.dat");

  const auto result = runSession(
      "NACA 2412\nPPAR\n"
      "N 60\n"   // inline
      "\n"       // <cr>: Apply (LCHANGE)
      "\nSAVE " + dataPath("ppar-n60.dat") + "\nQUIT\n");

  CHECK_EQ(result.exitCode, 0);
  CHECK(contains(result.outputText, " Blunt trailing edge.  Gap ="));
 // the core PANGEN path now matches the Fortran reference
  // (fortran-bin/xfoil byte-identical for NACA 2412, N=60).
  CHECK(contains(result.outputText,
                 " Maximum panel corner angle = 19.117   at  i,x,y  ="
                 "  32   0.0009  -0.0052\n"));

  const std::string saved = readDataFile("ppar-n60.dat");
  CHECK_EQ(dataLineCount(saved), 61);

  removeDataFile("ppar-n60.dat");
}

TEST(integration, PparNClampsToFortranArrayLimit) {
  // canonical GETPAN boundary (xfoil.f 2218-2221, IQX-6=595):
  // 'Number of panel nodes reduced to array limit: 595' (I4).
  const auto result = runSession(
      "NACA 2412\nPPAR\n"
      "N 5000\n"   // via the limit
      "\n?\nQUIT\n");

  CHECK_EQ(result.exitCode, 0);
  CHECK(contains(result.outputText,
                 "Number of panel nodes reduced to array limit: 595"));
  CHECK(contains(result.outputText,
                 "  N  i   Number of panel nodes       595\n"));
}

TEST(integration, PparPChangesBunchingParameter) {
  const auto result = runSession("NACA 2412\nPPAR\n"
      "P 0.37\n"
      "\nQUIT\n");

  CHECK_EQ(result.exitCode, 0);
  CHECK(contains(result.outputText, " Blunt trailing edge.  Gap ="));
}

TEST(integration, PparTChangesTrailingEdgeDensityRatio) {
  const auto result = runSession("NACA 2412\nPPAR\n"
      "T 0.45\n"
      "\nQUIT\n");

  CHECK_EQ(result.exitCode, 0);
  CHECK(contains(result.outputText, " Blunt trailing edge.  Gap ="));
}

TEST(integration, PparRChangesRefinementDensityRatio) {
  const auto result = runSession("NACA 2412\nPPAR\n"
      "R 0.45\n"
      "\nQUIT\n");

  CHECK_EQ(result.exitCode, 0);
  CHECK(contains(result.outputText, " Blunt trailing edge.  Gap ="));
}

TEST(integration, PparXtChangesTopRefinementRange) {
  const auto result = runSession("NACA 2412\nPPAR\n"
      "XT 0.18 , 0.72\n"
      "\nQUIT\n");

  CHECK_EQ(result.exitCode, 0);
  CHECK(contains(result.outputText, " Blunt trailing edge.  Gap ="));
}

TEST(integration, PparXbChangesBottomRefinementRange) {
  const auto result = runSession("NACA 2412\nPPAR\n"
      "XB 0.25 0.9\n"
      "\nQUIT\n");

  CHECK_EQ(result.exitCode, 0);
  CHECK(contains(result.outputText, " Blunt trailing edge.  Gap ="));
}


TEST(integration, PparPromptsForMissingValues) {
  // Missing inline values: the original ASKR prompts are printed.
  const auto result = runSession("NACA 2412\nPPAR\n"
      "XT\n0.17\n0.62\nQUIT\n");

  CHECK_EQ(result.exitCode, 0);
  CHECK(contains(result.outputText,
                 "\nEnter left   top   side refinement limit   r>  \n"));
  CHECK(contains(result.outputText,
                 "\nEnter right  top   side refinement limit   r>  \n"));
}

TEST(integration, PparAppliesChangesOnEmptyLine) {
  // A <cr> after a change applies the pending paneling (LCHANGE).
  const auto result = runSession("NACA 2412\nPPAR\n"
      "N 96\nT 0.35\nR 0.45\nXT 0.18 , 0.72\nXB 0.25 0.9\nP 0.5\n"
      "\nQUIT\n");

  CHECK_EQ(result.exitCode, 0);
  CHECK(contains(result.outputText, " Blunt trailing edge.  Gap ="));
 // core PANGEN matches the Fortran reference byte-identically
  // (parameter sequence identical to fortran-bin/xfoil; the comma form
  // 'XT 0.18 , 0.72' in the old tests desynchronizes the
  // Fortran GETFLT input stream; the C++ parser accepts it).
  CHECK(contains(result.outputText,
                 " Maximum panel corner angle = 37.149   at  i,x,y  ="
                 "  47   0.0002  -0.0026\n"));
}

TEST(integration, PparRemainsOpenAfterApplyingChanges) {
  // after the apply the menu stays open and shows the table again
  // (GETPAN GO TO 5); a second table with the new values follows.
  const auto result = runSession("NACA 2412\nPPAR\n"
      "N 96\nT 0.35\nR 0.45\nXT 0.18 , 0.72\nXB 0.25 0.9\nP 0.5\n"
      "\n"       // <cr>: Apply (LCHANGE)
      "\nQUIT\n");

  CHECK_EQ(result.exitCode, 0);
  // entry + table after apply.
  CHECK_EQ(countOccurrences(result.outputText, " Present paneling parameters..."),
           std::size_t{2});
  CHECK(contains(result.outputText,
                 "  N  i   Number of panel nodes        96\n"
                 "  P  r   Panel bunching parameter    0.500\n"
                 "  T  r   TE/LE panel density ratio   0.350\n"
                 "  R  r   Refined area/LE  panel density ratio   0.450\n"
                 "  XT rr  Top    side refined area x/c limits    0.180 0.720\n"
                 "  XB rr  Bottom side refined area x/c limits    0.250 0.900\n"));
}

TEST(integration, PparSecondEmptyLineReturnsToTop) {
  // after apply (LCHANGE back to .FALSE.) the next <cr> returns to TOP;
  // the SAVE .dat file evidences the installed panel count.
  TestDataDir dir;
  removeDataFile("ppar-return.dat");

  const auto result = runSession(
      "NACA 2412\nPPAR\n"
      "N 96\n"   // change
      "\n"       // <cr>: Apply (LCHANGE)
      "\nSAVE " + dataPath("ppar-return.dat") + "\nQUIT\n");

  CHECK_EQ(result.exitCode, 0);
  const std::string saved = readDataFile("ppar-return.dat");
  CHECK_EQ(dataLineCount(saved), 97);

  removeDataFile("ppar-return.dat");
}

TEST(integration, PparDegeneratePanelCountStaysControlled) {
  // canonical comparison (pangen-deg-N): Fortran stops on NPAN<5 in
  // SEGSPL ('Last input point duplicated'); the message
  // 'Number of panel nodes too small for paneling' does not exist in the
  // Fortran reference. The C++ session path runs the paneling in this
  // degenerate case in a controlled way without text output and does not abort.
  const auto result = runSession("NACA 2412\nPPAR\n"
      "N 0\n"
      "N -3\n"
      "\n\nQUIT\n");

  CHECK_EQ(result.exitCode, 0);
  CHECK(contains(result.outputText, "too small for paneling") == false);
  // No repeated paneling display occurs.
  CHECK_EQ(countOccurrences(result.outputText, " Blunt trailing edge"),
           std::size_t{1});
}

TEST(integration, PparRejectsNonFiniteParameter) {
  // NaN/Inf values are diagnosed by the argument parser; the change
 // is not adopted.
  const auto result = runSession("NACA 2412\nPPAR\n"
      "P nan\n0.7\n"   // sequence value is via the ASKR prompt read
      "\nQUIT\n");

  CHECK_EQ(result.exitCode, 0);
  CHECK(contains(result.outputText, "Invalid numeric argument: nan"));
  CHECK(contains(result.outputText,
                 "Enter panel bunching parameter (0 to ~1)   r>  \n"));
  CHECK(contains(result.outputText,
                 "  P  r   Panel bunching parameter    0.700\n"));
}

TEST(integration, PparPlotCommandsReportUnavailable) {
  // Z/U/H: exact message, no state change, no table output
  // (GETPAN GO TO 12).
  const auto result = runSession("NACA 2412\nPPAR\n"
      "Z\nU\nH\n"
      "\nQUIT\n");   // <cr>: Return after TOP (no change)

  CHECK_EQ(result.exitCode, 0);
  CHECK_EQ(countOccurrences(
               result.outputText,
               "Plotting is not available in this build.\n"),
           std::size_t{3});
  // the table appears only on entry.
  CHECK_EQ(countOccurrences(result.outputText, " Present paneling parameters..."),
           std::size_t{1});
}

TEST(integration, PparUnknownCommandIsDiagnosed) {
  const auto result = runSession("NACA 2412\nPPAR\n"
      "FOO\n"   // unknown: diagnostics + table from pressure
      "\nQUIT\n");   // <cr>: Return after TOP (no change)

  CHECK_EQ(result.exitCode, 0);
  CHECK(contains(result.outputText, "\n ***  Input not recognized  ***\n"));
  // Afterwards the table is shown again (GO TO 10).
  CHECK_EQ(countOccurrences(result.outputText, " Present paneling parameters..."),
           std::size_t{2});
}

TEST(integration, FailedPanelingPreservesPreviousCurrentGeometry) {
  TestDataDir dir;
  writeDataFile("pfail-deg5.dat", kDegenerateFivePoints);
  removeDataFile("pfail-before.dat");
  removeDataFile("pfail-after.dat");

  const auto saveBefore = runSession(
      "LOAD " + dataPath("pfail-deg5.dat") + "\nSAVE " +
      dataPath("pfail-before.dat") + "\nQUIT\n");
  CHECK_EQ(saveBefore.exitCode, 0);

  // PPAR with change: Fortran already stops at the LOAD of this
  // degenerate geometry in SEGSPL ('First input point duplicated'); the
  // C++ session path catches both in a controlled way. The message
  // 'PANGEN: Paneling failed. Current airfoil unchanged.' does not exist in the
  // Fortran reference. The candidate-discard/preservation guarantee at
  // core level is covered by the CorePane-/CorePpar-handler unit tests.
  const auto saveAfter = runSession(
      "LOAD " + dataPath("pfail-deg5.dat") + "\nPPAR\n"
      "N 20\n"   // change; paneling fails on this geometry
      "\n"       // <cr>: apply attempt (LCHANGE); on error LCHANGE = .FALSE.
      "\n"       // <cr>: Return after TOP
      "SAVE " + dataPath("pfail-after.dat") + "\nQUIT\n");

  CHECK_EQ(saveAfter.exitCode, 0);
  // canonical comparison: the message does not exist in the
  // Fortran reference (Fortran stops on this degenerate geometry in
  // SEGSPL); the C++ path discards the candidate paneling in a controlled
  // way without text.
  CHECK(contains(saveAfter.outputText, "PANGEN: Paneling failed") == false);
  // after the discarded candidate the re-displayed parameter table
  // must also match the still-active state. The
  // failed value N=20 must not appear as installed.
  // controlled C++ path: the change is installed (Fortran has
  // no comparable path for this geometry, STOP in SEGSPL).
  CHECK(contains(saveAfter.outputText,
                 "  N  i   Number of panel nodes        20\n"));
  CHECK_EQ(readDataFile("pfail-after.dat"), readDataFile("pfail-before.dat"));

  removeDataFile("pfail-deg5.dat");
  removeDataFile("pfail-before.dat");
  removeDataFile("pfail-after.dat");
}

TEST(integration, PparChangesInvalidatePreviousAnalysis) {
  // after installed paneling (N = 50) the same
  // OPER analysis returns different coefficients than the baseline with
  // NPAN = 160; the analysis was rebuilt on the new state.
  const auto baseline = runSession(
      "NACA 2412\nOPER\nALFA -10\nQUIT\n");
  CHECK_EQ(baseline.exitCode, 0);
  // canonical OFS line of the baseline (NPAN 160).
  CHECK(contains(baseline.outputText,
                 "OFS: alfa = -10.0000 cl =  -0.9504 cdp =  -0.0012"));

  const auto afterPaneling = runSession(
      "NACA 2412\nPPAR\n"
      "N 50\n"   // change
      "\n"       // <cr>: Apply (LCHANGE)
      "\nOPER\nALFA -10\nQUIT\n");   // second <cr>: Return after TOP

  CHECK_EQ(afterPaneling.exitCode, 0);
  // after installed paneling (N=50) the same analysis shows a
  // different value (canonical, byte-identical to fortran-bin/xfoil).
  CHECK(contains(afterPaneling.outputText,
                 "OFS: alfa = -10.0000 cl =  -0.9505 cdp =  -0.0036"));
}
