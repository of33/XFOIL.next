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

// File: src/gui/app/GuiFileAdapters.cpp
// Thin app adapters around existing file contracts. No parser or writer
// implementation is duplicated here.

#include "app/GuiFileAdapters.h"

#include <algorithm>
#include <string>
#include <system_error>
#include <vector>

#include <QFile>
#include <QIODevice>
#include <QSaveFile>
#include <QString>

#include "app/AirfoilWriter.h"
#include "app/DatFileReader.h"
#include "xfoil/PolarText.h"
#include "xfoil/io/DxfExport.h"
#include "xfoil/io/ExportFormats.h"

namespace xfoil::gui::app {
namespace {

std::string readErrorText(const xfoil::app::DatReadError error,
                          std::size_t overflowLimit) {
  switch (error) {
    case xfoil::app::DatReadError::cannotOpen:
      return "Cannot open the airfoil file.";
    case xfoil::app::DatReadError::missingName:
      return "The airfoil file has no profile name line.";
    case xfoil::app::DatReadError::invalidCoordinate:
      return "The airfoil file contains an invalid coordinate line.";
    case xfoil::app::DatReadError::tooFewCoordinates:
      return "The airfoil file contains fewer than 3 coordinates.";
    case xfoil::app::DatReadError::coordinateOverflow:
      return "The airfoil file exceeds the coordinate limit of " +
             std::to_string(overflowLimit) + " points.";
    case xfoil::app::DatReadError::elementNotFound:
      return "The requested airfoil element was not found.";
    case xfoil::app::DatReadError::none:
      break;
  }
  return "The airfoil file could not be read.";
}

struct CoordinateOrientation {
  bool valid{false};
  bool clockwise{false};
};

// XFOIL's LOAD path uses this signed area form and treats negative area as
// clockwise. The final point is implicitly joined to the first; no point is
// added to the stored geometry.
CoordinateOrientation coordinateOrientation(
    const std::vector<Point2D>& points) {
  if (points.size() < 3) {
    return {};
  }
  double area = 0.0;
  double xMinimum = points.front().x;
  double xMaximum = points.front().x;
  double yMinimum = points.front().y;
  double yMaximum = points.front().y;
  for (std::size_t index = 0; index < points.size(); ++index) {
    const Point2D& current = points[index];
    const Point2D& next = points[(index + 1) % points.size()];
    area += 0.5 * (current.y + next.y) * (current.x - next.x);
    xMinimum = std::min(xMinimum, current.x);
    xMaximum = std::max(xMaximum, current.x);
    yMinimum = std::min(yMinimum, current.y);
    yMaximum = std::max(yMaximum, current.y);
  }
  const double spanX = xMaximum - xMinimum;
  const double spanY = yMaximum - yMinimum;
  const double scale = spanX * spanY;
  if (!(area == area) || !(scale > 0.0)) {
    return {};
  }
  const double areaMagnitude = area < 0.0 ? -area : area;
  if (!(areaMagnitude > 1.0e-9 * scale)) {
    return {};
  }
  return {true, area < 0.0};
}

FileWriteResult checkTarget(const std::filesystem::path& path,
                            bool overwriteExisting) {
  if (path.empty()) {
    return {false, "No output file was selected."};
  }
  std::error_code error;
  const bool exists = std::filesystem::exists(path, error);
  if (error) {
    return {false, "The output path cannot be inspected."};
  }
  if (exists && !overwriteExisting) {
    return {false, "The output file already exists."};
  }
  return {true, {}};
}

FileWriteResult writeBytes(const std::filesystem::path& path,
                           std::string_view bytes,
                           bool overwriteExisting) {
  const FileWriteResult target = checkTarget(path, overwriteExisting);
  if (!target.success) {
    return target;
  }
  QSaveFile file(QString::fromStdString(path.string()));
  if (!file.open(QIODevice::WriteOnly)) {
    return {false, file.errorString().toStdString()};
  }
  if (file.write(bytes.data(), static_cast<qint64>(bytes.size())) !=
      static_cast<qint64>(bytes.size())) {
    file.cancelWriting();
    return {false, file.errorString().toStdString()};
  }
  if (!file.commit()) {
    return {false, file.errorString().toStdString()};
  }
  return {true, {}};
}

}  // namespace

AirfoilFileReadResult readAirfoilFile(
    const std::filesystem::path& path, int selectedElement,
    std::string_view suppliedName) {
  AirfoilFileReadResult result;
  const xfoil::app::CoordinateFileReadResult read =
      xfoil::app::DatFileReader::readCoordinateFile(path, selectedElement);
  if (read.elementSelectionRequired) {
    result.status = AirfoilFileReadStatus::elementSelectionRequired;
    result.message = "This coordinate file contains multiple elements.";
    return result;
  }
  if (!read.success()) {
    result.message = readErrorText(read.error, read.overflowLimit);
    return result;
  }

  std::string name = read.name;
  if (read.format == xfoil::app::CoordinateFileFormat::generic && suppliedName.empty()) {
    result.status = AirfoilFileReadStatus::nameRequired;
    result.message = "This coordinate file has no profile name.";
    return result;
  }
  if (read.format == xfoil::app::CoordinateFileFormat::generic) {
    name = std::string(suppliedName);
  }

  const CoordinateOrientation orientation =
      coordinateOrientation(read.coordinates);
  if (!orientation.valid) {
    result.message =
        "The airfoil coordinates have no measurable orientation.";
    return result;
  }
  std::vector<Point2D> coordinates = read.coordinates;
  if (orientation.clockwise) {
    std::reverse(coordinates.begin(), coordinates.end());
    result.clockwiseReversed = true;
  }
  const geometry::GeometryAssignmentResult assigned =
      result.geometry.assign(name, coordinates);
  if (!assigned.success()) {
    result.message = assigned.message;
    return result;
  }
  result.status = AirfoilFileReadStatus::success;
  return result;
}

FileWriteResult saveAirfoilFile(
    const std::filesystem::path& path,
    const geometry::AirfoilGeometry& geometry, bool overwriteExisting) {
  if (geometry.empty()) {
    return {false, "No airfoil geometry is available."};
  }
  const FileWriteResult target = checkTarget(path, overwriteExisting);
  if (!target.success) {
    return target;
  }

  xfoil::app::AirfoilWriteOptions options;
  options.format = xfoil::app::AirfoilOutputFormat::labeled;
  options.delimiter = xfoil::app::CoordinateDelimiter::blank;
  options.name = std::string(geometry.name());
  options.overwriteExisting = overwriteExisting;
  const xfoil::app::AirfoilWriteResult written =
      xfoil::app::writeAirfoil(path, geometry.coordinates(), options);
  if (!written.success) {
    return {false, written.diagnostic.empty()
                       ? "The airfoil file could not be written."
                       : written.diagnostic};
  }
  return {true, {}};
}

FileWriteResult exportDxfFile(const std::filesystem::path& path,
                              const SurfaceGeometry& panel,
                              bool overwriteExisting) {
  if (panel.points.empty()) {
    return {false, "No panel geometry is available."};
  }
  const FileWriteResult target = checkTarget(path, overwriteExisting);
  if (!target.success) {
    return target;
  }

  std::vector<double> x;
  std::vector<double> y;
  x.reserve(panel.points.size());
  y.reserve(panel.points.size());
  for (const Point2D& point : panel.points) {
    x.push_back(point.x);
    y.push_back(point.y);
  }
  const core::Result<std::string> encoded =
      io::dxfR12TextChecked(x, y);
  if (!encoded.isOk()) {
    return {false, encoded.error().message};
  }

  QSaveFile file(QString::fromStdString(path.string()));
  if (!file.open(QIODevice::WriteOnly)) {
    return {false, file.errorString().toStdString()};
  }
  const std::string& text = encoded.value();
  if (file.write(text.data(), static_cast<qint64>(text.size())) !=
      static_cast<qint64>(text.size())) {
    file.cancelWriting();
    return {false, file.errorString().toStdString()};
  }
  if (!file.commit()) {
    return {false, file.errorString().toStdString()};
  }
  return {true, {}};
}


Result<PolarData> readPolarFile(const std::filesystem::path& path) {
  if (path.empty()) {
    return Result<PolarData>::fail(Error::make(
        ErrorCategory::invalidArgument, "polar.path",
        "No polar file was selected", "app.polar"));
  }
  QFile file(QString::fromStdString(path.string()));
  if (!file.open(QIODevice::ReadOnly)) {
    return Result<PolarData>::fail(Error::make(
        ErrorCategory::invalidArgument, "polar.read",
        file.errorString().toStdString(), "app.polar"));
  }
  const QByteArray bytes = file.readAll();
  if (file.error() != QFileDevice::NoError) {
    return Result<PolarData>::fail(Error::make(
        ErrorCategory::invalidArgument, "polar.read",
        file.errorString().toStdString(), "app.polar"));
  }
  return xfoil::decodePolarText(
      std::string_view(bytes.constData(),
                       static_cast<std::size_t>(bytes.size())));
}

Result<std::string> encodePressureDumpText(
    const PressureExportData& data) {
  if (data.x.empty() || data.x.size() != data.gam.size()) {
    return Result<std::string>::fail(Error::make(
        ErrorCategory::invalidState, "pressure.export.incomplete",
        "Pressure export data is incomplete", "app.export"));
  }
  io::PressureDumpInput input;
  input.n = data.x.size();
  input.delimiter = io::DumpDelimiter::kBlank;
  input.minf = data.machNumber;
  input.qinf = data.qinf;
  input.x = data.x;
  input.gam = data.gam;
  return Result<std::string>::ok(io::pressureDumpText(input));
}

FileWriteResult savePolarFile(const std::filesystem::path& path,
                              const PolarData& polar,
                              bool overwriteExisting) {
  const Result<std::string> encoded = xfoil::encodePolarText(polar);
  if (encoded.isErr()) {
    return {false, encoded.error().message};
  }
  return writeBytes(path, encoded.value(), overwriteExisting);
}

FileWriteResult saveTextFile(const std::filesystem::path& path,
                             std::string_view text,
                             bool overwriteExisting) {
  return writeBytes(path, text, overwriteExisting);
}

FileWriteResult saveBinaryFile(const std::filesystem::path& path,
                               std::string_view bytes,
                               bool overwriteExisting) {
  return writeBytes(path, bytes, overwriteExisting);
}

}  // namespace xfoil::gui::app

// End: src/gui/app/GuiFileAdapters.cpp
