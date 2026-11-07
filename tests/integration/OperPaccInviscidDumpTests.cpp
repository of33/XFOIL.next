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

// file: tests/integration/OperPaccInviscidDumpTests.cpp
// Inviscid PACC points write exactly one canonical
// PLXADD point group (56+16+96+96 payload bytes, NSIDE=(2,2), IBLTE=(0,0))
// in the dump file; the success message "point written to dump file"
// appears exactly when the group was written successfully.
// viscous dump contract, append, two independent sessions and the
// mixed sequence inviscid -> viscous are retained.
// byte identity against the Fortran oracle is covered by
// regression.PaccInviscidDumpByteParityMatchesFortranReference.

#include "TestSupport.h"
#include "app/InteractiveApplication.h"

#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>

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

// Own, unique test directory per test run; the
// generated .pol/.dump files lie collision-free and are removed by
// RAII. The chmod-0444 error case stays preserved.
std::string capturePath(const testing::ScopedTempDirectory& work,
                        const std::string& fileName) {
  return work.makePath(fileName).string();
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

// Counts the PLXADD point groups in a dump file and checks the
// inviscid group layout (56,16,96,96 payload bytes with 4-byte markers).
// Return value: group count, or -1 on a structural error. The first
// five records are the header (name, parameters, types, indices,
// contour); the contour length is geometry dependent and is not
// hardcoded.
int countInviscidGroups(const std::string& bytes) {
  std::size_t pos = 0;
  auto readMarker = [&](std::size_t at) -> std::uint32_t {
    return static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[at])) |
           (static_cast<std::uint32_t>(
                static_cast<unsigned char>(bytes[at + 1]))
            << 8) |
           (static_cast<std::uint32_t>(
                static_cast<unsigned char>(bytes[at + 2]))
            << 16) |
           (static_cast<std::uint32_t>(
                static_cast<unsigned char>(bytes[at + 3]))
            << 24);
  };
  int groups = 0;
  int record = 0;
  while (pos + 4 <= bytes.size()) {
    const std::uint32_t len = readMarker(pos);
    if (pos + 4 + len + 4 > bytes.size()) {
      return -1;
    }
    if (readMarker(pos + 4 + len) != len) {
      return -1;
    }
    ++record;
    if (record > 5) {
      const int sub = (record - 6) % 4;
      const std::uint32_t expected[4] = {56u, 16u, 96u, 96u};
      if (len != expected[sub]) {
        return -1;
      }
      if (sub == 3) {
        ++groups;
      }
    }
    pos += 4 + len + 4;
  }
  if (pos != bytes.size() || record < 5) {
    return -1;
  }
  return groups;
}

const char* const kDumpWritten = " Point written to dump file ";
const char* const kPointAdded = " Point added to stored polar";

}  // namespace

TEST(integration, PaccInviscidDumpSinglePointWritesExactlyOneGroup) {
  const testing::ScopedTempDirectory work("pacc-inviscid-single");
  const std::string pol = capturePath(work, "one.pol");
  const std::string dump = capturePath(work, "one.dump");
  const RunResult run =
      runSession("OPER\nPACC\n" + pol + "\n" + dump +
                 "\nALFA 2\n\n\nQUIT\n");
  CHECK_EQ(run.exitCode, 0);
  std::string bytes;
  CHECK(readFile(dump, bytes));
  CHECK_EQ(countInviscidGroups(bytes), 1);
  CHECK_EQ(countOccurrences(run.outputText, kDumpWritten), std::size_t{1});
  CHECK_EQ(countOccurrences(run.outputText, kPointAdded), std::size_t{1});
}

TEST(integration, PaccInviscidDumpAseqWritesOneGroupPerPoint) {
  const testing::ScopedTempDirectory work("pacc-inviscid-aseq");
  const std::string pol = capturePath(work, "aseq.pol");
  const std::string dump = capturePath(work, "aseq.dump");
  const RunResult run =
      runSession("OPER\nPACC\n" + pol + "\n" + dump +
                 "\nASEQ 0 4 2\n\n\nQUIT\n");
  CHECK_EQ(run.exitCode, 0);
  std::string bytes;
  CHECK(readFile(dump, bytes));
  CHECK_EQ(countInviscidGroups(bytes), 3);
  CHECK_EQ(countOccurrences(run.outputText, kDumpWritten), std::size_t{3});
  CHECK_EQ(countOccurrences(run.outputText, kPointAdded), std::size_t{3});
}

// Writer status: if the dump file is not writable when the point is
// written, the success message stays missing; the point is still
// recorded in the save table (PLRADD branch unchanged), and the
// session state (file name, accumulation) stays preserved.
TEST(integration, PaccDumpWriteFailureSuppressesSuccessMessage) {
  const testing::ScopedTempDirectory work("pacc-inviscid-writefail");
  const std::string pol = capturePath(work, "ro.pol");
  const std::string dump = capturePath(work, "ro.dump");
  {
    std::ofstream seed(dump, std::ios::binary);
    seed << "placeholder";
  }
  chmod(dump.c_str(), 0444);
  const RunResult run =
      runSession("OPER\nPACC\n" + pol + "\n" + dump +
                 "\nALFA 2\n\n\nQUIT\n");
  chmod(dump.c_str(), 0644);
  CHECK_EQ(run.exitCode, 0);
  CHECK_EQ(countOccurrences(run.outputText, kPointAdded), std::size_t{1});
  CHECK_EQ(countOccurrences(run.outputText, kDumpWritten), std::size_t{0});
}

// append to an existing, parameter-compatible dump file: the second
// PACC pass adds further groups without repeating the header.
TEST(integration, PaccInviscidDumpAppendKeepsSingleHeader) {
  const testing::ScopedTempDirectory work("pacc-inviscid-append");
  const std::string pol = capturePath(work, "app.pol");
  const std::string dump = capturePath(work, "app.dump");
  const RunResult run = runSession(
      "OPER\nPACC\n" + pol + "\n" + dump +
      "\nALFA 2\nPACC\nPACC\n" + pol + "\n" + dump +
      "\nALFA 4\n\n\nQUIT\n");
  CHECK_EQ(run.exitCode, 0);
  std::string bytes;
  CHECK(readFile(dump, bytes));
  CHECK_EQ(countInviscidGroups(bytes), 2);
  CHECK_EQ(countOccurrences(run.outputText, kDumpWritten), std::size_t{2});
  CHECK(
      run.outputText.find("Old polar dump file available for appending") !=
      std::string::npos);
}

// two independent sessions produce byte-identical dump files
// (no session cross-talk, determinism).
TEST(integration, PaccInviscidDumpTwoSessionsDeterministic) {
  const testing::ScopedTempDirectory work("pacc-inviscid-twosessions");
  const std::string polA = capturePath(work, "sA.pol");
  const std::string dumpA = capturePath(work, "sA.dump");
  const std::string polB = capturePath(work, "sB.pol");
  const std::string dumpB = capturePath(work, "sB.dump");
  const RunResult first =
      runSession("OPER\nPACC\n" + polA + "\n" + dumpA +
                 "\nASEQ 0 4 2\n\n\nQUIT\n");
  const RunResult second =
      runSession("OPER\nPACC\n" + polB + "\n" + dumpB +
                 "\nASEQ 0 4 2\n\n\nQUIT\n");
  CHECK_EQ(first.exitCode, 0);
  CHECK_EQ(second.exitCode, 0);
  std::string a;
  std::string b;
  CHECK(readFile(dumpA, a));
  CHECK(readFile(dumpB, b));
  CHECK_EQ(countInviscidGroups(a), 3);
  CHECK(a == b);
}

// mixed sequence: inviscid points, PACC, VISC, PACC with a new
// dump file, viscous point. Exactly one group per point; the inviscid
// file keeps its (2,2) groups, the viscous file carries the
// viscous group with NSIDE/IBLTE set.
TEST(integration, PaccMixedInviscidThenViscousWritesOneGroupEach) {
  const testing::ScopedTempDirectory work("pacc-inviscid-mixed");
  const std::string pol1 = capturePath(work, "m1.pol");
  const std::string dump1 = capturePath(work, "m1.dump");
  const std::string pol2 = capturePath(work, "m2.pol");
  const std::string dump2 = capturePath(work, "m2.dump");
  const RunResult run = runSession(
      "OPER\nPACC\n" + pol1 + "\n" + dump1 +
      "\nALFA 2\nPACC\nVISC\n1000000\nPACC\n" + pol2 + "\n" + dump2 +
      "\nALFA 4\n\n\nQUIT\n");
  CHECK_EQ(run.exitCode, 0);
  std::string d1;
  std::string d2;
  CHECK(readFile(dump1, d1));
  CHECK(readFile(dump2, d2));
  CHECK_EQ(countInviscidGroups(d1), 1);
  // viscous group: NSIDE(1)=IBLTE(1)+NW, here clearly > 2, so the
  // inviscid group layout (96-byte records) does not apply.
  CHECK_EQ(countInviscidGroups(d2), -1);
  CHECK(d2.size() > d1.size());
  CHECK_EQ(countOccurrences(run.outputText, kDumpWritten), std::size_t{2});
  CHECK_EQ(countOccurrences(run.outputText, kPointAdded), std::size_t{2});
}

// End: tests/integration/OperPaccInviscidDumpTests.cpp
