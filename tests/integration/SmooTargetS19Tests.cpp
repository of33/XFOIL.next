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

// file: tests/integration/SmooTargetS19Tests.cpp
// Focused S19 integration coverage for MDES SMOO KQSP=1.

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
};

SessionResult runSession(const std::string& inputText) {
  std::istringstream input(inputText);
  std::ostringstream output;
  std::ostringstream errorOutput;
  xfoil::app::cli::InteractiveStartupOptions options;
  SessionResult result;
  result.exitCode = xfoil::app::runInteractiveApplication(
      input, output, errorOutput, options);
  result.outputText = output.str();
  return result;
}

std::string readText(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    return {};
  }
  std::ostringstream buffer;
  buffer << stream.rdbuf();
  return buffer.str();
}

bool contains(const std::string& text, const std::string& needle) {
  return text.find(needle) != std::string::npos;
}

}  // namespace

TEST(integration, SmooTargetS19MdesWithoutQdesUsesFirstColumn) {
  testing::ScopedTempDirectory work("smoo-s19-no-qdes", true);
  const auto result =
      runSession("NACA 2412\nMDES\nSMOO\nDUMP smoo-mdes.dat\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(contains(result.outputText,
                 " Qspec 1 :  alpha =   0.0000    CL =   0.267221"
                 "    CM =  -0.056649\n"));
  const std::string dump = readText(work.makePath("smoo-mdes.dat"));
  CHECK(contains(dump,
                 "   0   0.000000  -0.036881   0.630534  -0.065507\n"));
  CHECK(contains(dump,
                 "   1   0.899565  -0.003197   0.658267  -0.029856\n"));
}

TEST(integration, SmooTargetS19AfterQdesUsesFirstColumn) {
  testing::ScopedTempDirectory work("smoo-s19-after-qdes", true);
  const auto result = runSession(
      "NACA 2412\nQDES\n\nMDES\nSMOO\nDUMP smoo-qdes.dat\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(contains(result.outputText,
                 " Qspec 1 :  alpha =  -0.0948    CL =   0.255625"
                 "    CM =  -0.056490\n"));
  const std::string dump = readText(work.makePath("smoo-qdes.dat"));
  CHECK(contains(dump,
                 "   1   0.899560  -0.003182   0.658265  -0.029848\n"));
}
