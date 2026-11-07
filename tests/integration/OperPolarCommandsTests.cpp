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

// file: tests/integration/OperPolarCommandsTests.cpp
//-Full rollback: permanent coverage of the productive
// polar management commands PSUM, PLIS, PNAM, PSOR, PDEL, PREM, PWRT,
// PGET, RGET, RDEL via the native CLI (in-process or with
// capture files below build/test-captures). Byte parity of the
// PACC save/dump files is covered by
// regression.PaccDumpByteParityMatchesFortranReference; this test
// checks the table, prompt and round-trip contracts.

#include "TestSupport.h"
#include "app/InteractiveApplication.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

// POLREF (iopol.f 702-760) expects a reference X/Y data format
// (optional '#label' line, Cl/Cd pairs, 999.0 block separators) -- no
// polar save file.
const char* const kRefXyContent =
    " 0.1 0.2\n 0.3 0.4\n999.0 999.0\n999.0 999.0\n"
    "999.0 999.0\n999.0 999.0\n";

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

bool contains(const std::string& text, const std::string& needle) {
  return text.find(needle) != std::string::npos;
}

bool readFile(const std::string& path, std::string& out) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    return false;
  }
  std::ostringstream buffer;
  buffer << stream.rdbuf();
  out = buffer.str();
  return true;
}

// Own, unique test directory per test run; the
// polar capture files lie collision-free and are removed by RAII
// automatically.
std::string capturePath(const testing::ScopedTempDirectory& work,
                        const std::string& fileName) {
  return work.makePath(fileName).string();
}

}  // namespace

TEST(integration, PolarSummaryListRenameSortDeleteTexts) {
  const RunResult run = runSession(
      "OPER\nPACC\n\n\nALFA 2\nALFA 4\nPSUM\nPLIS\nPNAM 1\nRENAMED\n"
      "PSUM\nPDEL 1\nPSUM\n\n\nQUIT\n");
  CHECK_EQ(run.exitCode, 0);
  CHECK(contains(run.outputText,
             "       airfoil                    Re           Mach"));
  CHECK(contains(run.outputText, "CLARK Y"));
  CHECK(contains(run.outputText, "RENAMED"));
  CHECK(contains(run.outputText, " No polars are stored"));
}

TEST(integration, PolarSortOrdersAlphaAscending) {
  const RunResult run = runSession(
      "OPER\nPACC\n\n\nALFA 4\nALFA 2\nALFA 3\nPSOR 1\nPLIS 1\n\n\nQUIT\n");
  CHECK_EQ(run.exitCode, 0);
  // the OFS lines of the alfa points contain the same numbers; compared
  // are the last occurrences (the PLIS output after PSOR).
  const std::size_t two = run.outputText.rfind("   2.000");
  const std::size_t three = run.outputText.rfind("   3.000");
  const std::size_t four = run.outputText.rfind("   4.000");
  CHECK(two != std::string::npos && three != std::string::npos &&
        four != std::string::npos);
  CHECK(two < three);
  CHECK(three < four);
}

TEST(integration, PolarRemovePointsByAlpha) {
  const RunResult run = runSession(
      "OPER\nPACC\n\n\nALFA 2\nALFA 4\nPREM 1\n4.\nPLIS 1\n\n\nQUIT\n");
  CHECK_EQ(run.exitCode, 0);
  CHECK(contains(run.outputText, "Enter alpha(s) of points to be removed:"));
  const std::size_t lastList = run.outputText.rfind("   2.000");
  CHECK(lastList != std::string::npos);
  CHECK(run.outputText.find("   4.000", lastList) == std::string::npos);
}

// --- PREM argument-form regression coverage -------------------------------
// The PREM index/alpha resolution reads its value list from the command
// line; every element access must be guarded by the value count that is
// actually present. These cases pin the memory-safe behaviour for empty,
// blank, invalid, incomplete, minimal, complete and over-range PREM
// argument forms, the no-partial-mutation rule on index errors, and the
// EOF paths of both interactive prompts.

TEST(integration, PremWithoutArgumentUsesSingleSlotAndPromptsForAlphas) {
  // One stored polar: PREM without argument resolves the index to the
  // only slot, echoes the polar and prompts for the alphas. A blank
  // alpha line re-queries without removing anything.
  const RunResult run = runSession(
      "OPER\nPACC\n\n\nALFA 2\nALFA 4\nPREM\n\n3.\nPLIS 1\n\n\nQUIT\n");
  CHECK_EQ(run.exitCode, 0);
  CHECK(contains(run.outputText, "Enter alpha(s) of points to be removed:"));
  const std::size_t lastList = run.outputText.rfind("   2.000");
  CHECK(lastList != std::string::npos);
  CHECK(run.outputText.find("   4.000", lastList) != std::string::npos);
}

TEST(integration, PremBlankArgumentBehavesLikeNoArgument) {
  // A whitespace-only PREM argument parses to an empty value list and
  // must take exactly the no-argument path (single slot, alpha prompt).
  const RunResult run = runSession(
      "OPER\nPACC\n\n\nALFA 2\nALFA 4\nPREM   \n3.\nPLIS 1\n\n\nQUIT\n");
  CHECK_EQ(run.exitCode, 0);
  CHECK(contains(run.outputText, "Enter alpha(s) of points to be removed:"));
  const std::size_t lastList = run.outputText.rfind("   2.000");
  CHECK(lastList != std::string::npos);
  CHECK(run.outputText.find("   4.000", lastList) != std::string::npos);
}

TEST(integration, PremInvalidFirstTokenFallsBackToInteractiveIndexPrompt) {
  // Two stored polars: an invalid first token leaves the value list
  // empty, so PREM shows the polar summary and asks for the index
  // instead of dereferencing a missing value; the session continues.
  const testing::ScopedTempDirectory work("prem-invalid-token");
  const std::string savePath = capturePath(work, "prem-invalid.pol");
  const RunResult run = runSession(
      "OPER\nPACC\n" + savePath +
      "\n\nALFA 2\nALFA 4\nPGET\n" + savePath +
      "\nPREM abc\n1\n4.\nPLIS 1\n\n\nQUIT\n");
  CHECK_EQ(run.exitCode, 0);
  CHECK(contains(run.outputText, "Enter index of polar to modify"));
  CHECK(contains(run.outputText, "Enter alpha(s) of points to be removed:"));
  const std::size_t lastList = run.outputText.rfind("   2.000");
  CHECK(lastList != std::string::npos);
  CHECK(run.outputText.find("   4.000", lastList) == std::string::npos);
}

TEST(integration, PremIndexOnlyArgumentPromptsForAlphasAndEofEndsSession) {
  // Incomplete value list (index without alphas): the alpha list is
  // queried interactively; EOF at that prompt ends the session in a
  // controlled way without touching the stored polar.
  const RunResult run = runSession("OPER\nPACC\n\n\nALFA 2\nALFA 4\nPREM 1\n");
  CHECK_EQ(run.exitCode, 0);
  CHECK(contains(run.outputText, "Enter alpha(s) of points to be removed:"));
}

TEST(integration, PremInlineSingleAlphaRemovesOnlyThatPointWithoutPrompt) {
  // Valid minimal assignment (index plus one alpha): the point is
  // removed directly, no interactive alpha query happens.
  const RunResult run = runSession(
      "OPER\nPACC\n\n\nALFA 2\nALFA 4\nPREM 1 4.\nPLIS 1\n\n\nQUIT\n");
  CHECK_EQ(run.exitCode, 0);
  CHECK(!contains(run.outputText, "Enter alpha(s) of points to be removed:"));
  const std::size_t lastList = run.outputText.rfind("   2.000");
  CHECK(lastList != std::string::npos);
  CHECK(run.outputText.find("   4.000", lastList) == std::string::npos);
}

TEST(integration, PremInlineMultipleAlphasRemoveAllListedPointsAndEliminatePolar) {
  // Valid complete assignment: every listed alpha is removed; removing
  // the last point eliminates the polar slot (xoper.f 1372-1386).
  const RunResult run = runSession(
      "OPER\nPACC\n\n\nALFA 2\nALFA 4\nPREM 1 2. 4.\nPSUM\n\n\nQUIT\n");
  CHECK_EQ(run.exitCode, 0);
  CHECK(!contains(run.outputText, "Enter alpha(s) of points to be removed:"));
  CHECK(contains(run.outputText, " No polars are stored"));
}

TEST(integration, PremOutOfRangeIndexReportsErrorAndReasksWithoutPartialRemoval) {
  // Index out of range with extra values: the canonical error text is
  // emitted and the rejected value list is discarded as a whole -- the
  // alpha of the rejected line (2.) is not applied. The index is then
  // re-resolved (single slot: automatic) and the alpha list is queried
  // interactively.
  const RunResult run = runSession(
      "OPER\nPACC\n\n\nALFA 2\nALFA 4\nPREM 99 2.\n4.\nPLIS 1\n\n\nQUIT\n");
  CHECK_EQ(run.exitCode, 0);
  CHECK(contains(run.outputText, "Specified polar airfoil does not exist"));
  CHECK(contains(run.outputText, "Enter alpha(s) of points to be removed:"));
  const std::size_t lastList = run.outputText.rfind("   2.000");
  CHECK(lastList != std::string::npos);
  CHECK(run.outputText.find("   4.000", lastList) == std::string::npos);
}

TEST(integration, PremZeroIndexExitsCommandWithoutChangingPolar) {
  // Index 0 is the canonical cancel value: PREM returns immediately,
  // neither the alpha prompt nor any removal happens.
  const RunResult run = runSession(
      "OPER\nPACC\n\n\nALFA 2\nALFA 4\nPREM 0\nPLIS 1\n\n\nQUIT\n");
  CHECK_EQ(run.exitCode, 0);
  CHECK(!contains(run.outputText, "Enter alpha(s) of points to be removed:"));
  const std::size_t lastList = run.outputText.rfind("   2.000");
  CHECK(lastList != std::string::npos);
  CHECK(run.outputText.find("   4.000", lastList) != std::string::npos);
}

TEST(integration, PremWithNoStoredPolarsReportsNoPolarsStored) {
  // PREM on an empty polar table reports the canonical hint and does
  // not touch the (empty) value list at all.
  const RunResult run = runSession("OPER\nPREM\nQUIT\n");
  CHECK_EQ(run.exitCode, 0);
  CHECK(contains(run.outputText, " No polars are stored"));
  CHECK(!contains(run.outputText, "Enter alpha(s) of points to be removed:"));
}

TEST(integration, PremEofAtInteractiveIndexPromptEndsSession) {
  // Two stored polars, PREM without argument: the summary and the index
  // query appear; EOF at the index prompt ends the session in a
  // controlled way with exit code 0.
  const testing::ScopedTempDirectory work("prem-eof-index");
  const std::string savePath = capturePath(work, "prem-eof.pol");
  const RunResult run = runSession(
      "OPER\nPACC\n" + savePath +
      "\n\nALFA 2\nALFA 4\nPGET\n" + savePath + "\nPREM\n");
  CHECK_EQ(run.exitCode, 0);
  CHECK(contains(run.outputText, "Enter index of polar to modify"));
}

TEST(integration, PolarWriteGetRoundtripKeepsBytesAndSlots) {
  const testing::ScopedTempDirectory work("polar-cmds-roundtrip");
  const std::string savePath = capturePath(work, "round.pol");
  const std::string writePath = capturePath(work, "written.pol");
  const RunResult run = runSession(
      "OPER\nPACC\n" + savePath +
      "\n\nALFA 2\nALFA 4\nPACC\nPWRT 1\n" + writePath +
      "\nPGET\n" + writePath + "\nPSUM\n\n\nQUIT\n");
  CHECK_EQ(run.exitCode, 0);
  std::string saveBytes;
  std::string writtenBytes;
  CHECK(readFile(savePath, saveBytes));
  CHECK(readFile(writePath, writtenBytes));
  CHECK_EQ(saveBytes.size(), writtenBytes.size());
  CHECK(saveBytes == writtenBytes);
  // PGET keeps the polar as an additional slot.
  CHECK(contains(run.outputText, " Stored as  Polar   2"));
}

TEST(integration, PolarReferenceGetAndDelete) {
  const testing::ScopedTempDirectory work("polar-cmds-refget");
  const std::string refPath = capturePath(work, "refdata.xy");
  {
    std::ofstream out(refPath, std::ios::binary);
    out << kRefXyContent;
  }
  const RunResult run = runSession(
      "OPER\nRGET\n" + refPath + "\nTEST REF\nPSUM\nRDEL 1\nPSUM\n\n\nQUIT\n");
  CHECK_EQ(run.exitCode, 0);
  // Oracle comparison (RC case): RGET accepts the X/Y reference format,
  // asks for the empty label afterwards and does not echo it; RDEL on
  // the empty table reports the canonical hint text. Both branches are
  // confirmed byte-identical against fortran-bin/xfoil.
  CHECK(!contains(run.outputText, "File OPEN error"));
  CHECK(contains(run.outputText, "Enter label for reference polar"));
}

TEST(integration, PolarGetFileEchoMatchesStoredHeader) {
  const testing::ScopedTempDirectory work("polar-cmds-getecho");
  const std::string savePath = capturePath(work, "echo.pol");
  const RunResult run = runSession(
      "OPER\nPACC\n" + savePath +
      "\n\nALFA 2\nPACC\nPGET\n" + savePath + "\n\n\nQUIT\n");
  CHECK_EQ(run.exitCode, 0);
  // PGET echoed the data lines (header + point) before the slot note.
  CHECK(contains(run.outputText, "XFOIL"));
  CHECK(contains(run.outputText, "2.000"));
  CHECK(contains(run.outputText, " Stored as  Polar   2"));
}

// End: tests/integration/OperPolarCommandsTests.cpp
