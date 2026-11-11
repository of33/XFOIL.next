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

// File: tests/TestSupport.cpp
// Implementation of the small test support.

#include "TestSupport.h"

#include "app/cli/CoreGeometryHandlers.h"
#include "xfoil/session/SessionState.h"

#include <unistd.h>
#ifndef _WIN32
#include <sys/wait.h>
#endif

#include <atomic>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <utility>

namespace testing {
namespace {

std::string currentTestNameStorage;
std::string programNameStorage = "xfoil-test";
int g_failureCount = 0;
std::atomic<unsigned long long> sequenceCounter{0};
constexpr std::size_t kMaxTagLength = 80;
constexpr std::size_t kMaxBinaryLength = 32;

// Keeps only filesystem-friendly characters from a test name.
std::string sanitizePathComponent(const std::string& text,
                                  std::size_t maxLength) {
  std::string sanitized;
  sanitized.reserve(text.size());
  for (const char raw : text) {
    const unsigned char ch = static_cast<unsigned char>(raw);
    const bool allowed = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                         (ch >= '0' && ch <= '9') || ch == '.' || ch == '_' ||
                         ch == '-';
    sanitized.push_back(allowed ? static_cast<char>(ch) : '_');
  }
  if (sanitized.size() > maxLength) {
    sanitized.resize(maxLength);
  }
  while (!sanitized.empty() && sanitized.front() == '.') {
    sanitized.erase(sanitized.begin());
  }
  if (sanitized.empty()) {
    sanitized = "test";
  }
  return sanitized;
}

}  // namespace

std::vector<TestCase>& registry() {
  static std::vector<TestCase> tests;
  return tests;
}

void handleLegacyNacaCore(xfoil::session::SessionComposition& session,
                          std::ostream& out, int designation) {
  xfoil::app::cli::handleNacaCore(session, out, designation, false, 123);
}

Registrar::Registrar(const char* name, TestFunction function) {
  registry().push_back(TestCase{name, function});
}

void reportFailure(const char* file, int line, const std::string& detail) {
  ++g_failureCount;
  std::cout << "      FAILED  (test: " << currentTestNameStorage << ")\n"
            << "        file:    " << file << "\n"
            << "        line:    " << line << "\n"
            << "        " << detail << std::endl;
}

void reportRawFailure(const std::string& detail) {
  ++g_failureCount;
  std::cout << "      FAILED  (test: " << currentTestNameStorage << ")\n"
            << "        " << detail << std::endl;
}

int failureCount() { return g_failureCount; }

std::string currentTestName() { return currentTestNameStorage; }

std::filesystem::path testTempBasePath() {
  return std::filesystem::temp_directory_path();
}

bool keepTestArtifactsEnabled() {
  const char* const value = std::getenv("XFOIL_KEEP_TEST_ARTIFACTS");
  return value != nullptr && *value != '\0' && std::string_view{value} != "0";
}

ScopedTempDirectory::ScopedTempDirectory(std::string tag,
                                         bool enterWorkingDirectory)
    : ScopedTempDirectory(std::move(tag), testTempBasePath(),
                          enterWorkingDirectory) {}

ScopedTempDirectory::ScopedTempDirectory(std::string tag,
                                         const std::filesystem::path& base,
                                         bool enterWorkingDirectory) {
  if (tag.empty()) {
    tag = currentTestName();
  }
  const std::string binary = sanitizePathComponent(programNameStorage,
                                                   kMaxBinaryLength);
  const std::string name = sanitizePathComponent(tag, kMaxTagLength);
  const std::string pidPart =
      std::to_string(static_cast<long long>(static_cast<int>(::getpid())));

  std::error_code error;
  std::filesystem::create_directories(base, error);
  if (error) {
    throw std::system_error(error,
                            "test base directory not creatable: " +
                                base.string());
  }
  bool created = false;
  for (unsigned attempt = 0; attempt < 16U && !created; ++attempt) {
    const unsigned long long sequence = sequenceCounter.fetch_add(1ULL) + 1ULL;
    path_ = base / ("xfoil-test-" + binary + "-" + name + "-" + pidPart +
                    "-" + std::to_string(sequence));
    std::error_code existsError;
    if (std::filesystem::exists(path_, existsError)) {
      continue;
    }
    std::filesystem::create_directories(path_, error);
    if (!error && std::filesystem::is_directory(path_)) {
      created = true;
    }
  }
  if (!created) {
    throw std::runtime_error("Test working directory not creatable: " +
                             path_.string());
  }
  if (enterWorkingDirectory) {
    enter();
  }
}

void ScopedTempDirectory::enter() {
  std::error_code error;
  previousWorkingDirectory_ = std::filesystem::current_path(error);
  if (error) {
    throw std::system_error(error, "current working directory not readable");
  }
  std::filesystem::current_path(path_, error);
  if (error) {
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
    throw std::system_error(error,
                            "Test working directory not accessible: " +
                                path_.string());
  }
  entered_ = true;
}

ScopedTempDirectory::~ScopedTempDirectory() {
  try {
    if (entered_) {
      std::error_code error;
      std::filesystem::current_path(previousWorkingDirectory_, error);
    }
    if (keepTestArtifactsEnabled()) {
      std::cout << "      kept test directory: " << path_.string() << "\n";
      return;
    }
    std::error_code error;
    std::filesystem::remove_all(path_, error);
  } catch (const std::exception&) {
    // the destructor may not throw.
  } catch (...) {
    // the destructor may not throw.
  }
}

const std::filesystem::path& ScopedTempDirectory::path() const noexcept {
  return path_;
}

std::filesystem::path ScopedTempDirectory::makePath(
    const std::string& relativeName) const {
  return path_ / relativeName;
}

ScopedWorkingDirectory::ScopedWorkingDirectory(
    const std::filesystem::path& directory) {
  std::error_code error;
  previousWorkingDirectory_ = std::filesystem::current_path(error);
  if (error) {
    throw std::system_error(error, "current working directory not readable");
  }
  std::filesystem::current_path(directory, error);
  if (error) {
    previousWorkingDirectory_.clear();
    throw std::system_error(error,
                            "Working directory not accessible: " +
                                directory.string());
  }
  entered_ = true;
}

ScopedWorkingDirectory::~ScopedWorkingDirectory() {
  try {
    if (entered_) {
      std::error_code error;
      std::filesystem::current_path(previousWorkingDirectory_, error);
    }
  } catch (const std::exception&) {
    // the destructor may not throw.
  } catch (...) {
    // the destructor may not throw.
  }
}

void checkBool(bool value, bool expectedValue, const char* expressionText,
               const char* file, int line) {
  if (value != expectedValue) {
    reportFailure(file, line,
                  std::string(expectedValue ? "CHECK(" : "CHECK_FALSE(") +
                      expressionText + ")" +
                      "\n      expected: " + displayValue(expectedValue) +
                      "\n      actual:   " + displayValue(value));
  }
}

void checkNear(double actual, double expected, double absoluteTolerance,
               double relativeTolerance, const char* actualExpr,
               const char* expectedExpr, const char* file, int line) {
  const double difference = std::fabs(actual - expected);
  const double limit = absoluteTolerance + relativeTolerance * std::fabs(expected);
  if (!(difference <= limit)) {
    reportFailure(file, line,
                  std::string("CHECK_NEAR(") + actualExpr + ", " + expectedExpr +
                      ", " + displayValue(absoluteTolerance) + ", " +
                      displayValue(relativeTolerance) + ")" +
                      "\n      expected: " + displayValue(expected) +
                      "\n      actual:   " + displayValue(actual) +
                      "\n      |diff|:   " + displayValue(difference) +
                      " > limit " + displayValue(limit));
  }
}

// --- PUB-W1: platform-specific numeric comparison contract ------------
// See the header for the class documentation, the measured bounds and
// the provenance of every tolerance. On all non-Windows platforms this
// reproduces the historic exact comparisons; tolerances are only ever
// evaluated under _WIN32.

bool withinAbsoluteRelativeContract(double actual, double expected,
                                    double absoluteBound, double relativeBound) {
  if (!std::isfinite(actual) || !std::isfinite(expected)) {
    return false;  // NaN/Inf is never inside a portable contract.
  }
  const double difference = std::fabs(actual - expected);
  return difference <= absoluteBound ||
         difference <= relativeBound * std::fabs(expected);
}

std::uint64_t portableUlpDistance(double a, double b) {
  const std::uint64_t bitsA = std::bit_cast<std::uint64_t>(a);
  const std::uint64_t bitsB = std::bit_cast<std::uint64_t>(b);
  if (bitsA == bitsB) {
    return 0;
  }
  const bool zeroPair = a == 0.0 && b == 0.0;
  if (zeroPair) {
    return 0;  // +0.0 and -0.0 are one point on the value line.
  }
  const bool signA = (bitsA >> 63) != 0U;
  const bool signB = (bitsB >> 63) != 0U;
  const bool nanInvolved = std::isnan(a) || std::isnan(b);
  if (signA != signB || nanInvolved) {
    return std::numeric_limits<std::uint64_t>::max();
  }
  return bitsA > bitsB ? bitsA - bitsB : bitsB - bitsA;
}

namespace {

#ifdef _WIN32
constexpr std::uint64_t kPortableUlpBudget128 = 128;
constexpr std::uint64_t kPortableUlpBudget384 = 384;
constexpr std::uint64_t kPortableUlpBudget768 = 768;
constexpr std::uint64_t kPortableUlpBudget1024 = 1024;
constexpr double kPortableReplayAbs = 1e-14;
constexpr double kPortableReplayRel = 1e-13;
constexpr double kPortableNearZeroAbs = 1e-15;
constexpr double kPortableDijAbs = 1e-8;
constexpr double kPortableDijRel = 1e-6;
// PUB-W3 measured class-specific abs/rel bounds (see the header table).
constexpr double kPortableVelsAbs = 1e-14;
constexpr double kPortableVelsRel = 2e-10;
constexpr double kPortableFmomAbs = 5e-15;
constexpr double kPortableFmomRel = 1e-12;
constexpr double kPortableCpiAbs = 3e-13;
constexpr double kPortableCpiRel = 1e-11;
#endif

constexpr bool portableClassUsesBitwiseExactness(PortableNumericClass cls) {
  return cls == PortableNumericClass::kExactBits ||
         cls == PortableNumericClass::kReplayValue ||
         cls == PortableNumericClass::kUlpBudget128 ||
         cls == PortableNumericClass::kUlpBudget384 ||
         cls == PortableNumericClass::kUlpBudget768 ||
         cls == PortableNumericClass::kVelsReferenceValue ||
         cls == PortableNumericClass::kFmomReferenceValue ||
         cls == PortableNumericClass::kCpiReferenceValue;
}

}  // namespace

bool withinPortableNumericContract(double actual, double expected,
                                   PortableNumericClass cls) {
  if (std::bit_cast<std::uint64_t>(actual) ==
      std::bit_cast<std::uint64_t>(expected)) {
    return true;  // a bitwise-identical pair always satisfies any class.
  }
  if (kPortableNumericExactPlatform) {
    // Historic per-test Linux contract: bitwise classes compare bits,
    // value classes compare with == (which also accepts the -0.0/0.0
    // pair, as CHECK_EQ and CHECK_NEAR(...,0,0) did).
    if (portableClassUsesBitwiseExactness(cls)) {
      return false;  // bits differ (checked above).
    }
    return actual == expected;
  }
#ifdef _WIN32
  if (std::isnan(actual) || std::isnan(expected)) {
    return false;  // NaN never passes a tolerance branch.
  }
  switch (cls) {
    case PortableNumericClass::kExactBits:
      return false;  // no relaxation for unconditionally bitwise checks.
    case PortableNumericClass::kReplayValue:
      return std::fabs(actual - expected) <=
             kPortableReplayAbs + kPortableReplayRel * std::fabs(expected);
    case PortableNumericClass::kUlpBudget128:
      return portableUlpDistance(actual, expected) <= kPortableUlpBudget128;
    case PortableNumericClass::kUlpBudget384:
      return portableUlpDistance(actual, expected) <= kPortableUlpBudget384;
    case PortableNumericClass::kUlpBudget768:
      return portableUlpDistance(actual, expected) <= kPortableUlpBudget768;
    case PortableNumericClass::kUlpBudget1024:
      return portableUlpDistance(actual, expected) <= kPortableUlpBudget1024;
    case PortableNumericClass::kNearZero1e15:
      return std::fabs(actual - expected) <= kPortableNearZeroAbs;
    case PortableNumericClass::kDijMatrixValue:
      return std::fabs(actual - expected) <=
             kPortableDijAbs + kPortableDijRel * std::fabs(expected);
    case PortableNumericClass::kVelsReferenceValue:
      return withinAbsoluteRelativeContract(actual, expected,
                                            kPortableVelsAbs, kPortableVelsRel);
    case PortableNumericClass::kFmomReferenceValue:
      return withinAbsoluteRelativeContract(actual, expected,
                                            kPortableFmomAbs, kPortableFmomRel);
    case PortableNumericClass::kCpiReferenceValue:
      return withinAbsoluteRelativeContract(actual, expected,
                                            kPortableCpiAbs, kPortableCpiRel);
  }
  return false;
#else
  static_cast<void>(cls);
  return actual == expected;  // unreachable: exact platforms returned above.
#endif
}

void checkPortableNumeric(double actual, double expected,
                          PortableNumericClass cls, const char* actualExpr,
                          const char* expectedExpr, const char* file,
                          int line) {
  if (withinPortableNumericContract(actual, expected, cls)) {
    return;
  }
  std::ostringstream detail;
  detail.precision(17);
  const double absoluteDifference = std::fabs(actual - expected);
  const double relativeDifference =
      expected != 0.0 ? absoluteDifference / std::fabs(expected) : absoluteDifference;
  detail << "PORTABLE-NUMERIC(" << actualExpr << ", " << expectedExpr
         << ")"
         << "\n      expected: " << expected
         << "\n      actual:   " << actual
         << "\n      |diff|:   " << absoluteDifference
         << "\n      rel:      " << relativeDifference
         << "\n      expected bits: 0x" << std::hex
         << std::bit_cast<std::uint64_t>(expected) << "  actual bits: 0x"
         << std::bit_cast<std::uint64_t>(actual) << std::dec
         << "\n      ulp distance: " << portableUlpDistance(actual, expected)
         << "\n      class: "
         << static_cast<int>(cls)
         << " (0=exact, 1=replay, 2=ulp128, 3=ulp384, 4=ulp768, "
            "5=ulp1024, 6=nearzero1e-15, 7=dij-matrix, 8=vels-ref, "
            "9=fmom-ref, 10=cpi-ref)";
  reportFailure(file, line, detail.str());
}

int systemExitCode(int status) {
  if (status == -1) {
    return -1;
  }
#ifdef _WIN32
  return status;  // UCRT64 system(): direct child exit code.
#else
  if (!WIFEXITED(status)) {
    return -1;
  }
  return WEXITSTATUS(status);
#endif
}

// --- PUB-W2: central portable test-process contract -----------------------
// See the header for the rules. The renderers are pure so both platform
// renderings are unit-testable on either platform; only runProcess()
// performs the native execution.

bool resolveProgram(const std::string& program, std::string& resolved) {
  std::error_code error;
  if (std::filesystem::is_regular_file(program, error)) {
    resolved = program;
    return true;
  }
#ifdef _WIN32
  // The build produces xfoil.next.exe; a configured path without the
  // suffix is extended when the suffixed file exists.
  const std::string withSuffix = program + ".exe";
  if (std::filesystem::is_regular_file(withSuffix, error)) {
    resolved = withSuffix;
    return true;
  }
#endif
  static_cast<void>(error);
  return false;
}

namespace {

bool containsDoubleQuote(const std::string& value) {
  return value.find('"') != std::string::npos;
}

std::string posixQuote(const std::string& value) {
  std::string quoted = "'";
  for (const char character : value) {
    if (character == '\'') {
      quoted += "'\\''";
    } else {
      quoted += character;
    }
  }
  quoted += "'";
  return quoted;
}

std::string absoluteOrSelf(const std::string& path) {
  std::error_code error;
  const auto absolutePath = std::filesystem::absolute(path, error);
  if (error) {
    return path;
  }
  return absolutePath.string();
}

// Shared rendering core; the quote style and the environment handling
// differ. environmentIntoCommand: POSIX renders VAR=value prefixes;
// Windows applies them at run time, so they never appear in the command.
std::string renderCommand(const ProcessInvocation& invocation, bool windows) {
  std::string command;
  // cmd.exe /c strips the outermost quote pair of a command string that
  // begins with a quote, which corrupts quoted launch+redirection chains
  // (cmd.exe: "the command ... could not be found"). A parenthesised
  // compound command never starts with a quote and propagates the last
  // command's exit code; POSIX keeps the plain chain.
  if (windows) {
    command += "( ";
  }
  if (!invocation.workingDirectory.empty()) {
    command += "cd ";
    if (windows) {
      command += "/d ";  // allow drive changes under cmd.exe
    }
    command += "\"" + invocation.workingDirectory + "\" && ";
  }
  if (!windows) {
    for (const auto& entry : invocation.environment) {
      command += entry.first + "=" + posixQuote(entry.second) + " ";
    }
  }
  command += "\"" + invocation.program + "\"";
  for (const auto& argument : invocation.arguments) {
    if (windows) {
      command += " \"" + argument + "\"";
    } else {
      command += " " + posixQuote(argument);
    }
  }
  if (!invocation.stdinFile.empty()) {
    command += " < ";
    command += windows ? "\"" + invocation.stdinFile + "\""
                       : posixQuote(invocation.stdinFile);
  }
  if (!invocation.stdoutFile.empty()) {
    command += " > ";
    command += windows ? "\"" + invocation.stdoutFile + "\""
                       : posixQuote(invocation.stdoutFile);
  }
  if (!invocation.stderrFile.empty()) {
    command += " 2> ";
    command += windows ? "\"" + invocation.stderrFile + "\""
                       : posixQuote(invocation.stderrFile);
  }
  if (windows) {
    command += " )";
  }
  return command;
}

}  // namespace

std::string renderPosixCommand(const ProcessInvocation& invocation) {
  return renderCommand(invocation, /*windows=*/false);
}

std::string renderWindowsCommand(const ProcessInvocation& invocation) {
  return renderCommand(invocation, /*windows=*/true);
}

int runProcess(const ProcessInvocation& invocation) {
  const auto reject = [](const std::string& /*reason*/) {
    // Controlled start failure: the caller reports it in its own test
    // context; runProcess stays silent and reports -1.
    return -1;
  };
  std::string program;
  if (!resolveProgram(invocation.program, program)) {
    return reject("program not found: " + invocation.program);
  }
  if (containsDoubleQuote(program)) {
    return reject("double quote in program path");
  }
  ProcessInvocation prepared = invocation;
  prepared.program = absoluteOrSelf(program);
  for (const auto& argument : invocation.arguments) {
    if (containsDoubleQuote(argument)) {
      return reject("double quote in argument");
    }
  }

#ifdef _WIN32
  // Apply the environment for this child and restore it afterwards.
  std::vector<std::pair<std::string, std::string>> restore;
  for (const auto& entry : prepared.environment) {
    const char* previous = std::getenv(entry.first.c_str());
    restore.emplace_back(entry.first,
                         previous != nullptr ? previous : std::string{});
    const std::string assignment = entry.first + "=" + entry.second;
    static_cast<void>(::putenv(const_cast<char*>(assignment.c_str())));
  }
  const int status = std::system(renderWindowsCommand(prepared).c_str());
  for (const auto& entry : restore) {
    const std::string assignment =
        entry.second.empty() ? entry.first + "=" : entry.first + "=" + entry.second;
    static_cast<void>(::putenv(const_cast<char*>(assignment.c_str())));
  }
#else
  const int status = std::system(renderPosixCommand(prepared).c_str());
#endif
  return systemExitCode(status);
}

std::string canonicalizeCapturedText(const std::string& text) {
  std::string result;
  result.reserve(text.size());
  for (std::string::size_type i = 0; i < text.size(); ++i) {
    if (text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n') {
      continue;  // drop the CR of a CRLF pair; keep the LF
    }
    result += text[i];
  }
  return result;
}


namespace {

bool isNumericTextSeparator(char c) { return c == ' ' || c == '\t'; }

std::vector<std::string> splitNumericTextTokens(const std::string& line) {
  std::vector<std::string> tokens;
  std::string::size_type i = 0;
  while (i < line.size()) {
    while (i < line.size() && isNumericTextSeparator(line[i])) {
      ++i;
    }
    const std::string::size_type begin = i;
    while (i < line.size() && !isNumericTextSeparator(line[i])) {
      ++i;
    }
    if (i > begin) {
      tokens.push_back(line.substr(begin, i - begin));
    }
  }
  return tokens;
}

// Parses a whole token as a decimal number. Returns false unless the
// entire token is consumed, contains a digit and yields a finite value
// (NaN/Inf spellings and trailing garbage are never numeric here).
bool parseNumericTextToken(const std::string& token, double& value) {
  if (token.find_first_of("0123456789") == std::string::npos) {
    return false;
  }
  char* end = nullptr;
  const double parsed = std::strtod(token.c_str(), &end);
  if (end == nullptr || *end != '\0' || !std::isfinite(parsed)) {
    return false;
  }
  value = parsed;
  return true;
}

}  // namespace

int numericTextLineViolations(const std::string& actual,
                              const std::string& expected,
                              double absoluteBound, double relativeBound,
                              std::vector<std::string>& messages,
                              std::size_t maxMessages) {
  std::vector<std::string> actualLines;
  std::vector<std::string> expectedLines;
  const auto splitLines = [](const std::string& text,
                             std::vector<std::string>& lines) {
    std::string::size_type begin = 0;
    while (true) {
      const std::string::size_type nl = text.find('\n', begin);
      if (nl == std::string::npos) {
        lines.push_back(text.substr(begin));
        return;
      }
      lines.push_back(text.substr(begin, nl - begin));
      begin = nl + 1;
    }
  };
  splitLines(actual, actualLines);
  splitLines(expected, expectedLines);
  int violations = 0;
  const auto note = [&](const std::string& message) {
    if (messages.size() < maxMessages) {
      messages.push_back(message);
    }
    ++violations;
  };
  if (actualLines.size() != expectedLines.size()) {
    note("line count differs: actual " + std::to_string(actualLines.size()) +
         " expected " + std::to_string(expectedLines.size()));
    return violations;
  }
  for (std::size_t line = 0; line < actualLines.size(); ++line) {
    const std::string& a = actualLines[line];
    const std::string& e = expectedLines[line];
    if (a == e) {
      continue;
    }
    const std::vector<std::string> actualTokens = splitNumericTextTokens(a);
    const std::vector<std::string> expectedTokens = splitNumericTextTokens(e);
    if (actualTokens.size() != expectedTokens.size()) {
      note("line " + std::to_string(line + 1) + ": token count differs: [" +
           a + "] vs [" + e + "]");
      continue;
    }
    for (std::size_t t = 0; t < actualTokens.size(); ++t) {
      if (actualTokens[t] == expectedTokens[t]) {
        continue;
      }
      double actualValue = 0.0;
      double expectedValue = 0.0;
      const bool actualNumeric = parseNumericTextToken(actualTokens[t], actualValue);
      const bool expectedNumeric =
          parseNumericTextToken(expectedTokens[t], expectedValue);
      if (!actualNumeric || !expectedNumeric) {
        note("line " + std::to_string(line + 1) + " token " +
             std::to_string(t) + ": non-numeric difference: actual [" +
             actualTokens[t] + "] expected [" + expectedTokens[t] + "]");
        break;
      }
      // PUB-W4 signed zero: +0.0 == -0.0 (difference of the zeros is
      // zero anyway; the explicit branch documents the contract and
      // covers -0.0 vs 0.0 spellings).
      const double difference =
          (actualValue == expectedValue)
              ? 0.0
              : std::fabs(actualValue - expectedValue);
      if (!(difference <= absoluteBound ||
            difference <= relativeBound * std::fabs(expectedValue))) {
        std::ostringstream detail;
        detail.precision(17);
        detail << "line " << (line + 1) << " token " << t
               << ": numeric difference: actual [" << actualTokens[t]
               << "] expected [" << expectedTokens[t] << "] abs "
               << difference;
        note(detail.str());
        break;
      }
    }
  }
  return violations;
}

int runRegisteredTests(int argc, char** argv) {
  const char* filter = nullptr;
  bool listOnly = false;

  g_failureCount = 0;
  currentTestNameStorage.clear();

  if (argc > 0 && argv[0] != nullptr) {
    const std::string leaf =
        std::filesystem::path{std::string{argv[0]}}.filename().string();
    programNameStorage = leaf.empty() ? std::string{"xfoil-test"} : leaf;
  }

  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--list") == 0) {
      listOnly = true;
    } else {
      filter = argv[i];
    }
  }

  if (listOnly) {
    for (const TestCase& test : registry()) {
      std::cout << test.name << "\n";
    }
    return 0;
  }

  int selected = 0;
  for (const TestCase& test : registry()) {
    const std::string_view testName{test.name};

    if (filter != nullptr &&
        !testName.starts_with(std::string_view{filter})) {
      continue;
    }
    ++selected;
    currentTestNameStorage = test.name;
    std::cout << "[ RUN      ] " << test.name << "\n";
    const int failuresBefore = g_failureCount;
    try {
      test.function();
    } catch (const std::exception& error) {
      reportRawFailure(std::string("unexpected exception: ") + error.what());
    } catch (...) {
      reportRawFailure("unexpected non-std exception");
    }
    if (g_failureCount == failuresBefore) {
      std::cout << "[       OK ] " << test.name << "\n";
    } else {
      std::cout << "[  FAILED  ] " << test.name << "\n";
    }
  }

  if (selected == 0) {
    std::cout << "ERROR: no tests selected";
    if (filter != nullptr) {
      std::cout << " for filter \"" << filter << "\"";
    }
    std::cout << "\n";
    return 2;
  }

  std::cout << g_failureCount << " failure(s) in " << selected << " test(s)\n";
  return g_failureCount == 0 ? 0 : 1;
}

bool loadTextFile(const std::string& path, std::string& contents) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    return false;
  }
  std::ostringstream buffer;
  buffer << stream.rdbuf();
  contents = buffer.str();
  return true;
}

}  // namespace testing

// End: tests/TestSupport.cpp
