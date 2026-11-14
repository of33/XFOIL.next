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

// File: tests/regression/Wake2Alfa6ConvergenceParityTests.cpp
// Convergence parity proof: the canonical Fortran case
// polar/polar-wake2-alfa6-naca0012 (NACA 0012, PANE, OPER, VISC 1e6,
// alfa 4 with default wake, VPAR WAKE 2.0, alfa 6) is reproduced in
// production. The stdout must be byte-identical to the reference
// except for the established unzoom blank class (Fortran
// ' U unzoom | ' with a trailing space, C++ without)
// Additionally: no VISCAL non-convergence message; the
// iteration sequence of the alfa-6 point carries the Fortran values
// (iteration 5 rms 0.3146E-04, Cd 0.00973) and the FORMAT-9922
// completion block.
//
// No golden change: the reference stdout is read unchanged;
// the normalizations are the documented unzoom class
// and the branding class (product banner), each with counted
// replacement.

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>

#include "TestSupport.h"
#include "LegacyDefaults.h"
#include "app/ProductInfo.h"

namespace {

const char* const kCommandDir =
    "tests/fortran-reference/cases/polar/polar-wake2-alfa6-naca0012";
const char* const kReferenceDir =
    "tests/data/xfoil-6.996/s15/regression/polar-wake2-alfa6-naca0012";

bool readBytes(const std::string& path, std::string& out) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    return false;
  }
  std::ostringstream buffer;
  buffer << stream.rdbuf();
  out = buffer.str();
  return true;
}

void reportFailure(const std::string& message) {
  testing::reportFailure(__FILE__, __LINE__, message);
}

int countOccurrences(const std::string& haystack, const std::string& needle) {
  int count = 0;
  std::string::size_type pos = 0;
  while ((pos = haystack.find(needle, pos)) != std::string::npos) {
    ++count;
    pos += needle.size();
  }
  return count;
}

// established unzoom blank class: Fortran writes the
// plot frame line with a trailing space, C++ without.
// Exactly one replacement is counted and expected.
int normalizeUnzoomClass(std::string& text) {
  const std::string from = "Unzoom  | \n";
  const std::string to = "Unzoom  |\n";
  int count = 0;
  std::string::size_type pos = 0;
  while ((pos = text.find(from, pos)) != std::string::npos) {
    text.replace(pos, from.size(), to);
    pos += to.size();
    ++count;
  }
  return count;
}

// branding class: the C++ product banner carries XFOIL.next
// (product name and product version from the central source
// app/ProductInfo.h) plus the original XFOIL attribution; the canonical
// Fortran banner of the reference stays unchanged. Exactly this
// documented branding region is normalized with counted
// replacements. All numerical and domain stdout regions
// remain pinned byte-exact; a deviation outside the banners
// make the comparison fail further on.
int normalizeProductBannerClass(std::string& text) {
  const std::string productBanner =
      std::string{"\n ===================================================\n "} +
      std::string{xfoil::app::product::name} + " Version " +
      std::string{xfoil::app::product::version} +
      "\n Copyright (C) 2026 Olaf Frommann\n"
      "\n Based upon XFOIL Version 6.996\n"
      " Copyright (C) 2000   Mark Drela, Harold Youngren\n"
      "\n This software comes with ABSOLUTELY NO WARRANTY,\n"
      "   subject to the GNU General Public License.\n"
      "\n Caveat computor\n"
      " ===================================================\n";
  const std::string fortranBanner =
      "\n ===================================================\n"
      "  XFOIL Version 6.99\n"
      "  Copyright (C) 2000   Mark Drela, Harold Youngren\n"
      "\n"
      "  This software comes with ABSOLUTELY NO WARRANTY,\n"
      "    subject to the GNU General Public License.\n"
      "\n"
      "  Caveat computor\n"
      " ===================================================\n";
  int count = 0;
  std::string::size_type pos = 0;
  while ((pos = text.find(productBanner, pos)) != std::string::npos) {
    text.replace(pos, productBanner.size(), fortranBanner);
    pos += fortranBanner.size();
    ++count;
  }
  return count;
}

}  // namespace

TEST(regression, Wake2Alfa6ConvergenceParityMatchesFortranReference) {
  const char* configuredPath = std::getenv("XFOIL_CLI_PATH");
  const std::string program =
      configuredPath != nullptr ? configuredPath : "build/debug/xfoil.next";

 // Own, unique test directory per run; the
  // case artifacts lie collision-free and are removed by RAII.
  const testing::ScopedTempDirectory work("wake2-alfa6-parity");
  const std::string workDir = work.path().string();
  std::error_code error;

  std::filesystem::copy(std::filesystem::path(kCommandDir) / "commands.in",
                        std::filesystem::path(workDir) / "commands.in",
                        std::filesystem::copy_options::overwrite_existing,
                        error);
  testing::writeCommandsWithXfoil699Defaults(
      workDir, std::filesystem::path(workDir) / "commands.in",
      std::filesystem::path(workDir) / "commands-with-old-defaults.in");
  if (error) {
    reportFailure("commands.in not copyable");
    return;
  }

  std::error_code pathError;
  const std::string programPath =
      std::filesystem::absolute(program, pathError).string();
  if (pathError) {
    reportFailure("Program path not determinable: " + program);
    return;
  }
  // PUB-W2: central portable process helper. LC_ALL=C is applied as a
  // POSIX environment prefix and via putenv around the cmd.exe call.
  testing::ProcessInvocation invocation;
  invocation.program = programPath;
  invocation.workingDirectory = workDir;
  invocation.stdinFile = "commands-with-old-defaults.in";
  invocation.stdoutFile = "stdout.txt";
  invocation.stderrFile = "stderr.txt";
  invocation.environment = {{"LC_ALL", "C"}};
  if (testing::runProcess(invocation) == -1) {
    reportFailure("CLI-Run not correct ended");
    return;
  }

  std::string actual;
  std::string expected;
  if (!readBytes(std::string(workDir) + "/stdout.txt", actual)) {
    reportFailure("produced stdout not readable");
    return;
  }
  // PUB-W2: Windows stdout uses CRLF. Normalize before locating the
  // injected RDEF block; the block markers are LF text.
  actual = testing::canonicalizeCapturedText(actual);
  actual = testing::normalizeLegacyDefaultDisplay(
      testing::stripInjectedDefaultsBlock(std::move(actual)));
  if (!readBytes(std::string(kReferenceDir) + "/stdout.raw.txt", expected)) {
    reportFailure("reference stdout not readable");
    return;
  }
  // Captured console output is already CRLF-normalized above.

  const int normalized = normalizeUnzoomClass(expected);
  if (normalized != 1) {
    reportFailure("Expected exactly 1 Unzoom-Classes-Replacement in the "
                  "Reference, found: " +
                  std::to_string(normalized));
  }
  const int bannerNormalized = normalizeProductBannerClass(actual);
  if (bannerNormalized != 1) {
    reportFailure("Expected exactly 1 Branding-Classes-Replacement in the "
                  "generated stdout, found: " +
                  std::to_string(bannerNormalized));
  }
  if (actual != expected) {
    reportFailure("stdout not byte-identical after unzoom normalization "
                  "(produces " +
                  std::to_string(actual.size()) + " Bytes, Reference " +
                  std::to_string(expected.size()) + " Bytes)");
  }

  // convergence contract of the primary case: no non-convergence,
  // iteration sequence and completion block of the alfa-6 point carry the
  // canonical Fortran values.
  if (countOccurrences(actual, " VISCAL:  Convergence failed") != 0) {
    reportFailure("VISCAL non-convergence message in the B3 case");
  }
  if (actual.find("   5   rms: 0.2908E-04   max: -.5430E-03   D at   32  1") ==
      std::string::npos) {
    reportFailure("ALFA-6 iteration-5 line missing or differs");
  }
  if (actual.find("      Cm = -0.0043     CD =  0.00973   =>   CDf =  0.00578"
                  "    CDp =  0.00395") == std::string::npos) {
    reportFailure("ALFA-6 final load line missing or differs");
  }
  if (actual.find("      9.000    3.8780   0.009728") ==
      std::string::npos) {
    reportFailure("FORMAT-9922-Completion block missing");
  }
}

// End: tests/regression/Wake2Alfa6ConvergenceParityTests.cpp
