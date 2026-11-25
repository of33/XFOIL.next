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

"""Standard-library checks for the canonical GUI source, PNG and ICO."""

import pathlib
import struct
import zlib

ICO_SIZES = ((256, 256), (128, 128), (64, 64), (48, 48),
             (32, 32), (24, 24), (16, 16))
CORNER_POINTS = ((0, 0), (0, 511), (511, 0), (511, 511))
VISIBLE_ALPHA = 32
WHITE_ALPHA = 230
WHITE_MIN_RGB = 245


class IconError(Exception):
    pass


def _paeth(left, above, upper_left):
    p = left + above - upper_left
    pa = abs(p - left)
    pb = abs(p - above)
    pc = abs(p - upper_left)
    if pa <= pb and pa <= pc:
        return left
    if pb <= pc:
        return above
    return upper_left


def read_png_rgba(data):
    """Decode a non-interlaced 8-bit RGBA PNG into a row-major pixel list."""
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise IconError("not a PNG file")
    offset = 8
    width = height = None
    idat = bytearray()
    while offset + 8 <= len(data):
        length = struct.unpack_from(">I", data, offset)[0]
        kind = data[offset + 4:offset + 8]
        payload = data[offset + 8:offset + 8 + length]
        if len(payload) != length:
            raise IconError("truncated PNG chunk")
        if kind == b"IHDR":
            width, height, depth, color, compression, filtering, interlace = (
                struct.unpack(">IIBBBBB", payload))
            if depth != 8 or color != 6 or compression != 0 or filtering != 0:
                raise IconError("expected 8-bit non-filtered RGBA PNG")
            if interlace != 0:
                raise IconError("interlaced PNG is not supported")
        elif kind == b"IDAT":
            idat.extend(payload)
        elif kind == b"IEND":
            break
        offset += 12 + length
    if width is None or height is None or not idat:
        raise IconError("PNG has no IHDR/IDAT data")
    raw = zlib.decompress(bytes(idat))
    stride = width * 4
    if len(raw) != (stride + 1) * height:
        raise IconError("unexpected PNG data length")
    previous = bytearray(stride)
    rows = []
    position = 0
    for _ in range(height):
        filter_type = raw[position]
        position += 1
        scan = bytearray(raw[position:position + stride])
        position += stride
        if filter_type == 1:
            for i in range(4, stride):
                scan[i] = (scan[i] + scan[i - 4]) & 0xFF
        elif filter_type == 2:
            for i in range(stride):
                scan[i] = (scan[i] + previous[i]) & 0xFF
        elif filter_type == 3:
            for i in range(stride):
                left = scan[i - 4] if i >= 4 else 0
                scan[i] = (scan[i] + ((left + previous[i]) >> 1)) & 0xFF
        elif filter_type == 4:
            for i in range(stride):
                left = scan[i - 4] if i >= 4 else 0
                upper_left = previous[i - 4] if i >= 4 else 0
                scan[i] = (scan[i] + _paeth(left, previous[i],
                                            upper_left)) & 0xFF
        elif filter_type != 0:
            raise IconError(f"unsupported PNG filter {filter_type}")
        rows.append([tuple(scan[i:i + 4]) for i in range(0, stride, 4)])
        previous = scan
    return width, height, rows


def read_ico_entries(path):
    data = pathlib.Path(path).read_bytes()
    if len(data) < 6:
        raise IconError("truncated ICO header")
    reserved, kind, count = struct.unpack_from("<HHH", data, 0)
    if reserved != 0 or kind != 1 or count == 0:
        raise IconError("not a valid ICO file")
    if len(data) < 6 + count * 16:
        raise IconError("truncated ICO directory")
    entries = []
    for index in range(count):
        offset = 6 + index * 16
        width, height, colors, _, planes, bitcount, size, position = (
            struct.unpack_from("<BBBBHHII", data, offset))
        width = width or 256
        height = height or 256
        if position + size > len(data):
            raise IconError("ICO frame outside file")
        entries.append({
            "width": width,
            "height": height,
            "colors": colors,
            "planes": planes,
            "bitcount": bitcount,
            "data": data[position:position + size],
        })
    return entries


def decode_ico_frame(entry):
    data = entry["data"]
    if data[:8] == b"\x89PNG\r\n\x1a\n":
        width, height, rows = read_png_rgba(data)
        return width, height, rows
    if len(data) < 40:
        raise IconError("truncated ICO DIB header")
    header_size, width, double_height, planes, bitcount, compression = (
        struct.unpack_from("<IiiHHI", data, 0))
    if header_size < 40 or width <= 0 or double_height <= 0:
        raise IconError("invalid ICO DIB header")
    height = double_height // 2
    if planes != 1 or bitcount != 32 or compression != 0:
        raise IconError("only uncompressed 32-bit ICO frames are supported")
    stride = width * 4
    start = header_size
    if start + stride * height > len(data):
        raise IconError("truncated ICO pixel data")
    rows = []
    for y in range(height):
        source = start + (height - 1 - y) * stride
        row = []
        for x in range(width):
            b, g, r, a = data[source + x * 4:source + x * 4 + 4]
            row.append((r, g, b, a))
        rows.append(row)
    return width, height, rows


def visible_bbox(rows, threshold=VISIBLE_ALPHA):
    xs = []
    ys = []
    for y, row in enumerate(rows):
        for x, pixel in enumerate(row):
            if pixel[3] > threshold:
                xs.append(x)
                ys.append(y)
    if not xs:
        raise IconError("image has no visible pixels")
    return min(xs), min(ys), max(xs), max(ys)


def visible_aspect(rows, threshold=VISIBLE_ALPHA):
    left, top, right, bottom = visible_bbox(rows, threshold)
    return (right - left + 1) / (bottom - top + 1)


def check_canonical_source(path):
    width, height, rows = read_png_rgba(pathlib.Path(path).read_bytes())
    if width < 256 or height < 256:
        raise IconError("canonical icon source is too small")
    left, top, right, bottom = visible_bbox(rows)
    pixels = [pixel for row in rows for pixel in row]
    transparent = sum(1 for pixel in pixels if pixel[3] == 0)
    white = sum(1 for r, g, b, a in pixels
                if a >= WHITE_ALPHA and min(r, g, b) >= WHITE_MIN_RGB)
    if transparent == 0:
        raise IconError("canonical source has no transparent pixels")
    if white == 0:
        raise IconError("canonical source has no white region")
    if left == 0 and top == 0 and right == width - 1 and bottom == height - 1:
        pass
    return {
        "width": width,
        "height": height,
        "aspect": width / height,
        "visible_bbox": (left, top, right, bottom),
        "transparent_fraction": transparent / len(pixels),
        "white_fraction": white / len(pixels),
    }


def check_png(path, expected_width=512, expected_height=512, source_aspect=None):
    width, height, rows = read_png_rgba(pathlib.Path(path).read_bytes())
    if (width, height) != (expected_width, expected_height):
        raise IconError(
            f"PNG is {width}x{height}, expected {expected_width}x{expected_height}")
    for x, y in CORNER_POINTS:
        if y >= height or x >= width:
            continue
        if rows[y][x][3] > 8:
            raise IconError(f"PNG corner {(x, y)} is not transparent")
    left, top, right, bottom = visible_bbox(rows)
    visible_width = right - left + 1
    visible_height = bottom - top + 1
    if visible_width < 2 or visible_height < 2:
        raise IconError("PNG visible area is degenerate")
    if source_aspect is not None:
        actual = visible_width / visible_height
        if abs(actual - source_aspect) > 0.06:
            raise IconError(
                "PNG visible aspect ratio is distorted: "
                f"{actual:.4f} vs source {source_aspect:.4f}")
    left_pad = left
    right_pad = width - 1 - right
    top_pad = top
    bottom_pad = height - 1 - bottom
    if abs(left_pad - right_pad) > 2 or abs(top_pad - bottom_pad) > 2:
        raise IconError(
            "PNG visible area is not centred: "
            f"left/right={left_pad}/{right_pad} "
            f"top/bottom={top_pad}/{bottom_pad}")
    colours = set()
    alpha_zero = 0
    white = 0
    blue = 0
    black = 0
    total = width * height
    for row in rows:
        for r, g, b, a in row:
            if a == 0:
                alpha_zero += 1
            else:
                colours.add((r, g, b, a))
                if a >= WHITE_ALPHA and min(r, g, b) >= WHITE_MIN_RGB:
                    white += 1
                if b > 80 and r < 80 and g < 80:
                    blue += 1
                if r < 50 and g < 50 and b < 50:
                    black += 1
    if alpha_zero == 0:
        raise IconError("PNG has no transparent outside pixels")
    if white == 0:
        raise IconError("PNG lost the white interior")
    if blue == 0 or black == 0:
        raise IconError("PNG lost blue frame or black logo elements")
    if alpha_zero / total < 0.01:
        raise IconError("PNG has no meaningful transparent padding/outside")
    if white / total < 0.02:
        raise IconError("PNG has too little preserved white area")
    if len(colours) < 100:
        raise IconError("PNG has too few distinct visible colours")
    return width, height, len(colours), alpha_zero


def check_ico(path, source_aspect=None):
    entries = read_ico_entries(path)
    actual = {(entry["width"], entry["height"]) for entry in entries}
    missing = [size for size in ICO_SIZES if size not in actual]
    if missing:
        raise IconError(f"ICO is missing sizes: {missing}")
    for entry in entries:
        width, height, rows = decode_ico_frame(entry)
        if (width, height) != (entry["width"], entry["height"]):
            raise IconError("ICO frame dimensions do not match its directory")
        pixels = [pixel for row in rows for pixel in row]
        transparent = sum(1 for pixel in pixels if pixel[3] == 0)
        opaque = sum(1 for pixel in pixels if pixel[3] == 255)
        white = sum(1 for r, g, b, a in pixels
                    if a >= WHITE_ALPHA and min(r, g, b) >= WHITE_MIN_RGB)
        blue = sum(1 for r, g, b, a in pixels
                   if a and b > 80 and r < 80 and g < 80)
        black = sum(1 for r, g, b, a in pixels
                    if a and r < 50 and g < 50 and b < 50)
        if transparent == 0 or opaque == 0 or white == 0 or blue == 0:
            raise IconError(
                f"ICO frame {width}x{height} is empty, unicolour or missing "
                "white/blue structure")
        if black == 0:
            raise IconError(f"ICO frame {width}x{height} lost black logo elements")
        if len(set(pixels)) < 8:
            raise IconError(f"ICO frame {width}x{height} has too few colours")
        if source_aspect is not None:
            actual_aspect = visible_aspect(rows)
            if abs(actual_aspect - source_aspect) > 0.06:
                raise IconError(
                    f"ICO frame {width}x{height} aspect ratio is distorted: "
                    f"{actual_aspect:.4f} vs source {source_aspect:.4f}")
    return sorted(actual)


def check_pair(png, ico, source=None):
    source_info = None
    source_aspect = None
    if source is not None:
        source_info = check_canonical_source(source)
        source_aspect = source_info["aspect"]
    png_info = check_png(png, source_aspect=source_aspect)
    ico_sizes = check_ico(ico, source_aspect=source_aspect)
    return png_info, ico_sizes, source_info
