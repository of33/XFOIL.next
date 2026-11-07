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

// File: src/core/XFoilFacade.cpp
// Implementation of the facade class xfoil::XFoil. The facade
// owns an XFoil session through a Pimpl and delegates exclusively to it.
// There is no additional solver logic and no raw pointers.

#include "xfoil/XFoil.h"

#include <utility>

namespace xfoil {

class XFoil::Impl {
 public:
  XFoilSession session;
};

XFoil::XFoil() : impl_(std::make_unique<Impl>()) {}

// Out of line because std::unique_ptr<Impl> holds an incomplete Impl in the header.
XFoil::~XFoil() = default;

// Shifts only the unique_ptr; therefore noexcept.
XFoil::XFoil(XFoil&&) noexcept = default;

XFoil& XFoil::operator=(XFoil&&) noexcept = default;

GeometrySetResult XFoil::setGeometry(
    geometry::AirfoilGeometry geometry) {
  return impl_->session.setGeometry(std::move(geometry));
}

ConfigValidationResult XFoil::configure(const AnalysisConfig& config) {
  return impl_->session.configure(config);
}

AnalysisResult XFoil::analyzeAtAlpha(Degrees alpha) {
  return impl_->session.analyzeAtAlpha(alpha);
}

AnalysisResult XFoil::analyzeAtAlpha(
    Degrees alpha,
    const CancellationToken& cancellation) {
  return impl_->session.analyzeAtAlpha(alpha, cancellation);
}

std::vector<AnalysisResult> XFoil::analyzePolar(
    Degrees start,
    Degrees endExclusive,
    Degrees step) {
  return impl_->session.analyzePolar(start, endExclusive, step);
}

std::vector<AnalysisResult> XFoil::analyzePolar(
    Degrees start,
    Degrees endExclusive,
    Degrees step,
    const CancellationToken& cancellation) {
  return impl_->session.analyzePolar(start, endExclusive, step, cancellation);
}

const geometry::AirfoilGeometry& XFoil::geometry() const noexcept {
  return impl_->session.geometry();
}

const AnalysisConfig& XFoil::config() const noexcept {
  return impl_->session.config();
}

const AnalysisResult& XFoil::lastResult() const noexcept {
  return impl_->session.lastResult();
}

bool XFoil::hasGeometry() const noexcept {
  return impl_->session.hasGeometry();
}

bool XFoil::hasConfiguration() const noexcept {
  return impl_->session.hasConfiguration();
}

}  // namespace xfoil

// End: src/core/XFoilFacade.cpp
