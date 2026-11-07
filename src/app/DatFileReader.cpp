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

// File: src/app/DatFileReader.cpp
// Move the hardened DAT parsing logic from
// src/main.cpp into the application layer.
// Rules: no output on std::cout/std::cerr, no IBX, no fixed
// upper bound for the point count, no raw pointers in the API.

#include "app/DatFileReader.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <string>

#include "xfoil/Limits.h"

namespace xfoil::app {
namespace {

// Reads exactly two finite numbers in strtod format. Leading
// whitespace characters are allowed; after the second number only whitespace.
// (Adopted from src/main.cpp; logic unchanged.)
  bool parseTwoNumbers(const std::string& line,
                       double& first,
                       double& second) {
    const char* cursor = line.c_str();

    while (*cursor != '\0' &&
      std::isspace(static_cast<unsigned char>(*cursor)) != 0) {
      ++cursor;
      }

      if (*cursor == '\0') {
        return false;
      }

      char* endOfFirst = nullptr;
    errno = 0;
    const double parsedFirst = std::strtod(cursor, &endOfFirst);

    if (endOfFirst == cursor || errno == ERANGE ||
      !std::isfinite(parsedFirst)) {
      return false;
      }

      if (*endOfFirst == '\0' ||
        std::isspace(static_cast<unsigned char>(*endOfFirst)) == 0) {
        return false;
        }

        const char* secondStart = endOfFirst;
      while (*secondStart != '\0' &&
        std::isspace(static_cast<unsigned char>(*secondStart)) != 0) {
        ++secondStart;
        }

        if (*secondStart == '\0') {
          return false;
        }

        char* endOfSecond = nullptr;
      errno = 0;
      const double parsedSecond = std::strtod(secondStart, &endOfSecond);

      if (endOfSecond == secondStart || errno == ERANGE ||
        !std::isfinite(parsedSecond)) {
        return false;
        }

        const char* rest = endOfSecond;
      while (*rest != '\0' &&
        std::isspace(static_cast<unsigned char>(*rest)) != 0) {
        ++rest;
        }

        if (*rest != '\0') {
          return false;
        }

        first = parsedFirst;
      second = parsedSecond;
      return true;
}

// True if the line is empty or contains only whitespace.
bool isBlankLine(const std::string& line) {
  for (const char character : line) {
    if (std::isspace(static_cast<unsigned char>(character)) == 0) {
      return false;
    }
  }
  return true;
}

}  // namespace

DatReadResult DatFileReader::read(const std::filesystem::path& path) {
  DatReadResult result;

  std::ifstream fs(path);
  if (!fs) {
    result.error = DatReadError::cannotOpen;
    result.errorMessage = "Failed to open dat file";
    return result;
  }

  // The first line is the airfoil name.
  std::string line;
  if (!std::getline(fs, line)) {
    result.error = DatReadError::missingName;
    result.errorMessage = "Invalid dat file: missing profile name line";
    return result;
  }
  result.airfoil.name = line;

  std::size_t lineNumber = 1;
  while (std::getline(fs, line)) {
    ++lineNumber;

    // Skip empty lines and lines containing only whitespace.
    if (isBlankLine(line)) {
      continue;
    }

    double valueX = 0.0;
    double valueY = 0.0;
    if (!parseTwoNumbers(line, valueX, valueY)) {
      result.error = DatReadError::invalidCoordinate;
      result.lineNumber = lineNumber;
      result.errorMessage = "Invalid coordinate line in dat file: " + line;
      return result;
    }

    // The reader does not limit the point count; the Fortran boundary
    // (IBX - 1) is only checked at the transition to the session layer.
    result.airfoil.coordinates.push_back(Coordinate{valueX, valueY});
  }

  if (result.airfoil.coordinates.size() < 3) {
    result.error = DatReadError::tooFewCoordinates;
    result.errorMessage = "Invalid dat file: fewer than 3 coordinate points";
    return result;
  }

  return result;
}

namespace {

// AREAD/GETFLT helper. Number recognition follows
// fortran-src/userio.f::GETFLT on a record padded to 80 characters,
// with the sentinel ' ,' appended by GETFLT at positions
// 81 and 82.

constexpr std::size_t kRecordWidth = 80;

// Reads a line; removes an optional carriage return (CRLF).
bool getNextLine(std::istream& stream, std::string& line) {
  if (!std::getline(stream, line)) {
    return false;
  }
  if (!line.empty() && line.back() == '\r') {
    line.pop_back();
  }
  return true;
}

bool isCommentLine(const std::string& line) {
  return !line.empty() && (line.front() == '#' || line.front() == '!');
}

// Builds the GETFLT working record: tabs replaced by spaces,
// everything after '!' removed, normalized to 80 characters, sentinel " ," appended.
std::string makeRecord(const std::string& line) {
  std::string record = line.substr(0, kRecordWidth);
  for (char& character : record) {
    if (character == '\t') {
      character = ' ';
    }
  }
  const std::size_t commentPosition = record.find('!');
  if (commentPosition != std::string::npos) {
    record.resize(commentPosition);
  }
  record.resize(kRecordWidth, ' ');
  record += " ,";
  return record;
}

// Field counting as in GETFLT: commas increase the number, tokens are
// counted, and detected boundaries fill the field up to the buffer length.
int countFields(const std::string& record) {
  const int length = static_cast<int>(kRecordWidth);
  int count = 0;
  int position = 1;  // 1-based as in the Fortran original
  for (int pass = 1; pass <= length; ++pass) {
    const std::size_t windowStart = static_cast<std::size_t>(position - 1);
    const std::size_t spaceIndex = record.find(' ', windowStart);
    const std::size_t commaIndex = record.find(',', windowStart);
    const int spaceAt =
        spaceIndex == std::string::npos ? 0 : static_cast<int>(spaceIndex) + 1;
    const int commaAt =
        commaIndex == std::string::npos ? 0 : static_cast<int>(commaIndex) + 1;

    if (position == spaceAt) {
      ++position;
    } else if (position == commaAt) {
      ++count;
      ++position;
    } else {
      ++count;
      position = std::min(spaceAt, commaAt) + 1;
    }
    if (position >= length) {
      break;
    }
  }
  return count;
}

// True if the token matches Fortran's list-directed numeric notation
// (sign, digits, decimal point, exponent with e/E/d/D).
bool isRealToken(const std::string& token) {
  std::size_t index = 0;
  if (index < token.size() && (token[index] == '+' || token[index] == '-')) {
    ++index;
  }
  bool digitsBeforePoint = false;
  bool digitsAfterPoint = false;
  while (index < token.size() &&
         std::isdigit(static_cast<unsigned char>(token[index])) != 0) {
    digitsBeforePoint = true;
    ++index;
  }
  if (index < token.size() && token[index] == '.') {
    ++index;
    while (index < token.size() &&
           std::isdigit(static_cast<unsigned char>(token[index])) != 0) {
      digitsAfterPoint = true;
      ++index;
    }
  }
  if (!digitsBeforePoint && !digitsAfterPoint) {
    return false;
  }
  if (index < token.size() &&
      (token[index] == 'e' || token[index] == 'E' || token[index] == 'd' ||
       token[index] == 'D')) {
    ++index;
    if (index < token.size() &&
        (token[index] == '+' || token[index] == '-')) {
      ++index;
    }
    if (index >= token.size()) {
      return false;
    }
    while (index < token.size() &&
           std::isdigit(static_cast<unsigned char>(token[index])) != 0) {
      ++index;
    }
  }
  return index == token.size();
}

// List-directed read of exactly `count` real values from the record.
// An abort caused by end of line or an invalid token corresponds to the
// ERR=20 branch of GETFLT. Non-finite values are rejected.
bool listReadValues(const std::string& record, int count, double* values) {
  if (count <= 0) {
    return true;  // READ without list objects always succeeds
  }
  std::size_t position = 0;
  for (int index = 0; index < count; ++index) {
    while (position < kRecordWidth &&
           (record[position] == ' ' || record[position] == ',')) {
      ++position;
    }
    if (position >= kRecordWidth) {
      return false;  // End of the internal record
    }
    std::string token;
    while (position < kRecordWidth && record[position] != ' ' &&
           record[position] != ',') {
      token.push_back(record[position]);
      ++position;
    }
    if (!isRealToken(token)) {
      return false;
    }
    std::string normalized = token;
    for (char& character : normalized) {
      if (character == 'd' || character == 'D') {
        character = 'e';
      }
    }
    char* end = nullptr;
    const double value = std::strtod(normalized.c_str(), &end);
    if (end != normalized.c_str() + normalized.size() || !std::isfinite(value)) {
      return false;
    }
    values[index] = value;
  }
  return true;
}

// Assembles a GETFLT call: returns the truncated field count and the
// read error status.
struct FieldScan {
  int count{};
  bool error{};
  double first{};
  double second{};
};

FieldScan scanLine(const std::string& line) {
  const std::string record = makeRecord(line);
  FieldScan scan;
  scan.count = std::min(countFields(record), 2);
  double values[2] = {0.0, 0.0};
  if (!listReadValues(record, scan.count, values)) {
    scan.error = true;
    scan.count = 0;
    return scan;
  }
  if (scan.count >= 1) {
    scan.first = values[0];
  }
  if (scan.count >= 2) {
    scan.second = values[1];
  }
  return scan;
}

// Counts fields like GETFLT with NINP=4 but only derives the decision
// "defective / below two / two to three / four or more numbers".
struct DomainScan {
  int count{};
  bool error{};
};

DomainScan scanDomainLine(const std::string& line) {
  const std::string record = makeRecord(line);
  DomainScan scan;
  scan.count = std::min(countFields(record), 4);
  double values[4] = {0.0, 0.0, 0.0, 0.0};
  if (!listReadValues(record, scan.count, values)) {
    scan.error = true;
    scan.count = 0;
  }
  return scan;
}

}  // namespace

CoordinateFileReadResult DatFileReader::readCoordinateFile(
    const std::filesystem::path& path,
    int selectedElement,
    int maxPoints) {
  CoordinateFileReadResult result;

  std::ifstream stream(path);
  if (!stream) {
    result.error = DatReadError::cannotOpen;
    return result;
  }

  // First two non-comment lines as AREAD; end of file at this point
  // corresponds to the END=99 branch ("Unrecognizable file format").
  std::string line1;
  bool haveLine1 = false;
  while (getNextLine(stream, line1)) {
    if (!isCommentLine(line1)) {
      haveLine1 = true;
      break;
    }
  }
  if (!haveLine1) {
    result.error = DatReadError::invalidCoordinate;
    return result;
  }

  std::string line2;
  bool haveLine2 = false;
  while (getNextLine(stream, line2)) {
    if (!isCommentLine(line2)) {
      haveLine2 = true;
      break;
    }
  }
  if (!haveLine2) {
    result.error = DatReadError::invalidCoordinate;
    return result;
  }

  // Format determination as in AREAD.
  const FieldScan firstScan = scanLine(line1);
  std::vector<std::string> pendingLines;
  if (firstScan.error || firstScan.count < 2) {
    // First record is a name; the second line decides between a
    // labeled file (two to three numbers) or an MSES file (four or more).
    const DomainScan secondScan = scanDomainLine(line2);
    if (secondScan.error || secondScan.count < 2) {
      result.error = DatReadError::invalidCoordinate;
      return result;
    }
    result.name = line1.substr(0, kRecordWidth);
    if (secondScan.count < 4) {
      result.format = CoordinateFileFormat::labeled;
      // AREAD rewinds, then reads only the name line and the coordinate
      // loop from the second line: line2 counts as a point.
      pendingLines.push_back(line2);
    } else {
      result.format = CoordinateFileFormat::msesSingle;
      result.ispars = line2.substr(0, kRecordWidth);
    }
  } else {
    // Two valid numbers: simple file; both lines are coordinates.
    result.format = CoordinateFileFormat::generic;
    pendingLines.push_back(line1);
    pendingLines.push_back(line2);
  }

  // Coordinate loop according to AREAD labels 50/51/55.
  int element = 0;
  std::vector<Coordinate> points;
  for (;;) {
    ++element;
    points.clear();
    bool reachedEnd = false;
    bool foundMarker = false;
    for (int index = 1; index <= maxPoints; ++index) {
      std::string line;
      if (!pendingLines.empty()) {
        line = pendingLines.front();
        pendingLines.erase(pendingLines.begin());
      } else if (!getNextLine(stream, line)) {
        reachedEnd = true;  // end of file: the current state is the result
        break;
      }
      if (isCommentLine(line)) {
        --index;  // Comment line does not count as a point (GO TO 51)
        continue;
      }
      const FieldScan scan = scanLine(line);
      if (scan.error) {
        result.error = DatReadError::invalidCoordinate;
        result.format = CoordinateFileFormat::none;
        return result;
      }
      if (scan.count < 2) {
        --index;  // line without two numbers is skipped (GO TO 51)
        continue;
      }
      if (scan.first == 999.0 && scan.second == 999.0) {
        foundMarker = true;
        break;
      }
      points.push_back(Coordinate{scan.first, scan.second});
    }

    if (!reachedEnd && !foundMarker) {
      // The element fills the capacity: reported as an AREAD overflow.
      // The detected marker format level stays preserved (OVER prints
      // the header line before the overflow message, aread.f INFO=1).
      result.error = DatReadError::coordinateOverflow;
      result.overflowLimit = static_cast<std::size_t>(maxPoints);
      return result;
    }

    if (selectedElement == element) {
      break;
    }

    if (reachedEnd) {
      if (selectedElement > 0) {
        result.error = DatReadError::elementNotFound;
        result.format = CoordinateFileFormat::none;
        result.coordinates.clear();
        return result;
      }
      break;
    }

    if (selectedElement == 0) {
      result.elementSelectionRequired = true;
      result.format = CoordinateFileFormat::none;
      return result;
    }
    // Next element: points of the element being read are discarded.
  }

  if (selectedElement > 0 && points.empty()) {
    result.error = DatReadError::elementNotFound;
    result.format = CoordinateFileFormat::none;
    return result;
  }

  result.coordinates = std::move(points);
  // A successful selection after element markers corresponds to the
  // original ITYPE return value 4 (IEL set).
  if (selectedElement > 0 &&
      result.format == CoordinateFileFormat::msesSingle) {
    result.format = CoordinateFileFormat::msesMulti;
  }
  return result;
}

}  // namespace xfoil::app

// End: src/app/DatFileReader.cpp
