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

"""Check the Windows subsystem split between GUI and CLI executables."""

import argparse
import pathlib
import sys

from check_gui_windows import CheckError, pe_subsystem

EXPECTED = {"gui": 2, "cli": 3}
NAMES = {2: "IMAGE_SUBSYSTEM_WINDOWS_GUI",
         3: "IMAGE_SUBSYSTEM_WINDOWS_CUI"}


def main():
    parser = argparse.ArgumentParser(
        description="Check Windows GUI and CLI PE subsystem values")
    parser.add_argument("--gui")
    parser.add_argument("--cli")
    args = parser.parse_args()
    if not args.gui and not args.cli:
        parser.error("pass --gui and/or --cli")
    try:
        for kind, path in (("gui", args.gui), ("cli", args.cli)):
            if not path:
                continue
            wanted = EXPECTED[kind]
            actual = pe_subsystem(pathlib.Path(path))
            if actual != wanted:
                raise CheckError(
                    f"{kind} subsystem is {actual}, expected {wanted}")
            print(f"{kind}: {actual} ({NAMES[actual]}) PASS")
    except (CheckError, OSError) as error:
        print(f"check_windows_subsystems.py: error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
