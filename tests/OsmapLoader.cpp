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

// file: tests/OsmapLoader.cpp
// Test/diagnostics adapter: reads the binary Fortran unformatted
// sequential file osmap.dat (4-byte length prefix/suffix, little-endian,
// REAL*4) and fills OrrSommerfeldRaw in flat Fortran memory order. The
// numerical core (OrrSommerfeldMap::fromRaw) sees only decoded data.
#include "OsmapLoader.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

using xfoil::core::inverse::kOsMaxH;
using xfoil::core::inverse::kOsMaxR;
using xfoil::core::inverse::kOsMaxW;
using xfoil::core::inverse::OrrSommerfeldMap;
using xfoil::core::inverse::OrrSommerfeldRaw;

namespace {

constexpr std::size_t sz(int v) { return static_cast<std::size_t>(v); }

std::size_t flatIdx(int ir, int iw, int ih, int ic) {
  return (static_cast<std::size_t>(ic - 1) * kOsMaxH +
          static_cast<std::size_t>(ih - 1)) *
             static_cast<std::size_t>(kOsMaxW) * kOsMaxR +
         static_cast<std::size_t>(iw - 1) * kOsMaxR +
         static_cast<std::size_t>(ir - 1);
}

class RecordReader {
 public:
  explicit RecordReader(std::vector<uint8_t> buf) : buf_(std::move(buf)) {}
  bool record(std::vector<uint8_t>& out) {
    int32_t n = 0;
    if (!readInt(n)) return false;
    if (n < 0 || pos_ + static_cast<std::size_t>(n) > buf_.size()) return false;
    out.assign(buf_.begin() + static_cast<std::ptrdiff_t>(pos_),
               buf_.begin() +
                   static_cast<std::ptrdiff_t>(pos_ + static_cast<std::size_t>(n)));
    pos_ += static_cast<std::size_t>(n);
    int32_t n2 = 0;
    if (!readInt(n2) || n2 != n) return false;
    return true;
  }

 private:
  bool readInt(int32_t& v) {
    if (pos_ + 4 > buf_.size()) return false;
    std::memcpy(&v, buf_.data() + pos_, 4);
    pos_ += 4;
    return true;
  }
  std::vector<uint8_t> buf_;
  std::size_t pos_{0};
};

void copyFloats(const std::vector<uint8_t>& rec, std::vector<double>& dst,
                std::size_t count) {
  for (std::size_t k = 0; k < count; ++k) {
    float v = 0.0f;
    std::memcpy(&v, rec.data() + k * 4, 4);
    dst[k] = static_cast<double>(v);
  }
}

}  // namespace

xfoil::core::Result<OrrSommerfeldMap> loadOsmapForTest(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return OrrSommerfeldMap::fromRaw(OrrSommerfeldRaw{});  // empty -> error
  }
  std::vector<uint8_t> buf((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
  RecordReader rr(std::move(buf));
  OrrSommerfeldRaw raw;
  std::vector<uint8_t> rec;

  if (!rr.record(rec) || rec.size() < 12) {
    return OrrSommerfeldMap::fromRaw(OrrSommerfeldRaw{});
  }
  std::memcpy(&raw.nr, rec.data(), 4);
  std::memcpy(&raw.nw, rec.data() + 4, 4);
  std::memcpy(&raw.nh, rec.data() + 8, 4);
  if (raw.nr > kOsMaxR || raw.nw > kOsMaxW || raw.nh > kOsMaxH) {
    return OrrSommerfeldMap::fromRaw(OrrSommerfeldRaw{});
  }

  if (!rr.record(rec)) return OrrSommerfeldMap::fromRaw(OrrSommerfeldRaw{});
  raw.rl.resize(sz(raw.nr));
  copyFloats(rec, raw.rl, sz(raw.nr));
  if (!rr.record(rec)) return OrrSommerfeldMap::fromRaw(OrrSommerfeldRaw{});
  raw.wl.resize(sz(raw.nw));
  copyFloats(rec, raw.wl, sz(raw.nw));
  if (!rr.record(rec)) return OrrSommerfeldMap::fromRaw(OrrSommerfeldRaw{});
  raw.hl.resize(sz(raw.nh));
  copyFloats(rec, raw.hl, sz(raw.nh));
  if (!rr.record(rec)) return OrrSommerfeldMap::fromRaw(OrrSommerfeldRaw{});
  const std::size_t nidx = sz(4 * raw.nh);
  raw.ir1.assign(sz(raw.nh), 0);
  raw.ir2.assign(sz(raw.nh), 0);
  raw.iw1.assign(sz(raw.nh), 0);
  raw.iw2.assign(sz(raw.nh), 0);
  for (std::size_t k = 0; k < nidx; ++k) {
    int32_t v = 0;
    std::memcpy(&v, rec.data() + k * 4, 4);
    if (k % 4 == 0) raw.ir1[k / 4] = v;
    else if (k % 4 == 1) raw.ir2[k / 4] = v;
    else if (k % 4 == 2) raw.iw1[k / 4] = v;
    else raw.iw2[k / 4] = v;
  }

  const std::size_t ntot = sz(2 * kOsMaxH * kOsMaxW * kOsMaxR);
  raw.a.assign(ntot, 0.0);
  raw.ar.assign(ntot, 0.0);
  raw.aw.assign(ntot, 0.0);
  raw.ah.assign(ntot, 0.0);
  raw.arw.assign(ntot, 0.0);
  raw.arh.assign(ntot, 0.0);
  raw.awh.assign(ntot, 0.0);
  raw.arwh.assign(ntot, 0.0);

  bool incomplete = false;
  for (int ic = 2; ic >= 1 && !incomplete; --ic) {
    for (int ih = 1; ih <= raw.nh && !incomplete; ++ih) {
      for (int iw = raw.iw1[sz(ih - 1)]; iw <= raw.iw2[sz(ih - 1)] && !incomplete;
           ++iw) {
        const int cnt = raw.ir2[sz(ih - 1)] - raw.ir1[sz(ih - 1)] + 1;
        std::vector<double>* tables[8] = {&raw.a,   &raw.ar,  &raw.aw,  &raw.ah,
                                          &raw.arw, &raw.arh, &raw.awh, &raw.arwh};
        for (int t = 0; t < 8; ++t) {
          if (!rr.record(rec)) {
            incomplete = true;
            break;
          }
          for (int k = 0; k < cnt; ++k) {
            const int ir = raw.ir1[sz(ih - 1)] + k;
            float v = 0.0f;
            std::memcpy(&v, rec.data() + k * 4, 4);
            (*tables[t])[flatIdx(ir, iw, ih, ic)] = static_cast<double>(v);
          }
        }
      }
    }
  }
  raw.complete = !incomplete;
  return OrrSommerfeldMap::fromRaw(std::move(raw));
}

// End: tests/OsmapLoader.cpp
