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

// File: include/xfoil/io/ThicknessCamberCodec.h
// Self-contained IO codec for CAMB-WRTT/WRTC (xtcam.f 257-328):
// normalized thickness/camber distributions in format (1X,3G15.7).
// No session or CLI dependency.
#ifndef XFOIL_IO_THICKNESSCAMBERCODEC_H_
#define XFOIL_IO_THICKNESSCAMBERCODEC_H_

#include <span>
#include <string>

namespace xfoil::io {

// name is the canonical CHARACTER*48 profile-name field; the codec
// truncates to 48 characters and pads to 48 (format A writes the
// full field length including edge blanks).
std::string fortranNameField48(const std::string& name);

// WRTT (xtcam.f 279-291): header 'Thickness: '//Name, then per point
// XTN = (XTK(K)-XTK(1))/XBCH
// YTN = (YTK(K)-YTK(1))/XBCH
// DCDX = (YTK(KP)-YTK(KM))/(XTK(KP)-XTK(KM)), KM=MAX(1,K-1),
// KP=MIN(NTK,K+1)
// as Format(1X,3G15.7).
std::string thicknessDistributionText(
    const std::string& name, double xbch,
    std::span<const double> xtk, std::span<const double> ytk);

// WRTC (xtcam.f 313-318): header 'Camber: '//Name, then per point only
// XCN = (XCM(K)-XCM(1))/XBCH, YCN = (YCM(K)-YCM(1))/XBCH
// with Format(1X,3G15.7); the third format item is not used, the
// line ends after the second value (no third column).
std::string camberDistributionText(
    const std::string& name, double xbch,
    std::span<const double> xcm, std::span<const double> ycm);

}  // namespace xfoil::io

#endif  // XFOIL_IO_THICKNESSCAMBERCODEC_H_
