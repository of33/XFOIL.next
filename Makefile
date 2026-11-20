# XFOIL.next
#
# Copyright (C) 2026 Olaf Frommann
# https://github.com/of33/XFOIL.next
#
# XFOIL.next is based on XFOIL 6.99 by Mark Drela and
# Harold Youngren.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.
#
# SPDX-License-Identifier: GPL-3.0-or-later

# File: Makefile
# Reproducible build for the C++ implementation of XFOIL.next.
# Standard build: release. Artifacts lie below build/<mode>/.
# The public target contract is permanent; the targets are listed
# in make help.

CXX      ?= g++
CPPFLAGS := -Iinclude -Isrc
CXXSTD   := -std=c++20 -pthread
DEPFLAGS := -MMD -MP

WARNINGS := \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Wshadow \
	-Wconversion \
	-Wsign-conversion \
	-Wnon-virtual-dtor \
	-Wold-style-cast \
	-Woverloaded-virtual \
	-Wnull-dereference \
	-Wdouble-promotion \
	-Wformat=2 \
	-Werror

DEBUG_FLAGS    := -O0 -g3
RELEASE_FLAGS  := -O3 -DNDEBUG
SANITIZE_FLAGS := \
	-O1 \
	-g3 \
	-fsanitize=address,undefined \
	-fno-omit-frame-pointer
TSAN_FLAGS := \
	-O1 \
	-g3 \
	-fsanitize=thread \
	-fno-omit-frame-pointer

# Native C++ CLI product binary. POSIX: xfoil.next; MSYS2/MinGW/
# Cygwin and native Windows: xfoil.next.exe. The suffix is derived
# explicitly (no assumption that the toolchain appends it); a single
# variable feeds every build, test and tool reference.
XFOIL_CLI_NAME := xfoil.next
ifeq ($(OS),Windows_NT)
EXEEXT := .exe
else ifneq (,$(filter Msys Cygwin,$(shell uname -o 2>/dev/null)))
EXEEXT := .exe
else
EXEEXT :=
endif
XFOIL_CLI := $(XFOIL_CLI_NAME)$(EXEEXT)

# ---------------------------------------------------------------------------
# Source groups (domain-specific, unchanged; object lists are derived
# mechanically from these groups).
# ---------------------------------------------------------------------------

# Application layer: DAT reader, iteration control, CLI interpreter and
# native batch path (src/app/cli/BatchRunner.cpp). Linked into the
# product and test programs.
APP_SRCS := \
	src/app/AirfoilWriter.cpp \
	src/app/DatFileReader.cpp \
	src/app/DumpFileAdapter.cpp \
	src/app/CommandLine.cpp \
	src/app/InteractiveApplication.cpp \
	src/app/cli/CommandParser.cpp \
	src/app/cli/BatchConfigParser.cpp \
	src/app/cli/BatchRunner.cpp \
	src/app/cli/CommandInterpreter.cpp \
	src/app/cli/NumericArguments.cpp \
	src/app/cli/CoreDxfHandlers.cpp \
	src/app/cli/CoreGeometryHandlers.cpp \
	src/app/cli/CoreInverseDesignHandlers.cpp \
	src/app/cli/CoreOperHandlers.cpp \
	src/app/cli/CoreVparHandlers.cpp

# Numerical helpers: Fortran compatibility, splines and linear
# equation solver. Linked into the product and test programs.
MATH_SRCS := \
	src/core/math/FortranCompat.cpp \
	src/core/math/LinearAlgebra.cpp \
	src/core/math/Spline.cpp

# Geometry data and algorithms. Linked into the product and
# test programs.
GEOMETRY_SRCS := \
	src/core/geometry/AirfoilGeometry.cpp \
	src/core/geometry/GeometryAlgorithms.cpp

# Inviscid compute kernels. Linked into the product and test programs.
INVISCID_SRCS := \
	src/core/inviscid/InviscidAnalysis.cpp

# Stateless boundary-layer correlations. Linked into the product
# and test programs.
BOUNDARY_LAYER_SRCS := \
	src/core/boundary_layer/BoundaryLayerCorrelations.cpp

# Stateless circle/Fourier kernels of the inverse methods. Linked
# into the product and test programs.
INVERSE_SRCS := \
	src/core/inverse/CirclePlaneMath.cpp

# Analysis configuration, result objects and session. The
# facade class xfoil::XFoil. Session and use-case layer under src/session/
# (namespace xfoil::session); the public XfoilSession
# implementation delegates only to Session/Core.
SESSION_SRCS := \
	src/core/AnalysisConfig.cpp \
	src/core/XFoilFacade.cpp \
	src/session/Analyze.cpp \
	src/session/PublicSession.cpp \
	src/session/SessionOps.cpp \
	src/session/SessionProjections.cpp \
	src/session/XFoilSession.cpp

# 0-based core: foundation without app dependency.
FOUNDATION_SRCS := \
	src/core/foundation/Matrix.cpp

# 0-based core: basic numerical routines.
NUMERICS_SRCS := \
	src/core/numerics/FortranCompat.cpp \
	src/core/numerics/Sort.cpp \
	src/core/numerics/Spline.cpp \
	src/core/numerics/LinearAlgebra.cpp

# 0-based core: geometry and NACA kernels.
GEOMETRY_CORE_SRCS := \
	src/core/geometry_core/GeometryTypes.cpp \
	src/core/geometry_core/Naca.cpp \
	src/core/geometry_core/LeadingEdge.cpp \
	src/core/geometry_core/GeometryParameters.cpp \
	src/core/geometry_core/Transformations.cpp \
	src/core/geometry_core/Refinement.cpp

# 0-based core: paneling and panel geometry kernels.
PANELING_CORE_SRCS := \
	src/core/paneling/PanelTypes.cpp \
	src/core/paneling/PanelGeometryKernels.cpp \
	src/core/paneling/Pangen.cpp \
	src/core/paneling/PanelParameters.cpp \
	src/core/paneling/Abcopy.cpp

# 0-based core: inviscid solver.
INVISCID_CORE_SRCS := \
	src/core/inviscid_core/Compressibility.cpp \
	src/core/inviscid_core/TrailingEdge.cpp \
	src/core/inviscid_core/Stagnation.cpp \
	src/core/inviscid_core/Loads.cpp \
	src/core/inviscid_core/Influence.cpp \
	src/core/inviscid_core/SpecifiedPoint.cpp

# 0-based core: boundary-layer data model and state mapping.
BOUNDARY_LAYER_CORE_SRCS := \
	src/core/boundary_layer_core/BoundaryLayerMapping.cpp \
	src/core/boundary_layer_core/BoundaryLayerParameters.cpp \
	src/core/boundary_layer_core/BoundaryLayerStation.cpp

# 0-based core: viscous and coupled solver.
BOUNDARY_LAYER_SOLVER_SRCS := \
	src/core/boundary_layer_solver/BoundaryLayerCorrelations.cpp \
	src/core/boundary_layer_solver/BoundaryLayerSolverKernels.cpp \
	src/core/boundary_layer_solver/NewtonSystem.cpp \
	src/core/boundary_layer_solver/Coupling.cpp \
	src/core/boundary_layer_solver/Update.cpp \
	src/core/boundary_layer_solver/Marching.cpp \
	src/core/boundary_layer_solver/BlForceReport.cpp \
	src/core/boundary_layer_solver/Viscal.cpp

# 0-based core: geometry-design kernels (GDES/CAMB sub-paths).
GEOMETRY_DESIGN_SRCS := \
	src/core/geometry_design/CamberSession.cpp \
	src/core/geometry_design/CamberThickness.cpp \
	src/core/geometry_design/GeometryDesignOps.cpp \
	src/core/geometry_design/PlotFrame.cpp \
	src/core/geometry_design/GeometryDesignBuffer.cpp \
	src/core/geometry_design/ThicknessCamberOps.cpp

# 0-based core: inverse methods (circle mapping, MDES/QDES/MIXED).
INVERSE_DESIGN_SRCS := \
	src/core/inverse_design/CircleKernels.cpp \
	src/core/inverse_design/CircleGeometry.cpp \
	src/core/inverse_design/OrrSommerfeldMap.cpp \
	src/core/inverse_design/WallProfileKernels.cpp \
	src/core/inverse_design/QSpecKernels.cpp \
	src/core/inverse_design/InverseState.cpp \
	src/core/inverse_design/MixedInverseKernels.cpp

# 0-based core: polar state and polar operations.
POLAR_CORE_SRCS := \
	src/core/polar/PolarOps.cpp

# 0-based core: structured export-data projections.
EXPORT_DATA_SRCS := \
	src/core/export_data/ExportData.cpp

# Non-core IO layer: parser, serializer, codecs and adapters.
# Depends only on xfoil::core, never on app, CLI or tests.
IO_SRCS := \
	src/io/TextFormat.cpp \
	src/io/PolarText.cpp \
	src/io/DumpCodec.cpp \
	src/io/DefaultsCodec.cpp \
	src/io/DxfExport.cpp \
	src/io/ExportFormats.cpp \
	src/io/OrrSommerfeldCodec.cpp \
	src/io/MsesMultielementCodec.cpp \
	src/io/AddedCamberCodec.cpp \
	src/io/MdesInverseFileCodec.cpp \
	src/io/ThicknessCamberCodec.cpp \
	src/io/OutputPathPolicy.cpp

# Productive sources without main(); linked with the test objects
# into xfoil_tests.
CORE_SRCS := \
	$(APP_SRCS) \
	$(MATH_SRCS) \
	$(GEOMETRY_SRCS) \
	$(INVISCID_SRCS) \
	$(BOUNDARY_LAYER_SRCS) \
	$(INVERSE_SRCS) \
	$(SESSION_SRCS) \
	$(FOUNDATION_SRCS) \
	$(NUMERICS_SRCS) \
	$(GEOMETRY_CORE_SRCS) \
	$(PANELING_CORE_SRCS) \
	$(INVISCID_CORE_SRCS) \
	$(BOUNDARY_LAYER_CORE_SRCS) \
	$(BOUNDARY_LAYER_SOLVER_SRCS) \
	$(GEOMETRY_DESIGN_SRCS) \
	$(INVERSE_DESIGN_SRCS) \
	$(POLAR_CORE_SRCS) \
	$(EXPORT_DATA_SRCS) \
	$(IO_SRCS)

SRCS := \
	src/main.cpp \
	$(CORE_SRCS)

# ---------------------------------------------------------------------------
# Library artifacts. The closure is explicit: core, session and pure IO
# codecs, but never src/main.cpp or the app/CLI adapters.
# ---------------------------------------------------------------------------
LIB_SRCS := \
	$(MATH_SRCS) \
	$(GEOMETRY_SRCS) \
	$(INVISCID_SRCS) \
	$(BOUNDARY_LAYER_SRCS) \
	$(INVERSE_SRCS) \
	$(SESSION_SRCS) \
	$(FOUNDATION_SRCS) \
	$(NUMERICS_SRCS) \
	$(GEOMETRY_CORE_SRCS) \
	$(PANELING_CORE_SRCS) \
	$(INVISCID_CORE_SRCS) \
	$(BOUNDARY_LAYER_CORE_SRCS) \
	$(BOUNDARY_LAYER_SOLVER_SRCS) \
	$(GEOMETRY_DESIGN_SRCS) \
	$(INVERSE_DESIGN_SRCS) \
	$(POLAR_CORE_SRCS) \
	$(EXPORT_DATA_SRCS) \
	$(IO_SRCS)

AR ?= ar

# GNU/MinGW convention: the import library and the static library have
# distinct names. Linux uses the conventional unversioned names.
ifeq ($(EXEEXT),.exe)
LIB_STATIC_NAME := libxfoil_next_static.a
LIB_SHARED_NAME := xfoil_next.dll
LIB_IMPORT_NAME := libxfoil_next.dll.a
else
LIB_STATIC_NAME := libxfoil_next.a
LIB_SHARED_NAME := libxfoil_next.so
LIB_IMPORT_NAME :=
endif

PRODUCT_VERSION := $(shell sed -n 's/.*inline constexpr std::string_view version = "\([^"]*\)".*/\1/p' src/app/ProductInfo.h)

PREFIX ?= /usr/local
DESTDIR ?=
includedir ?= $(PREFIX)/include
libdir ?= $(PREFIX)/lib
bindir ?= $(PREFIX)/bin
datadir ?= $(PREFIX)/share

INSTALL_MODE ?= release
LIB_MODE ?= release
L6_STAGE ?= $(BUILDROOT)/l6-stage
L6_CONSUMER ?= $(BUILDROOT)/l6-consumer

# Core test suite: framework plus tests/core/*. It forms both the
# standalone core suite (test-core) and a part of the full
# Suite (TEST_SRCS).
CORE_TEST_SRCS := \
	tests/TestMain.cpp \
	tests/TestSupport.cpp \
	tests/OsmapLoader.cpp \
	tests/core/FoundationTests.cpp \
	tests/core/MathTests.cpp \
	tests/core/GeometryTests.cpp \
	tests/core/PanelingTests.cpp \
	tests/core/InviscidTests.cpp \
	tests/core/CoreCpiBitTests.cpp \
	tests/core/CoreIntxHalfBitTests.cpp \
	tests/core/XycmReferenceTests.cpp \
	tests/core/BoundaryLayerTests.cpp \
	tests/core/BoundaryLayerCorrelationTests.cpp \
	tests/core/GeometryDesignTests.cpp \
	tests/core/InverseTests.cpp \
	tests/core/QdcalcRegressionTests.cpp \
	tests/core/QdcalcSharpTeTests.cpp \
	tests/core/MrchueRecoveryTests.cpp \
	tests/core/ClarkYViscousCoreTests.cpp \
	tests/core/SessionStateHeaderProbe.cpp \
	tests/core/SessionOpsHeaderProbe.cpp \
	tests/core/XFoilSessionHeaderProbe.cpp \
	tests/core/XcpDerivationTests.cpp \
	tests/core/PolarTests.cpp \
	tests/core/IoTests.cpp

# Full C++ test suite (unit./integration./regression./batch.).
#: TestSupportTests.cpp covers the RAII test infrastructure and
# runs only under the unit. filter.
TEST_SRCS := \
	$(CORE_TEST_SRCS) \
	tests/unit/AirfoilGeometryTests.cpp \
	tests/unit/GeometryAlgorithmsTests.cpp \
	tests/unit/InviscidAnalysisTests.cpp \
	tests/unit/DatFileReaderTests.cpp \
	tests/unit/AirfoilWriterTests.cpp \
	tests/unit/MsesMultielementCodecTests.cpp \
	tests/unit/OutputPathPolicyTests.cpp \
	tests/unit/CommandLineTests.cpp \
	tests/unit/BatchConfigParserTests.cpp \
	tests/unit/CommandParserTests.cpp \
	tests/unit/NumericArgumentsTests.cpp \
	tests/unit/InputCapacityS20Tests.cpp \
	tests/unit/TypesAndLimitsTests.cpp \
	tests/unit/OneBasedContainerTests.cpp \
	tests/unit/FortranCompatTests.cpp \
	tests/unit/LinearAlgebraTests.cpp \
	tests/unit/SplineTests.cpp \
	tests/unit/AnalysisConfigTests.cpp \
	tests/unit/CancellationTokenTests.cpp \
	tests/unit/BoundaryLayerStateTests.cpp \
	tests/unit/BoundaryLayerCorrelationTests.cpp \
	tests/unit/CirclePlaneMathTests.cpp \
	tests/unit/ModernXFoilApiTests.cpp \
	tests/unit/SessionCoreStateWiringTests.cpp \
	tests/unit/SessionCpiStateTests.cpp \
	tests/unit/SessionIntxHalfStateTests.cpp \
	tests/unit/CoreIntxHalfHandlerTests.cpp \
	tests/unit/OperDampCoreTests.cpp \
	tests/unit/OperAsetCoreTests.cpp \
	tests/unit/BlfFblgetBitTests.cpp \
	tests/unit/OperBlfCoreTests.cpp \
	tests/unit/OperP7P2CombinedTests.cpp \
	tests/unit/FmomMhingeBitTests.cpp \
	tests/unit/VelsPsilinBitTests.cpp \
	tests/unit/OperFmomCoreTests.cpp \
	tests/unit/OperFnewCoreTests.cpp \
	tests/unit/OperVelsCoreTests.cpp \
	tests/unit/LibraryDataModelL2HeaderSmoke.cpp \
	tests/unit/LibraryDataModelL2Tests.cpp \
	tests/unit/LibrarySessionL3Tests.cpp \
	tests/unit/LibraryOneShotL4HeaderSmoke.cpp \
	tests/unit/LibraryOneShotL4Tests.cpp \
	tests/unit/LibraryProgressL12Tests.cpp \
	tests/unit/LibrarySkinFrictionL13Tests.cpp \
	tests/unit/LibraryOperatingPointCompletenessTests.cpp \
	tests/unit/LibrarySequencePointCompletenessTests.cpp \
	tests/unit/LibraryIsolationL7Tests.cpp \
	tests/unit/LibraryConcurrencyL7Tests.cpp \
	tests/unit/LibraryDemoL9Tests.cpp \
	tests/unit/SessionPolarOpsTests.cpp \
	tests/unit/SessionDefaultsOpsTests.cpp \
	tests/unit/SessionDesignStateTests.cpp \
	tests/unit/SessionInverseStateTests.cpp \
	tests/unit/SessionMdesInitTests.cpp \
	tests/unit/SessionMdesCommandTests.cpp \
	tests/unit/CoreMdesMenuHandlerTests.cpp \
	tests/unit/MdesFileCodecTests.cpp \
	tests/unit/SessionQdesCommandTests.cpp \
	tests/unit/CoreQdesMenuHandlerTests.cpp \
	tests/unit/SessionCommitTests.cpp \
	tests/unit/CoreGdesMenuHandlerTests.cpp \
	tests/unit/CoreGdesCambPlotCursorBoundaryTests.cpp \
	tests/unit/CoreGdesCambCapacityTests.cpp \
	tests/unit/CoreGdesExecAtomicityTests.cpp \
	tests/unit/CoreGdesNestingProbeTests.cpp \
	tests/unit/CoreCamberMenuHandlerTests.cpp \
	tests/unit/CambIoCodecTests.cpp \
	tests/unit/CliFortranTextTests.cpp \
	tests/unit/ProductInfoTests.cpp \
	tests/unit/LicenseHeaderGateTests.cpp \
	tests/unit/PortableNumericContractTests.cpp \
	tests/unit/PortableBlFilenamesTests.cpp \
	tests/unit/PortableProcessCommandTests.cpp \
	tests/unit/CoreGeometryHandlerTests.cpp \
	tests/unit/GeometryCommandS15Tests.cpp \
	tests/unit/BlouCommandS16Tests.cpp \
	tests/unit/DxfExportS14Tests.cpp \
	tests/unit/DxfCommandS16Tests.cpp \
	tests/unit/StateInvalidationS17aTests.cpp \
	tests/unit/CoreOperHandlerTests.cpp \
	tests/unit/CoreVparHandlerTests.cpp \
	tests/unit/CoreOperSinglePointTests.cpp \
	tests/unit/CoreOperSinglePointHandlerTests.cpp \
	tests/unit/TestSupportTests.cpp \
	tests/unit/ConsoleTextS18Tests.cpp \
	tests/integration/XFoilSessionTests.cpp \
	tests/integration/ModernXFoilTests.cpp \
	tests/integration/CommandInterpreterTests.cpp \
	tests/integration/OperAlfaTests.cpp \
	tests/integration/OperClTests.cpp \
	tests/integration/OperConfigTests.cpp \
	tests/integration/OperMenuTests.cpp \
	tests/integration/OperNameTests.cpp \
	tests/integration/OperPlotBlockedTests.cpp \
	tests/integration/OperPlotAliasTests.cpp \
	tests/integration/OperP7CProductiveTests.cpp \
	tests/integration/OperPaccLifecycleTests.cpp \
	tests/integration/OperPaccInviscidDumpTests.cpp \
	tests/integration/OperPolarCommandsTests.cpp \
	tests/integration/OperFileExportTests.cpp \
	tests/integration/TopFileExportTests.cpp \
	tests/integration/DefaultsCommandsTests.cpp \
	tests/integration/OperSequenceTests.cpp \
	tests/integration/ConsoleTextS18Tests.cpp \
	tests/integration/OperVparTests.cpp \
	tests/integration/TopFileCommandsTests.cpp \
	tests/integration/PanelingCommandsTests.cpp \
	tests/integration/ProductBrandingTests.cpp \
	tests/integration/NativeBatchTests.cpp \
	tests/integration/GeometryCommandS15Tests.cpp \
	tests/integration/BlouCommandS16Tests.cpp \
	tests/integration/DxfExportS14Tests.cpp \
	tests/integration/DxfCommandS16Tests.cpp \
	tests/integration/StateInvalidationS17aTests.cpp \
	tests/integration/SmooTargetS19Tests.cpp \
	tests/integration/InputCapacityS20Tests.cpp \
	tests/integration/LibrarySessionL3Tests.cpp \
	tests/integration/LibraryOneShotL4Tests.cpp \
	tests/integration/LibraryCliParityL5Tests.cpp \
	tests/integration/LibraryIsolationL7Tests.cpp \
	tests/integration/LibraryDemoL9Tests.cpp \
	tests/integration/LibraryProgressL12Tests.cpp \
	tests/integration/LibrarySkinFrictionL13Tests.cpp \
	tests/integration/LibraryOperatingPointCompletenessTests.cpp \
	tests/integration/LibrarySequencePointCompletenessTests.cpp \
	tests/regression/ClarkYGoldenTests.cpp \
	tests/regression/PaccDumpByteParityTests.cpp \
	tests/regression/PaccInviscidDumpParityTests.cpp \
	tests/regression/Wake2Alfa6ConvergenceParityTests.cpp \
	tests/regression/CseqHeadDumpParityTests.cpp \
	tests/regression/CliTranscriptTests.cpp

BUILDROOT := build

GOLDEN_DIR  := tests/golden
CAPTURE_DIR := $(BUILDROOT)/regression-capture

# Portable runtime packaging. The product version remains the canonical
# value above. SOURCE_DATE_EPOCH is taken from the environment or the
# current source commit and is exported to the deterministic packager.
PORTABLE_ROOT ?= $(BUILDROOT)/portable
PORTABLE_DIST ?= $(PORTABLE_ROOT)/dist
PORTABLE_LINUX_BUILDROOT ?= $(PORTABLE_ROOT)/linux-x86_64
PORTABLE_WINDOWS_BUILDROOT ?= $(PORTABLE_ROOT)/windows-x86_64
PORTABLE_LINUX_BINARY := $(PORTABLE_LINUX_BUILDROOT)/release/xfoil.next
PORTABLE_WINDOWS_BINARY := $(PORTABLE_WINDOWS_BUILDROOT)/release/xfoil.next.exe
PORTABLE_LINUX_ARCHIVE := $(PORTABLE_DIST)/XFOIL.next-$(PRODUCT_VERSION)-linux-x86_64.tar.xz
PORTABLE_WINDOWS_ARCHIVE := $(PORTABLE_DIST)/XFOIL.next-$(PRODUCT_VERSION)-windows-x86_64.zip
PORTABLE_WINDOWS_RUN_VERSION := $(if $(filter .exe,$(EXEEXT)),--run-version,)
SOURCE_DATE_EPOCH ?= $(shell git show -s --format=%ct HEAD 2>/dev/null)
export SOURCE_DATE_EPOCH

# Mode -> Flags. Object and output directories remain separate.
debug_FLAGS    := $(DEBUG_FLAGS)
release_FLAGS  := $(RELEASE_FLAGS)
sanitize_FLAGS := $(SANITIZE_FLAGS)
tsan_FLAGS     := $(TSAN_FLAGS)

# Shared-library objects use hidden visibility on ELF and explicit
# XFOIL_API exports for the public API. MinGW gets the same public
# declarations through __declspec(dllexport).
ifeq ($(EXEEXT),.exe)
LIB_SHARED_VISIBILITY :=
else
LIB_SHARED_VISIBILITY := -fvisibility=hidden
endif

LIB_SHARED_IMPORT_FLAG := -Wl,--out-implib

# Toolchain contract: each compiler uses its own BUILDROOT root
# (e.g. BUILDROOT=build/clang for clang++); switching CXX in the
# same buildroot without clean is not intended. The core tree is
# additionally separated by toolchain (md5(CXX)).
.PHONY: all debug release sanitize \
	test test-core test-unit test-integration test-regression test-batch \
	test-sanitize test-valgrind \
	test-reference test-reference-determinism test-harnesses \
	check-fortran-sha check-dependencies \
	check-release-inventory check-license-headers \
	check-public-docs \
	core-link-check session-link-check library-link-check \
	check-library-dependencies library-symbol-check print-library-sources \
	test-library-isolation test-library-progress test-library-skin-friction \
	test-library-operating-point test-library-sequence-point \
	test-library-concurrency test-tsan-library \
	test-library-noio test-library-noio-static test-library-noio-shared \
	library library-static library-shared install \
	install-headers install-libraries install-cli install-metadata \
	l6-stage-install test-consumer-static test-consumer-shared test-consumers \
	library-demo test-library-demo update-golden \
	portable-linux portable-windows portable-package-linux portable-package-windows \
	portable-check-linux portable-check-windows portable-repro-linux \
	portable-repro-windows portable-sha256sums \
	gui gui-debug gui-release test-gui gui-qcustomplot-check gui-link-check \
	clean-gui check-gui-dependencies \
	clean help

all: release

# ---------------------------------------------------------------------------
# Optional GUI skeleton. The root Makefile delegates only; Qt is required
# only when a gui target is actually invoked.
# ---------------------------------------------------------------------------
gui:
	$(MAKE) -f Makefile.gui gui

gui-debug:
	$(MAKE) -f Makefile.gui gui-debug

gui-release:
	$(MAKE) -f Makefile.gui gui-release

test-gui:
	$(MAKE) -f Makefile.gui test-gui

gui-qcustomplot-check:
	$(MAKE) -f Makefile.gui gui-qcustomplot-check

gui-link-check:
	$(MAKE) -f Makefile.gui gui-link-check

check-gui-dependencies:
	$(MAKE) -f Makefile.gui check-gui-dependencies

clean-gui:
	$(MAKE) -f Makefile.gui clean

# ---------------------------------------------------------------------------
# Build: each variant gets build/<mode>/obj, build/<mode>/obj-tests,
# build/<mode>/$(XFOIL_CLI) and build/<mode>/xfoil_tests. Object lists
# are derived mechanically from the source groups.
# ---------------------------------------------------------------------------
define MODE_RULES
OBJS_$(1) := $$(patsubst src/%.cpp,$(BUILDROOT)/$(1)/obj/%.o,$(SRCS))
DEPS_$(1) := $$(OBJS_$(1):.o=.d)
CORE_OBJS_$(1) := $$(patsubst src/%.cpp,$(BUILDROOT)/$(1)/obj/%.o,$(CORE_SRCS))
APP_OBJS_$(1) := $$(patsubst src/%.cpp,$(BUILDROOT)/$(1)/obj/%.o,$$(APP_SRCS))
CLI_OBJS_$(1) := $(BUILDROOT)/$(1)/obj/main.o $$(APP_OBJS_$(1))
TEST_OBJS_$(1) := $$(patsubst tests/%.cpp,$(BUILDROOT)/$(1)/obj-tests/%.o,$(TEST_SRCS))
DEMO_SUPPORT_OBJ_$(1) := $(BUILDROOT)/$(1)/obj-demo-support/DemoSupport.o
TEST_DEPS_$(1) := $$(TEST_OBJS_$(1):.o=.d) $$(DEMO_SUPPORT_OBJ_$(1):.o=.d)
LIB_STATIC_$(1) := $(BUILDROOT)/$(1)/lib/$(LIB_STATIC_NAME)
LIB_SHARED_$(1) := $(BUILDROOT)/$(1)/lib/$(LIB_SHARED_NAME)
LIB_IMPORT_$(1) := $(BUILDROOT)/$(1)/lib/$(LIB_IMPORT_NAME)
LIB_STATIC_OBJS_$(1) := $$(patsubst src/%.cpp,$(BUILDROOT)/$(1)/libobj-static/%.o,$$(LIB_SRCS))
LIB_SHARED_OBJS_$(1) := $$(patsubst src/%.cpp,$(BUILDROOT)/$(1)/libobj-shared/%.o,$$(LIB_SRCS))
LIB_STATIC_DEPS_$(1) := $$(LIB_STATIC_OBJS_$(1):.o=.d)
LIB_SHARED_DEPS_$(1) := $$(LIB_SHARED_OBJS_$(1):.o=.d)
LIB_SHARED_LINK_$(1) := -shared $(if $(filter .exe,$(EXEEXT)),$(LIB_SHARED_IMPORT_FLAG) $(BUILDROOT)/$(1)/lib/$(LIB_IMPORT_NAME))

$(1): $(BUILDROOT)/$(1)/$(XFOIL_CLI)

$(BUILDROOT)/$(1)/obj/%.o: src/%.cpp
	@mkdir -p $$(dir $$@)
	$$(CXX) $$(CXXSTD) $$(CPPFLAGS) $$(DEPFLAGS) $$(WARNINGS) $$($(1)_FLAGS) -c $$< -o $$@

$(BUILDROOT)/$(1)/libobj-static/%.o: src/%.cpp
	@mkdir -p $$(dir $$@)
	$$(CXX) $$(CXXSTD) $$(CPPFLAGS) $$(DEPFLAGS) $$(WARNINGS) $$($(1)_FLAGS) -fPIC -c $$< -o $$@

$(BUILDROOT)/$(1)/libobj-shared/%.o: src/%.cpp
	@mkdir -p $$(dir $$@)
	$$(CXX) $$(CXXSTD) $$(CPPFLAGS) $$(DEPFLAGS) $$(WARNINGS) $$($(1)_FLAGS) -fPIC $(LIB_SHARED_VISIBILITY) -DXFOIL_NEXT_BUILD_SHARED -c $$< -o $$@

$(BUILDROOT)/$(1)/lib/$(LIB_STATIC_NAME): $(patsubst src/%.cpp,$(BUILDROOT)/$(1)/libobj-static/%.o,$(LIB_SRCS))
	@mkdir -p $$(dir $$@)
	rm -f $$@
	$$(AR) rcs $$@ $$^

$(BUILDROOT)/$(1)/lib/$(LIB_SHARED_NAME): $(patsubst src/%.cpp,$(BUILDROOT)/$(1)/libobj-shared/%.o,$(LIB_SRCS))
	@mkdir -p $$(dir $$@)
	$$(CXX) $$(CXXSTD) $$($(1)_FLAGS) $$(LIB_SHARED_LINK_$(1)) $$^ -o $$@

$(BUILDROOT)/$(1)/$(XFOIL_CLI): $$(CLI_OBJS_$(1)) $(BUILDROOT)/$(1)/lib/$(LIB_STATIC_NAME)
	@mkdir -p $$(dir $$@)
	$$(CXX) $$(CXXSTD) $$($(1)_FLAGS) $$(CLI_OBJS_$(1)) $(BUILDROOT)/$(1)/lib/$(LIB_STATIC_NAME) -o $$@

$(BUILDROOT)/$(1)/obj-demo-support/%.o: library-demo/%.cpp
	@mkdir -p $$(dir $$@)
	$$(CXX) $$(CXXSTD) $$(CPPFLAGS) -Ilibrary-demo $$(DEPFLAGS) $$(WARNINGS) $$($(1)_FLAGS) -c $$< -o $$@

$(BUILDROOT)/$(1)/obj-tests/%.o: tests/%.cpp
	@mkdir -p $$(dir $$@)
	$$(CXX) $$(CXXSTD) $$(CPPFLAGS) -Itests -Ilibrary-demo $$(DEPFLAGS) $$(WARNINGS) $$($(1)_FLAGS) -c $$< -o $$@

$(BUILDROOT)/$(1)/xfoil_tests: $$(TEST_OBJS_$(1)) $$(DEMO_SUPPORT_OBJ_$(1)) $$(CORE_OBJS_$(1))
	@mkdir -p $$(dir $$@)
	$$(CXX) $$(CXXSTD) $$($(1)_FLAGS) $$^ -o $$@

-include $$(DEPS_$(1)) $$(TEST_DEPS_$(1)) $$(LIB_STATIC_DEPS_$(1)) $$(LIB_SHARED_DEPS_$(1))
endef

$(foreach mode,debug release sanitize tsan,$(eval $(call MODE_RULES,$(mode))))

# ---------------------------------------------------------------------------
# Tests: standard run against the debug binary, sanitizer run against
# the sanitizer binary. An optional argument is a name filter.
# ---------------------------------------------------------------------------
test: $(BUILDROOT)/debug/xfoil_tests $(BUILDROOT)/debug/$(XFOIL_CLI)
	XFOIL_CLI_PATH=$(BUILDROOT)/debug/$(XFOIL_CLI) \
	$(BUILDROOT)/debug/xfoil_tests

test-unit: $(BUILDROOT)/debug/xfoil_tests
	$(BUILDROOT)/debug/xfoil_tests unit.

test-integration: $(BUILDROOT)/debug/xfoil_tests $(BUILDROOT)/debug/$(XFOIL_CLI)
	$(BUILDROOT)/debug/xfoil_tests integration.

test-regression: $(BUILDROOT)/debug/xfoil_tests $(BUILDROOT)/debug/$(XFOIL_CLI)
	XFOIL_CLI_PATH=$(BUILDROOT)/debug/$(XFOIL_CLI) \
	$(BUILDROOT)/debug/xfoil_tests regression.

# Native batch suite: filter 'batch.' against the debug test binary;
# XFOIL_CLI_PATH points at the current debug product binary.
test-batch: $(BUILDROOT)/debug/xfoil_tests $(BUILDROOT)/debug/$(XFOIL_CLI)
	XFOIL_CLI_PATH=$(BUILDROOT)/debug/$(XFOIL_CLI) \
	$(BUILDROOT)/debug/xfoil_tests batch.

test-sanitize: $(BUILDROOT)/sanitize/xfoil_tests $(BUILDROOT)/sanitize/$(XFOIL_CLI)
	XFOIL_CLI_PATH=$(BUILDROOT)/sanitize/$(XFOIL_CLI) \
	$(BUILDROOT)/sanitize/xfoil_tests

# ---------------------------------------------------------------------------
# 0-based core: separate build range build/core/<toolchain>/.
# The core targets link only core and session objects, not app/CLI
# objects.
#
# Toolchain separation: <toolchain> is a stable name derived from CXX.
# This prevents GCC and Clang objects from being accidentally
# reused, even without make clean between switches.
# ---------------------------------------------------------------------------
CORE_TOOLCHAIN   := $(shell printf '%s' "$(CXX)" | md5sum | cut -c1-8)
CORE_ROOT        := $(BUILDROOT)/core/$(CORE_TOOLCHAIN)
CORE_OBJDIR      := $(CORE_ROOT)/obj
CORE_TEST_OBJDIR := $(CORE_ROOT)/obj-tests
CORE_TEST_FLAGS  := -O0 -g3

# Link sets, derived mechanically from the source groups:
# - CORE_LINK_SRCS: pure core objects for the core link check
# - SESSION_LINK_SRCS: session layer + pure value-type/core objects
# - CORE_TEST_LINK_SRCS: full core test link set plus the app sources
#   required by core test fixtures; no product main
CORE_LINK_SRCS := \
	$(FOUNDATION_SRCS) \
	$(NUMERICS_SRCS) \
	$(GEOMETRY_CORE_SRCS) \
	$(PANELING_CORE_SRCS) \
	$(INVISCID_CORE_SRCS) \
	$(BOUNDARY_LAYER_CORE_SRCS) \
	$(BOUNDARY_LAYER_SOLVER_SRCS) \
	$(GEOMETRY_DESIGN_SRCS) \
	$(INVERSE_DESIGN_SRCS) \
	$(POLAR_CORE_SRCS) \
	$(EXPORT_DATA_SRCS)

SESSION_LINK_SRCS := \
	$(SESSION_SRCS) \
	src/core/geometry/AirfoilGeometry.cpp \
	$(CORE_LINK_SRCS)

CORE_TEST_LINK_SRCS := \
	$(filter-out $(MATH_SRCS) $(BOUNDARY_LAYER_SRCS) \
	$(INVERSE_SRCS) src/core/geometry/GeometryAlgorithms.cpp,\
	$(CORE_SRCS))

CORE_LINK_OBJS := \
	$(patsubst src/%.cpp,$(CORE_OBJDIR)/%.o,$(CORE_LINK_SRCS))
SESSION_LINK_OBJS := \
	$(patsubst src/%.cpp,$(CORE_OBJDIR)/%.o,$(SESSION_LINK_SRCS))
CORE_TEST_LINK_OBJS := \
	$(patsubst src/%.cpp,$(CORE_OBJDIR)/%.o,$(CORE_TEST_LINK_SRCS))
CORE_TEST_OBJS := \
	$(patsubst tests/%.cpp,$(CORE_TEST_OBJDIR)/%.o,$(CORE_TEST_SRCS))

# link checks as own objects with dependency files (react to
# header changes, analogous to the remaining core objects).
CORE_LINKCHECK_OBJ    := $(CORE_TEST_OBJDIR)/core/CoreLinkCheck.o
SESSION_LINKCHECK_OBJ := $(CORE_TEST_OBJDIR)/core/SessionLinkCheck.o

$(CORE_OBJDIR)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXSTD) $(CPPFLAGS) $(DEPFLAGS) $(WARNINGS) $(CORE_TEST_FLAGS) -c $< -o $@

$(CORE_TEST_OBJDIR)/%.o: tests/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXSTD) $(CPPFLAGS) -Itests $(DEPFLAGS) $(WARNINGS) $(CORE_TEST_FLAGS) -c $< -o $@

# Standalone core-test binary: Framework + core tests + core/session/IO
# and the fixed app test-support sources. No product main is linked.
$(CORE_ROOT)/xfoil_core_tests: $(CORE_TEST_OBJS) $(CORE_TEST_LINK_OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXSTD) $(CORE_TEST_FLAGS) $^ -o $@

# Minimal program against only the new core objects.
$(CORE_ROOT)/core-link-check: $(CORE_LINKCHECK_OBJ) $(CORE_LINK_OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXSTD) $(CORE_TEST_FLAGS) $^ -o $@

# Link check of the session layer: only session, core and pure
# analysis-configuration/facade objects; no app/CLI objects.
$(CORE_ROOT)/session-link-check: $(SESSION_LINKCHECK_OBJ) $(SESSION_LINK_OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXSTD) $(CORE_TEST_FLAGS) $^ -o $@

CORE_ALL_OBJS := \
	$(sort $(CORE_TEST_LINK_OBJS) $(CORE_LINK_OBJS) $(SESSION_LINK_OBJS) \
	$(CORE_TEST_OBJS) $(CORE_LINKCHECK_OBJ) $(SESSION_LINKCHECK_OBJ))
-include $(CORE_ALL_OBJS:.o=.d)

test-core: $(CORE_ROOT)/xfoil_core_tests
	$(CORE_ROOT)/xfoil_core_tests

core-link-check: $(CORE_ROOT)/core-link-check
	$(CORE_ROOT)/core-link-check

session-link-check: $(CORE_ROOT)/session-link-check
	$(CORE_ROOT)/session-link-check

# ---------------------------------------------------------------------------
# Library build and install (L6). The library closure is LIB_SRCS; it
# excludes src/main.cpp and APP_SRCS. Static and shared objects live in
# separate roots so PIC/shared visibility never changes the CLI test
# objects.
# ---------------------------------------------------------------------------
LIB_LINK_FLAGS := $(if $(filter debug,$(LIB_MODE)),$(DEBUG_FLAGS),$(if $(filter sanitize,$(LIB_MODE)),$(SANITIZE_FLAGS),$(RELEASE_FLAGS)))
LIBRARY_LINKCHECK := $(BUILDROOT)/$(LIB_MODE)/library-link-check$(EXEEXT)

.PHONY: library library-static library-shared library-link-check \
	print-library-sources check-library-dependencies library-symbol-check \
	test-library-isolation test-library-progress test-library-skin-friction \
	test-library-operating-point test-library-sequence-point \
	test-library-concurrency test-tsan-library \
	test-library-noio test-library-noio-static test-library-noio-shared \
	install install-headers install-libraries install-cli install-metadata \
	l6-stage-install test-consumer-static test-consumer-shared test-consumers

library-static: $(LIB_STATIC_$(LIB_MODE))
library-shared: $(LIB_SHARED_$(LIB_MODE))
library: library-static library-shared

print-library-sources:
	@for source in $(LIB_SRCS); do printf '%s\n' "$$source"; done

check-library-dependencies:
	python3 tools/check-core-dependencies.py --scope=library

ifeq ($(EXEEXT),.exe)
library-symbol-check: library
	@echo "library-symbol-check: use the focused Windows import-table audit."
	@exit 1
else
library-symbol-check: library
	python3 tools/check-library-symbols.py \
		--static "$(LIB_STATIC_$(LIB_MODE))" \
		--shared "$(LIB_SHARED_$(LIB_MODE))"
endif

test-library-isolation: $(BUILDROOT)/debug/xfoil_tests $(BUILDROOT)/debug/$(XFOIL_CLI)
	XFOIL_CLI_PATH=$(BUILDROOT)/debug/$(XFOIL_CLI) \
		$(BUILDROOT)/debug/xfoil_tests$(EXEEXT) unit.LibraryIsolationL7
	XFOIL_CLI_PATH=$(BUILDROOT)/debug/$(XFOIL_CLI) \
		$(BUILDROOT)/debug/xfoil_tests$(EXEEXT) integration.LibraryIsolationL7

test-library-progress: $(BUILDROOT)/debug/xfoil_tests $(BUILDROOT)/debug/$(XFOIL_CLI)
	XFOIL_CLI_PATH=$(BUILDROOT)/debug/$(XFOIL_CLI) \
		$(BUILDROOT)/debug/xfoil_tests$(EXEEXT) unit.LibraryProgressL12
	XFOIL_CLI_PATH=$(BUILDROOT)/debug/$(XFOIL_CLI) \
		$(BUILDROOT)/debug/xfoil_tests$(EXEEXT) integration.LibraryProgressL12

test-library-skin-friction: $(BUILDROOT)/debug/xfoil_tests $(BUILDROOT)/debug/$(XFOIL_CLI)
	XFOIL_CLI_PATH=$(BUILDROOT)/debug/$(XFOIL_CLI) \
		$(BUILDROOT)/debug/xfoil_tests$(EXEEXT) unit.LibrarySkinFrictionL13
	XFOIL_CLI_PATH=$(BUILDROOT)/debug/$(XFOIL_CLI) \
		$(BUILDROOT)/debug/xfoil_tests$(EXEEXT) integration.LibrarySkinFrictionL13

test-library-operating-point: $(BUILDROOT)/debug/xfoil_tests $(BUILDROOT)/debug/$(XFOIL_CLI)
	XFOIL_CLI_PATH=$(BUILDROOT)/debug/$(XFOIL_CLI) \
		$(BUILDROOT)/debug/xfoil_tests$(EXEEXT) unit.LibraryOperatingPointCompleteness
	XFOIL_CLI_PATH=$(BUILDROOT)/debug/$(XFOIL_CLI) \
		$(BUILDROOT)/debug/xfoil_tests$(EXEEXT) integration.LibraryOperatingPointCompleteness

test-library-sequence-point: $(BUILDROOT)/debug/xfoil_tests $(BUILDROOT)/debug/$(XFOIL_CLI)
	XFOIL_CLI_PATH=$(BUILDROOT)/debug/$(XFOIL_CLI) \
		$(BUILDROOT)/debug/xfoil_tests$(EXEEXT) unit.LibrarySequencePointCompleteness
	XFOIL_CLI_PATH=$(BUILDROOT)/debug/$(XFOIL_CLI) \
		$(BUILDROOT)/debug/xfoil_tests$(EXEEXT) integration.LibrarySequencePointCompleteness

test-library-concurrency: $(BUILDROOT)/debug/xfoil_tests
	$(BUILDROOT)/debug/xfoil_tests$(EXEEXT) unit.LibraryConcurrencyL7

test-tsan-library: $(BUILDROOT)/tsan/xfoil_tests
	@if command -v setarch >/dev/null 2>&1; then \
		setarch "$$(uname -m)" -R $(BUILDROOT)/tsan/xfoil_tests unit.LibraryConcurrencyL7; \
	else \
		$(BUILDROOT)/tsan/xfoil_tests unit.LibraryConcurrencyL7; \
	fi

$(LIBRARY_LINKCHECK): tests/consumer/LibraryConsumer.cpp $(LIB_STATIC_$(LIB_MODE))
	@mkdir -p $(dir $@)
	$(CXX) $(CXXSTD) -Iinclude $(LIB_LINK_FLAGS) $< $(LIB_STATIC_$(LIB_MODE)) -o $@

library-link-check: $(LIBRARY_LINKCHECK)
	$(LIBRARY_LINKCHECK)

install-headers:
	@mkdir -p "$(DESTDIR)$(includedir)"
	cp -R include/xfoil "$(DESTDIR)$(includedir)/"

install-libraries: $(LIB_STATIC_$(INSTALL_MODE)) $(LIB_SHARED_$(INSTALL_MODE))
	@mkdir -p "$(DESTDIR)$(libdir)" "$(DESTDIR)$(bindir)"
	cp "$(LIB_STATIC_$(INSTALL_MODE))" "$(DESTDIR)$(libdir)/"
	@if [ -n "$(LIB_IMPORT_NAME)" ]; then \
		cp "$(LIB_SHARED_$(INSTALL_MODE))" "$(DESTDIR)$(bindir)/$(LIB_SHARED_NAME)"; \
		cp "$(LIB_IMPORT_$(INSTALL_MODE))" "$(DESTDIR)$(libdir)/$(LIB_IMPORT_NAME)"; \
	else \
		cp "$(LIB_SHARED_$(INSTALL_MODE))" "$(DESTDIR)$(libdir)/$(LIB_SHARED_NAME)"; \
	fi

install-cli: $(BUILDROOT)/$(INSTALL_MODE)/$(XFOIL_CLI)
	@mkdir -p "$(DESTDIR)$(bindir)"
	cp "$(BUILDROOT)/$(INSTALL_MODE)/$(XFOIL_CLI)" "$(DESTDIR)$(bindir)/"

install-metadata:
	@mkdir -p "$(DESTDIR)$(libdir)/pkgconfig" \
		"$(DESTDIR)$(datadir)/licenses/xfoil-next"
	cp LICENSE NOTICE "$(DESTDIR)$(datadir)/licenses/xfoil-next/"
	@printf '%s\n' \
		'prefix=$(PREFIX)' \
		'exec_prefix=$${prefix}' \
		'libdir=$${exec_prefix}/lib' \
		'includedir=$${prefix}/include' \
		'' \
		'Name: xfoil-next' \
		'Description: XFOIL.next embedded aerodynamic analysis library' \
		'Version: $(PRODUCT_VERSION)' \
		'Libs: -L$${libdir} -lxfoil_next' \
		'Libs.private: -lm' \
		'Cflags: -I$${includedir}' \
		> "$(DESTDIR)$(libdir)/pkgconfig/xfoil-next.pc"

install: install-headers install-libraries install-cli install-metadata

# Staged install used by the consumer smoke tests. The real system is
# never touched.
L6_STAGE_MARKER := $(L6_STAGE)/.installed
$(L6_STAGE_MARKER): library $(BUILDROOT)/release/$(XFOIL_CLI)
	rm -rf "$(L6_STAGE)"
	@mkdir -p "$(L6_STAGE)"
	$(MAKE) install DESTDIR=$(abspath $(L6_STAGE)) PREFIX=/usr INSTALL_MODE=release
	@touch "$(L6_STAGE_MARKER)"

l6-stage-install: $(L6_STAGE_MARKER)

test-consumer-static: $(L6_STAGE_MARKER)
	@mkdir -p "$(L6_CONSUMER)"
	$(CXX) $(CXXSTD) -I"$(L6_STAGE)/usr/include" $(LIB_LINK_FLAGS) \
		tests/consumer/LibraryConsumer.cpp \
		"$(L6_STAGE)/usr/lib/$(LIB_STATIC_NAME)" \
		-o "$(L6_CONSUMER)/consumer-static$(EXEEXT)"
	"$(L6_CONSUMER)/consumer-static$(EXEEXT)"

test-consumer-shared: $(L6_STAGE_MARKER)
	@mkdir -p "$(L6_CONSUMER)"
	$(CXX) $(CXXSTD) -I"$(L6_STAGE)/usr/include" -DXFOIL_NEXT_USE_SHARED $(LIB_LINK_FLAGS) \
		tests/consumer/LibraryConsumer.cpp \
		-L"$(L6_STAGE)/usr/lib" -lxfoil_next \
		-Wl,-rpath,$(abspath $(L6_STAGE))/usr/lib \
		-o "$(L6_CONSUMER)/consumer-shared$(EXEEXT)"
	@if [ -n "$(EXEEXT)" ]; then \
		PATH="$(abspath $(L6_STAGE))/usr/bin:$$PATH" \
			"$(L6_CONSUMER)/consumer-shared$(EXEEXT)"; \
	else \
		"$(L6_CONSUMER)/consumer-shared$(EXEEXT)"; \
	fi

test-consumers: test-consumer-static test-consumer-shared

# ---------------------------------------------------------------------------
# Standalone library demo. The demo is built only against the staged
# installed package and is not part of the default product build.
# ---------------------------------------------------------------------------
DEMO_LIB_DIR := library-demo
DEMO_BUILD_ROOT := $(DEMO_LIB_DIR)/build
DEMO_SHARED := $(DEMO_BUILD_ROOT)/shared/xfoil-next-lib-demo$(EXEEXT)
DEMO_STATIC := $(DEMO_BUILD_ROOT)/static/xfoil-next-lib-demo$(EXEEXT)
DEMO_PROFILE := $(CURDIR)/library-demo/profiles/naca0012.dat
DEMO_PKG_CONFIG_PATH := $(abspath $(L6_STAGE))/usr/lib/pkgconfig
DEMO_PKG_CONFIG_SYSROOT := $(abspath $(L6_STAGE))

library-demo: $(L6_STAGE_MARKER)
	$(MAKE) -C $(DEMO_LIB_DIR) clean
	PKG_CONFIG_PATH="$(DEMO_PKG_CONFIG_PATH)" \
	PKG_CONFIG_SYSROOT_DIR="$(DEMO_PKG_CONFIG_SYSROOT)" \
	XFOIL_NEXT_PREFIX="$(abspath $(L6_STAGE))/usr" \
	$(MAKE) -C $(DEMO_LIB_DIR) LINK_MODE=shared
	PKG_CONFIG_PATH="$(DEMO_PKG_CONFIG_PATH)" \
	PKG_CONFIG_SYSROOT_DIR="$(DEMO_PKG_CONFIG_SYSROOT)" \
	XFOIL_NEXT_PREFIX="$(abspath $(L6_STAGE))/usr" \
	$(MAKE) -C $(DEMO_LIB_DIR) LINK_MODE=static

test-library-demo: library-demo $(BUILDROOT)/debug/xfoil_tests
	XFOIL_DEMO_SHARED_PATH="$(abspath $(DEMO_SHARED))" \
	XFOIL_DEMO_STATIC_PATH="$(abspath $(DEMO_STATIC))" \
	XFOIL_DEMO_PROFILE="$(DEMO_PROFILE)" \
	XFOIL_DEMO_STAGE="$(abspath $(L6_STAGE))" \
	$(BUILDROOT)/debug/xfoil_tests$(EXEEXT) integration.LibraryDemoL9

# ---------------------------------------------------------------------------
# Portable runtime packaging (Linux x86_64 and Windows x86_64). Builds
# are reproducible from the pinned Linux builder image and the accepted
# MSYS2 UCRT64 Windows baseline. Archives contain LICENSE, NOTICE and the
# single runtime executable; SHA256SUMS is a separate file.
# ---------------------------------------------------------------------------
portable-linux:
	BUILDROOT="$(PORTABLE_LINUX_BUILDROOT)" tools/build-portable-linux.sh
	$(MAKE) portable-package-linux

portable-windows:
	BUILDROOT="$(PORTABLE_WINDOWS_BUILDROOT)" tools/build-portable-windows.sh
	$(MAKE) portable-package-windows

portable-package-linux:
	python3 packaging/portable/package_portable.py package \
		--platform linux \
		--executable "$(PORTABLE_LINUX_BINARY)" \
		--output-dir "$(PORTABLE_DIST)" \
		--metadata "$(PORTABLE_LINUX_BUILDROOT)/source-metadata.json"

portable-package-windows:
	python3 packaging/portable/package_portable.py package \
		--platform windows \
		--executable "$(PORTABLE_WINDOWS_BINARY)" \
		--output-dir "$(PORTABLE_DIST)" \
		--metadata "$(PORTABLE_WINDOWS_BUILDROOT)/source-metadata.json"

portable-check-linux:
	python3 packaging/portable/check_portable_binary.py \
		--platform linux \
		--binary "$(PORTABLE_LINUX_BINARY)" \
		--archive "$(PORTABLE_LINUX_ARCHIVE)" \
		--expected-version "$(PRODUCT_VERSION)" \
		--run-version

portable-check-windows:
	python3 packaging/portable/check_portable_binary.py \
		--platform windows \
		--binary "$(PORTABLE_WINDOWS_BINARY)" \
		--archive "$(PORTABLE_WINDOWS_ARCHIVE)" \
		--expected-version "$(PRODUCT_VERSION)" $(PORTABLE_WINDOWS_RUN_VERSION)

portable-repro-linux:
	tools/repro-portable-linux.sh

portable-repro-windows:
	tools/repro-portable-windows.sh

portable-sha256sums:
	python3 packaging/portable/package_portable.py sums \
		--output-dir "$(PORTABLE_DIST)"

# ---------------------------------------------------------------------------
# Armed no-I/O proof (Linux). The interposition shim is disarmed for
# loader/harness work and is armed only around the public API workflow.
# ---------------------------------------------------------------------------
NOIO_ROOT := $(BUILDROOT)/library-noio
NOIO_SHIM := $(NOIO_ROOT)/libxfoil_noio_shim.so
NOIO_STATIC := $(NOIO_ROOT)/noio-static$(EXEEXT)
NOIO_SHARED := $(NOIO_ROOT)/noio-shared$(EXEEXT)
NOIO_RUNNER := tests/tools/run-library-noio.sh

ifneq ($(EXEEXT),.exe)
$(NOIO_SHIM): tests/tools/LibraryNoIoShim.cpp tests/tools/LibraryNoIoShim.h
	@mkdir -p $(dir $@)
	$(CXX) $(CXXSTD) $(WARNINGS) -fPIC -shared $< -ldl -o $@

$(NOIO_STATIC): tests/tools/LibraryNoIoProof.cpp tests/tools/LibraryNoIoShim.h $(L6_STAGE_MARKER) $(NOIO_SHIM)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXSTD) $(WARNINGS) $(RELEASE_FLAGS) \
		-I"$(L6_STAGE)/usr/include" -Itests/tools $< \
		"$(L6_STAGE)/usr/lib/$(LIB_STATIC_NAME)" \
		-L"$(NOIO_ROOT)" -lxfoil_noio_shim \
		-Wl,-rpath,$(abspath $(NOIO_ROOT)) -o $@

$(NOIO_SHARED): tests/tools/LibraryNoIoProof.cpp tests/tools/LibraryNoIoShim.h $(L6_STAGE_MARKER) $(NOIO_SHIM)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXSTD) $(WARNINGS) $(RELEASE_FLAGS) -DXFOIL_NEXT_USE_SHARED \
		-I"$(L6_STAGE)/usr/include" -Itests/tools $< \
		-L"$(L6_STAGE)/usr/lib" -lxfoil_next \
		-L"$(NOIO_ROOT)" -lxfoil_noio_shim \
		-Wl,-rpath,$(abspath $(L6_STAGE))/usr/lib \
		-Wl,-rpath,$(abspath $(NOIO_ROOT)) -o $@

test-library-noio-static: $(NOIO_STATIC)
	$(NOIO_RUNNER) "$(NOIO_STATIC)" "$(NOIO_SHIM)" static

test-library-noio-shared: $(NOIO_SHARED)
	@ldd "$(NOIO_SHARED)" | grep -F \
		"$(abspath $(L6_STAGE))/usr/lib/libxfoil_next.so" >/dev/null
	$(NOIO_RUNNER) "$(NOIO_SHARED)" "$(NOIO_SHIM)" shared

test-library-noio: test-library-noio-static test-library-noio-shared
else
test-library-noio-static test-library-noio-shared test-library-noio:
	@echo "test-library-noio: armed libc interposition is Linux-only."
	@exit 1
endif

# ---------------------------------------------------------------------------
# Permanent guard gates.
# ---------------------------------------------------------------------------

# Dependency and forbidden-state gates: the current layer gates
# (core manifest and dependencies, io layer, session layer); the
# Makefile synchronization check and the manifest check are unchanged.
check-dependencies:
	python3 tools/check-core-dependencies.py --self-test
	python3 tools/check-core-dependencies.py
	python3 tools/check-core-dependencies.py --scope=session
	python3 tools/check-core-dependencies.py --scope=cli
	python3 tools/check-core-dependencies.py --scope=full

# Release inventory and hygiene gate (non-writing): required files,
# forbidden root runtime artifacts, cache artifacts, OSMAP selection and SHA,
# .bl reference visibility, IoTests reference paths and file modes.
check-release-inventory:
	python3 tools/check-release-inventory.py

# License-header gate (non-writing): binding
# GPL-3.0-or-later project header in all hand-written project sources,
# derived sentence only in category A, original Fortran sources unchanged.
# The self-test checks the negative cases in isolated diagnostic copies.
check-license-headers:
	python3 tools/check-license-headers.py --self-test
	python3 tools/check-license-headers.py

# Public-documentation gate (non-writing): binding documentation set
# (README, docs/public-*, LICENSE, NOTICE, SECURITY) present and linked.
check-public-docs:
	python3 tools/check-public-docs.py

# ---------------------------------------------------------------------------
# Fortran reference and harness checks (non-mutating; no gfortran
# needed for test-reference/test-reference-determinism, since the precompiled
# fortran-bin/xfoil is used; test-harnesses need gfortran).
# ---------------------------------------------------------------------------
check-fortran-sha:
	@echo "XFOIL 6.99 source integrity"
	sha256sum --check fortran-src/xfoil-6.99/SHA256SUMS
	@echo "XFOIL 6.996 source integrity"
	sha256sum --check fortran-src/xfoil-6.996/SHA256SUMS

test-reference:
	tools/fortran-ref.sh all-verify

test-reference-determinism:
	tools/fortran-ref.sh all-determinism

# 16 harness families with --check (never --write); each builder checks
# internally the active OSMAP selection (tools/check-osmap-selection.sh) and
# never compiles osmap_DP.f. The paneling family produces the shared
# byte-exact extractions below build/core-paneling-harness, which the cpi,
# inviscid, spec-failure and xycm-reference use; it therefore runs
# first.
HARNESS_BUILDERS := \
	tools/build-paneling-harnesses.sh \
	$(filter-out tools/build-paneling-harnesses.sh,\
	$(wildcard tools/build-*-harnesses.sh))
test-harnesses:
	@total=0; failed=0; \
	for builder in $(HARNESS_BUILDERS); do \
		total=$$((total+1)); \
		echo "== $$builder --check"; \
		if ! "$$builder" --check; then \
			failed=$$((failed+1)); \
			echo "ERROR: harness family failed: $$builder"; \
		fi; \
	done; \
	echo "Harness families: $$total, failed: $$failed"; \
	test $$((failed)) -eq 0

# ---------------------------------------------------------------------------
# Valgrind: representative paths (not the full suite). Sequential;
# --trace-children captures spawning CLI child processes.
# ---------------------------------------------------------------------------
VALGRIND_FLAGS := -q \
	--trace-children=yes \
	--leak-check=full \
	--show-leak-kinds=definite \
	--errors-for-leak-kinds=definite \
	--error-exitcode=1

VALGRIND_TEST_FILTERS := \
	unit.LibrarySessionL3 \
	integration.LibrarySessionL3 \
	unit.LibraryOneShotL4 \
	integration.LibraryOneShotL4 \
	unit.LibraryProgressL12 \
	integration.LibraryProgressL12 \
	unit.LibrarySkinFrictionL13 \
	integration.LibrarySkinFrictionL13 \
	unit.LibraryOperatingPointCompleteness \
	integration.LibraryOperatingPointCompleteness \
	unit.LibrarySequencePointCompleteness \
	integration.LibrarySequencePointCompleteness \
	integration.LibraryCliParityL5 \
	unit.ApiRunSpecAlphaColdStartUnchanged \
	unit.SessionOperClSequenceIndependentSessions \
	unit.SessionNaca0012SymmetricInviscidRegression \
	unit.DxfExportS14 \
	integration.DxfExportS14 \
	unit.DxfCommandS16 \
	integration.DxfCommandS16 \
	unit.MdesInitNativeAlfaSurvivesPane \
	unit.QdesMenuRedoRepeatsSmoo \
	unit.OperBlfDefaultDprByteExactAgainstFortranCli \
	unit.OperFmomGetxyfDefaultByteExactAgainstFortranCli \
	unit.OperVelsInlineByteExactAgainstFortranCli \
	regression.PaccInviscidDumpByteParityMatchesFortranReference \
	regression.CseqHeadDumpParityMatchesFortranReference \
	regression.Wake2Alfa6ConvergenceParityMatchesFortranReference \
	unit.BatchParserAcceptsMinimalJob \
	batch.ViscousPointRuns \
	batch.PrefixAppliesToNativeOutputFiles \
	batch.QuietSuppressesStdoutKeepsTranscript \
	batch.ClarkYNativeBatchMatchesGolden

test-valgrind: $(BUILDROOT)/debug/xfoil_tests $(BUILDROOT)/debug/$(XFOIL_CLI) \
	$(CORE_ROOT)/core-link-check $(CORE_ROOT)/session-link-check
	@if ! command -v valgrind >/dev/null 2>&1; then \
		echo "ERROR: valgrind is not installed; test-valgrind cannot run"; \
		exit 1; \
	fi
	@for filter in $(VALGRIND_TEST_FILTERS); do \
		echo "== valgrind: $$filter"; \
		XFOIL_CLI_PATH=$(CURDIR)/$(BUILDROOT)/debug/$(XFOIL_CLI) \
		valgrind $(VALGRIND_FLAGS) $(BUILDROOT)/debug/xfoil_tests "$$filter" || exit 1; \
	done
	@echo "== valgrind: core-link-check"
	@valgrind $(VALGRIND_FLAGS) $(CORE_ROOT)/core-link-check
	@echo "== valgrind: session-link-check"
	@valgrind $(VALGRIND_FLAGS) $(CORE_ROOT)/session-link-check

# ---------------------------------------------------------------------------
# Maintainer (writing): golden re-recording only for the
# native batch goldens. Guard and
# triple determinism are binding.
# ---------------------------------------------------------------------------
BATCH_GOLDEN_DIR := $(GOLDEN_DIR)/batch
BATCH_CASES := clark_y naca2312 naca4415 naca0012

update-golden: $(BUILDROOT)/debug/$(XFOIL_CLI)
	@if [ "$(CONFIRM_GOLDEN_UPDATE)" != "yes" ]; then \
		echo "*** WARNING: golden files are only updated with explicit ***"; \
		echo "*** confirmation: CONFIRM_GOLDEN_UPDATE=yes              ***"; \
		echo "    make update-golden CONFIRM_GOLDEN_UPDATE=yes"; \
		exit 1; \
	fi
	@rm -rf $(CAPTURE_DIR)
	@mkdir -p $(CAPTURE_DIR)
	@for batch_case in $(BATCH_CASES); do \
		for run in 1 2 3; do \
			run_dir="$(CAPTURE_DIR)/$${batch_case}-$${run}"; \
			rm -rf "$$run_dir"; \
			mkdir -p "$$run_dir/sample"; \
			cp "$(BATCH_GOLDEN_DIR)/$${batch_case}.xfb" "$$run_dir/job.xfb"; \
			cp sample/CLARK_Y.dat sample/NACA2312.dat "$$run_dir/sample/"; \
			( cd "$$run_dir" \
				&& "$(CURDIR)/$(BUILDROOT)/debug/$(XFOIL_CLI)" --batch job.xfb \
					> /dev/null 2> stderr.txt ); \
			echo $$? > "$$run_dir/exit.txt"; \
		done; \
		echo "Determinism check for case $${batch_case} (three runs):"; \
		cmp "$(CAPTURE_DIR)/$${batch_case}-1/transcript.txt" \
			"$(CAPTURE_DIR)/$${batch_case}-2/transcript.txt"; \
		cmp "$(CAPTURE_DIR)/$${batch_case}-1/transcript.txt" \
			"$(CAPTURE_DIR)/$${batch_case}-3/transcript.txt"; \
		cmp "$(CAPTURE_DIR)/$${batch_case}-1/results.dat" \
			"$(CAPTURE_DIR)/$${batch_case}-2/results.dat"; \
		cmp "$(CAPTURE_DIR)/$${batch_case}-1/results.dat" \
			"$(CAPTURE_DIR)/$${batch_case}-3/results.dat"; \
		cmp "$(CAPTURE_DIR)/$${batch_case}-1/exit.txt" \
			"$(CAPTURE_DIR)/$${batch_case}-2/exit.txt"; \
		cmp "$(CAPTURE_DIR)/$${batch_case}-1/exit.txt" \
			"$(CAPTURE_DIR)/$${batch_case}-3/exit.txt"; \
		test "$$(cat "$(CAPTURE_DIR)/$${batch_case}-1/exit.txt")" = "0"; \
		mkdir -p "$(BATCH_GOLDEN_DIR)"; \
		cp "$(CAPTURE_DIR)/$${batch_case}-1/transcript.txt" \
			"$(BATCH_GOLDEN_DIR)/$${batch_case}_transcript.txt.tmp"; \
		cp "$(CAPTURE_DIR)/$${batch_case}-1/results.dat" \
			"$(BATCH_GOLDEN_DIR)/$${batch_case}_results.dat.tmp"; \
		cp "$(CAPTURE_DIR)/$${batch_case}-1/exit.txt" \
			"$(BATCH_GOLDEN_DIR)/$${batch_case}_exit_code.txt.tmp"; \
		mv "$(BATCH_GOLDEN_DIR)/$${batch_case}_transcript.txt.tmp" \
			"$(BATCH_GOLDEN_DIR)/$${batch_case}_transcript.txt"; \
		mv "$(BATCH_GOLDEN_DIR)/$${batch_case}_results.dat.tmp" \
			"$(BATCH_GOLDEN_DIR)/$${batch_case}_results.dat"; \
		mv "$(BATCH_GOLDEN_DIR)/$${batch_case}_exit_code.txt.tmp" \
			"$(BATCH_GOLDEN_DIR)/$${batch_case}_exit_code.txt"; \
	done
	@echo "Determinism verified; batch golden files updated."
	@echo "Note: manually check/update tests/golden/metadata.md."

# ---------------------------------------------------------------------------
# Cleanup: allowlist of generated paths under build/ only.
# Versioned references (tests/data,
# tests/fortran-reference, tests/golden, fortran-bin, sample, manifests)
# are never removed.
# ---------------------------------------------------------------------------
clean:
	rm -rf $(BUILDROOT)/debug $(BUILDROOT)/debug-clang \
		$(BUILDROOT)/release $(BUILDROOT)/release-clang \
		$(BUILDROOT)/sanitize $(BUILDROOT)/clang \
		$(BUILDROOT)/core $(CAPTURE_DIR) \
		$(BUILDROOT)/test-data $(BUILDROOT)/test-captures \
		$(BUILDROOT)/core-*-harness \
		$(BUILDROOT)/c11a-state-harness-dbl \
		$(BUILDROOT)/core-xycm-harness-dbl \
		$(BUILDROOT)/fortran-reference-* \
		$(PORTABLE_ROOT) \
		$(L6_STAGE) $(L6_CONSUMER) $(DEMO_BUILD_ROOT)

help:
	@echo "Build:"
	@echo "  make                 - release build (default)"
	@echo "  make debug           - debug build (-O0 -g3)"
	@echo "  make release         - release build (-O3 -DNDEBUG)"
	@echo "  make sanitize        - ASan+UBSan build (-O1 -g3)"
	@echo "  make library         - static + shared library (LIB_MODE=release)"
	@echo "  make library-static  - static library only"
	@echo "  make library-shared  - shared library/DLL only"
	@echo "  make install         - staged install via DESTDIR/PREFIX"
	@echo "  make test-consumers  - static/shared consumer smoke"
	@echo "  make library-demo       - standalone staged-library demo (static/shared)"
	@echo "  make test-library-demo  - standalone demo integration tests"
	@echo "  make check-library-dependencies - LIB_SRCS/header dependency check"
	@echo "  make library-symbol-check       - static/shared symbol audit"
	@echo "  make test-library-isolation     - session isolation tests"
	@echo "  make test-library-skin-friction - owning Cf projection tests"
	@echo "  make test-library-operating-point - operating-point CDF/Cf tests"
	@echo "  make test-library-sequence-point - sequence-point scalar tests"
	@echo "  make test-library-noio          - armed static/shared no-I/O proof"
	@echo "  make test-tsan-library          - TSan concurrency diagnostic"
	@echo "  make portable-linux             - build+package Linux portable runtime"
	@echo "  make portable-windows           - build+package Windows portable runtime"
	@echo "  make portable-check-linux       - validate Linux binary and archive"
	@echo "  make portable-check-windows     - validate Windows binary and archive"
	@echo "  make portable-sha256sums        - write separate SHA256SUMS"
	@echo "  make portable-repro-linux       - two clean Linux builds and compare"
	@echo "  make portable-repro-windows     - two clean Windows builds and compare"
	@echo ""
	@echo "Optional GUI skeleton (requires Qt 6.9.x and QT_ROOT):"
	@echo "  make gui                 - build libxfoil_next_gui.a (debug)"
	@echo "  make gui-debug           - build the debug GUI archive"
	@echo "  make gui-release         - build the release GUI archive"
	@echo "  make test-gui            - offscreen GUI tests"
	@echo "  make gui-qcustomplot-check - QCustomPlot vendor/hash and smoke check"
	@echo "  make gui-link-check      - minimal public-header GUI consumer"
	@echo "  make clean-gui           - remove build/gui"
	@echo ""
	@echo "Test:"
	@echo "  make test                - full C++ test suite (debug)"
	@echo "  make test-core           - core test suite (408 tests)"
	@echo "  make test-unit           - unit. prefix filter"
	@echo "  make test-integration    - integration. prefix filter"
	@echo "  make test-regression     - regression. prefix filter"
	@echo "  make test-batch          - native batch suite (batch. prefix)"
	@echo "  make test-sanitize       - full suite against sanitizer build"
	@echo "  make test-valgrind       - representative paths under Valgrind"
	@echo "  make test-reference      - Fortran reference verification (236 cases)"
	@echo "  make test-reference-determinism - Fortran reference determinism"
	@echo "  make test-harnesses      - Fortran harness rebuild/check (requires gfortran)"
	@echo "  make check-fortran-sha   - Fortran source integrity (SHA256SUMS)"
	@echo ""
	@echo "Gates:"
	@echo "  make check-dependencies  - dependency and forbidden-state gates"
	@echo "  make check-release-inventory - release inventory / hygiene gate"
	@echo "  make check-license-headers   - GPL-3.0-or-later source header gate"
	@echo "  make check-public-docs     - public documentation set gate"
	@echo "  make check-gui-dependencies - GUI public-header/source/QCP gate"
	@echo ""
	@echo "Maintainer:"
	@echo "  make update-golden       - recapture goldens (requires CONFIRM_GOLDEN_UPDATE=yes)"
	@echo ""
	@echo "Cleanup:"
	@echo "  make clean"
	@echo ""
	@echo "Variables: CXX (default g++), BUILDROOT (default build)."
	@echo "Example:   make debug CXX=clang++ BUILDROOT=build/clang"
	@echo "Note: run make from the repository root; tests resolve sample/ paths"
	@echo "relative to the root."

# End: Makefile
