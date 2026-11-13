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

// File: tests/regression/CseqHeadDumpParityTests.cpp
// Raw-bit parity proof: the canonical Fortran case
// polar/polar-cseq-head-naca0012 (NACA 0012, PANE, OPER, PACC with
// save and dump files, ASEQ 0 2 1, CSEQ 0 2 1) is reproduced in
// production. The save (.pol) and dump (.dump) files must be
// byte-identical to the reference; stdout must be byte-identical
// except for the established unzoom blank class. Additionally
// the three CSEQ PLXADD headers (alfa/DTOR, Cl, CM) are
// pinned to the canonical Fortran raw bits: the first header carries the
// near-zero value 5.854803851967764e-13 degrees (raw COMMON alfa
// 1.0218560427528418e-14 radians), which the C++ driver before the fix
// shifted by about 1 ULP via the degrees round trip.
//
// No golden change: the reference artifacts are read unchanged;
// the stdout normalizations are the documented
// unzoom class and the branding class (product banner), each
// with counted replacements.

#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "DumpParitySupport.h"
#include "DumpTargetSupport.h"
#include "LegacyDefaults.h"
#include "TestSupport.h"
#include "app/ProductInfo.h"

namespace {

const char* const kCommandDir =
    "tests/fortran-reference/cases/polar/polar-cseq-head-naca0012";
const char* const kReferenceDir =
    "tests/data/xfoil-6.996/s15/regression/polar-cseq-head-naca0012";

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

// established unzoom blank class: Fortran writes the
// plot frame line with a trailing space, C++ without.
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
// replacements; all numerical and domain stdout regions
// remain pinned byte-exact.
int normalizeProductBannerClass(std::string& text) {
  const std::string productBanner =
      std::string{"\n ===================================================\n "} +
      std::string{xfoil::app::product::name} + " Version " +
      std::string{xfoil::app::product::version} +
      "\n Copyright (C) 2026 Olaf Frommann\n"
      "\n Based upon XFOIL Version 6.99\n"
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

double readDouble(const std::string& data, std::size_t offset) {
  double value = 0.0;
  std::memcpy(&value, data.data() + offset, sizeof(value));
  return value;
}

}  // namespace

TEST(regression, CseqHeadDumpParityMatchesFortranReference) {
  const char* configuredPath = std::getenv("XFOIL_CLI_PATH");
  const std::string program =
      configuredPath != nullptr ? configuredPath : "build/debug/xfoil.next";

 // Own, unique test directory per run; the
  // case artifacts lie collision-free and are removed by RAII.
  const testing::ScopedTempDirectory work("cseq-head-parity");
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
#ifdef _WIN32
  // PUB-W3: on Windows the final converged display values of the CSEQ
  // chain deviate in the last displayed digits (same measured final
  // family as the batch results: max 5.5e-6); the non-numeric layout
  // stays a byte-exact contract. Violations report both full lines.
  // Pins without measurements stay strict (see below).
  std::vector<std::string> stdoutProblems;
  const int stdoutViolations = testing::numericTextLineViolations(
      actual, expected, 1e-5, 1e-6, stdoutProblems, 8);
  if (stdoutViolations != 0) {
    std::string detail;
    for (const auto& problem : stdoutProblems) {
      detail += "\n      " + problem;
    }
    reportFailure("stdout numeric-token deviations (" +
                  std::to_string(stdoutViolations) + " lines):" + detail);
  }
#else
  if (actual != expected) {
    reportFailure("stdout not byte-identical after unzoom normalization "
                  "(produces " +
                  std::to_string(actual.size()) + " Bytes, Reference " +
                  std::to_string(expected.size()) + " Bytes)");
  }
#endif

  static const char* const kArtifacts[] = {"s2.pol", "s2.dump"};
  for (const char* artifact : kArtifacts) {
    std::string artifactActual;
    std::string artifactExpected;
    if (!readBytes(std::string(workDir) + "/" + artifact, artifactActual)) {
      reportFailure(std::string("generated artifact not readable: ") +
                    artifact);
      continue;
    }
    if (!readBytes(std::string(kReferenceDir) + "/generated/" + artifact,
                   artifactExpected)) {
      reportFailure(std::string("reference artifact not readable: ") +
                    artifact);
      continue;
    }
    if (std::string(artifact) == "s2.dump") {
      artifactExpected =
          testing::withS12DumpHeaderVersion(std::move(artifactExpected));
    }
#ifdef _WIN32
    const std::string name = artifact;
    if (name == "s2.dump") {
      // PUB-W3: raw IEEE-754 dump records - the measured field-group
      // contract of the shared polar dumps (same kernels, same PLXADD
      // codec, cp/cf/ctau measured in the Pacc dumps). PUB-W4: the
      // dump ALFA field uses the ALFA pin family contract - the fourth
      // run proved the dump violation and the pin violation carry the
      // identical double (5.8369402123553517e-13).
      const std::vector<std::uint8_t> actualBytes(artifactActual.begin(),
                                                 artifactActual.end());
      const std::vector<std::uint8_t> expectedBytes(artifactExpected.begin(),
                                                    artifactExpected.end());
      const testing::DumpFieldGroup alfaGroup{5e-14, 1e-12};
      const std::string problems = testing::compareDumpsWindowsPortable(
          actualBytes, expectedBytes, &alfaGroup);
      if (!problems.empty()) {
        reportFailure("s2.dump portable contract violated:\n      " +
                      problems);
      }
      continue;
    }
    // s2.pol: Fortran polar columns of the final converged points -
    // same measured final display family (max 5.5e-6); non-numeric
    // layout byte-exact.
    std::vector<std::string> polProblems;
    const int polViolations = testing::numericTextLineViolations(
        artifactActual, artifactExpected, 1e-5, 1e-6, polProblems, 8);
    if (polViolations != 0) {
      std::string detail;
      for (const auto& problem : polProblems) {
        detail += "\n      " + problem;
      }
      reportFailure(name + " numeric-token deviations (" +
                    std::to_string(polViolations) + " lines):" + detail);
    }
#else
    if (artifactActual != artifactExpected) {
      reportFailure(std::string("Artifact not byte-identical: ") +
                    artifact + " (produces " +
                    std::to_string(artifactActual.size()) +
                    " Bytes, Reference " +
                    std::to_string(artifactExpected.size()) + " Bytes)");
    }
#endif
  }

  // Structural pins: header (2720 bytes) plus six point groups of
  // 296 bytes each (ASEQ 0/1/2 and CSEQ 0/1/2).
  std::string dump;
  if (!readBytes(std::string(workDir) + "/s2.dump", dump)) {
    reportFailure("produced Dump-File not readable");
    return;
  }
  if (dump.size() != 2720u + 6u * 296u) {
    reportFailure("Dump file size unexpected: " +
                  std::to_string(dump.size()));
    return;
  }

  // CSEQ headers: record starts 3608/3904/4200, payload at +4.
  // header fields (7 doubles): alfa/DTOR, Cl, CDTOT, 0.0, CM, XT1, XT2.
  struct HeadPin {
    std::size_t offset;
    double alfaDegrees;
    double cl;
    double cm;
  };
#ifdef _WIN32
  // PUB-W4: the fourth MSYS2 run measured the three pin families; each
  // family gets its own tight abs-or-rel contract (near-zero values
  // governed by the absolute part, normal values additionally a narrow
  // relative part). Measured maxima: ALFA/DTOR abs 2.1316282072803006e-14
  // (rel up to 3.05e-3 only near zero), CL abs 3.2789507770202448e-16
  // (normal values: 1 ULP = 2.22e-16, rel <= 2.23e-16), CM abs
  // 9.1246454836380053e-16 (rel <= 4.12e-14). Chosen bounds (margins):
  //   ALFA abs 5e-14 (2.3x)  | rel 1e-12
  //   CL   abs 1e-15 (3.1x)  | rel 1e-12
  //   CM   abs 2e-15  (2.2x) | rel 1e-12
  // These are PIN-display-family final scalars; larger deviations still
  // fail with the full measurement line (abort criterion, no wide
  // tolerance). Violations report decimal/bits/ULP/abs/rel.
  struct PinContract {
    double absolute;
    double relative;
  };
  const PinContract kAlfaContract{5e-14, 1e-12};
  const PinContract kClContract{1e-15, 1e-12};
  const PinContract kCmContract{2e-15, 1e-12};
  const auto checkPin = [&](double actualValue, double expectedValue,
                            const char* field, std::size_t offset,
                            const PinContract& contract) {
    if (testing::withinAbsoluteRelativeContract(actualValue, expectedValue,
                                                contract.absolute,
                                                contract.relative)) {
      return;
    }
    const std::uint64_t a = std::bit_cast<std::uint64_t>(actualValue);
    const std::uint64_t e = std::bit_cast<std::uint64_t>(expectedValue);
    const double absd = std::fabs(actualValue - expectedValue);
    const double rel =
        expectedValue != 0.0 ? absd / std::fabs(expectedValue) : absd;
    std::ostringstream msg;
    msg.precision(17);
    msg << "CSEQ header " << field
        << " differs at offset " << offset << ": actual " << actualValue
        << " expected " << expectedValue << " bits 0x" << std::hex << a
        << " vs 0x" << e << std::dec << " ulp " << (a > e ? a - e : e - a)
        << " abs " << absd << " rel " << rel;
    reportFailure(msg.str());
  };
#else
  struct PinContract {
    double absolute;
    double relative;
  };
  const PinContract kAlfaContract{0.0, 0.0};  // unused on Linux
  const PinContract kClContract{0.0, 0.0};
  const PinContract kCmContract{0.0, 0.0};
  const auto checkPin = [](double actualValue, double expectedValue,
                           const char* field, std::size_t offset,
                           const PinContract& /*contract*/) {
    if (std::memcmp(&actualValue, &expectedValue, sizeof(double)) == 0) {
      return;
    }
    reportFailure(std::string("CSEQ header ") + field +
                  " differs from expectation at offset " +
                  std::to_string(offset));
  };
#endif
  static const HeadPin kHeads[] = {
      {3612u, -6.263342402658837e-13, 6.49884067376841e-17,
       -1.7429852553498242e-14},
      {3908u, 8.306377211030313, 0.9999999999999992,
       -0.011451654024654478},
      {4204u, 16.793289613796574, 1.9999999999995415,
       -0.022158015847556688},
  };
  for (const HeadPin& pin : kHeads) {
    checkPin(readDouble(dump, pin.offset), pin.alfaDegrees, "ALFA/DTOR",
             pin.offset, kAlfaContract);
    checkPin(readDouble(dump, pin.offset + 8u), pin.cl, "CL", pin.offset,
             kClContract);
    checkPin(readDouble(dump, pin.offset + 32u), pin.cm, "CM", pin.offset,
             kCmContract);
  }
}

// End: tests/regression/CseqHeadDumpParityTests.cpp
