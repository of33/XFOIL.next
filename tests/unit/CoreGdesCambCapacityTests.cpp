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

// File: tests/unit/CoreGdesCambCapacityTests.cpp
// canonical capacity and error contracts of the
// GDES/CAMB paths. No magic
// values: all bounds come from xfoil::limits or the
// core constants.

#include "TestSupport.h"

#include "app/cli/CoreGeometryHandlers.h"
#include "xfoil/Limits.h"
#include "xfoil/core/geometry/GeometryTypes.h"
#include "xfoil/core/geometry_design/CamberSession.h"
#include "xfoil/core/paneling/PanelingTypes.h"
#include "xfoil/io/AddedCamberCodec.h"
#include "xfoil/session/SessionOps.h"
#include "xfoil/session/SessionState.h"

#include <cmath>
#include <sstream>
#include <string>
#include <vector>

namespace {

namespace cli = xfoil::app::cli;
namespace session = xfoil::session;

// contour with NB points and a sharp corner at index cornerIndex
// (kink of about 30 degrees) so that AREFINE with ATOL=5 adds points.
xfoil::core::geometry::AirfoilBuffer makeCornerBuffer(std::size_t nb,
                                                      std::size_t corner) {
  std::vector<double> x;
  std::vector<double> y;
  x.reserve(nb);
  y.reserve(nb);
  for (std::size_t i = 0; i < nb; ++i) {
    const double t = static_cast<double>(i) / static_cast<double>(nb - 1);
    const double angle = std::acos(-1.0) * t;
    const double xi = 0.5 * (1.0 - std::cos(angle));
    double yi = 0.2 * std::sin(angle);
    if (i > corner) {
      yi -= 0.05 * (t - static_cast<double>(corner) /
                                static_cast<double>(nb - 1));
    }
    x.push_back(xi);
    y.push_back(yi);
  }
  // Return path mirror (closed contour as in the buffer).
  const std::size_t half = nb / 2;
  for (std::size_t i = half; i >= 1; --i) {
    x.push_back(x[i - 1]);
    y.push_back(-y[i - 1]);
  }
  return xfoil::core::geometry::AirfoilBuffer(x, y, "CAP TEST");
}

}  // namespace

TEST(unit, CoreGdesExecCapacityBoundaryIqx5) {
  // NB = IQX-5 = 596 is still allowed; NB = 597 is rejected with the
  // four-line ABCOPY message. The valid panel contour remains unchanged
  // on rejection.
  const std::size_t limit =
      static_cast<std::size_t>(xfoil::core::paneling::kMaxAbcopyNodeCount);

  auto atLimit = makeCornerBuffer(398, 10);  // 597 points, trimmed to limit
  CHECK_EQ(atLimit.x.size(), limit + 1);
  atLimit.x.resize(limit);
  atLimit.y.resize(limit);
  auto sessionOk = std::make_unique<session::SessionComposition>();
  {
    std::ostringstream setup;
    testing::handleLegacyNacaCore(*sessionOk, setup, 12);
  }
  CHECK(session::setBufferGeometry(*sessionOk, std::move(atLimit)).success());
  const auto okStatus = session::executeGeometryDesignBuffer(*sessionOk);
  CHECK(okStatus.ok);
  CHECK_EQ(okStatus.bufferPointCount, limit);
  CHECK_EQ(sessionOk->paneling.panel.x.size(), limit);

  auto big = makeCornerBuffer(398, 10);  // 597 > 596
  CHECK_EQ(big.x.size(), limit + 1);
  auto sessionBig = std::make_unique<session::SessionComposition>();
  {
    std::ostringstream setup;
    testing::handleLegacyNacaCore(*sessionBig, setup, 12);
  }
  CHECK(session::setBufferGeometry(*sessionBig, std::move(big)).success());
  // The panel was invalidated by the buffer replacement; regenerate it from
  // the oversized buffer so atomic preservation of a valid panel can be
  // checked.
  {
    std::ostringstream paneOut;
    CHECK(cli::handlePaneCore(*sessionBig, paneOut) ==
          cli::PaneCoreStatus::applied);
  }
  const std::size_t panelBefore = sessionBig->paneling.panel.x.size();
  const bool panelValidBefore = sessionBig->paneling.panelingValid;
  const auto status = session::executeGeometryDesignBuffer(*sessionBig);
  CHECK(status.capacityExceeded);
  CHECK(!status.ok);
  CHECK_EQ(status.maxPointCount, limit);
  CHECK_EQ(sessionBig->paneling.panel.x.size(), panelBefore);
  CHECK_EQ(sessionBig->paneling.panelingValid, panelValidBefore);
  CHECK(!sessionBig->design.lgSame);
}

TEST(unit, CoreGdesExecHandlerCapacityTexts) {
  // Handler path: four-line message with list-directed field width 12.
  auto session = std::make_unique<session::SessionComposition>();
  {
    std::ostringstream setup;
    testing::handleLegacyNacaCore(*session, setup, 12);
    (void)session::setBufferGeometry(
        *session,
        makeCornerBuffer(398, 10));  // 597 > IQX-5
  }
  std::istringstream in("EXEC\n\n");
  std::ostringstream out;
  const auto status = cli::handleGeometryDesignCore(*session, in, out);
  CHECK(status == cli::GeometryDesignCoreStatus::returned);
  const std::string text = out.str();
  CHECK(text.find(" Maximum number of panel nodes  :") !=
        std::string::npos);
  CHECK(text.find(std::to_string(
            xfoil::core::paneling::kMaxAbcopyNodeCount)) !=
        std::string::npos);
  CHECK(text.find(" Number of buffer airfoil points:") !=
        std::string::npos);
  CHECK(text.find(" Current airfoil cannot be set.") != std::string::npos);
  CHECK(text.find(" Try executing PANE at Top Level instead.") !=
        std::string::npos);
}

TEST(unit, CoreCambInpcAcceptsFullTwoIqxPath) {
  // S02b enables the full 2*IQX=1202 INPC block now that NMAX=2500.
  auto session = std::make_unique<session::SessionComposition>();
  {
    std::ostringstream setup;
    testing::handleLegacyNacaCore(*session, setup, 12);
    (void)session::initializeGeometryDesign(*session);
  }
  std::ostringstream menu;
  const std::size_t kPoints = xfoil::limits::overlayCoordinateCapacity;
  CHECK_EQ(kPoints, std::size_t{1202});
  menu << "INPC\n";
  for (std::size_t i = 0; i < kPoints; ++i) {
    const double x = static_cast<double>(i) /
                     static_cast<double>(kPoints - 1);
    menu << x << " " << 0.01 * std::sin(6.0 * x) << "\n";
  }
  // At 1202 points the canonical DO loop ends without consuming a blank
  // terminator; the next line is the menu command. CLR resets added camber.
  menu << "CLR\n\n";
  std::istringstream in(menu.str());
  std::ostringstream out;
  const auto status = cli::handleCamberDesignCore(*session, in, out);
  CHECK(status == cli::CamberDesignCoreStatus::returned);
  CHECK(out.str().find("try again") == std::string::npos);
  // CLR resets the added camber (new 100 block, NCADD=2).
  CHECK(session->design.camber.has_value());
  CHECK_EQ(session->design.camber->ncadd, std::size_t{2});
}

TEST(unit, CoreCambRdacCodecPointLimitIsNtx) {
  // AddedCamberCodec: DO K=1,NTX (NTX=4808); pairs beyond that limit
  // are discarded with truncatedByPointLimit.
  std::ostringstream file;
  file << "camber file header\n";
  const std::size_t n = xfoil::io::kAddedCamberPointLimit + 5;
  for (std::size_t i = 0; i < n; ++i) {
    const double x = static_cast<double>(i) / static_cast<double>(n - 1);
    file << x << " " << 0.02 * std::sin(4.0 * x) << "\n";
  }
  std::istringstream in(file.str());
  const auto data = xfoil::io::decodeAddedCamberFile(in);
  CHECK(data.truncatedByPointLimit);
  CHECK_EQ(data.points.size(), xfoil::io::kAddedCamberPointLimit);
}

TEST(unit, CoreCambNcamConstantMatchesFortran) {
  // NCAM = MIN(201, NTX); NTX = 2*IBX = 8*IQX.
  CHECK_EQ(xfoil::core::geometry_design::kNcamMax, std::size_t{201});
  CHECK_EQ(xfoil::core::geometry_design::kNtx,
           2 * xfoil::limits::maxInputCoordinates);
  CHECK_EQ(xfoil::io::kAddedCamberPointLimit,
           xfoil::core::geometry_design::kNtx);
}

// End: tests/unit/CoreGdesCambCapacityTests.cpp
