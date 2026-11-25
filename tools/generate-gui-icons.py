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

"""Regenerate the GUI icon derivatives once from XFOIL.next.png.

This is an offline icon-generation tool, not a normal build dependency. It
fills the transparent interior of the closed hexagon with opaque white while
leaving the blue frame, logo, text, colours and outside transparency intact.

The interior is found from the actual transparent region enclosed by the
hexagon, not from a guessed square or a hard-coded polygon. The white layer is
then composited under the original RGBA image and the result is resized to
512x512. A multi-resolution ICO is written from that PNG.

Requirements for this one-time regeneration only: Python 3, Pillow, NumPy and
SciPy.
"""

import argparse
import pathlib

try:
    import numpy as np
    from PIL import Image, ImageFilter
    from scipy import ndimage
except Exception as error:  # pragma: no cover - environment diagnostic
    raise SystemExit(
        "generate-gui-icons.py: Pillow, NumPy and SciPy are required for icon "
        f"regeneration: {error}")

SIZES = ((256, 256), (128, 128), (64, 64), (48, 48),
         (32, 32), (24, 24), (16, 16))


def find_inner_hole(alpha):
    """Return the transparent component enclosed by the hexagon frame."""
    transparent = alpha == 0
    labels, count = ndimage.label(
        transparent, structure=np.ones((3, 3), dtype=int))
    if not count:
        raise SystemExit("no transparent region found")
    border = set(np.unique(np.concatenate((
        labels[0, :], labels[-1, :], labels[:, 0], labels[:, -1]))))
    candidates = []
    for label in range(1, count + 1):
        if label in border:
            continue
        size = int(np.count_nonzero(labels == label))
        if size:
            candidates.append((size, label))
    if not candidates:
        raise SystemExit("the enclosed transparent region was not found")
    _, label = max(candidates)
    return labels == label


def row_spans(mask):
    """Fill the horizontal span of every row that belongs to the hole."""
    result = np.zeros_like(mask)
    for y in np.flatnonzero(mask.any(axis=1)):
        columns = np.flatnonzero(mask[y])
        if columns.size:
            result[y, columns.min():columns.max() + 1] = True
    return result


def build_icon(source):
    image = source.convert("RGBA")
    rgba = np.array(image)
    alpha = rgba[:, :, 3]
    hole = find_inner_hole(alpha)
    interior = row_spans(hole)
    # Move two source pixels under the inner frame edge so the white layer does
    # not leave a transparent seam. The frame is much thicker than this.
    interior = np.array(
        Image.fromarray((interior * 255).astype(np.uint8), "L")
        .filter(ImageFilter.MaxFilter(5))) > 0
    rgb = rgba[:, :, :3].astype(np.float32)
    alpha_f = rgba[:, :, 3:4].astype(np.float32) / 255.0
    over_white = (rgb * alpha_f + 255.0 * (1.0 - alpha_f)).astype(np.uint8)
    result = rgba.copy()
    result[:, :, :3] = np.where(interior[:, :, None],
                                over_white, rgba[:, :, :3])
    result[:, :, 3] = np.where(interior, 255, rgba[:, :, 3])
    return Image.fromarray(result, "RGBA").resize(
        (512, 512), Image.Resampling.LANCZOS)


def main():
    parser = argparse.ArgumentParser(
        description="Regenerate the XFOIL.next GUI PNG/ICO from the canonical "
                    "root image")
    parser.add_argument("--source", default="XFOIL.next.png")
    parser.add_argument("--png", default="resources/gui/xfoil-next-icon.png")
    parser.add_argument("--ico", default="resources/gui/xfoil-next-icon.ico")
    args = parser.parse_args()
    source = Image.open(args.source).convert("RGBA")
    icon = build_icon(source)
    png = pathlib.Path(args.png)
    ico = pathlib.Path(args.ico)
    png.parent.mkdir(parents=True, exist_ok=True)
    ico.parent.mkdir(parents=True, exist_ok=True)
    icon.save(png, format="PNG", optimize=True)
    icon.save(ico, format="ICO", sizes=list(SIZES))
    print(f"wrote {png} ({icon.width}x{icon.height})")
    print(f"wrote {ico} ({len(SIZES)} sizes)")


if __name__ == "__main__":
    main()
