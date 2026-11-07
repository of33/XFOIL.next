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

// File: tests/unit/PortableBlFilenamesTests.cpp
// PUB-W1: stored reference artifacts must be unpackable on Windows.
// The XFOIL 6.99 VISCAL side-effect file is therefore stored under the
// portable name 100.bl, and no stored file name may contain a colon.
// The harness runner may map exactly this one known name; the guard
// tests below pin the mapping to that single exact case.

#include "TestSupport.h"

#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>

namespace {

const char* const kCasesRoot = "tests/fortran-reference/cases";

std::string readFileText(const std::string& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    return {};
  }
  std::ostringstream buffer;
  buffer << stream.rdbuf();
  return buffer.str();
}

std::size_t countSubstring(const std::string& text, const std::string& needle) {
  std::size_t count = 0;
  std::string::size_type pos = 0;
  while ((pos = text.find(needle, pos)) != std::string::npos) {
    ++count;
    pos += needle.size();
  }
  return count;
}

}  // namespace

TEST(unit, StoredReferenceArtifactsHaveNoColonNames) {
  std::error_code error;
  std::size_t colonNames = 0;
  std::size_t portableBlNames = 0;
  for (std::filesystem::recursive_directory_iterator it(kCasesRoot, error), end;
       it != end; it.increment(error)) {
    if (error) {
      break;
    }
    if (!it->is_regular_file(error)) {
      continue;
    }
    const std::string name = it->path().filename().string();
    if (name.find(':') != std::string::npos) {
      ++colonNames;
    }
    if (name == "100.bl") {
      ++portableBlNames;
    }
  }
  CHECK_EQ(colonNames, std::size_t(0));
  // PUB-W1 recorded 28 stored VISCAL side-effect files.
  CHECK_EQ(portableBlNames, std::size_t(28));
}

TEST(unit, ArtifactInventoriesListOnlyExistingFiles) {
  // Every artifacts.sha256 entry must exist on disk (this is what a
  // Windows checkout unpacks); the inventories are the release and
  // reference contract for the portable names.
  std::error_code error;
  std::size_t inventories = 0;
  std::size_t missing = 0;
  std::size_t listedColon = 0;
  for (std::filesystem::recursive_directory_iterator it(kCasesRoot, error), end;
       it != end; it.increment(error)) {
    if (error) {
      break;
    }
    if (!it->is_regular_file(error) ||
        it->path().filename().string() != "artifacts.sha256") {
      continue;
    }
    ++inventories;
    std::ifstream stream(it->path());
    std::string line;
    while (std::getline(stream, line)) {
      const std::string::size_type sep = line.find("  ");
      if (sep == std::string::npos || line.size() - sep - 2 <= 1) {
        testing::reportFailure(__FILE__, __LINE__,
                               "malformed inventory line in " +
                                   it->path().string());
        continue;
      }
      const std::string rel = line.substr(sep + 2);
      if (rel.find(':') != std::string::npos) {
        ++listedColon;
      }
      std::error_code existsError;
      if (!std::filesystem::is_regular_file(it->path().parent_path() / rel,
                                            existsError)) {
        ++missing;
        testing::reportFailure(
            __FILE__, __LINE__,
            "inventory entry missing on disk: " + it->path().string() +
                " -> " + rel);
      }
    }
  }
  // All 236 cases carry an inventory.
  CHECK_EQ(inventories, std::size_t(236));
  CHECK_EQ(listedColon, std::size_t(0));
  CHECK_EQ(missing, std::size_t(0));
}

TEST(unit, HarnessNormalizerMapsOnlyTheKnownViscalName) {
  // tools/fortran-ref.sh must contain exactly one name-mapping rule,
  // the exact ':00.bl' -> '100.bl' assignment, and no generic colon
  // rewriting.
  const std::string script = readFileText("tools/fortran-ref.sh");
  CHECK(!script.empty());
  const std::string exactRule =
      "if [[ \"${rel}\" == ':00.bl' ]]; then rel='100.bl'; fi";
  CHECK_EQ(countSubstring(script, exactRule), std::size_t(1));
  // No other code line may map a produced name.
  CHECK_EQ(countSubstring(script, "rel='100.bl'"), std::size_t(1));
  // No generic colon-substitution machinery.
  CHECK_EQ(countSubstring(script, "tr ':'"), std::size_t(0));
  CHECK_EQ(countSubstring(script, "sed 's/:"), std::size_t(0));
}

// End: tests/unit/PortableBlFilenamesTests.cpp
