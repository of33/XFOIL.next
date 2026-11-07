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

// File: src/session/XFoilSession.cpp
// Implementation of the public session API. The session
// owns the domain-separated session composition and delegates
// exclusively to the session/use-case operations
// (xfoil::session::SessionOps). No app dependency, no static solver state. The move
// is trivial (all part states are movable value types); no
// diagnostic stream is bound; the core paths deliver
// structured diagnostics.

#include "xfoil/XFoilSession.h"

#include <utility>

#include "xfoil/session/SessionOps.h"

namespace xfoil {

XFoilSession::XFoilSession() = default;

XFoilSession::~XFoilSession() = default;

// Trivial move: all part states are movable value types; no
// bound diagnostic stream that would need rebinding.
XFoilSession::XFoilSession(XFoilSession&&) = default;

XFoilSession& XFoilSession::operator=(XFoilSession&&) = default;

GeometrySetResult XFoilSession::setGeometry(
    geometry::AirfoilGeometry geometry) {
  return session::setGeometry(state_, std::move(geometry));
}

ConfigValidationResult XFoilSession::configure(const AnalysisConfig& config) {
  return session::configure(state_, config);
}

AnalysisResult XFoilSession::analyzeAtAlpha(Degrees alpha) {
  return session::runSpecAlpha(state_, alpha, nullptr);
}

AnalysisResult XFoilSession::analyzeAtAlpha(
    Degrees alpha,
    const CancellationToken& cancellation) {
  return session::runSpecAlpha(state_, alpha, &cancellation);
}

std::vector<AnalysisResult> XFoilSession::analyzePolar(
    Degrees start,
    Degrees endExclusive,
    Degrees step) {
  return session::runAlphaSequence(state_, start, endExclusive, step, nullptr);
}

std::vector<AnalysisResult> XFoilSession::analyzePolar(
    Degrees start,
    Degrees endExclusive,
    Degrees step,
    const CancellationToken& cancellation) {
  return session::runAlphaSequence(state_, start, endExclusive, step,
                                   &cancellation);
}

const geometry::AirfoilGeometry& XFoilSession::geometry() const noexcept {
  return state_.geometry.airfoil;
}

const AnalysisConfig& XFoilSession::config() const noexcept {
  return state_.config;
}

const AnalysisResult& XFoilSession::lastResult() const noexcept {
  return state_.result.lastResult;
}

bool XFoilSession::hasGeometry() const noexcept {
  return state_.geometry.valid;
}

bool XFoilSession::hasConfiguration() const noexcept {
  return state_.operating.configured;
}

}  // namespace xfoil

// End: src/session/XFoilSession.cpp
