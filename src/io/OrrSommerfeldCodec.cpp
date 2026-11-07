// XFOIL.next
//
// Copyright (C) 2026 Olaf Frommann
// https://github.com/of33/XFOIL.next
//
// XFOIL.next is based on XFOIL 6.99 by Mark Drela and
// Harold Youngren.
//
// This file contains a C++ implementation derived from XFOIL 6.99.
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

#include "xfoil/io/OrrSommerfeldCodec.h"

#include <cstring>
#include <vector>

namespace xfoil::io {
namespace {

using core::inverse::kOsMaxH;
using core::inverse::kOsMaxR;
using core::inverse::kOsMaxW;
using core::inverse::OrrSommerfeldRaw;

std::int32_t readI32(const std::uint8_t* p) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(p[0]) |
                                   (static_cast<std::uint32_t>(p[1]) << 8) |
                                   (static_cast<std::uint32_t>(p[2]) << 16) |
                                   (static_cast<std::uint32_t>(p[3]) << 24));
}

double readF32(const std::uint8_t* p) {
  const std::uint32_t bits = static_cast<std::uint32_t>(readI32(p));
  float v = 0.0f;
  std::memcpy(&v, &bits, sizeof(v));
  return static_cast<double>(v);
}

class RecordReader {
 public:
  RecordReader(const std::uint8_t* data, std::size_t size) : data_(data), size_(size) {}
  bool next(std::vector<std::uint8_t>& out, OsmapDecodeResult& result) {
    if (pos_ + 4 > size_) {
      result.error = OsmapError::kTruncated;
      result.message = "Record marker missing";
      return false;
    }
    const std::int32_t n = readI32(data_ + pos_);
    if (n < 0) {
      result.error = OsmapError::kTruncated;
      result.message = "negative Record length";
      return false;
    }
    if (pos_ + 4 + static_cast<std::size_t>(n) + 4 > size_) {
      result.error = OsmapError::kTruncated;
      result.message = "Record truncated";
      return false;
    }
    out.assign(data_ + pos_ + 4, data_ + pos_ + 4 + static_cast<std::size_t>(n));
    pos_ += 4 + static_cast<std::size_t>(n);
    const std::int32_t back = readI32(data_ + pos_);
    if (back != n) {
      result.error = OsmapError::kMarkerMismatch;
      result.message = "Record marker front/back mismatch";
      return false;
    }
    pos_ += 4;
    return true;
  }

 private:
  const std::uint8_t* data_;
  std::size_t size_;
  std::size_t pos_ = 0;
};

}  // namespace

OsmapDecodeResult decodeOrrSommerfeldRaw(const std::uint8_t* data, std::size_t size) {
  OsmapDecodeResult result;
  RecordReader reader(data, size);
  std::vector<std::uint8_t> rec;
  OrrSommerfeldRaw raw;

  if (!reader.next(rec, result)) return result;
  if (rec.size() < 12) {
    result.error = OsmapError::kTruncated;
    result.message = "Dimensions record too short";
    return result;
  }
  raw.nr = readI32(rec.data());
  raw.nw = readI32(rec.data() + 4);
  raw.nh = readI32(rec.data() + 8);
  if (raw.nr <= 0 || raw.nw <= 0 || raw.nh <= 0 || raw.nr > kOsMaxR || raw.nw > kOsMaxW ||
      raw.nh > kOsMaxH) {
    result.error = OsmapError::kBadDimensions;
    result.message = "Dimensions outside kernel capacity";
    return result;
  }

  if (!reader.next(rec, result)) return result;
  if (rec.size() < static_cast<std::size_t>(4 * raw.nr)) {
    result.error = OsmapError::kTruncated;
    result.message = "rl record too short";
    return result;
  }
  raw.rl.resize(static_cast<std::size_t>(raw.nr));
  for (int i = 0; i < raw.nr; ++i) raw.rl[static_cast<std::size_t>(i)] = readF32(rec.data() + 4 * i);

  if (!reader.next(rec, result)) return result;
  if (rec.size() < static_cast<std::size_t>(4 * raw.nw)) {
    result.error = OsmapError::kTruncated;
    result.message = "wl record too short";
    return result;
  }
  raw.wl.resize(static_cast<std::size_t>(raw.nw));
  for (int i = 0; i < raw.nw; ++i) raw.wl[static_cast<std::size_t>(i)] = readF32(rec.data() + 4 * i);

  if (!reader.next(rec, result)) return result;
  if (rec.size() < static_cast<std::size_t>(4 * raw.nh)) {
    result.error = OsmapError::kTruncated;
    result.message = "hl record too short";
    return result;
  }
  raw.hl.resize(static_cast<std::size_t>(raw.nh));
  for (int i = 0; i < raw.nh; ++i) raw.hl[static_cast<std::size_t>(i)] = readF32(rec.data() + 4 * i);

  if (!reader.next(rec, result)) return result;
  if (rec.size() < static_cast<std::size_t>(16 * raw.nh)) {
    result.error = OsmapError::kTruncated;
    result.message = "index record too short";
    return result;
  }
  raw.ir1.assign(static_cast<std::size_t>(raw.nh), 0);
  raw.ir2.assign(static_cast<std::size_t>(raw.nh), 0);
  raw.iw1.assign(static_cast<std::size_t>(raw.nh), 0);
  raw.iw2.assign(static_cast<std::size_t>(raw.nh), 0);
  for (int k = 0; k < 4 * raw.nh; ++k) {
    const std::int32_t v = readI32(rec.data() + 4 * k);
    if (k % 4 == 0) raw.ir1[static_cast<std::size_t>(k / 4)] = v;
    else if (k % 4 == 1) raw.ir2[static_cast<std::size_t>(k / 4)] = v;
    else if (k % 4 == 2) raw.iw1[static_cast<std::size_t>(k / 4)] = v;
    else raw.iw2[static_cast<std::size_t>(k / 4)] = v;
  }

  const std::size_t ntot = static_cast<std::size_t>(2) * kOsMaxH * kOsMaxW * kOsMaxR;
  raw.a.assign(ntot, 0.0);
  raw.ar.assign(ntot, 0.0);
  raw.aw.assign(ntot, 0.0);
  raw.ah.assign(ntot, 0.0);
  raw.arw.assign(ntot, 0.0);
  raw.arh.assign(ntot, 0.0);
  raw.awh.assign(ntot, 0.0);
  raw.arwh.assign(ntot, 0.0);

  auto flat = [](int ir, int iw, int ih, int ic) {
    return (static_cast<std::size_t>(ic - 1) * static_cast<std::size_t>(kOsMaxH) +
            static_cast<std::size_t>(ih - 1)) *
               static_cast<std::size_t>(kOsMaxW) * static_cast<std::size_t>(kOsMaxR) +
           static_cast<std::size_t>(iw - 1) * static_cast<std::size_t>(kOsMaxR) +
           static_cast<std::size_t>(ir - 1);
  };

  bool incomplete = false;
  for (int ic = 2; ic >= 1 && !incomplete; --ic) {
    for (int ih = 1; ih <= raw.nh && !incomplete; ++ih) {
      for (int iw = raw.iw1[static_cast<std::size_t>(ih - 1)];
           iw <= raw.iw2[static_cast<std::size_t>(ih - 1)] && !incomplete; ++iw) {
        const int cnt = raw.ir2[static_cast<std::size_t>(ih - 1)] -
                        raw.ir1[static_cast<std::size_t>(ih - 1)] + 1;
        std::vector<double>* tables[8] = {&raw.a,   &raw.ar,  &raw.aw,  &raw.ah,
                                          &raw.arw, &raw.arh, &raw.awh, &raw.arwh};
        for (int t = 0; t < 8; ++t) {
          if (!reader.next(rec, result)) {
            incomplete = true;
            result.error = OsmapError::kNone;
            break;
          }
          if (rec.size() < static_cast<std::size_t>(4 * cnt)) {
            incomplete = true;
            result.error = OsmapError::kNone;
            break;
          }
          for (int k = 0; k < cnt; ++k) {
            const int ir = raw.ir1[static_cast<std::size_t>(ih - 1)] + k;
            (*tables[t])[flat(ir, iw, ih, ic)] = readF32(rec.data() + 4 * k);
          }
        }
      }
    }
    if (ic == 2 && incomplete) {
      // Fortran NOFILE analogue: only the IC=2 path is present.
      raw.complete = false;
      incomplete = false;
      break;
    }
  }

  result.raw = std::move(raw);
  return result;
}

}  // namespace xfoil::io
