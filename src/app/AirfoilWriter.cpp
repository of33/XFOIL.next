// XFOIL.next
//
// Copyright (C) 2026 Olaf Frommann
// https://github.com/of33/XFOIL.next
//
// XFOIL.next is based on XFOIL 6.99 by Mark Drela and
// Harold Youngren.
//
// This file contains a C++ implementation derived from XFOIL 6.99.
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

// File: src/app/AirfoilWriter.cpp
// Implementation of the airfoil coordinate file writer.
// Format references: fortran-src/xfoil.f SUBROUTINE SAVE with
// 1000 FORMAT(A), 1100 FORMAT(1X,G15.7,G15.7),
// 1200 FORMAT(1X,F10.6,A,F10.6) plus BSTRIP (userio.f),
// 1400 FORMAT(1X,I12,I12).
// The G and F output algorithms correspond to the command-interpreter
// helpers verified against gfortran; they are replicated
// here independently, so that the writing core needs no interpreter header
// (documented duplication).

#include "app/AirfoilWriter.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <atomic>
#include <atomic>
#include <chrono>
#include <system_error>

namespace xfoil::app {
namespace {

// Fortran Fw.d: if the value does not fit the width, Fortran writes
// asterisks '*' (verified against gfortran).
[[nodiscard]]
std::string formatFortranF(int width, int decimals, double value) {
  if (!std::isfinite(value)) {
    return std::string(static_cast<std::size_t>(width), '*');
  }
  std::ostringstream stream;
  stream << std::fixed << std::setprecision(decimals) << std::setw(width)
         << value;
  std::string text = stream.str();
  if (static_cast<int>(text.size()) > width) {
    return std::string(static_cast<std::size_t>(width), '*');
  }
  return text;
}

// Fortran Gw.d following the behavior verified against gfortran: after
// rounding to d significant digits, fixed-format output is used when the
// decimal exponent EE (value = m * 10^EE, 0.1 <= m < 1) lies between 0
// and d; the fixed format then reserves the last four field characters as
// if an exponent were omitted. Otherwise the scientific format
// "0.ddddE+ee" appears, always with a two-digit exponent.
[[nodiscard]]
std::string formatFortranG(int width, int digits, double value) {
  if (!std::isfinite(value)) {
    return std::string(static_cast<std::size_t>(width), '*');
  }
  // Used by G15.7 (hence d = 7); the limits keep the
  // snprintf precisions buffer-safe and reject nonsensical
  // formats by emitting the Fortran asterisk field.
  if (digits < 1 || digits > 17) {
    return std::string(static_cast<std::size_t>(width), '*');
  }

  const double magnitude = std::fabs(value);
  char buffer[64];

  if (magnitude == 0.0) {
    std::snprintf(buffer, sizeof buffer, "%.*f", digits - 1, 0.0);
    std::ostringstream stream;
    stream << std::setw(width - 4) << std::right << buffer << "    ";
    return stream.str();
  }

  // Normalization m x 10^E with 1 <= m < 10 and d-1 digits after the decimal
  // point; rounding to d significant digits is done by %.e.
  std::snprintf(buffer, sizeof buffer, "%.*e", digits - 1, magnitude);
  const std::string normalized(buffer);
  const std::size_t exponentPosition = normalized.find('e');
  const double mantissa =
      std::strtod(normalized.substr(0, exponentPosition).c_str(), nullptr);
  const int baseExponent =
      std::atoi(normalized.substr(exponentPosition + 1).c_str());
  const int ee = baseExponent + 1;

  std::ostringstream stream;
  if (ee >= 0 && ee <= digits) {
    const int fractionDigits = digits - ee;
    if (fractionDigits > 0) {
      std::snprintf(buffer, sizeof buffer, "%.*f", fractionDigits, magnitude);
    } else {
      std::snprintf(buffer, sizeof buffer, "%.0f.", magnitude);
    }
    stream << std::setw(width - 4) << std::right
           << (value < 0.0 ? "-" + std::string(buffer) : std::string(buffer))
           << "    ";
  } else {
    std::snprintf(buffer, sizeof buffer, "%.*fE%c%02d", digits, mantissa / 10.0,
                  ee >= 0 ? '+' : '-', ee >= 0 ? ee : -ee);
    stream << std::setw(width) << std::right
           << (value < 0.0 ? "-" + std::string(buffer) : std::string(buffer));
  }

  std::string text = stream.str();
  if (static_cast<int>(text.size()) > width) {
    return std::string(static_cast<std::size_t>(width), '*');
  }
  return text;
}

// Removes all spaces as BSTRIP from fortran-src/userio.f.
// Tabs are preserved (as in the original).
[[nodiscard]]
std::string stripAllSpaces(std::string text) {
  std::string stripped;
  stripped.reserve(text.size());
  for (const char character : text) {
    if (character != ' ') {
      stripped.push_back(character);
    }
  }
  return stripped;
}

// Right-side trimming of spaces, as the 80-character backward run in
// SUBROUTINE SAVE before the ISPARS write.
[[nodiscard]]
std::string trimRightSpaces(const std::string& text) {
  std::size_t end = text.size();
  while (end > 0 && text[end - 1] == ' ') {
    --end;
  }
  return text.substr(0, end);
}

// USAV integer conversion: INT(X + SIGN(0.5,X)) with Fortran INT
// truncation toward zero. If the result leaves the representable I12
// limits, Fortran writes asterisk fields instead of letting it run over.
[[nodiscard]]
std::string formatIntegerField(double value) {
  // I12 holds positive values up to 12 digits and negative values up to
  // 11 digits plus sign; beyond that Fortran writes asterisk fields.
  constexpr double kMaxPositive = 999999999999.0;
  constexpr double kMinNegative = -99999999999.0;
  if (!std::isfinite(value)) {
    return std::string(12, '*');
  }
  const double shifted = value + std::copysign(0.5, value);
  const double truncated = std::trunc(shifted);
  if (truncated < kMinNegative || truncated > kMaxPositive) {
    return std::string(12, '*');
  }
  std::ostringstream stream;
  stream << std::setw(12) << std::right
         << static_cast<long long>(truncated);
  return stream.str();
}

// Fortran INT(SIGN) conversion for USAV: as in the original, the value
// is rounded with its sign and truncated toward zero.
[[nodiscard]]
std::string formatCoordinatePair(const xfoil::Point2D& point,
                                 CoordinateDelimiter delimiter) {
  std::string line(1, ' ');  // 1X descriptor of every SAVE coordinate line
  switch (delimiter) {
    case CoordinateDelimiter::blank:
      line += formatFortranG(15, 7, point.x);
      line += formatFortranG(15, 7, point.y);
      break;
    case CoordinateDelimiter::comma:
    case CoordinateDelimiter::tab: {
      const char separator =
          delimiter == CoordinateDelimiter::comma ? ',' : '\t';
      std::string raw = formatFortranF(10, 6, point.x);
      raw.push_back(separator);
      raw += formatFortranF(10, 6, point.y);
      line = stripAllSpaces(raw);
      break;
    }
  }
  return line;
}

[[nodiscard]]
std::string buildFileContents(std::span<const xfoil::Point2D> coordinates,
                              const AirfoilWriteOptions& options) {
  std::string contents;

  if (options.format == AirfoilOutputFormat::labeled ||
      options.format == AirfoilOutputFormat::ises) {
    contents += options.name;
    contents.push_back('\n');
  }
  if (options.format == AirfoilOutputFormat::ises) {
    contents += trimRightSpaces(options.isesDomainParameters);
    contents.push_back('\n');
  }

  const std::size_t count = coordinates.size();
  for (std::size_t step = 0; step < count; ++step) {
    // LCLOCK: reverse pass as IBEG=N, IEND=1, INCR=-1 in SAVE.
    const xfoil::Point2D& point =
        options.clockwise ? coordinates[count - 1 - step] : coordinates[step];
    if (options.format == AirfoilOutputFormat::integerCoordinates) {
      // Format(1X,I12,I12): the 1X descriptor also applies to the
      // USAV integer lines and was later confirmed against canonical Fortran
      // file bytes.
      contents.push_back(' ');
      contents += formatIntegerField(point.x);
      contents += formatIntegerField(point.y);
      contents.push_back('\n');
    } else {
      contents += formatCoordinatePair(point, options.delimiter);
      contents.push_back('\n');
    }
  }

  return contents;
}

std::filesystem::path makeUniqueSiblingPath(
    const std::filesystem::path& target,
    std::string_view role) {
  static std::atomic<unsigned long long> counter{};

  const auto sequence =
      counter.fetch_add(1, std::memory_order_relaxed);

  const auto ticks =
      std::chrono::steady_clock::now()
          .time_since_epoch()
          .count();

  std::filesystem::path candidate = target;
  candidate +=
      ".xfoil-" + std::string(role) + "-" +
      std::to_string(ticks) + "-" +
      std::to_string(sequence);

  return candidate;
}

std::filesystem::path findUnusedSiblingPath(
    const std::filesystem::path& target,
    std::string_view role) {
  for (int attempt = 0; attempt < 100; ++attempt) {
    const auto candidate =
        makeUniqueSiblingPath(target, role);

    std::error_code error;
    const bool exists =
        std::filesystem::exists(candidate, error);

    if (!error && !exists) {
      return candidate;
    }
  }

  return {};
}

}  // namespace

AirfoilWriteResult writeAirfoil(
  const std::filesystem::path& path,
  std::span<const xfoil::Point2D> coordinates,
  const AirfoilWriteOptions& options) {
  AirfoilWriteResult result;

  if (path.empty()) {
    result.diagnostic = "Empty output path";
    return result;
  }

  for (const xfoil::Point2D& point : coordinates) {
    if (!std::isfinite(point.x) ||
      !std::isfinite(point.y)) {
      result.diagnostic =
      "Non-finite airfoil coordinate";
    return result;
      }
  }

  std::error_code error;
  const bool targetExists =
  std::filesystem::exists(path, error);

  if (error) {
    result.diagnostic =
    "Cannot inspect output path";
    return result;
  }

  if (targetExists && !options.overwriteExisting) {
    result.existingFileNotOverwritten = true;
    result.diagnostic = "Output file exists";
    return result;
  }

  const std::string contents = buildFileContents(coordinates, options);

  // Safe write path: temporary file in the target directory followed by
  // replacement. On error the target file remains unchanged; the write
  // attempt itself does not change the output.
  const std::filesystem::path temporaryPath =
  findUnusedSiblingPath(path, "tmp");

  if (temporaryPath.empty()) {
    result.diagnostic =
    "Cannot create temporary output path";
  return result;
  }

  {
    std::ofstream stream(
      temporaryPath,
      std::ios::out |
      std::ios::trunc |
      std::ios::binary);

    if (!stream) {
      result.diagnostic =
      "Cannot open temporary output file";
  return result;
    }

    stream.write(
      contents.data(),
                 static_cast<std::streamsize>(
                   contents.size()));

    stream.flush();
    if (!stream) {
      stream.close();
      std::filesystem::remove(
        temporaryPath, error);
      result.diagnostic =
      "Write error on output file";
  return result;
    }

    stream.close();
    if (!stream) {
      std::filesystem::remove(
        temporaryPath, error);
      result.diagnostic =
      "Close error on output file";
  return result;
    }
  }

  if (!targetExists) {
    std::filesystem::rename(
      temporaryPath, path, error);

    if (error) {
      std::filesystem::remove(
        temporaryPath, error);
      result.diagnostic =
      "Cannot install output file";
  return result;
    }

    result.success = true;
    return result;
  }

  // Platform-neutral replacement path:
  // first move the old target to a backup file in the same directory,
  // then install the new file. If the second step fails, the backup is
  // restored.
  const std::filesystem::path backupPath =
  findUnusedSiblingPath(path, "backup");

  if (backupPath.empty()) {
    std::filesystem::remove(
      temporaryPath, error);
    result.diagnostic =
    "Cannot create backup output path";
  return result;
  }

  std::filesystem::rename(
    path, backupPath, error);

  if (error) {
    std::filesystem::remove(
      temporaryPath, error);
    result.diagnostic =
    "Cannot preserve existing output file";
    return result;
  }

  std::filesystem::rename(
    temporaryPath, path, error);

  if (error) {
    std::error_code restoreError;
    std::filesystem::rename(
      backupPath, path, restoreError);

    std::error_code cleanupError;
    std::filesystem::remove(
      temporaryPath, cleanupError);

    result.diagnostic =
    restoreError
    ? "Cannot install output file and cannot restore previous file"
    : "Cannot install output file";

    return result;
  }

  std::error_code cleanupError;
  std::filesystem::remove(
    backupPath, cleanupError);

  // The new target file is still installed correctly when only the
  // deletion of the backup file fails. In this rare case the backup
  // path is kept so that manual restoration remains possible.
  result.success = true;

  if (cleanupError) {
    result.diagnostic =
    "Output written; backup cleanup failed";
  }

  return result;
}

}  // namespace xfoil::app

// End: src/app/AirfoilWriter.cpp
