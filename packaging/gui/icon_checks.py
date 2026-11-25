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

"""Standard-library checks for the corrected GUI PNG and ICO."""

import io
import pathlib
import struct
import zlib

ICO_SIZES = ((256, 256), (128, 128), (64, 64), (48, 48),
             (32, 32), (24, 24), (16, 16))
CORNER_POINTS = ((0, 0), (0, 511), (511, 0), (511, 511))
WHITE_POINTS = ((256, 100), (256, 400), (80, 256), (432, 256),
                (200, 240), (300, 350))


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


def check_png(path, expected_width=512, expected_height=512):
    width, height, rows = read_png_rgba(pathlib.Path(path).read_bytes())
    if (width, height) != (expected_width, expected_height):
        raise IconError(
            f"PNG is {width}x{height}, expected {expected_width}x{expected_height}")
    for x, y in CORNER_POINTS:
        if rows[y][x][3] > 8:
            raise IconError(f"PNG corner {(x, y)} is not transparent")
    for x, y in WHITE_POINTS:
        pixel = rows[y][x]
        if pixel[3] != 255 or min(pixel[:3]) < 250:
            raise IconError(
                f"PNG interior point {(x, y)} is not opaque white: {pixel}")
    colours = set()
    alpha_zero = 0
    blue = 0
    black = 0
    for row in rows:
        for r, g, b, a in row:
            if a == 0:
                alpha_zero += 1
            else:
                colours.add((r, g, b, a))
                if b > 80 and r < 80 and g < 80:
                    blue += 1
                if r < 50 and g < 50 and b < 50:
                    black += 1
    if alpha_zero == 0:
        raise IconError("PNG has no transparent outside pixels")
    if blue == 0 or black == 0:
        raise IconError("PNG lost blue frame or black logo elements")
    if len(colours) < 100:
        raise IconError("PNG has too few distinct visible colours")
    return width, height, len(colours), alpha_zero


def check_ico(path):
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
                    if a == 255 and min(r, g, b) >= 250)
        blue = sum(1 for r, g, b, a in pixels
                   if a and b > 80 and r < 80 and g < 80)
        if transparent == 0 or opaque == 0 or white == 0 or blue == 0:
            raise IconError(
                f"ICO frame {width}x{height} is empty, unicolour or missing "
                "white/blue structure")
        if len(set(pixels)) < 8:
            raise IconError(f"ICO frame {width}x{height} has too few colours")
    return sorted(actual)


def check_pair(png, ico):
    png_info = check_png(png)
    ico_sizes = check_ico(ico)
    return png_info, ico_sizes
