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

// File: src/io/PolarDataText.cpp
// Public owning PolarText projection over the canonical POLREAD/POLWRIT
// codec. No filesystem, prompt, path or application adapter is used.

#include "xfoil/PolarText.h"

#include <algorithm>
#include <array>
#include <string>
#include <utility>
#include <vector>

#include "xfoil/io/PolarText.h"

namespace xfoil {
namespace {

Error publicError(ErrorCategory category, std::string code,
                  std::string message) {
  return Error::make(category, std::move(code), std::move(message), "polar");
}

Error coreError(const core::Error& error) {
  ErrorCategory category = ErrorCategory::invalidArgument;
  switch (error.code) {
    case core::ErrorCode::capacityExceeded:
      category = ErrorCategory::capacityExceeded;
      break;
    case core::ErrorCode::sizeMismatch:
      category = ErrorCategory::invalidArgument;
      break;
    case core::ErrorCode::invalidState:
      category = ErrorCategory::invalidState;
      break;
    case core::ErrorCode::numericalFailure:
      category = ErrorCategory::numericalFailure;
      break;
    case core::ErrorCode::nonConvergence:
      category = ErrorCategory::nonConvergence;
      break;
    case core::ErrorCode::ioError:
      category = ErrorCategory::internal;
      break;
    case core::ErrorCode::unavailable:
      category = ErrorCategory::unsupported;
      break;
    case core::ErrorCode::invalidArgument:
    case core::ErrorCode::outOfRange:
      category = ErrorCategory::invalidArgument;
      break;
  }
  return publicError(category, "polar.text", error.message);
}

int baseIndex(PolarColumn column) {
  switch (column) {
    case PolarColumn::alpha: return io::kIal;
    case PolarColumn::liftCoefficient: return io::kIcl;
    case PolarColumn::dragCoefficient: return io::kIcd;
    case PolarColumn::momentCoefficient: return io::kIcm;
    case PolarColumn::dragCoefficientWave: return io::kIcw;
    case PolarColumn::dragCoefficientViscous: return io::kIcv;
    case PolarColumn::dragCoefficientPressure: return io::kIcp;
    case PolarColumn::machNumber: return io::kIma;
    case PolarColumn::reynoldsNumber: return io::kIre;
    case PolarColumn::hingeMoment: return io::kIch;
    case PolarColumn::minimumPressureCoefficient: return io::kImc;
    case PolarColumn::hingeDragCoefficient: return io::kIcdh;
    case PolarColumn::momentCoefficientDerivative: return io::kIcmdot;
  }
  return 0;
}

int sideIndex(PolarSideColumn column) {
  switch (column) {
    case PolarSideColumn::nCritical: return io::kJnc;
    case PolarSideColumn::tripX: return io::kJtp;
    case PolarSideColumn::transitionX: return io::kJtn;
    case PolarSideColumn::transitionIndex: return io::kJti;
  }
  return 0;
}

core::polar::PolarElementQuantity coreElement(PolarElementQuantity value) {
  switch (value) {
    case PolarElementQuantity::cx:
      return core::polar::PolarElementQuantity::cx;
    case PolarElementQuantity::cy:
      return core::polar::PolarElementQuantity::cy;
    case PolarElementQuantity::cm:
      return core::polar::PolarElementQuantity::cm;
    case PolarElementQuantity::cdVisc:
      return core::polar::PolarElementQuantity::cdVisc;
  }
  return core::polar::PolarElementQuantity::cx;
}

PolarColumn publicBase(int index) {
  switch (index) {
    case io::kIal: return PolarColumn::alpha;
    case io::kIcl: return PolarColumn::liftCoefficient;
    case io::kIcd: return PolarColumn::dragCoefficient;
    case io::kIcm: return PolarColumn::momentCoefficient;
    case io::kIcw: return PolarColumn::dragCoefficientWave;
    case io::kIcv: return PolarColumn::dragCoefficientViscous;
    case io::kIcp: return PolarColumn::dragCoefficientPressure;
    case io::kIma: return PolarColumn::machNumber;
    case io::kIre: return PolarColumn::reynoldsNumber;
    case io::kIch: return PolarColumn::hingeMoment;
    case io::kImc: return PolarColumn::minimumPressureCoefficient;
    case io::kIcdh: return PolarColumn::hingeDragCoefficient;
    case io::kIcmdot: return PolarColumn::momentCoefficientDerivative;
    default: return PolarColumn::alpha;
  }
}

PolarSideColumn publicSide(int index) {
  switch (index) {
    case io::kJnc: return PolarSideColumn::nCritical;
    case io::kJtp: return PolarSideColumn::tripX;
    case io::kJtn: return PolarSideColumn::transitionX;
    case io::kJti: return PolarSideColumn::transitionIndex;
    default: return PolarSideColumn::nCritical;
  }
}

PolarElementQuantity publicElement(
    core::polar::PolarElementQuantity value) {
  switch (value) {
    case core::polar::PolarElementQuantity::cx:
      return PolarElementQuantity::cx;
    case core::polar::PolarElementQuantity::cy:
      return PolarElementQuantity::cy;
    case core::polar::PolarElementQuantity::cm:
      return PolarElementQuantity::cm;
    case core::polar::PolarElementQuantity::cdVisc:
      return PolarElementQuantity::cdVisc;
  }
  return PolarElementQuantity::cx;
}

ReynoldsDependency publicReType(int value) {
  switch (value) {
    case 2: return ReynoldsDependency::inverseSquareRootLift;
    case 3: return ReynoldsDependency::inverseLift;
    case 1:
    default: return ReynoldsDependency::constant;
  }
}

MachDependency publicMachType(int value) {
  switch (value) {
    case 2: return MachDependency::inverseSquareRootLift;
    case 3: return MachDependency::inverseLift;
    case 1:
    default: return MachDependency::constant;
  }
}

bool validElementSelection(const PolarElementSelection& selection) {
  if (selection.count > polarTextElementColumnCount) {
    return false;
  }
  std::array<bool, polarTextElementColumnCount + 1> seen{};
  for (std::size_t i = 0; i < polarTextElementColumnCount; ++i) {
    if (i >= selection.count) {
      if (selection.columns[i].has_value()) {
        return false;
      }
      continue;
    }
    if (!selection.columns[i].has_value()) {
      return false;
    }
    const int index = static_cast<int>(*selection.columns[i]) + 1;
    if (index < 1 || index > 4 || seen[static_cast<std::size_t>(index)]) {
      return false;
    }
    seen[static_cast<std::size_t>(index)] = true;
  }
  return true;
}

Result<void> validatePolarForEncode(const PolarData& polar) {
  if (polar.metadata.elementCount < 1 ||
      polar.metadata.elementCount > maxPolarTextElements) {
    return Result<void>::fail(publicError(
        ErrorCategory::invalidArgument, "polar.elementCount",
        "Polar element count is outside the supported range"));
  }
  if (polar.points.size() > maxPolarTextPoints) {
    return Result<void>::fail(publicError(
        ErrorCategory::capacityExceeded, "polar.pointLimit",
        "Polar point count exceeds the PolarText capacity"));
  }
  if (polar.layout.baseColumns.empty()) {
    return Result<void>::fail(publicError(
        ErrorCategory::invalidArgument, "polar.layout",
        "Polar layout has no base columns"));
  }
  std::array<bool, 14> baseSeen{};
  for (const PolarColumn column : polar.layout.baseColumns) {
    const int index = baseIndex(column);
    if (index < 1 || index > static_cast<int>(baseSeen.size()) - 1 ||
        baseSeen[static_cast<std::size_t>(index)]) {
      return Result<void>::fail(publicError(
          ErrorCategory::invalidArgument, "polar.layout",
          "Polar base columns contain an invalid or duplicate column"));
    }
    baseSeen[static_cast<std::size_t>(index)] = true;
  }
  std::array<bool, 5> sideSeen{};
  for (const PolarSideColumn column : polar.layout.sideColumns) {
    const int index = sideIndex(column);
    if (index < 1 || index > 4 || sideSeen[static_cast<std::size_t>(index)]) {
      return Result<void>::fail(publicError(
          ErrorCategory::invalidArgument, "polar.layout",
          "Polar side columns contain an invalid or duplicate column"));
    }
    sideSeen[static_cast<std::size_t>(index)] = true;
  }
  if (!validElementSelection(polar.layout.elementSelection)) {
    return Result<void>::fail(publicError(
        ErrorCategory::invalidArgument, "polar.elementSelection",
        "Polar element selection is invalid"));
  }
  if (!polar.layout.sideColumns.empty() &&
      polar.sideData.size() != polar.points.size()) {
    return Result<void>::fail(publicError(
        ErrorCategory::invalidArgument, "polar.sideData",
        "Polar side data does not match the point count"));
  }
  if (polar.layout.elementSelection.count > 0 &&
      polar.elementData.size() != polar.points.size()) {
    return Result<void>::fail(publicError(
        ErrorCategory::invalidArgument, "polar.elementData",
        "Polar element data does not match the point count"));
  }

  const auto selected = [&](PolarColumn column) {
    return std::find(polar.layout.baseColumns.begin(),
                     polar.layout.baseColumns.end(),
                     column) != polar.layout.baseColumns.end();
  };
  for (const PolarPoint& point : polar.points) {
    const std::array<std::pair<bool, bool>, 9> required = {{
        {selected(PolarColumn::dragCoefficientWave),
         point.dragCoefficientWave.has_value()},
        {selected(PolarColumn::dragCoefficientViscous),
         point.dragCoefficientViscous.has_value()},
        {selected(PolarColumn::dragCoefficientPressure),
         point.dragCoefficientPressure.has_value()},
        {selected(PolarColumn::machNumber), point.machNumber.has_value()},
        {selected(PolarColumn::reynoldsNumber),
         point.reynoldsNumber.has_value()},
        {selected(PolarColumn::hingeMoment), point.hingeMoment.has_value()},
        {selected(PolarColumn::minimumPressureCoefficient),
         point.minimumPressureCoefficient.has_value()},
        {selected(PolarColumn::hingeDragCoefficient),
         point.hingeDragCoefficient.has_value()},
        {selected(PolarColumn::momentCoefficientDerivative),
         point.momentCoefficientDerivative.has_value()},
    }};
    for (const auto& [needed, present] : required) {
      if (needed && !present) {
        return Result<void>::fail(publicError(
            ErrorCategory::invalidArgument, "polar.missingColumnValue",
            "A selected polar column has no value for a point"));
      }
    }
  }
  return Result<void>::ok();
}

}  // namespace

Result<PolarData> decodePolarText(std::string_view text,
                                  PolarTextCompatibility compatibility) {
  const io::PolarReadCompatibility internal =
      compatibility == PolarTextCompatibility::xfoil6996BrokenSideOrder
          ? io::PolarReadCompatibility::xfoil6996BrokenSideOrder
          : io::PolarReadCompatibility::canonical;
  const io::ParsedPolar parsed = io::readPolarText(text, io::kNax, internal);
  if (parsed.error) {
    return Result<PolarData>::fail(publicError(
        ErrorCategory::invalidArgument, "polar.parse",
        "PolarText line " + std::to_string(parsed.errorLine) + ": " +
            parsed.errorMessage));
  }

  PolarData polar;
  polar.metadata.name = parsed.name;
  polar.metadata.code = parsed.code;
  polar.metadata.version = parsed.version;
  polar.metadata.reynoldsDependency = publicReType(parsed.iretyp);
  polar.metadata.machDependency = publicMachType(parsed.imatyp);
  polar.metadata.referenceReynoldsNumber = parsed.reyn1;
  polar.metadata.referenceMachNumber = parsed.mach1;
  polar.metadata.nCritical = parsed.acrit;
  polar.metadata.tripX = parsed.xtrip;
  polar.metadata.pressureParameter = parsed.ptrat;
  polar.metadata.etaParameter = parsed.etap;
  polar.metadata.elementCount = static_cast<std::size_t>(parsed.nbl);

  for (const io::PolarField& field : parsed.layout.fields) {
    switch (field.kind) {
      case io::PolarFieldKind::base:
        polar.layout.baseColumns.push_back(publicBase(field.baseIndex));
        break;
      case io::PolarFieldKind::sideTop:
      case io::PolarFieldKind::sideBottom:
        if (std::find(polar.layout.sideColumns.begin(),
                      polar.layout.sideColumns.end(),
                      publicSide(field.sideIndex)) ==
            polar.layout.sideColumns.end()) {
          polar.layout.sideColumns.push_back(publicSide(field.sideIndex));
        }
        break;
      case io::PolarFieldKind::element:
        break;
    }
  }
  polar.layout.elementSelection.count = parsed.elementSelection.count;
  for (std::size_t i = 0; i < polarTextElementColumnCount; ++i) {
    if (parsed.elementSelection.columns[i].has_value()) {
      polar.layout.elementSelection.columns[i] =
          publicElement(*parsed.elementSelection.columns[i]);
    }
  }

  polar.points.reserve(parsed.points.size());
  polar.sideData.reserve(parsed.points.size());
  for (const io::PolarPoint& row : parsed.points) {
    PolarPoint point;
    point.alpha = Degrees{row.cpol[io::kIal - 1]};
    point.liftCoefficient = row.cpol[io::kIcl - 1];
    point.dragCoefficient = row.cpol[io::kIcd - 1];
    point.momentCoefficient = row.cpol[io::kIcm - 1];
    point.dragCoefficientWave = row.cpol[io::kIcw - 1];
    point.dragCoefficientViscous = row.cpol[io::kIcv - 1];
    point.dragCoefficientPressure = row.cpol[io::kIcp - 1];
    point.machNumber = row.cpol[io::kIma - 1];
    point.reynoldsNumber = row.cpol[io::kIre - 1];
    point.hingeMoment = row.cpol[io::kIch - 1];
    point.minimumPressureCoefficient = row.cpol[io::kImc - 1];
    point.hingeDragCoefficient = row.cpol[io::kIcdh - 1];
    point.momentCoefficientDerivative = row.cpol[io::kIcmdot - 1];
    point.convergence = ConvergenceStatus::converged;
    polar.points.push_back(std::move(point));

    std::array<PolarSideData, polarTextSideCount> sides{};
    for (std::size_t side = 0; side < polarTextSideCount; ++side) {
      sides[side].nCritical = row.side[side][io::kJnc - 1];
      sides[side].tripX = row.side[side][io::kJtp - 1];
      sides[side].transitionX = row.side[side][io::kJtn - 1];
      sides[side].transitionIndex = row.side[side][io::kJti - 1];
    }
    polar.sideData.push_back(sides);
  }
  polar.elementData.reserve(parsed.elementData.size());
  for (const core::polar::PolarElementRow& row : parsed.elementData) {
    std::array<PolarElementData, maxPolarTextElements> converted{};
    for (std::size_t i = 0; i < maxPolarTextElements; ++i) {
      converted[i].cx = row[i].cx;
      converted[i].cy = row[i].cy;
      converted[i].cm = row[i].cm;
      converted[i].cdVisc = row[i].cdVisc;
    }
    polar.elementData.push_back(converted);
  }
  return Result<PolarData>::ok(std::move(polar));
}

Result<std::string> encodePolarText(const PolarData& polar) {
  const Result<void> validation = validatePolarForEncode(polar);
  if (validation.isErr()) {
    return Result<std::string>::fail(validation.error());
  }

  io::PolarWriteRequest request;
  request.lhead = true;
  request.ia1 = 1;
  request.ia2 = static_cast<int>(polar.points.size());
  for (const PolarColumn column : polar.layout.baseColumns) {
    request.ipol.push_back(baseIndex(column));
  }
  for (const PolarSideColumn column : polar.layout.sideColumns) {
    request.jpol.push_back(sideIndex(column));
  }
  request.elementSelection.count = polar.layout.elementSelection.count;
  for (std::size_t i = 0; i < polarTextElementColumnCount; ++i) {
    if (polar.layout.elementSelection.columns[i].has_value()) {
      request.elementSelection.columns[i] =
          coreElement(*polar.layout.elementSelection.columns[i]);
    }
  }
  request.code = polar.metadata.code;
  request.version = polar.metadata.version;
  request.name = polar.metadata.name;
  request.nbl = static_cast<int>(polar.metadata.elementCount);
  request.iretyp = static_cast<int>(polar.metadata.reynoldsDependency);
  request.imatyp = static_cast<int>(polar.metadata.machDependency);
  request.mach1 = polar.metadata.referenceMachNumber;
  request.reyn1 = polar.metadata.referenceReynoldsNumber;
  request.acrit = polar.metadata.nCritical;
  request.xtrip = polar.metadata.tripX;
  request.ptrat = polar.metadata.pressureParameter;
  request.etap = polar.metadata.etaParameter;

  const auto baseSelected = [&](PolarColumn column) {
    return std::find(polar.layout.baseColumns.begin(),
                     polar.layout.baseColumns.end(),
                     column) != polar.layout.baseColumns.end();
  };
  const auto value = [](const std::optional<double>& optional) {
    return optional.value_or(0.0);
  };
  for (std::size_t index = 0; index < polar.points.size(); ++index) {
    const PolarPoint& point = polar.points[index];
    io::PolarPoint row;
    if (baseSelected(PolarColumn::alpha)) {
      row.cpol[io::kIal - 1] = point.alpha.value;
    }
    row.cpol[io::kIcl - 1] = point.liftCoefficient;
    row.cpol[io::kIcd - 1] = point.dragCoefficient;
    row.cpol[io::kIcm - 1] = point.momentCoefficient;
    row.cpol[io::kIcw - 1] = value(point.dragCoefficientWave);
    row.cpol[io::kIcv - 1] = value(point.dragCoefficientViscous);
    row.cpol[io::kIcp - 1] = value(point.dragCoefficientPressure);
    row.cpol[io::kIma - 1] = value(point.machNumber);
    row.cpol[io::kIre - 1] = value(point.reynoldsNumber);
    row.cpol[io::kIch - 1] = value(point.hingeMoment);
    row.cpol[io::kImc - 1] =
        value(point.minimumPressureCoefficient);
    row.cpol[io::kIcdh - 1] = value(point.hingeDragCoefficient);
    row.cpol[io::kIcmdot - 1] =
        value(point.momentCoefficientDerivative);

    const auto sides = index < polar.sideData.size()
                           ? polar.sideData[index]
                           : std::array<PolarSideData, polarTextSideCount>{};
    for (std::size_t side = 0; side < polarTextSideCount; ++side) {
      row.side[side][io::kJnc - 1] = sides[side].nCritical;
      row.side[side][io::kJtp - 1] = sides[side].tripX;
      row.side[side][io::kJtn - 1] = sides[side].transitionX;
      row.side[side][io::kJti - 1] = sides[side].transitionIndex;
    }
    request.points.push_back(row);
  }
  if (polar.layout.elementSelection.count > 0 &&
      polar.elementData.size() == polar.points.size()) {
    request.elementData.reserve(polar.elementData.size());
    for (const auto& row : polar.elementData) {
      core::polar::PolarElementRow converted{};
      for (std::size_t i = 0; i < maxPolarTextElements; ++i) {
        converted[i].cx = row[i].cx;
        converted[i].cy = row[i].cy;
        converted[i].cm = row[i].cm;
        converted[i].cdVisc = row[i].cdVisc;
      }
      request.elementData.push_back(converted);
    }
  }

  const core::Result<std::string> encoded =
      io::writePolarTextChecked(request);
  if (encoded.isErr()) {
    return Result<std::string>::fail(coreError(encoded.error()));
  }
  return Result<std::string>::ok(encoded.value());
}

}  // namespace xfoil

// End: src/io/PolarDataText.cpp
