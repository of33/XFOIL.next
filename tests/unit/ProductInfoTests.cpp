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

// file: tests/unit/ProductInfoTests.cpp
// Contract and source-scan tests for the central product source
// src/app/ProductInfo.h. The source scan runs against the
// working tree of the repository root directory; the test suite is
// started from the project root, so the root paths are relative.

#include "TestSupport.h"
#include "app/ProductInfo.h"
#include "xfoil/core/polar/PolarTypes.h"
#include "xfoil/io/PolarText.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

bool readWholeFile(const std::filesystem::path& path, std::string& text) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }
  std::ostringstream buffer;
  buffer << file.rdbuf();
  text = buffer.str();
  return true;
}

bool isSourceFile(const std::filesystem::path& path) {
  const std::string extension = path.extension().string();
  return extension == ".cpp" || extension == ".h";
}

std::vector<std::filesystem::path> collectSourceFiles(
    const std::filesystem::path& root) {
  std::vector<std::filesystem::path> files;
  std::error_code error;
  if (!std::filesystem::is_directory(root, error)) {
    return files;
  }
  for (const auto& entry :
       std::filesystem::recursive_directory_iterator(root)) {
    if (entry.is_regular_file() && isSourceFile(entry.path())) {
      files.push_back(entry.path());
    }
  }
  return files;
}

const char* const kProductSourceHeader = "src/app/ProductInfo.h";

}  // namespace

TEST(unit, ProductInfoValuesMatchContract) {
  CHECK_EQ(std::string{xfoil::app::product::name}, std::string{"XFOIL.next"});
  CHECK_EQ(std::string{xfoil::app::product::version}, std::string{"1.0.0"});
  CHECK_EQ(std::string{xfoil::app::product::licenseId},
           std::string{"GPL-3.0-or-later"});
  CHECK_EQ(std::string{xfoil::app::product::projectUrl},
           std::string{"https://github.com/of33/XFOIL.next"});
}

TEST(unit, ProductInfoIsTheOnlyProductIdentitySource) {
  // No production C++ source except ProductInfo.h may carry the
  // product name or the product version as a string literal.
  const std::string nameLiteral = std::string{"\""} +
                                  std::string{xfoil::app::product::name} +
                                  "\"";
  const std::string versionLiteral = std::string{"\""} +
                                     std::string{xfoil::app::product::version} +
                                     "\"";
  const std::vector<std::filesystem::path> roots{"src", "include"};
  std::size_t nameHits = 0;
  std::size_t versionHits = 0;
  for (const std::filesystem::path& root : roots) {
    for (const std::filesystem::path& file : collectSourceFiles(root)) {
      std::string text;
      if (!readWholeFile(file, text)) {
        testing::reportFailure(__FILE__, __LINE__,
                               "Source file not readable: " + file.string());
        continue;
      }
      const bool isProductHeader =
          file.generic_string() == std::string{kProductSourceHeader};
      if (text.find(nameLiteral) != std::string::npos) {
        ++nameHits;
        CHECK(isProductHeader);
      }
      if (text.find(versionLiteral) != std::string::npos) {
        ++versionHits;
        CHECK(isProductHeader);
      }
    }
  }
  CHECK_EQ(nameHits, std::size_t{1});
  CHECK_EQ(versionHits, std::size_t{1});
}

TEST(unit, ProductVersionStaysSeparateFromFileFormatVersions) {
  // the file and protocol versions (polar/dump format 6.99) remain
  // explicitly separate from the product version.
  CHECK_NE(std::string{xfoil::app::product::version}, std::string{"6.99"});
  CHECK_EQ(xfoil::io::PolarWriteRequest{}.version, 6.99);
  CHECK_EQ(xfoil::core::polar::PolarMetadata{}.version, 6.99);
}

// End: tests/unit/ProductInfoTests.cpp
