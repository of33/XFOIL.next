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

// File: tests/unit/LibraryDemoL9Tests.cpp
// L9 demo-local parser, profile reader, formatting and exit-code tests.

#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "DemoSupport.h"
#include "TestSupport.h"

namespace {

using xfoil_demo::ExitCode;
using xfoil_demo::ParseResult;
using xfoil_demo::ParseStatus;
using xfoil_demo::ProfileResult;
using xfoil_demo::ProfileStatus;

ParseResult parse(std::initializer_list<std::string_view> arguments) {
  std::vector<std::string> storage;
  storage.reserve(arguments.size());
  for (const std::string_view argument : arguments) {
    storage.emplace_back(argument);
  }
  std::vector<const char*> argv;
  argv.reserve(storage.size() + 1);
  argv.push_back("xfoil-next-lib-demo");
  for (const std::string& argument : storage) {
    argv.push_back(argument.c_str());
  }
  return xfoil_demo::parseArguments(
      static_cast<int>(argv.size()), argv.data());
}

bool hasText(const std::string& text, std::string_view needle) {
  return text.find(needle) != std::string::npos;
}

xfoil::SkinFrictionDistribution validFriction(
    std::size_t upperCount, std::size_t lowerCount) {
  xfoil::SkinFrictionDistribution distribution;
  distribution.upper.side = xfoil::SurfaceSide::upper;
  distribution.lower.side = xfoil::SurfaceSide::lower;
  distribution.upper.x.resize(upperCount);
  distribution.upper.y.resize(upperCount);
  distribution.upper.s.resize(upperCount);
  distribution.upper.cf.resize(upperCount);
  distribution.lower.x.resize(lowerCount);
  distribution.lower.y.resize(lowerCount);
  distribution.lower.s.resize(lowerCount);
  distribution.lower.cf.resize(lowerCount);
  return distribution;
}

}  // namespace

TEST(unit, LibraryDemoL9ArgumentContract) {
  const ParseResult result = parse({
      "--airfoil", "profile.dat",
      "--re", "1000000",
      "--alpha-start", "-2",
      "--alpha-end", "8",
      "--alpha-step", "1",
      "--cl", "0.6",
      "--mach", "0.1",
      "--iterations", "150",
      "--panels", "200"});
  CHECK(result.status == ParseStatus::ok);
  CHECK_EQ(result.options.airfoilPath, std::string{"profile.dat"});
  CHECK_EQ(result.options.reynoldsNumber, 1000000.0);
  CHECK_EQ(result.options.alphaStart, -2.0);
  CHECK_EQ(result.options.alphaEndExclusive, 8.0);
  CHECK_EQ(result.options.alphaStep, 1.0);
  CHECK_EQ(result.options.targetLiftCoefficient, 0.6);
  CHECK_EQ(result.options.machNumber.value(), 0.1);
  CHECK_EQ(result.options.maximumIterations.value(), 150);
  CHECK_EQ(result.options.panelCount.value(), std::size_t{200});
}

TEST(unit, LibraryDemoL9ArgumentErrors) {
  CHECK(parse({"--help"}).status == ParseStatus::help);
  CHECK(parse({"--viscous", "yes"}).status == ParseStatus::error);
  CHECK(parse({"--airfoil", "p", "--re", "0", "--alpha-start", "0",
               "--alpha-end", "1", "--alpha-step", "1"}).status ==
        ParseStatus::error);
  CHECK(parse({"--airfoil", "p", "--re", "0", "--re", "1",
               "--alpha-start", "0", "--alpha-end", "1", "--alpha-step",
               "1", "--cl", "0.2"}).status == ParseStatus::error);
  CHECK(parse({"--airfoil", "p", "--re", "0", "--alpha-start", "0",
               "--alpha-end", "1", "--alpha-step", "1", "--cl", "nan"})
            .status == ParseStatus::error);
  CHECK(parse({"--airfoil", "p", "--re", "inf", "--alpha-start", "0",
               "--alpha-end", "1", "--alpha-step", "1", "--cl", "0.2"})
            .status == ParseStatus::error);
  CHECK(parse({"--airfoil", "p", "--re", "-1", "--alpha-start", "0",
               "--alpha-end", "1", "--alpha-step", "1", "--cl", "0.2"})
            .status == ParseStatus::error);
  CHECK(parse({"--airfoil", "p", "--re", "0", "--alpha-start", "0",
               "--alpha-end", "1", "--alpha-step", "0", "--cl", "0.2"})
            .status == ParseStatus::error);
  CHECK(parse({"--airfoil", "p", "--re", "0", "--alpha-start", "0",
               "--alpha-end", "1", "--alpha-step", "-1", "--cl", "0.2"})
            .status == ParseStatus::error);
  CHECK(parse({"--airfoil", "p", "--re", "0", "--alpha-start", "1",
               "--alpha-end", "1", "--alpha-step", "1", "--cl", "0.2"})
            .status == ParseStatus::error);
  CHECK(parse({"--airfoil", "p", "--re", "0", "--alpha-start", "0",
               "--alpha-end", "1", "--alpha-step", "1", "--cl", "0.2",
               "--bogus"}).status == ParseStatus::error);
  CHECK(parse({"--airfoil", "p", "--re", "0", "--alpha-start", "0",
               "--alpha-end", "1", "--alpha-step", "1", "--cl", "0.2",
               "--mach", "0.1", "--mach", "0.2"}).status ==
        ParseStatus::error);
}

TEST(unit, LibraryDemoL9OptionalArguments) {
  const ParseResult defaults = parse({
      "--airfoil", "p", "--re", "0", "--alpha-start", "0",
      "--alpha-end", "2", "--alpha-step", "1", "--cl", "0.1"});
  CHECK(defaults.status == ParseStatus::ok);
  CHECK_FALSE(defaults.options.machNumber.has_value());
  CHECK_FALSE(defaults.options.maximumIterations.has_value());
  CHECK_FALSE(defaults.options.panelCount.has_value());

  CHECK(parse({"--airfoil", "p", "--re", "0", "--alpha-start", "0",
               "--alpha-end", "2", "--alpha-step", "1", "--cl", "0.1",
               "--iterations", "0"}).status == ParseStatus::error);
  CHECK(parse({"--airfoil", "p", "--re", "0", "--alpha-start", "0",
               "--alpha-end", "2", "--alpha-step", "1", "--cl", "0.1",
               "--iterations", "-1"}).status == ParseStatus::error);
  CHECK(parse({"--airfoil", "p", "--re", "0", "--alpha-start", "0",
               "--alpha-end", "2", "--alpha-step", "1", "--cl", "0.1",
               "--iterations", "1.5"}).status == ParseStatus::error);
  CHECK(parse({"--airfoil", "p", "--re", "0", "--alpha-start", "0",
               "--alpha-end", "2", "--alpha-step", "1", "--cl", "0.1",
               "--iterations", "999999999999999999999999"}).status ==
        ParseStatus::error);
  CHECK(parse({"--airfoil", "p", "--re", "0", "--alpha-start", "0",
               "--alpha-end", "2", "--alpha-step", "1", "--cl", "0.1",
               "--panels", "0"}).status == ParseStatus::error);
  CHECK(parse({"--airfoil", "p", "--re", "0", "--alpha-start", "0",
               "--alpha-end", "2", "--alpha-step", "1", "--cl", "0.1",
               "--panels", "-4"}).status == ParseStatus::error);
  CHECK(parse({"--airfoil", "p", "--re", "0", "--alpha-start", "0",
               "--alpha-end", "2", "--alpha-step", "1", "--cl", "0.1",
               "--panels", "1.5"}).status == ParseStatus::error);
  CHECK(parse({"--airfoil", "p", "--re", "0", "--alpha-start", "0",
               "--alpha-end", "2", "--alpha-step", "1", "--cl", "0.1",
               "--panels", "999999999999999999999999"}).status ==
        ParseStatus::error);
}

TEST(unit, LibraryDemoL9ViscousFromReynolds) {
  const ParseResult inviscid = parse({
      "--airfoil", "p", "--re", "0", "--alpha-start", "0",
      "--alpha-end", "2", "--alpha-step", "1", "--cl", "0.1"});
  CHECK(inviscid.status == ParseStatus::ok);
  CHECK_FALSE(xfoil_demo::usesViscousPath(inviscid.options));

  const ParseResult negativeZero = parse({
      "--airfoil", "p", "--re", "-0", "--alpha-start", "0",
      "--alpha-end", "2", "--alpha-step", "1", "--cl", "0.1"});
  CHECK(negativeZero.status == ParseStatus::ok);
  CHECK_EQ(negativeZero.options.reynoldsNumber, 0.0);
  CHECK_FALSE(xfoil_demo::usesViscousPath(negativeZero.options));

  const ParseResult viscous = parse({
      "--airfoil", "p", "--re", "1000000", "--alpha-start", "0",
      "--alpha-end", "2", "--alpha-step", "1", "--cl", "0.1"});
  CHECK(viscous.status == ParseStatus::ok);
  CHECK(xfoil_demo::usesViscousPath(viscous.options));
}

TEST(unit, LibraryDemoL9ProfileContract) {
  const ProfileResult named = xfoil_demo::parseProfileText(
      "# comment\n\nNACA 0012\n# data\n1 0\n0.5 0.1\n0 0\n", "fallback");
  CHECK(named.status == ProfileStatus::ok);
  CHECK_EQ(named.profile.name, std::string{"NACA 0012"});
  CHECK_EQ(named.profile.points.size(), std::size_t{3});
  CHECK_EQ(named.profile.points[1].x, 0.5);
  CHECK_EQ(named.profile.points[1].y, 0.1);

  const ProfileResult unnamed = xfoil_demo::parseProfileText(
      "1 0\n0.5 0.1\n0 0\n", "fallback-name");
  CHECK(unnamed.status == ProfileStatus::ok);
  CHECK_EQ(unnamed.profile.name, std::string{"fallback-name"});

  const ProfileResult crlf = xfoil_demo::parseProfileText(
      "CRLF 0012\r\n1 0\r\n0.5 0.1\r\n0 0\r\n", "fallback");
  CHECK(crlf.status == ProfileStatus::ok);
  CHECK_EQ(crlf.profile.name, std::string{"CRLF 0012"});
  CHECK_EQ(crlf.profile.points.size(), std::size_t{3});
}

TEST(unit, LibraryDemoL9ProfileErrors) {
  CHECK(xfoil_demo::parseProfileText(
            "NAME\n1 0 2\n0.5 0.1\n0 0\n", "fallback").status ==
        ProfileStatus::parseError);
  CHECK(xfoil_demo::parseProfileText(
            "1 2 3\n1 0\n0 0\n", "fallback").status ==
        ProfileStatus::parseError);
  CHECK(xfoil_demo::parseProfileText(
            "NAME\n1 0\nx 0\n0 0\n", "fallback").status ==
        ProfileStatus::parseError);
  CHECK(xfoil_demo::parseProfileText(
            "NAME\nnan 0\n0.5 0.1\n0 0\n", "fallback").status ==
        ProfileStatus::parseError);
  CHECK(xfoil_demo::parseProfileText(
            "nan 0\n0.5 0.1\n0 0\n", "fallback").status ==
        ProfileStatus::parseError);
  CHECK(xfoil_demo::parseProfileText(
            "NAME\n1 0\n0 0\n", "fallback").status ==
        ProfileStatus::parseError);
  CHECK(xfoil_demo::readProfileFile(
            "l9-profile-does-not-exist.dat").status ==
        ProfileStatus::fileError);
}

TEST(unit, LibraryDemoL9OutputAndStatus) {
  xfoil_demo::Options options;
  options.alphaStart = -2.0;
  options.alphaEndExclusive = 8.0;
  options.alphaStep = 1.0;
  xfoil_demo::Profile profile;
  profile.name = "NACA 0012";
  xfoil::AnalysisConfig config;
  config.reynoldsNumber = 1000000.0;
  config.viscous = true;
  config.machNumber = 0.1;
  config.maximumIterations = 150;
  xfoil::PanelingOptions paneling;
  paneling.panelCount = 200;
  const std::string header =
      xfoil_demo::formatHeader(profile, config, paneling, options);
  CHECK(hasText(header, "Airfoil: NACA 0012"));
  CHECK(hasText(header, "Mode: viscous"));
  CHECK(hasText(header, "Reynolds: 1000000"));
  CHECK(hasText(header, "Mach: 0.1"));
  CHECK(hasText(header, "Iterations: 150"));
  CHECK(hasText(header, "Panels: 200"));
  CHECK(hasText(header, "Alpha interval: [-2, 8), step 1"));

  CHECK(hasText(xfoil_demo::sequenceTableHeader(), "CDF"));
  CHECK(hasText(xfoil_demo::sequenceTableHeader(), "CDv"));
  CHECK(hasText(xfoil_demo::clTableHeader(), "CDF"));
  xfoil::PolarPoint point;
  point.alpha = xfoil::Degrees{1.25};
  point.liftCoefficient = 0.5;
  point.dragCoefficient = 0.01;
  point.dragCoefficientPressure = 0.002;
  point.dragCoefficientFriction = 0.004;
  point.dragCoefficientViscous = 0.123456;
  point.momentCoefficient = -0.03;
  point.iterations = std::size_t{6};
  point.convergence = xfoil::ConvergenceStatus::converged;
  const std::string sequenceRow = xfoil_demo::formatSequenceRow(point);
  CHECK(hasText(sequenceRow, "1.2500"));
  CHECK(hasText(sequenceRow, "0.500000"));
  CHECK(hasText(sequenceRow, "0.002000"));
  CHECK(hasText(sequenceRow, "0.004000"));
  CHECK(hasText(sequenceRow, "0.123456"));
  CHECK(hasText(sequenceRow, "6"));
  CHECK(hasText(sequenceRow, "converged"));

  xfoil::OperatingPointData clPoint;
  clPoint.alpha = xfoil::Degrees{3.3114};
  clPoint.liftCoefficient = 0.4;
  clPoint.viscous = true;
  clPoint.dragCoefficient = 0.020;
  clPoint.dragCoefficientPressure = 0.005;
  clPoint.dragCoefficientFriction = 0.004;
  clPoint.momentCoefficient = -0.01;
  clPoint.iterations = 7;
  clPoint.convergence = xfoil::ConvergenceStatus::converged;
  clPoint.skinFriction = validFriction(3, 2);
  const std::string clRow = xfoil_demo::formatClRow(0.4, clPoint);
  CHECK(hasText(clRow, "0.4000"));
  CHECK(hasText(clRow, "3.3114"));
  CHECK(hasText(clRow, "0.400000"));
  CHECK(hasText(clRow, "0.020000"));
  CHECK(hasText(clRow, "0.005000"));
  CHECK(hasText(clRow, "0.004000"));
  CHECK_FALSE(hasText(clRow, "0.015000"));
  CHECK_FALSE(hasText(clRow, "0.123456"));
  CHECK(hasText(clRow, "7"));
  CHECK(hasText(clRow, "converged"));

  CHECK_EQ(xfoil_demo::statusText(xfoil::ConvergenceStatus::notRun),
           std::string{"not-run"});
  CHECK_EQ(xfoil_demo::statusText(xfoil::ConvergenceStatus::unconverged),
           std::string{"unconverged"});
  CHECK_EQ(xfoil_demo::statusText(xfoil::ConvergenceStatus::cancelled),
           std::string{"cancelled"});
  CHECK_EQ(xfoil_demo::statusText(xfoil::ConvergenceStatus::failed),
           std::string{"failed"});
  CHECK_EQ(xfoil_demo::cpPointCount(240), std::string{"Cp points: 240"});
}

TEST(unit, LibraryDemoL9FrictionOutputContract) {
  xfoil::OperatingPointData inviscid;
  const xfoil_demo::FrictionOutputResult noFriction =
      xfoil_demo::formatClFriction(inviscid);
  CHECK(noFriction.status == xfoil_demo::FrictionOutputStatus::ok);
  CHECK_EQ(noFriction.text,
           std::string{"Cf upper stations: n/a\nCf lower stations: n/a"});

  xfoil::OperatingPointData viscous;
  viscous.viscous = true;
  viscous.dragCoefficientFriction = 0.004;
  viscous.skinFriction = validFriction(124, 116);
  const xfoil_demo::FrictionOutputResult complete =
      xfoil_demo::formatClFriction(viscous);
  CHECK(complete.status == xfoil_demo::FrictionOutputStatus::ok);
  CHECK_EQ(complete.text,
           std::string{"Cf upper stations: 124\nCf lower stations: 116"});

  xfoil::OperatingPointData cdfOnly;
  cdfOnly.viscous = true;
  cdfOnly.dragCoefficientFriction = 0.004;
  CHECK(xfoil_demo::formatClFriction(cdfOnly).status ==
        xfoil_demo::FrictionOutputStatus::inconsistent);

  xfoil::OperatingPointData cfOnly;
  cfOnly.viscous = true;
  cfOnly.skinFriction = validFriction(3, 2);
  CHECK(xfoil_demo::formatClFriction(cfOnly).status ==
        xfoil_demo::FrictionOutputStatus::inconsistent);

  xfoil::OperatingPointData malformed = viscous;
  malformed.skinFriction->upper.x.resize(2);
  CHECK(xfoil_demo::formatClFriction(malformed).status ==
        xfoil_demo::FrictionOutputStatus::inconsistent);

  xfoil::OperatingPointData lowerMalformed = viscous;
  lowerMalformed.skinFriction->lower.s.resize(1);
  CHECK(xfoil_demo::formatClFriction(lowerMalformed).status ==
        xfoil_demo::FrictionOutputStatus::inconsistent);
}

TEST(unit, LibraryDemoL9ExitCodes) {
  CHECK_EQ(static_cast<int>(ExitCode::success), 0);
  CHECK_EQ(static_cast<int>(ExitCode::usage), 2);
  CHECK_EQ(static_cast<int>(ExitCode::profile), 3);
  CHECK_EQ(static_cast<int>(ExitCode::geometry), 4);
  CHECK_EQ(static_cast<int>(ExitCode::analysis), 5);
  CHECK_EQ(static_cast<int>(ExitCode::nonConverged), 6);
}

TEST(unit, LibraryDemoL9HelpContract) {
  const std::string help = xfoil_demo::helpText();
  CHECK(hasText(help, "--airfoil PATH"));
  CHECK(hasText(help, "--re VALUE"));
  CHECK(hasText(help, "--alpha-end DEG"));
  CHECK(hasText(help, "--mach VALUE"));
  CHECK(hasText(help, "--iterations COUNT"));
  CHECK(hasText(help, "--panels COUNT"));
  CHECK(hasText(help, "Re = 0 selects inviscid analysis."));
  CHECK(hasText(help, "Re > 0 selects viscous analysis."));
  CHECK(hasText(help, "alpha-end is exclusive."));
  CHECK(hasText(help, "The demo application reads the profile file"));
  CHECK_FALSE(hasText(help, "--viscous"));
}
