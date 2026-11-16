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

# File: library-demo/Makefile
# Standalone demo build against an installed xfoil-next package.

CXX ?= c++
CXXSTD ?= -std=c++20 -pthread
CXXFLAGS ?= -O2 -Wall -Wextra -Wpedantic
LINK_MODE ?= shared
PKG_CONFIG ?= pkg-config
PKG_CONFIG_PATH ?=
PKG_CONFIG_SYSROOT_DIR ?=
XFOIL_NEXT_PREFIX ?=
XFOIL_NEXT_INCLUDE ?= $(XFOIL_NEXT_PREFIX)/include
XFOIL_NEXT_LIBDIR ?= $(XFOIL_NEXT_PREFIX)/lib

PKG_CONFIG_AVAILABLE := $(shell command -v "$(PKG_CONFIG)" >/dev/null 2>&1 && echo yes)

ifeq ($(OS),Windows_NT)
EXEEXT := .exe
else
EXEEXT :=
endif

BUILD_DIR := build/$(LINK_MODE)
TARGET := $(BUILD_DIR)/xfoil-next-lib-demo$(EXEEXT)
SOURCES := main.cpp DemoSupport.cpp

ifeq ($(PKG_CONFIG_AVAILABLE),yes)
PKG_CFLAGS := $(shell PKG_CONFIG_PATH="$(PKG_CONFIG_PATH)" \
	PKG_CONFIG_SYSROOT_DIR="$(PKG_CONFIG_SYSROOT_DIR)" \
	$(PKG_CONFIG) --cflags xfoil-next)
ifeq ($(LINK_MODE),static)
PKG_LIBDIR := $(shell PKG_CONFIG_PATH="$(PKG_CONFIG_PATH)" \
	PKG_CONFIG_SYSROOT_DIR="$(PKG_CONFIG_SYSROOT_DIR)" \
	$(PKG_CONFIG) --variable=libdir xfoil-next)
ifeq ($(OS),Windows_NT)
DEMO_LIBS := $(PKG_LIBDIR)/libxfoil_next_static.a -lm
else
DEMO_LIBS := $(PKG_LIBDIR)/libxfoil_next.a -lm
endif
else
DEMO_LIBS := $(shell PKG_CONFIG_PATH="$(PKG_CONFIG_PATH)" \
	PKG_CONFIG_SYSROOT_DIR="$(PKG_CONFIG_SYSROOT_DIR)" \
	$(PKG_CONFIG) --libs xfoil-next)
endif
else
ifeq ($(strip $(XFOIL_NEXT_INCLUDE)),)
$(error pkg-config is not available; set XFOIL_NEXT_PREFIX or XFOIL_NEXT_INCLUDE and XFOIL_NEXT_LIBDIR)
endif
PKG_CFLAGS := -I"$(XFOIL_NEXT_INCLUDE)"
ifeq ($(LINK_MODE),static)
ifeq ($(OS),Windows_NT)
DEMO_LIBS := "$(XFOIL_NEXT_LIBDIR)/libxfoil_next_static.a" -lm
else
DEMO_LIBS := "$(XFOIL_NEXT_LIBDIR)/libxfoil_next.a" -lm
endif
else
DEMO_LIBS := -L"$(XFOIL_NEXT_LIBDIR)" -lxfoil_next
endif
endif

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SOURCES) DemoSupport.h
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXSTD) $(CXXFLAGS) $(PKG_CFLAGS) $(SOURCES) \
		$(DEMO_LIBS) -o $@

clean:
	rm -rf build

# End: library-demo/Makefile
