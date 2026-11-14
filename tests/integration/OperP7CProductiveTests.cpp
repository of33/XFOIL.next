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

// File: tests/integration/OperP7CProductiveTests.cpp
// productive contracts for the eight
// OPER commands (INTX, HALF, ASET, BLF, DAMP, FMOM, FNEW, VELS). Each
// test goes exclusively through the real CLI path and pin
// handler reachability, prompt order, state mutation,
// mutation freedom, menu synchronization, the BLF guard, VELS redo, and
// polar hinge.

#include "TestSupport.h"
#include "app/InteractiveApplication.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

struct RunResult {
  int exitCode{};
  std::string outputText;
};

RunResult runSession(const std::string& inputText) {
  std::istringstream input(inputText);
  std::ostringstream output;
  std::ostringstream errorOutput;

  xfoil::app::cli::InteractiveStartupOptions options;
  options.inputPath = std::string("sample/CLARK_Y.dat");

  RunResult result;
  result.exitCode = xfoil::app::runInteractiveApplication(
      input, output, errorOutput, options);
  result.outputText = output.str();
  return result;
}

std::size_t countOccurrences(const std::string& text,
                             const std::string& needle) {
  std::size_t count = 0;
  std::size_t position = text.find(needle);
  while (position != std::string::npos) {
    ++count;
    position = text.find(needle, position + needle.size());
  }
  return count;
}

// Own, unique test working directory per test run with
// a "build/test-data" subdirectory and a copied start profile: the
// checked relative path names stay unchanged, and the
// files are created process-isolated and removed by RAII.
class TestDataDir {
 public:
  TestDataDir()
      : startupProfile_(std::filesystem::absolute("sample/CLARK_Y.dat")),
        work_("p7c", true) {
    std::error_code error;
    std::filesystem::create_directories(work_.path() / "build" / "test-data",
                                        error);
    std::filesystem::create_directories(work_.path() / "sample", error);
    std::filesystem::copy_file(startupProfile_,
                               work_.path() / "sample" / "CLARK_Y.dat",
                               std::filesystem::copy_options::overwrite_existing,
                               error);
  }

  TestDataDir(const TestDataDir&) = delete;
  TestDataDir& operator=(const TestDataDir&) = delete;

 private:
  std::filesystem::path startupProfile_;
  testing::ScopedTempDirectory work_;
};

std::string dataPath(const std::string& fileName) {
  return "build/test-data/" + fileName;
}

void removeDataFile(const std::string& fileName) {
  std::error_code ignored;
  std::filesystem::remove(dataPath(fileName), ignored);
}

std::string readDataFile(const std::string& name) {
  std::ifstream file(dataPath(name));
  if (!file) {
    return {};
  }
  std::ostringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

std::size_t countNumericLines(const std::string& text) {
  std::size_t lines = 0;
  std::istringstream stream(text);
  std::string line;
  while (std::getline(stream, line)) {
    const std::size_t first = line.find_first_not_of(" \t");
    if (first != std::string::npos &&
        (line[first] == '-' || line[first] == '.' ||
         (line[first] >= '0' && line[first] <= '9'))) {
      ++lines;
    }
  }
  return lines;
}

}  // namespace

// polar hinge with a defined flap: HINC switches the hinge column
// (LHMOMP, xoper.f 1698-1706); FNEW+FMOM define LFLAP; the
// polar accumulation computes HMOM via the same MHINGE path as FMOM.
TEST(integration, OperPolarHingeWithFlapWritesChingeColumn) {
  TestDataDir dir;
  removeDataFile("p7c-hinge.pol");
  const auto result = runSession(
      "NACA 0012\nPANE\nOPER\nALFA 2\nFNEW 0.7 -0.05\nFMOM\n"
      "HINC\nPACC\n" + dataPath("p7c-hinge.pol") + "\n\nALFA 4\nP\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find("not available in this build yet") ==
        std::string::npos);
  CHECK(result.outputText.find(" Hinge moment/span = 0.002416") !=
        std::string::npos);
  CHECK(result.outputText.find(
            "Hinge moment will be written to polar save file") !=
        std::string::npos);
  CHECK(result.outputText.find("Note: Flap hinge location not defined") ==
        std::string::npos);
  const std::string polar = readDataFile("p7c-hinge.pol");
  CHECK(polar.find("Chinge") != std::string::npos);
  CHECK(polar.find("0.00431") != std::string::npos);
  removeDataFile("p7c-hinge.pol");
}

// polar hinge without a defined flap: HINC reports the note, and the
// hinge column stays 0.
TEST(integration, OperPolarHingeWithoutFlapStaysZero) {
  TestDataDir dir;
  removeDataFile("p7c-hinge.pol");
  const auto result = runSession(
      "NACA 0012\nPANE\nOPER\nALFA 2\n"
      "HINC\nPACC\n" + dataPath("p7c-hinge.pol") + "\n\nALFA 4\nP\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find("Note: Flap hinge location not defined") !=
        std::string::npos);
  CHECK(result.outputText.find("Set it with FNEW,FMOM commands") !=
        std::string::npos);
  const std::string polar = readDataFile("p7c-hinge.pol");
  CHECK(polar.find("Chinge") != std::string::npos);
  CHECK(polar.find("0.00431") == std::string::npos);
  removeDataFile("p7c-hinge.pol");
}

// ASET production: single-slot path without an index prompt,
// confirmation prompt, adoption of the polar airfoil contour into current;
// Aborting with 'n' leaves the current contour unchanged (160 panel points).
TEST(integration, OperAsetTakeoverAndAbortAreProductive) {
  TestDataDir dir;
  removeDataFile("p7c-aset-abort.dat");
  const auto abort = runSession(
      "NACA 0012\nPANE\nOPER\nPACC\n\n\nALFA 2\nP\nASET\nn\n\n"
      "SAVE " + dataPath("p7c-aset-abort.dat") + "\nQUIT\n");
  CHECK_EQ(abort.exitCode, 0);
  CHECK(abort.outputText.find("not available in this build yet") ==
        std::string::npos);
  CHECK(abort.outputText.find("Current airfoil will be overwritten.  "
                              "Proceed?  Y") != std::string::npos);
  CHECK(abort.outputText.find("No action taken") != std::string::npos);
  CHECK_EQ(countNumericLines(readDataFile("p7c-aset-abort.dat")),
           std::size_t{160});
  removeDataFile("p7c-aset-abort.dat");

  const auto proceed = runSession(
      "NACA 0012\nPANE\nOPER\nPACC\n\n\nALFA 2\nP\nASET\nY\n"
      "\nQUIT\n");
  CHECK_EQ(proceed.exitCode, 0);
  CHECK(proceed.outputText.find("No action taken") == std::string::npos);
}

// DAMP production: toggle in both directions, menu synchronization
// via TOP re-entry, no prompt consumption.
TEST(integration, OperDampTogglesAcrossMenuReentry) {
  const auto result = runSession(
      "NACA 0012\nPANE\nOPER\nDAMP\n\nOPER\nDAMP\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK_EQ(countOccurrences(result.outputText, "Modified amplification used"),
           std::size_t{1});
  CHECK_EQ(countOccurrences(result.outputText, "Original amplification used"),
           std::size_t{1});
  CHECK(result.outputText.find("not available in this build yet") ==
        std::string::npos);
}

// BLF guard production: without BL data only the guard
// message, no core values, no input consumption afterwards; the
// sequence command runs correctly (menu synchronous).
TEST(integration, OperBlfGuardWithoutBlData) {
  const auto result = runSession(
      "NACA 0012\nPANE\nOPER\nBLF\nALFA 2\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK_EQ(countOccurrences(result.outputText,
                            " Boundary-layer data are not available."),
           std::size_t{1});
  CHECK(result.outputText.find("xBL, Fint") == std::string::npos);
  CHECK(result.outputText.find("OFS:") == std::string::npos);
  CHECK(result.outputText.find(
            " Calculating unit vorticity distributions ...") !=
        std::string::npos);
}

// VELS-redo production: '!' repeats VELS with ARGOLD (xoper.f
// 1662-1663); COMOLD='VELS' takes precedence over the point continuation.
TEST(integration, OperVelsRedoFollowsComoldPrecedence) {
  const auto velsFirst = runSession(
      "NACA 0012\nPANE\nOPER\nVELS 0.5 0.1\n!\n\nQUIT\n");
  // without a preceding analysis PSILIN returns only the
  // free-stream share (Fortran oracle u=1.0000, v=0.0000).
  CHECK_EQ(countOccurrences(velsFirst.outputText,
                            " u/Uinf =   1.0000   v/Uinf =   0.0000"),
           std::size_t{2});
  CHECK(velsFirst.outputText.find("not available in this build yet") ==
        std::string::npos);

  const auto pointFirst = runSession(
      "NACA 0012\nPANE\nOPER\nALFA 2\nVELS 0.5 0.1\n!\n\nQUIT\n");
  CHECK_EQ(countOccurrences(pointFirst.outputText, " u/Uinf =   1.1284"),
           std::size_t{2});
}

// INTX/HALF productive and buffer-only: SAVE still writes the
// PANE current contour (160 points), not the decimated
// buffer contour; PCOP takes over only afterwards.
TEST(integration, TopIntxHalfLeaveCurrentUntouchedUntilPcop) {
  TestDataDir dir;
  removeDataFile("p7c-intx-before.dat");
  removeDataFile("p7c-intx-after.dat");
  runSession("NACA 2412\nPANE\nSAVE " + dataPath("p7c-intx-before.dat") +
             "\nQUIT\n");
  const auto after = runSession(
      "NACA 2412\nPANE\nINTX\nC\nC\n0.5\n\nHALF\nHALF\nSAVE " +
      dataPath("p7c-intx-after.dat") + "\nQUIT\n");
  CHECK_EQ(after.exitCode, 0);
  CHECK(after.outputText.find("not available in this build yet") ==
        std::string::npos);
  CHECK(after.outputText.find("Result has been placed in buffer") !=
        std::string::npos);
  CHECK_EQ(countNumericLines(readDataFile("p7c-intx-before.dat")),
           countNumericLines(readDataFile("p7c-intx-after.dat")));
  removeDataFile("p7c-intx-before.dat");
  removeDataFile("p7c-intx-after.dat");
}

// End: tests/integration/OperP7CProductiveTests.cpp
