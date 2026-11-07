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

// File: tests/unit/TestSupportTests.cpp
// Registered tests for the test RAII infrastructure
// (testing::ScopedTempDirectory, testing::ScopedWorkingDirectory, and
// the diagnostics variable XFOIL_KEEP_TEST_ARTIFACTS). These tests change
// no product or reference files; they work exclusively inside
// unique temporary directories below the test temp base.

#include "TestSupport.h"

#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace {

// PUB-W1: portable environment write for this test only. POSIX uses
// setenv/unsetenv; Windows (MinGW/UCRT64) uses putenv, where
// "NAME=VALUE" sets and "NAME=" removes the variable (documented
// Windows CRT semantics; the callers never assign an empty value). The
// child-observable semantics of std::getenv afterwards match on both
// platforms.
void setKeepArtifactsEnv(const char* value) {  // nullptr removes it.
#ifdef _WIN32
  const std::string assignment =
      std::string("XFOIL_KEEP_TEST_ARTIFACTS=") + (value != nullptr ? value : "");
  // MinGW declares putenv(const char*); the cast keeps the line
  // compilable under headers that declare it char* (the string is not
  // retained by the Windows CRT).
  static_cast<void>(::putenv(const_cast<char*>(assignment.c_str())));
#else
  if (value != nullptr) {
    ::setenv("XFOIL_KEEP_TEST_ARTIFACTS", value, 1);
  } else {
    ::unsetenv("XFOIL_KEEP_TEST_ARTIFACTS");
  }
#endif
}

// Ensures XFOIL_KEEP_TEST_ARTIFACTS restores the process state after the
// test, even when it exits through an exception, to the value it found.
class ScopedKeepArtifactsEnv {
 public:
  explicit ScopedKeepArtifactsEnv(const char* value)
      : hadPrevious_(std::getenv("XFOIL_KEEP_TEST_ARTIFACTS") != nullptr),
        previousValue_(hadPrevious_ ? std::string{std::getenv("XFOIL_KEEP_TEST_ARTIFACTS")}
                                    : std::string{}) {
    setKeepArtifactsEnv(value);
  }

  ScopedKeepArtifactsEnv(const ScopedKeepArtifactsEnv&) = delete;
  ScopedKeepArtifactsEnv& operator=(const ScopedKeepArtifactsEnv&) = delete;

  ~ScopedKeepArtifactsEnv() {
    setKeepArtifactsEnv(hadPrevious_ ? previousValue_.c_str() : nullptr);
  }

 private:
  bool hadPrevious_;
  std::string previousValue_;
};

}  // namespace

TEST(unit, TestSupportScopedTempDirectoriesAreDistinct) {
  testing::ScopedTempDirectory first{"distinct-a"};
  testing::ScopedTempDirectory second{"distinct-b"};
  testing::ScopedTempDirectory sameTagA{"distinct-same"};
  testing::ScopedTempDirectory sameTagB{"distinct-same"};
  CHECK_NE(first.path(), second.path());
  CHECK_NE(sameTagA.path(), sameTagB.path());
  CHECK(std::filesystem::exists(first.path()));
  CHECK(std::filesystem::exists(second.path()));
  CHECK(std::filesystem::exists(sameTagA.path()));
  CHECK(std::filesystem::exists(sameTagB.path()));
}

TEST(unit, TestSupportScopedTempDirectoryCreatedAndRemoved) {
  std::filesystem::path created;
  {
    testing::ScopedTempDirectory scope{"created-and-removed"};
    created = scope.path();
    CHECK(std::filesystem::is_directory(created));
    const std::filesystem::path inner = scope.makePath("inner.txt");
    std::filesystem::create_directories(scope.makePath("sub"));
    CHECK(std::filesystem::is_directory(scope.makePath("sub")));
    CHECK_EQ(inner.filename().string(), std::string{"inner.txt"});
  }
  CHECK_FALSE(std::filesystem::exists(created));
}

TEST(unit, TestSupportScopedTempDirectoryEntersAndRestoresWorkingDirectory) {
  const std::filesystem::path before = std::filesystem::current_path();
  {
    testing::ScopedTempDirectory scope{"enter-restore", true};
    CHECK(std::filesystem::equivalent(std::filesystem::current_path(),
                                      scope.path()));
  }
  CHECK_EQ(std::filesystem::current_path(), before);
}

TEST(unit, TestSupportScopedTempDirectoryRestoresWorkingDirectoryOnException) {
  const std::filesystem::path before = std::filesystem::current_path();
  std::filesystem::path inner;
  try {
    testing::ScopedTempDirectory scope{"enter-exception", true};
    inner = scope.path();
    CHECK(std::filesystem::equivalent(std::filesystem::current_path(), inner));
    throw std::runtime_error{"testbody exception"};
  } catch (const std::exception&) {
    // the RAII destructor must restore the working directory even in the
    // exception case.
  }
  CHECK_EQ(std::filesystem::current_path(), before);
  CHECK_FALSE(std::filesystem::exists(inner));
}

TEST(unit, TestSupportScopedTempDirectoryTagIsSanitized) {
  testing::ScopedTempDirectory scope{"we/ird*tag and../points"};
  const std::string leaf = scope.path().filename().string();
  CHECK(leaf.find('/') == std::string::npos);
  CHECK(leaf.find(' ') == std::string::npos);
  CHECK(leaf.find('*') == std::string::npos);
  CHECK(leaf.find("we_ird_tag") != std::string::npos);
  CHECK(leaf.front() != '.');
  CHECK(std::filesystem::exists(scope.path()));
}

TEST(unit, TestSupportScopedTempDirectoryCustomBaseIsUsed) {
  const std::filesystem::path base =
      testing::testTempBasePath() / "xfoil-testsupporttests-custom-base";
  std::error_code ignored;
  std::filesystem::remove_all(base, ignored);
  {
    testing::ScopedTempDirectory scope{"custom-base", base, false};
    CHECK(std::filesystem::exists(scope.path()));
    CHECK_EQ(scope.path().parent_path(), base);
  }
  std::filesystem::remove_all(base, ignored);
  CHECK_FALSE(std::filesystem::exists(base));
}

TEST(unit, TestSupportScopedWorkingDirectoryRestoresOnNormalExit) {
  testing::ScopedTempDirectory host{"swd-normal"};
  const std::filesystem::path before = std::filesystem::current_path();
  {
    testing::ScopedWorkingDirectory enter{host.path()};
    CHECK(std::filesystem::equivalent(std::filesystem::current_path(),
                                      host.path()));
  }
  CHECK_EQ(std::filesystem::current_path(), before);
}

TEST(unit, TestSupportScopedWorkingDirectoryRestoresOnException) {
  testing::ScopedTempDirectory host{"swd-exception"};
  const std::filesystem::path before = std::filesystem::current_path();
  try {
    testing::ScopedWorkingDirectory enter{host.path()};
    CHECK(std::filesystem::equivalent(std::filesystem::current_path(),
                                      host.path()));
    throw std::runtime_error{"swd exception"};
  } catch (const std::exception&) {
    // the guard destructor must restore the previous working directory.
  }
  CHECK_EQ(std::filesystem::current_path(), before);
}

TEST(unit, TestSupportScopedWorkingDirectoryMissingTargetThrows) {
  const std::filesystem::path before = std::filesystem::current_path();
  const std::filesystem::path missing =
      testing::testTempBasePath() /
      "xfoil-testsupporttests-does-not-exist-target";
  std::error_code ignored;
  std::filesystem::remove_all(missing, ignored);
  bool threw = false;
  try {
    testing::ScopedWorkingDirectory enter{missing};
  } catch (const std::exception&) {
    threw = true;
  }
  CHECK(threw);
  CHECK_EQ(std::filesystem::current_path(), before);
}

TEST(unit, TestSupportKeepArtifactsEnvControlsCleanupAndLeavesNoEnvState) {
  const bool envAbsentBefore =
      std::getenv("XFOIL_KEEP_TEST_ARTIFACTS") == nullptr;

  std::filesystem::path kept;
  {
    ScopedKeepArtifactsEnv env{"1"};
    CHECK(testing::keepTestArtifactsEnabled());
    testing::ScopedTempDirectory scope{"keep-enabled"};
    kept = scope.path();
    CHECK(std::filesystem::exists(kept));
  }
  CHECK(std::filesystem::exists(kept));
  std::error_code ignored;
  std::filesystem::remove_all(kept, ignored);
  CHECK_FALSE(std::filesystem::exists(kept));

  std::filesystem::path removed;
  {
    ScopedKeepArtifactsEnv env{"0"};
    CHECK_FALSE(testing::keepTestArtifactsEnabled());
    testing::ScopedTempDirectory scope{"keep-disabled"};
    removed = scope.path();
  }
  CHECK_FALSE(std::filesystem::exists(removed));

  {
    ScopedKeepArtifactsEnv env{nullptr};
    CHECK_FALSE(testing::keepTestArtifactsEnabled());
  }
  const bool envAbsentAfter =
      std::getenv("XFOIL_KEEP_TEST_ARTIFACTS") == nullptr;
  CHECK_EQ(envAbsentAfter, envAbsentBefore);
}

// PUB-W4: focused contract of the Windows numeric token comparison.
TEST(unit, TestSupportNumericTextTokenComparisonContract) {
  std::vector<std::string> problems;
  // Signed zero: -0.0000 vs 0.0000 is the same value.
  CHECK_EQ(testing::numericTextLineViolations(
               "line  cl =  -0.0000  cm =   0.0000",
               "line  cl =   0.0000  cm =   0.0000", 1e-5, 1e-6, problems, 4),
           0);
  // A truly different value outside the contract is rejected.
  problems.clear();
  CHECK_EQ(testing::numericTextLineViolations("x 0.5000", "x 0.5010", 1e-5,
                                              1e-6, problems, 4),
           1);
  CHECK(!problems.empty());
  // Within the absolute bound passes.
  problems.clear();
  CHECK_EQ(testing::numericTextLineViolations("x 0.5000001", "x 0.5000002",
                                              1e-5, 1e-6, problems, 4),
           0);
  // Non-numeric token differences outside the numeric tokens are
  // rejected.
  problems.clear();
  CHECK_EQ(testing::numericTextLineViolations("a -0.0000 z", "a 0.0000 y",
                                              1e-5, 1e-6, problems, 4),
           1);
  // NaN/Inf spellings and overflow spellings are never numeric tokens:
  // they must differ-as-non-numeric, never silently compare equal.
  problems.clear();
  CHECK_EQ(testing::numericTextLineViolations("v nan", "v 0.0", 1e-5, 1e-6,
                                              problems, 4),
           1);
  problems.clear();
  CHECK_EQ(testing::numericTextLineViolations("v inf", "v nan", 1e-5, 1e-6,
                                              problems, 4),
           1);
  problems.clear();
  CHECK_EQ(testing::numericTextLineViolations("v 1.0E999", "v 2.0E999", 1e-5,
                                              1e-6, problems, 4),
           1);
  // Token count must stay exact.
  problems.clear();
  CHECK_EQ(testing::numericTextLineViolations("a b", "a b c", 1e-5, 1e-6,
                                              problems, 4),
           1);
  // Line count must stay exact.
  problems.clear();
  CHECK_EQ(testing::numericTextLineViolations("a\n", "a", 1e-5, 1e-6, problems,
                                              4),
           1);
  // A byte-identical line passes even without any numeric parse.
  problems.clear();
  CHECK_EQ(testing::numericTextLineViolations("OFS: alfa =   0.0000",
                                              "OFS: alfa =   0.0000", 1e-5,
                                              1e-6, problems, 4),
           0);
}

// End: tests/unit/TestSupportTests.cpp
