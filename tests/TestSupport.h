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

// file: tests/TestSupport.h
// Small standalone test support for the XFoil project.
// No external test library.

#pragma once

#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <sstream>
#include <string>
#include <vector>

namespace xfoil::session {
struct SessionComposition;
}

namespace testing {

using TestFunction = void (*)();

struct TestCase {
  const char* name;
  TestFunction function;
};

// Global registration of all tests.
std::vector<TestCase>& registry();

class Registrar {
 public:
  Registrar(const char* name, TestFunction function);
};

// Reports a failed check (file/line known).
void reportFailure(const char* file, int line, const std::string& detail);

// Reports a failed check without location (e.g. an uncaught exception).
void reportRawFailure(const std::string& detail);

// Counts the test errors that occurred since start.
int failureCount();

// Runs all registered tests. Optional argv[1] is a
// name prefix filter (e.g. "unit." or "integration.").
// "--list" lists the registered tests.
// Return value: 0 on success, != 0 on at least one error.
int runRegisteredTests(int argc, char** argv);

// name of the currently running registered test (e.g.
// "unit.CoreGdesMenuHelpUnknownExitMatchesFortran"); outside a
// test run it is empty.
std::string currentTestName();

// Test-only setup helper for cases whose reference transcript predates
// the S15 NACA command contract. It exercises the same core handler and
// commit path with the explicit S04 NSIDE=123 (NB=245) baseline.
void handleLegacyNacaCore(xfoil::session::SessionComposition& session,
                          std::ostream& out, int designation);

// S18 removes the project-specific OFS record and appends CLf/CDp fields
// to format 2020. Legacy oracle tests compare against the pre-S18 text;
// these helpers project both sides onto that older text.
std::string normalizeS18LegacyText(std::string text);

// S18 changed TCBUF/TCSCAL thickness/camber widths. Legacy CAMB tests
// compare against the pre-S18 F8.4/F7.3 reference; this normalizes only
// those two report lines on both sides.
std::string normalizeS18ThicknessCamber(std::string text);

// base directory for test working directories:
// std::filesystem::temp_directory_path.
std::filesystem::path testTempBasePath();

// diagnostics mode: active if the environment variable
// XFOIL_KEEP_TEST_ARTIFACTS is set to a value other than "0".
// Then test working directories are not removed and their path
// appears in the test diagnostics output.
bool keepTestArtifactsEnabled();

// RAII contract for a unique, empty test working directory.
// creation in the constructor, guaranteed removal in the destructor
// (except in diagnostics mode). Optionally switches into the
// directory; the original process working path is always
// restored in the destructor, also on test-body exceptions.
// name: xfoil-test-<binary>-<tag>-<pid>-<running counter>.
class ScopedTempDirectory {
 public:
  // tag is sanitized for filesystem paths; empty means
  // "current test name".
  explicit ScopedTempDirectory(std::string tag = std::string{},
                               bool enterWorkingDirectory = false);
  // Explicit base (e.g. a build directory) for cases that must
  // keep their artifacts under the build tree. The base is
  // created; the actual directory stays unique per run.
  ScopedTempDirectory(std::string tag, const std::filesystem::path& base,
                      bool enterWorkingDirectory = false);

  ScopedTempDirectory(const ScopedTempDirectory&) = delete;
  ScopedTempDirectory& operator=(const ScopedTempDirectory&) = delete;
  ScopedTempDirectory(ScopedTempDirectory&&) = delete;
  ScopedTempDirectory& operator=(ScopedTempDirectory&&) = delete;

  ~ScopedTempDirectory();

  const std::filesystem::path& path() const noexcept;
  std::filesystem::path makePath(const std::string& relativeName) const;

 private:
  void enter();

  std::filesystem::path path_;
  std::filesystem::path previousWorkingDirectory_;
  bool entered_ = false;
};

// RAII guard for a working-directory change into an already
// existing directory. The previous process working path is always
// restored in the destructor; the destructor does not throw.
// If the change throws in the constructor, the process working
// path stays unchanged and the exception propagates.
class ScopedWorkingDirectory {
 public:
  explicit ScopedWorkingDirectory(const std::filesystem::path& directory);

  ScopedWorkingDirectory(const ScopedWorkingDirectory&) = delete;
  ScopedWorkingDirectory& operator=(const ScopedWorkingDirectory&) = delete;
  ScopedWorkingDirectory(ScopedWorkingDirectory&&) = delete;
  ScopedWorkingDirectory& operator=(ScopedWorkingDirectory&&) = delete;

  ~ScopedWorkingDirectory();

 private:
  std::filesystem::path previousWorkingDirectory_;
  bool entered_ = false;
};

template <typename T>
std::string displayValue(const T& value) {
  std::ostringstream stream;
  stream << value;
  return stream.str();
}

inline std::string displayValue(bool value) {
  return value ? "true" : "false";
}

void checkBool(bool value, bool expectedValue, const char* expressionText,
               const char* file, int line);

template <typename ActualType, typename ExpectedType>
void checkEqual(const ActualType& actual, const ExpectedType& expected,
                const char* actualExpr, const char* expectedExpr,
                const char* file, int line) {
  if (actual != expected) {
    reportFailure(file, line,
                  std::string("CHECK_EQ(") + actualExpr + ", " + expectedExpr +
                      ")\n      expected: " + displayValue(expected) +
                      "\n      actual:   " + displayValue(actual));
  }
}

template <typename ActualType, typename ExpectedType>
void checkNotEqual(const ActualType& actual, const ExpectedType& expected,
                   const char* actualExpr, const char* expectedExpr,
                   const char* file, int line) {
  if (actual == expected) {
    reportFailure(file, line,
                  std::string("CHECK_NE(") + actualExpr + ", " + expectedExpr +
                      ")\n      both are: " + displayValue(actual));
  }
}

// |actual - expected| <= absoluteTolerance + relativeTolerance * |expected|
void checkNear(double actual, double expected, double absoluteTolerance,
               double relativeTolerance, const char* actualExpr,
               const char* expectedExpr, const char* file, int line);

// --- PUB-W1: platform-specific numeric comparison contract ------------
// Some core tests pin numeric results of the floating-point kernels at
// bit level. Linux is and stays the canonical platform: on every
// non-Windows platform the helpers below reproduce exactly the
// comparison the respective test file used before (bitwise equality or
// value equality) and never apply a tolerance.
//
// On Windows/MSYS2 UCRT64 the recorded reference run
// (windows-core-test.log, 1064 assertion failures in six tests) shows
// small platform-dependent deviations (libm/code-generation) for the
// same kernels. Only for those individual, documented comparisons the
// classes below define a narrow portable contract:
//
//   kReplayValue     |a-e| <= 1e-14 + 1e-13*|e|
//                    measured (full precision): max abs 2.00e-15,
//                    max rel 1.33e-14, max ULP 117
//                    margins: 5.0x (abs floor), 7.5x (rel)
//   kUlpBudget128    ULP distance <= 128
//                    measured: 2, 6, 33, 33, 102, 105 ULP
//                    margin: 1.22x; real algorithmic deviations are
//                    orders of magnitude larger
//                    (PUB-W2 note: those six values were the first
//                    deviation per array of an earlier run; see
//                    kUlpBudget768 for the corrected maximum of the
//                    same kernel)
//   kUlpBudget1024   ULP distance <= 1024   (PROVISIONAL)
//                    load-integration replay; same kernels as
//                    kUlpBudget128 (measured max 105 ULP); the six-digit
//                    console log does not allow full precision here;
//                    must be re-measured on the MSYS2 acceptance run
//   kNearZero1e15    |a-e| <= 1e-15
//                    measured (6-digit): max abs 3.28e-16; relative
//                    tolerances are unsuitable near zero; margin ~3x
//   kDijMatrixValue  |a-e| <= 1e-8 + 1e-6*|e|   (PROVISIONAL)
//                    influence matrix; 790/792 logged pairs display
//                    identically at six digits (rel < ~5e-7), two pairs
//                    differ by <= 1.5e-9 absolute; must be re-measured
//
// A bitwise-identical pair always passes every class. NaN never
// passes a tolerance branch. On Windows every contract violation is
// reported with the full-precision values, bit patterns and ULP
// distance (measurement instrumentation for the acceptance run).
//
// On the exact (non-Windows) platform the classes keep the historic
// per-test semantics: kExactBits, kReplayValue, kUlpBudget128,
// kUlpBudget384 and kUlpBudget768 compare raw bits; kUlpBudget1024,
// kNearZero1e15 and kDijMatrixValue compare by value (==), matching the
// original CHECK_EQ / CHECK_NEAR(...,0,0) contracts.
//   kUlpBudget384    ULP distance <= 384
//                    measured (PUB-W2 run, full bit patterns):
//                    Xywake wake-node y values, 13 entries, max 302
//                    ULP (all same-sign negatives); margin 1.27x
//   kUlpBudget768    ULP distance <= 768
//                    measured (PUB-W2 run, full bit patterns):
//                    CpiBit influence kernels, ULP
//                    133/133/159/384/398/636; margin 1.21x. Replaces
//                    the PUB-W1 kUlpBudget128 for that test after
//                    re-measurement showed the W1 values were only the
//                    first deviations per array.
//   kExactBits, kReplayValue, kUlpBudget128, kUlpBudget1024,
//   kNearZero1e15, kDijMatrixValue: as documented in PUB-W1 above.
//
// PUB-W3 additions - measured combined absolute/relative contracts
// (measured maxima from the third MSYS2 run, logs saved under
// /tmp/opencode/pub-w3/windows-input/; ULP alone is unsuitable for
// near-zero values, so these classes bound |a-e| <= A or
// |a-e| <= R*|e|, reject NaN/Inf, and remain bitwise on Linux):
//   kVelsReferenceValue
//                    A = 1e-14 (measured 3.64e-15 normal-value max,
//                        near-zero max 8.9e-16; margin 2.7x)
//                    R = 2e-10 (measured 5.9e-11 near-zero max;
//                        margin 3.4x)
//   kFmomReferenceValue
//                    A = 5e-15 (measured 2.27e-15; margin 2.2x)
//                    R = 1e-12 (measured 3.11e-13; margin 3.2x)
//   kCpiReferenceValue (PUB-W4 re-measured, fourth MSYS2 run)
//                    A = 3e-13 (measured 1.4877e-13; margin 2.0x)
//                    R = 1e-11 (measured 5.005e-12; margin 2.0x)
// Replacing the PUB-W2 kUlpBudget768 for core.CpiBitAgainstFortranHarness
// (re-measured twice: 5179 ULP / 3.59e-14 in run 3, 29568 ULP /
// 1.4877e-13 in run 4 - a ULP-only budget was the wrong instrument and
// the PUB-W3 abs/rel bounds were too tight). Determinism of the run-4
// values is re-confirmed by three repeated MSYS2 runs before
// acceptance; growing deviations trigger the PUB-W4 abort criterion.
enum class PortableNumericClass {
  kExactBits,
  kReplayValue,
  kUlpBudget128,
  kUlpBudget384,
  kUlpBudget768,
  kUlpBudget1024,
  kNearZero1e15,
  kDijMatrixValue,
  kVelsReferenceValue,
  kFmomReferenceValue,
  kCpiReferenceValue,
};

// PUB-W3: raw absolute/relative contract predicate (shared by the
// class-based contract and the field-group dump contracts). Returns
// false for NaN/Inf on either side.
bool withinAbsoluteRelativeContract(double actual, double expected,
                                    double absoluteBound, double relativeBound);

// True on every platform that must enforce the exact contract (all
// non-Windows platforms).
constexpr bool kPortableNumericExactPlatform =
#ifdef _WIN32
    false;
#else
    true;
#endif

// ULP distance along the IEEE-754 line: 0 for equal bits and for the
// +0.0/-0.0 pair; a huge value for opposite infinities/NaN mixes and
// for different-bit NaN pairs.
std::uint64_t portableUlpDistance(double a, double b);

// Pure predicate behind checkPortableNumeric (unit-testable).
bool withinPortableNumericContract(double actual, double expected,
                                   PortableNumericClass cls);

// Reports a failure if the class contract is violated.
void checkPortableNumeric(double actual, double expected,
                          PortableNumericClass cls, const char* actualExpr,
                          const char* expectedExpr, const char* file,
                          int line);

// PUB-W1: decodes a std::system() result into the child exit code.
// POSIX: the value is a wait status (WIFEXITED/WEXITSTATUS). MinGW/
// UCRT64: system() returns the child exit code directly. A child that
// could not be started or did not exit normally yields -1. Used by the
// test files that drive the product binary or the Python tools as a
// subprocess.
int systemExitCode(int status);

// --- PUB-W2: central portable test-process contract ----------------------
// Several test files start the product binary as a subprocess. Native
// MinGW test programs run std::system() through cmd.exe, which rejects
// POSIX syntax (LC_ALL=C prefixes) and needs the .exe suffix and quoting
// that works under cmd.exe. ProcessInvocation describes one run; the
// renderers produce the platform command string (pure, unit-testable on
// both platforms), runProcess() executes the native rendering.
//
// Rules:
// - program: resolved through resolveProgram() first (Windows appends
//   .exe when the given path lacks a suffix and <path>.exe exists),
//   made absolute, and quoted with double quotes under cmd.exe;
// - arguments: quoted individually per platform (POSIX: single quotes
//   with embedded-quote escaping; Windows: double quotes);
// - environment entries: POSIX rendered as VAR=value prefixes; Windows
//   applied with putenv around the call and restored afterwards;
// - stdin/stdout/stderr file redirection: < in, > out, 2> err on both
//   platforms (cmd.exe supports the same redirection operators);
// - inputs containing double quotes are rejected as a test-harness
//   error (the temporary paths are harness-generated and quote-free).
struct ProcessInvocation {
  std::string program;
  std::vector<std::string> arguments;
  std::string workingDirectory;  // empty: keep the process cwd.
  std::string stdinFile;         // empty: no redirection.
  std::string stdoutFile;        // empty: no redirection.
  std::string stderrFile;        // empty: no redirection.
  std::vector<std::pair<std::string, std::string>> environment;
};

// Resolves the configured CLI path for the current platform. On Windows
// a path without a ".exe" suffix is extended to "<path>.exe" when that
// file exists; otherwise (and on POSIX) the path is returned unchanged.
// Returns false when no candidate exists.
bool resolveProgram(const std::string& program, std::string& resolved);

// Pure command renderers (unit-testable cross-platform).
std::string renderPosixCommand(const ProcessInvocation& invocation);
std::string renderWindowsCommand(const ProcessInvocation& invocation);

// Executes through the native rendering. Returns the child exit code
// (systemExitCode semantics); -1 when the program cannot be resolved,
// the invocation is invalid (quotes in harness inputs), or the shell
// could not be started.
int runProcess(const ProcessInvocation& invocation);

// PUB-W2: canonicalizes captured console output: CRLF -> LF. Lone CR
// characters are kept. Idempotent on LF-only text, therefore safe on
// all platforms. Intended for captured stdout/stderr (console streams
// use CRLF on Windows); never apply it to byte-contract project files.
std::string canonicalizeCapturedText(const std::string& text);

// PUB-W3/PUB-W4: Windows-only line-wise numeric text comparison for
// captured console / Fortran column output. Contract:
// - exact line count; a byte-identical line always passes;
// - otherwise both lines are split on runs of the Fortran column
//   separator bytes (" " and \t); token counts must match, every
//   non-numeric token must match byte-exactly, and numeric tokens
//   (fully parseable by strtod, finite, digit-bearing) must satisfy
//   |actual - expected| <= absoluteBound or <= relativeBound *
//   |expected|;
// - PUB-W4: +0.0 == -0.0 (signed zero: "-0.0000" vs " 0.0000" is the
//   same value); NaN/Inf tokens are rejected on either side.
// Returns the number of violating lines (a differing line count or
// token count counts as one) and appends up to maxMessages
// "line N: ..." diagnostics. On Linux this helper must not be used -
// the byte-exact contract stays in force there.
int numericTextLineViolations(const std::string& actual,
                              const std::string& expected,
                              double absoluteBound, double relativeBound,
                              std::vector<std::string>& messages,
                              std::size_t maxMessages);

// Loads a text file completely into a string. Returns false if
// the file cannot be opened.
bool loadTextFile(const std::string& path, std::string& contents);

}  // namespace testing

#define TEST(suiteName, testName)                                          \
  static void test_##suiteName##_##testName();                             \
  static const ::testing::Registrar                                        \
      registrar_##suiteName##_##testName(                                  \
          #suiteName "." #testName,                                        \
          &test_##suiteName##_##testName);                                 \
  static void test_##suiteName##_##testName()

#define CHECK(condition)                                                    \
  ::testing::checkBool(static_cast<bool>(condition), true, #condition,      \
                       __FILE__, __LINE__)

#define CHECK_FALSE(condition)                                              \
  ::testing::checkBool(static_cast<bool>(condition), false, #condition,     \
                       __FILE__, __LINE__)

#define CHECK_EQ(actual, expected)                                          \
  ::testing::checkEqual((actual), (expected), #actual, #expected,           \
                        __FILE__, __LINE__)

#define CHECK_NE(actual, expected)                                          \
  ::testing::checkNotEqual((actual), (expected), #actual, #expected,        \
                           __FILE__, __LINE__)

#define CHECK_NEAR(actual, expected, absoluteTolerance, relativeTolerance)  \
  ::testing::checkNear(static_cast<double>(actual),                         \
                       static_cast<double>(expected),                       \
                       static_cast<double>(absoluteTolerance),              \
                       static_cast<double>(relativeTolerance), #actual,     \
                       #expected, __FILE__, __LINE__)

// End: tests/TestSupport.h
