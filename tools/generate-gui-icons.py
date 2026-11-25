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

"""Regenerate the GUI icon derivatives from the owner-approved icon source.

This is an offline icon-generation tool, not a normal build dependency. The
canonical source is resources/gui/Airfoil.Next_icon.png. It is used exactly as
provided: the source is not masked, reconstructed, recoloured or stretched.

A non-square source is scaled proportionally into a transparent 512x512 canvas
and centred. The same proportional square PNG is the base for every ICO frame.
Pillow is required only when this tool is run explicitly.
"""

import argparse
import pathlib

try:
    from PIL import Image
except Exception as error:  # pragma: no cover - environment diagnostic
    raise SystemExit(
        "generate-gui-icons.py: Pillow is required for icon regeneration: "
        f"{error}")

CANONICAL_SOURCE = "resources/gui/Airfoil.Next_icon.png"
PNG_OUTPUT = "resources/gui/xfoil-next-icon.png"
ICO_OUTPUT = "resources/gui/xfoil-next-icon.ico"
CANVAS_SIZE = 512
SIZES = ((256, 256), (128, 128), (64, 64), (48, 48),
         (32, 32), (24, 24), (16, 16))


def proportional_square(source, size=CANVAS_SIZE):
    """Scale the source without distortion and pad with transparency."""
    image = source.convert("RGBA")
    scale = min(size / image.width, size / image.height)
    width = max(1, round(image.width * scale))
    height = max(1, round(image.height * scale))
    resized = image.resize((width, height), Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    canvas.alpha_composite(resized, ((size - width) // 2,
                                     (size - height) // 2))
    return canvas


def main():
    parser = argparse.ArgumentParser(
        description="Regenerate the XFOIL.next GUI PNG/ICO from the canonical "
                    "Airfoil.Next_icon.png")
    parser.add_argument("--source", default=CANONICAL_SOURCE)
    parser.add_argument("--png", default=PNG_OUTPUT)
    parser.add_argument("--ico", default=ICO_OUTPUT)
    args = parser.parse_args()
    source = Image.open(args.source).convert("RGBA")
    icon = proportional_square(source)
    png = pathlib.Path(args.png)
    ico = pathlib.Path(args.ico)
    png.parent.mkdir(parents=True, exist_ok=True)
    ico.parent.mkdir(parents=True, exist_ok=True)
    icon.save(png, format="PNG", optimize=True)
    icon.save(ico, format="ICO", sizes=list(SIZES))
    print(f"source {args.source} ({source.width}x{source.height})")
    print(f"wrote {png} ({icon.width}x{icon.height})")
    print(f"wrote {ico} ({len(SIZES)} sizes)")


if __name__ == "__main__":
    main()
