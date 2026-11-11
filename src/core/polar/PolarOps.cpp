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

// File: src/core/polar/PolarOps.cpp
// Implementation of the deterministic polar operations.
// Direct port from fortran-src/xpol.f (PLRADD, PLRSRT, PLRCOP,
// PRFCOP, PLRSUM codec, PLRINI-/PLXINI comparison, PLRSET kernel, APCOPY kernel)
// and fortran-src/xoper.f (NAMMOD string kernel).

#include "xfoil/core/polar/PolarOps.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <span>
#include <string>
#include <vector>

#include "xfoil/core/geometry/LeadingEdge.h"
#include "xfoil/core/geometry/Transformations.h"
#include "xfoil/core/math/Sort.h"
#include "xfoil/core/math/Spline.h"
#include "xfoil/core/paneling/PanelGeometryKernels.h"

namespace xfoil::core::polar {
namespace {

// STRIP (userio.f:435): strips leading blanks and returns the non-blank share.
std::string stripBlanks(const std::string& text) {
  const std::size_t last = text.find_last_not_of(' ');
  if (last == std::string::npos) {
    return {};
  }
  const std::size_t first = text.find_first_not_of(' ');
  return text.substr(first, last - first + 1);
}

double columnValue(const PolarPoint& point, SortKey key) {
  switch (key) {
    case SortKey::alpha: return point.alpha;
    case SortKey::cl: return point.cl;
    case SortKey::cd: return point.cd;
    case SortKey::cm: return point.cm;
    case SortKey::cdWave: return point.cdWave;
    case SortKey::cdVisc: return point.cdVisc;
    case SortKey::cdPres: return point.cdPres;
    case SortKey::mach: return point.mach;
    case SortKey::re: return point.re;
    case SortKey::hinge: return point.hinge;
    case SortKey::cpMin: return point.cpMin;
    case SortKey::cdh: return point.cdh;
    case SortKey::cmDot: return point.cmDot;
  }
  return 0.0;
}

}  // namespace

Status addPoint(PolarDataset& polar, const PolarPointInput& input) {
  // PLRADD: NAPOL(IP) == NAX -> "Polar storage arrays full. Point not
  // stored"; IP == 0 -> "No active polar is declared". Both cases without
  // partial mutation.
  if (polar.points.size() >= maxPolarPoints) {
    return Status::fail(
        Error::make(ErrorCode::capacityExceeded,
                    "PLRADD: Polar storage arrays full (NAX=800)"));
  }
  if (!polar.elementData.empty() &&
      polar.elementData.size() != polar.points.size()) {
    return Status::fail(Error::make(
        ErrorCode::sizeMismatch,
        "PLRADD: element data count does not fit"));
  }

  PolarPoint point;
  point.alpha = input.alphaDeg;
  point.cl = input.cl;
  point.cd = input.viscous ? input.cd : 0.0;
  point.cm = input.cm;
  point.cdWave = 0.0;
  point.cdVisc = input.viscous ? input.cd : 0.0;
  point.cdPres = input.cdPres;
  point.mach = input.mach;
  point.re = input.viscous ? input.reInf : 0.0;
  point.hinge = input.hingeMoment;
  point.cpMin = input.cpMin;
  point.cdh = 0.0;
  point.cmDot = 0.0;

  std::array<PolarSideData, sideCount> sides{};
  for (std::size_t is = 0; is < sideCount; ++is) {
    sides[is].ncrit = input.acrit[is];
    sides[is].xtrip = input.xtrip[is];
    sides[is].xtr = input.viscous ? input.xoctr[is] : 0.0;
    sides[is].itr = input.tindex[is];
  }

  if (polar.elementData.empty() && !polar.points.empty()) {
    polar.elementData.resize(polar.points.size());
  }
  polar.points.push_back(point);
  polar.sideData.push_back(sides);
  polar.elementData.emplace_back();
  return Status::ok();
}

Status sortPoints(PolarDataset& polar, SortKey key) {
  const std::size_t n = polar.points.size();
  if (n == 0) {
    return Status::ok();
  }
  if (polar.sideData.size() != n) {
    return Status::fail(Error::make(ErrorCode::sizeMismatch,
                                    "PLRSRT: side data count does not fit"));
  }
  const bool haveElementData = !polar.elementData.empty();
  if (haveElementData && polar.elementData.size() != n) {
    return Status::fail(Error::make(ErrorCode::sizeMismatch,
                                    "PLRSRT: element data count does not fit"));
  }

  // HSORT (sort.f) returns the canonical Permutation INDX.
  std::vector<double> keys(n);
  for (std::size_t i = 0; i < n; ++i) {
    keys[i] = columnValue(polar.points[i], key);
  }
  std::vector<int> indx(n);
  math::hSort(std::span<const double>(keys), std::span<int>(indx));

  // ASORT applies the same permutation to all columns; here the
  // shared permutation is applied to the point and page records.
  std::vector<PolarPoint> sortedPoints(n);
  std::vector<std::array<PolarSideData, sideCount>> sortedSides(n);
  std::vector<PolarElementRow> sortedElements(haveElementData ? n : 0);
  for (std::size_t i = 0; i < n; ++i) {
    const int source = indx[i];
    if (source < 0 || static_cast<std::size_t>(source) >= n) {
      return Status::fail(Error::make(ErrorCode::outOfRange,
                                      "PLRSRT: Sortierindex except Range"));
    }
    const auto sourceIndex = static_cast<std::size_t>(source);
    sortedPoints[i] = polar.points[sourceIndex];
    sortedSides[i] = polar.sideData[sourceIndex];
    if (haveElementData) {
      sortedElements[i] = polar.elementData[sourceIndex];
    }
  }
  polar.points.swap(sortedPoints);
  polar.sideData.swap(sortedSides);
  if (haveElementData) {
    polar.elementData.swap(sortedElements);
  }
  return Status::ok();
}

Status copySlot(const PolarDataset& source, PolarDataset& destination) {
  // PLRCOP: header, points, page data, contour. Self-copy is
  // safe due to value-semantic assignment.
  destination = source;
  return Status::ok();
}

Status copyReferenceSlot(const ReferencePolarDataset& source,
                         ReferencePolarDataset& destination) {
  // PRFCOP: Name, NDREF(1..4), CPOLREF(IA,IS,K).
  destination = source;
  return Status::ok();
}

Result<std::string> applyVersionTag(const std::string& name, int kDel,
                                    int kMod0) {
  // NAMMOD (xoper.f:1831), deterministic kernel.
  std::string base = stripBlanks(name);
  const std::size_t kbrack1 = base.find('[');
  const std::size_t kbrack2 = base.find(']');

  int kmodp = 0;
  // NAMDEF keeps the original names canonical if no KMODP branch
  // applies (xoper.f:1846,1873-1884).
  const std::string original = base;
  std::string namdef = original;
  const bool brackets = kbrack1 != std::string::npos &&
                        kbrack2 != std::string::npos &&
                        static_cast<int>(kbrack2) - static_cast<int>(kbrack1) >
                            1;
  if (brackets) {
    const std::string inner =
        stripBlanks(base.substr(kbrack1 + 1, kbrack2 - kbrack1 - 1));
    int kmod = 0;
    const char* first = inner.data();
    const char* last = inner.data() + inner.size();
    const std::from_chars_result parsed =
        std::from_chars(first, last, kmod);
    if (parsed.ec != std::errc{} || parsed.ptr != last) {
 // READ(*,*,ERR=40): the prompt fallback is.
      return Result<std::string>::fail(
          Error::make(ErrorCode::invalidArgument,
                      "NAMMOD: bracket content is not a valid integer"));
    }
    kmod = std::abs(kmod);
    // Fortran MOD(KMOD+KDEL,100): the sign follows the dividend.
    kmodp = (kmod + kDel) % 100;
    if (kbrack1 >= 1) {
      base = stripBlanks(base.substr(0, kbrack1));
    } else {
      base.clear();
    }
  } else if (kMod0 > 0) {
    kmodp = kMod0 % 100;
  } else {
    kmodp = 0;
  }

  if (kmodp >= 10) {
    // FORMAT(I2) at positions NNAME+3..NNAME+4.
    namdef = base + " [  ]";
    const std::string digits = std::to_string(kmodp);
    namdef[base.size() + 2] = digits[0];
    namdef[base.size() + 3] = digits[1];
  } else if (kmodp >= 1) {
    // FORMAT(I1) at position NNAME+3.
    namdef = base + " [ ]";
    namdef[base.size() + 2] = static_cast<char>('0' + kmodp);
  }
  return Result<std::string>::ok(namdef);
}

ReEncoding encodeReMantissaExponent(double re) {
  // PLRSUM (xpol.f:793-799).
  if (re > 0.0) {
    int iexp = static_cast<int>(std::log10(re));
    iexp = std::max(0, std::min(iexp, 9));
    double power = 1.0;
    for (int i = 0; i < iexp; ++i) {
      power *= 10.0;
    }
    return ReEncoding{re / power, iexp};
  }
  return ReEncoding{0.0, 0};
}

SaveHeaderComparison compareSaveHeader(const PolarMetadata& fileHeader,
                                       const SessionParameters& current) {
  // PLRINI (xpol.f:246-265).
  SaveHeaderComparison cmp;
  cmp.nameDiffers = stripBlanks(fileHeader.airfoilName) !=
                    stripBlanks(current.name);
  cmp.reDiffers = fileHeader.reCl1 != current.reinf1;
  cmp.machDiffers = fileHeader.machCl1 != current.minf1;
  cmp.reTypeDiffers =
      static_cast<int>(fileHeader.reType) != static_cast<int>(current.reType);
  cmp.machTypeDiffers = static_cast<int>(fileHeader.machType) !=
                        static_cast<int>(current.machType);
  cmp.ncritDiffers = fileHeader.ncrit != current.acrit;
  cmp.xtripDiffers = fileHeader.xtrip != current.xtrip;
  return cmp;
}

DumpHeaderComparison compareDumpHeader(const DumpHeader& fileHeader,
                                       const PolarMetadata& current) {
  // PLXINI (xpol.f:450-467); REYNX is already with 1e6 scaled.
  DumpHeaderComparison cmp;
  cmp.nameDiffers = stripBlanks(fileHeader.name) !=
                    stripBlanks(current.airfoilName);
  cmp.reDiffers = fileHeader.re != current.reCl1;
  cmp.machDiffers = fileHeader.mach != current.machCl1;
  cmp.reTypeDiffers = fileHeader.retyp != static_cast<int>(current.reType);
  cmp.machTypeDiffers = fileHeader.matyp != static_cast<int>(current.machType);
  cmp.ncritDiffers = fileHeader.ncrit != current.ncrit;
  return cmp;
}

SlotSetupResult prepareAppendSlot(const PolarDataset* existing,
                                  const SessionParameters& current,
                                  const PolarGeometrySnapshot& currentGeometry) {
  // PLRSET (xpol.f:22-136), deterministic kernel without ASKI/stdout.
  SlotSetupResult result;
  if (existing == nullptr) {
    // New polar slot (xpol.f:102-133).
    result.kind = SlotSetupResult::Kind::newSlot;
    result.slot.metadata.airfoilName = current.name;
    result.slot.metadata.reType = current.reType;
    result.slot.metadata.machType = current.machType;
    result.slot.metadata.reCl1 = current.viscous ? current.reinf1 : 0.0;
    result.slot.metadata.machCl1 = current.minf1;
    result.slot.metadata.ncrit = current.acrit;
    result.slot.metadata.xtrip = current.xtrip;
    result.slot.geometry = currentGeometry;
    return result;
  }

  // Geometry comparison (xpol.f:56-63): N must match, afterwards
  // DSFRAC = sqrt(dx^2+dy^2)/(S(N)-S(1)) <= 1e-5 for all points.
  const PolarGeometrySnapshot& stored = existing->geometry;
  if (stored.size() != currentGeometry.size()) {
    result.kind = SlotSetupResult::Kind::geometryConflict;
    return result;
  }
  const std::size_t n = currentGeometry.size();
  std::vector<double> s(n, 0.0);
  if (n >= 1) {
    math::scalc(std::span<const double>(currentGeometry.x),
                std::span<const double>(currentGeometry.y),
                std::span<double>(s));
  }
  const double sizref = n >= 1 ? s[n - 1] - s[0] : 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    const double dx = currentGeometry.x[i] - stored.x[i];
    const double dy = currentGeometry.y[i] - stored.y[i];
    const double dsfrac = std::sqrt(dx * dx + dy * dy) / sizref;
    if (dsfrac > 0.00001) {
      result.kind = SlotSetupResult::Kind::geometryConflict;
      return result;
    }
  }

  // Parameter adoption (xpol.f:87-100).
  result.kind = SlotSetupResult::Kind::adoptStoredParameters;
  result.adopted.name = existing->metadata.airfoilName;
  result.adopted.reType = existing->metadata.reType;
  result.adopted.machType = existing->metadata.machType;
  result.adopted.minf1 = existing->metadata.machCl1;
  result.adopted.reinf1 = existing->metadata.reCl1;
  result.adopted.acrit = existing->metadata.ncrit;
  result.adopted.xtrip = existing->metadata.xtrip;
  result.adopted.viscous = current.viscous;
  return result;
}

Result<GeometryReconstruction> restoreArchivedGeometry(
    const PolarGeometrySnapshot& snapshot, std::size_t oldN,
    const geometry::TrailingEdgeInputs& teInputs) {
  // APCOPY (xpol.f:139-176) with the confirmed core kernels.
  const std::size_t n = snapshot.size();
  if (snapshot.x.size() != snapshot.y.size()) {
    return Result<GeometryReconstruction>::fail(
        Error::make(ErrorCode::sizeMismatch, "APCOPY: x/y lengths diverge"));
  }
  if (n < 3) {
    // LEFIND requires N >= 3; Fortran would read out of bounds.
    return Result<GeometryReconstruction>::fail(
        Error::make(ErrorCode::invalidArgument,
                    "APCOPY: Contour with N < 3 rejected"));
  }

  GeometryReconstruction rec;
  rec.n = n;
  rec.x.assign(snapshot.x.begin(), snapshot.x.end());
  rec.y.assign(snapshot.y.begin(), snapshot.y.end());
  rec.s.assign(n, 0.0);
  rec.xp.assign(n, 0.0);
  rec.yp.assign(n, 0.0);
  rec.nx.assign(n, 0.0);
  rec.ny.assign(n, 0.0);

  math::scalc(std::span<const double>(rec.x), std::span<const double>(rec.y),
              std::span<double>(rec.s));
  math::segspl(std::span<const double>(rec.x), std::span<const double>(rec.s),
               std::span<double>(rec.xp));
  math::segspl(std::span<const double>(rec.y), std::span<const double>(rec.s),
               std::span<double>(rec.yp));
  paneling::ncalc(std::span<const double>(rec.x), std::span<const double>(rec.y),
                  std::span<const double>(rec.s), std::span<double>(rec.nx),
                  std::span<double>(rec.ny));

  const geometry::LeadingEdgeResult le =
      geometry::lefind(std::span<const double>(rec.x),
                       std::span<const double>(rec.xp),
                       std::span<const double>(rec.y),
                       std::span<const double>(rec.yp),
                       std::span<const double>(rec.s));
  rec.sle = le.sle;
  rec.xle = math::seval(rec.sle, std::span<const double>(rec.x),
                        std::span<const double>(rec.xp),
                        std::span<const double>(rec.s));
  rec.yle = math::seval(rec.sle, std::span<const double>(rec.y),
                        std::span<const double>(rec.yp),
                        std::span<const double>(rec.s));
  rec.xte = 0.5 * (rec.x[0] + rec.x[n - 1]);
  rec.yte = 0.5 * (rec.y[0] + rec.y[n - 1]);
  rec.chord = std::sqrt((rec.xte - rec.xle) * (rec.xte - rec.xle) +
                        (rec.yte - rec.yle) * (rec.yte - rec.yle));

  geometry::TrailingEdgeInputs teIn = teInputs;
  teIn.x0 = rec.x[0];
  teIn.y0 = rec.y[0];
  teIn.xp0 = rec.xp[0];
  teIn.yp0 = rec.yp[0];
  teIn.xn = rec.x[n - 1];
  teIn.yn = rec.y[n - 1];
  teIn.xpn = rec.xp[n - 1];
  teIn.ypn = rec.yp[n - 1];
  rec.te = geometry::tecalc(teIn);

  rec.apanel = paneling::apcalc(std::span<const double>(rec.x),
                                std::span<const double>(rec.y),
                                std::span<const double>(rec.nx),
                                std::span<const double>(rec.ny), rec.te.sharp);

  rec.resetBoundaryLayerInit = (oldN != n);
  return Result<GeometryReconstruction>::ok(std::move(rec));
}

}  // namespace xfoil::core::polar

// End: src/core/polar/PolarOps.cpp
