#!/usr/bin/env python3
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

"""Read-only gate for the canonical product executable layout.

The CLI product is ``xfoil.next-cli``; the standalone GUI product is
``xfoil.next``. Products live below BINROOT/<mode>/; tests, libraries and
generated intermediates live below BUILDROOT. The former GUI name
``xfoil.next-gui`` must not reappear.
"""

import argparse
import os
import stat
import sys

CLI_NAME = "xfoil.next-cli"
GUI_NAME = "xfoil.next"
OLD_GUI_NAME = "xfoil.next-gui"
TEST_MARKERS = (
    "xfoil_tests",
    "xfoil_core_tests",
    "xfoil_gui_tests",
    "core-link-check",
    "session-link-check",
    "library-link-check",
    "gui-link-check",
    "consumer-",
    "noio-",
    "_tests",
    "-tests",
)
LIBRARY_SUFFIXES = (".a", ".so", ".dylib", ".dll", ".lib")


def executable(path):
    try:
        mode = os.stat(path).st_mode
    except OSError:
        return False
    return bool(mode & (stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH))


def main(argv):
    parser = argparse.ArgumentParser(
        description="Check the canonical bin/ product executable layout.")
    parser.add_argument("--root", default=".", help="repository root")
    parser.add_argument("--binroot", default="bin",
                        help="product binary root (default: bin)")
    parser.add_argument("--buildroot", default="build",
                        help="intermediate build root (default: build)")
    parser.add_argument("--suffix", default="",
                        help="platform executable suffix, for example .exe")
    parser.add_argument("--modes", default="debug,release",
                        help="comma-separated modes (default: debug,release)")
    parser.add_argument("--require-gui", action="store_true",
                        help="require the GUI product binary as well as the CLI")
    args = parser.parse_args(argv[1:])

    root = os.path.abspath(args.root)
    if not os.path.isdir(root):
        print("ERROR: root directory not found: %s" % root, file=sys.stderr)
        return 2

    binroot = os.path.join(root, args.binroot)
    buildroot = os.path.join(root, args.buildroot)
    modes = [mode.strip() for mode in args.modes.split(",") if mode.strip()]
    if not modes:
        print("ERROR: at least one mode is required", file=sys.stderr)
        return 2

    failures = []
    products = []
    for mode in modes:
        cli = os.path.join(binroot, mode, CLI_NAME + args.suffix)
        products.append(cli)
        if not os.path.isfile(cli):
            failures.append("missing CLI product binary: %s" %
                            os.path.relpath(cli, root))
        elif not args.suffix and not executable(cli):
            failures.append("CLI product binary is not executable: %s" %
                            os.path.relpath(cli, root))

        old_gui = os.path.join(binroot, mode,
                               OLD_GUI_NAME + args.suffix)
        if os.path.lexists(old_gui):
            failures.append("old GUI product binary still exists: %s" %
                            os.path.relpath(old_gui, root))

        if args.require_gui:
            gui = os.path.join(binroot, mode, GUI_NAME + args.suffix)
            products.append(gui)
            if not os.path.isfile(gui):
                failures.append("missing GUI product binary: %s" %
                                os.path.relpath(gui, root))
            elif not args.suffix and not executable(gui):
                failures.append("GUI product binary is not executable: %s" %
                                os.path.relpath(gui, root))

        for old_name in (CLI_NAME, CLI_NAME + ".exe", GUI_NAME,
                         GUI_NAME + ".exe", OLD_GUI_NAME,
                         OLD_GUI_NAME + ".exe"):
            old = os.path.join(buildroot, mode, old_name)
            if os.path.lexists(old):
                failures.append("product path below build/: %s" %
                                os.path.relpath(old, root))

    if os.path.isdir(binroot):
        for dirpath, _dirnames, filenames in os.walk(binroot):
            for name in filenames:
                rel = os.path.relpath(os.path.join(dirpath, name), root)
                lowered = name.lower()
                if OLD_GUI_NAME in lowered:
                    failures.append("old GUI name below bin/: %s" % rel)
                if any(marker in lowered for marker in TEST_MARKERS):
                    failures.append("test artifact below bin/: %s" % rel)
                if lowered.endswith(LIBRARY_SUFFIXES):
                    failures.append("library artifact below bin/: %s" % rel)
    else:
        failures.append("product binary root missing: %s" % args.binroot)

    if failures:
        print("Product binary layout: FAIL (%d finding(s))" % len(failures))
        for failure in failures:
            print("  - %s" % failure)
        return 1

    print("Product binary layout: OK")
    for product in products:
        print("  %s" % os.path.relpath(product, root))
    print("  intermediates: %s/%s" % (args.buildroot, "<mode>"))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
