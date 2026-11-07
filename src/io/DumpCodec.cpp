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

#include "xfoil/io/DumpCodec.h"

#include <cstring>

namespace xfoil::io {
namespace {

constexpr std::size_t kMaxAlloc = 64u * 1024u * 1024u;

std::uint32_t readU32(const std::uint8_t* p) {
  return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
         (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

std::int32_t readI32(const std::uint8_t* p) {
  const std::uint32_t v = readU32(p);
  return static_cast<std::int32_t>(v);
}

double readReal(const std::uint8_t* p, DumpRealWidth width) {
  if (width == DumpRealWidth::k8) {
    const std::uint64_t bits = static_cast<std::uint64_t>(readU32(p)) |
                               (static_cast<std::uint64_t>(readU32(p + 4)) << 32);
    double value = 0.0;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
  }
  const std::uint32_t bits = readU32(p);
  float value = 0.0f;
  std::memcpy(&value, &bits, sizeof(value));
  return static_cast<double>(value);
}

void appendU32(std::vector<std::uint8_t>& out, std::uint32_t v) {
  out.push_back(static_cast<std::uint8_t>(v & 0xffu));
  out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xffu));
  out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xffu));
  out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xffu));
}

void appendReal(std::vector<std::uint8_t>& out, double v, DumpRealWidth width) {
  if (width == DumpRealWidth::k8) {
    std::uint64_t bits = 0;
    std::memcpy(&bits, &v, sizeof(bits));
    appendU32(out, static_cast<std::uint32_t>(bits & 0xffffffffu));
    appendU32(out, static_cast<std::uint32_t>(bits >> 32));
  } else {
    const float f = static_cast<float>(v);
    std::uint32_t bits = 0;
    std::memcpy(&bits, &f, sizeof(bits));
    appendU32(out, bits);
  }
}

class RecordReader {
 public:
  RecordReader(const std::uint8_t* data, std::size_t size) : data_(data), size_(size) {}

  bool next(std::vector<std::uint8_t>& record, DumpDecodeResult& result) {
    if (pos_ >= size_) {
      result.error = DumpError::kMissingRecord;
      result.message = "File ends unexpectedly";
      return false;
    }
    if (size_ - pos_ < 4) {
      result.error = DumpError::kTruncated;
      result.message = "Record marker truncated";
      return false;
    }
    const std::uint32_t front = readU32(data_ + pos_);
    if (front > size_ - pos_ - 8) {
      result.error = DumpError::kTruncated;
      result.message = "Record length exceeds end of file";
      return false;
    }
    if (front > kMaxAlloc) {
      result.error = DumpError::kTooLarge;
      result.message = "Record size exceeds limit";
      return false;
    }
    pos_ += 4;
    record.assign(data_ + pos_, data_ + pos_ + front);
    pos_ += front;
    if (size_ - pos_ < 4) {
      result.error = DumpError::kTruncated;
      result.message = "Trailing record marker missing";
      return false;
    }
    const std::uint32_t back = readU32(data_ + pos_);
    if (front != back) {
      result.error = DumpError::kMarkerMismatch;
      result.message = "Record marker front/back mismatch";
      return false;
    }
    pos_ += 4;
    return true;
  }

  std::size_t position() const { return pos_; }
  std::size_t size() const { return size_; }

 private:
  const std::uint8_t* data_;
  std::size_t size_;
  std::size_t pos_ = 0;
};

std::size_t realBytes(DumpRealWidth width) { return width == DumpRealWidth::k8 ? 8 : 4; }

bool need(const std::vector<std::uint8_t>& rec, std::size_t bytes, DumpDecodeResult& result) {
  if (rec.size() < bytes) {
    result.error = DumpError::kRecordLength;
    result.message = "Record length does not match the expected layout";
    return false;
  }
  return true;
}

}  // namespace

DumpDecodeResult decodeDump(const std::uint8_t* data, std::size_t size, DumpRealWidth realWidth) {
  DumpDecodeResult result;
  RecordReader reader(data, size);
  std::vector<std::uint8_t> rec;
  const std::size_t rb = realBytes(realWidth);
  DumpFile file;

  if (!reader.next(rec, result)) return result;
  if (!need(rec, 48 + 8 + rb, result)) return result;
  file.header.name = std::string(reinterpret_cast<const char*>(rec.data()), 48);
  file.header.code = std::string(reinterpret_cast<const char*>(rec.data() + 48), 8);
  file.header.version = readReal(rec.data() + 56, realWidth);

  if (!reader.next(rec, result)) return result;
  if (!need(rec, 4 * rb, result)) return result;
  file.header.mach = readReal(rec.data(), realWidth);
  file.header.reynMillions = readReal(rec.data() + rb, realWidth);
  file.header.acritTop = readReal(rec.data() + 2 * rb, realWidth);
  file.header.acritBottom = readReal(rec.data() + 3 * rb, realWidth);

  if (!reader.next(rec, result)) return result;
  if (!need(rec, 8, result)) return result;
  file.header.matyp = readI32(rec.data());
  file.header.retyp = readI32(rec.data() + 4);

  if (!reader.next(rec, result)) return result;
  if (!need(rec, 16, result)) return result;
  file.header.iix = readI32(rec.data());
  file.header.ilex = readI32(rec.data() + 4);
  file.header.itex = readI32(rec.data() + 8);
  file.header.iibx = readI32(rec.data() + 12);
  const int n = file.header.iibx;
  if (n < 0 || static_cast<std::size_t>(n) > kMaxAlloc / (2 * rb)) {
    result.error = DumpError::kTooLarge;
    result.message = "Geometry size is outside the supported range";
    return result;
  }

  if (!reader.next(rec, result)) return result;
  if (rec.size() != static_cast<std::size_t>(2) * static_cast<std::size_t>(n) * rb) {
    result.error = DumpError::kRecordLength;
    result.message = "Geometry record does not match N";
    return result;
  }
  file.header.geometry.reserve(static_cast<std::size_t>(n));
  for (int i = 0; i < n; ++i) {
    const double x = readReal(rec.data() + static_cast<std::size_t>(2 * i) * rb, realWidth);
    const double y = readReal(rec.data() + static_cast<std::size_t>(2 * i + 1) * rb, realWidth);
    file.header.geometry.push_back({x, y});
  }

  while (reader.position() < reader.size()) {
    if (!reader.next(rec, result)) return result;
    if (!need(rec, 7 * rb, result)) return result;
    DumpPoint point;
    point.alfaRad = readReal(rec.data(), realWidth);
    point.cl = readReal(rec.data() + rb, realWidth);
    point.cdTot = readReal(rec.data() + 2 * rb, realWidth);
    point.zero = readReal(rec.data() + 3 * rb, realWidth);
    point.cm = readReal(rec.data() + 4 * rb, realWidth);
    point.xtrTop = readReal(rec.data() + 5 * rb, realWidth);
    point.xtrBottom = readReal(rec.data() + 6 * rb, realWidth);

    if (!reader.next(rec, result)) return result;
    if (!need(rec, 16, result)) return result;
    point.nside1 = readI32(rec.data());
    point.nside2 = readI32(rec.data() + 4);
    point.iblte1 = readI32(rec.data() + 8);
    point.iblte2 = readI32(rec.data() + 12);
    if (point.nside1 < 0 || point.nside2 < 0 ||
        static_cast<std::size_t>(point.nside1) > kMaxAlloc / (6 * rb) ||
        static_cast<std::size_t>(point.nside2) > kMaxAlloc / (6 * rb)) {
      result.error = DumpError::kTooLarge;
      result.message = "Side count is outside the supported range";
      return result;
    }

    const int nsides[2] = {point.nside1, point.nside2};
    for (int s = 0; s < 2; ++s) {
      if (!reader.next(rec, result)) return result;
      const std::size_t expected = static_cast<std::size_t>(6) * static_cast<std::size_t>(nsides[s]) * rb;
      if (rec.size() != expected) {
        result.error = DumpError::kRecordLength;
        result.message = "Side record does not match NSIDE";
        return result;
      }
      DumpSide& side = point.sides[s];
      const int ns = nsides[s];
      side.x.resize(static_cast<std::size_t>(ns));
      side.cp.resize(static_cast<std::size_t>(ns));
      side.thet.resize(static_cast<std::size_t>(ns));
      side.dstr.resize(static_cast<std::size_t>(ns));
      side.cf.resize(static_cast<std::size_t>(ns));
      side.ctau.resize(static_cast<std::size_t>(ns));
      for (int i = 0; i < ns; ++i) {
        const std::uint8_t* base = rec.data() + static_cast<std::size_t>(6 * i) * rb;
        side.x[static_cast<std::size_t>(i)] = readReal(base, realWidth);
        side.cp[static_cast<std::size_t>(i)] = readReal(base + rb, realWidth);
        side.thet[static_cast<std::size_t>(i)] = readReal(base + 2 * rb, realWidth);
        side.dstr[static_cast<std::size_t>(i)] = readReal(base + 3 * rb, realWidth);
        side.cf[static_cast<std::size_t>(i)] = readReal(base + 4 * rb, realWidth);
        side.ctau[static_cast<std::size_t>(i)] = readReal(base + 5 * rb, realWidth);
      }
    }
    file.points.push_back(std::move(point));
  }

  result.file = std::move(file);
  return result;
}

std::vector<std::uint8_t> encodeDumpHeader(const DumpHeader& header, DumpRealWidth realWidth) {
  std::vector<std::uint8_t> out;
  const std::size_t rb = realBytes(realWidth);

  std::vector<std::uint8_t> rec;
  std::string name = header.name;
  name.resize(48, ' ');
  rec.insert(rec.end(), name.begin(), name.end());
  std::string code = header.code;
  code.resize(8, ' ');
  rec.insert(rec.end(), code.begin(), code.end());
  appendReal(rec, header.version, realWidth);
  appendU32(out, static_cast<std::uint32_t>(rec.size()));
  out.insert(out.end(), rec.begin(), rec.end());
  appendU32(out, static_cast<std::uint32_t>(rec.size()));

  rec.clear();
  appendReal(rec, header.mach, realWidth);
  appendReal(rec, header.reynMillions, realWidth);
  appendReal(rec, header.acritTop, realWidth);
  appendReal(rec, header.acritBottom, realWidth);
  appendU32(out, static_cast<std::uint32_t>(rec.size()));
  out.insert(out.end(), rec.begin(), rec.end());
  appendU32(out, static_cast<std::uint32_t>(rec.size()));

  rec.clear();
  appendU32(rec, static_cast<std::uint32_t>(header.matyp));
  appendU32(rec, static_cast<std::uint32_t>(header.retyp));
  appendU32(out, static_cast<std::uint32_t>(rec.size()));
  out.insert(out.end(), rec.begin(), rec.end());
  appendU32(out, static_cast<std::uint32_t>(rec.size()));

  rec.clear();
  appendU32(rec, static_cast<std::uint32_t>(header.iix));
  appendU32(rec, static_cast<std::uint32_t>(header.ilex));
  appendU32(rec, static_cast<std::uint32_t>(header.itex));
  appendU32(rec, static_cast<std::uint32_t>(header.iibx));
  appendU32(out, static_cast<std::uint32_t>(rec.size()));
  out.insert(out.end(), rec.begin(), rec.end());
  appendU32(out, static_cast<std::uint32_t>(rec.size()));

  rec.clear();
  for (const auto& xy : header.geometry) {
    appendReal(rec, xy.first, realWidth);
    appendReal(rec, xy.second, realWidth);
  }
  appendU32(out, static_cast<std::uint32_t>(rec.size()));
  out.insert(out.end(), rec.begin(), rec.end());
  appendU32(out, static_cast<std::uint32_t>(rec.size()));
  (void)rb;
  return out;
}

DumpPoint toDumpPoint(const core::export_data::PlxaddPoint& point) {
  DumpPoint out;
  out.alfaRad = point.alfaRad;
  out.cl = point.cl;
  out.cdTot = point.cdTot;
  out.zero = point.zero;
  out.cm = point.cm;
  out.xtrTop = point.xtTop;
  out.xtrBottom = point.xtBottom;
  out.nside1 = point.nside1;
  out.nside2 = point.nside2;
  out.iblte1 = point.iblte1;
  out.iblte2 = point.iblte2;
  for (int s = 0; s < 2; ++s) {
    const auto& src = (s == 0) ? point.top : point.bottom;
    DumpSide& dst = out.sides[s];
    dst.x.reserve(src.size());
    dst.cp.reserve(src.size());
    dst.thet.reserve(src.size());
    dst.dstr.reserve(src.size());
    dst.cf.reserve(src.size());
    dst.ctau.reserve(src.size());
    for (const auto& st : src) {
      dst.x.push_back(st.xx);
      dst.cp.push_back(st.cp);
      dst.thet.push_back(st.thet);
      dst.dstr.push_back(st.dstr);
      dst.cf.push_back(st.cf);
      dst.ctau.push_back(st.ctau);
    }
  }
  return out;
}

std::vector<std::uint8_t> encodeDumpPoint(const DumpPoint& point, DumpRealWidth realWidth) {
  std::vector<std::uint8_t> out;
  std::vector<std::uint8_t> rec;

  appendReal(rec, point.alfaRad, realWidth);
  appendReal(rec, point.cl, realWidth);
  appendReal(rec, point.cdTot, realWidth);
  appendReal(rec, point.zero, realWidth);
  appendReal(rec, point.cm, realWidth);
  appendReal(rec, point.xtrTop, realWidth);
  appendReal(rec, point.xtrBottom, realWidth);
  appendU32(out, static_cast<std::uint32_t>(rec.size()));
  out.insert(out.end(), rec.begin(), rec.end());
  appendU32(out, static_cast<std::uint32_t>(rec.size()));

  rec.clear();
  appendU32(rec, static_cast<std::uint32_t>(point.nside1));
  appendU32(rec, static_cast<std::uint32_t>(point.nside2));
  appendU32(rec, static_cast<std::uint32_t>(point.iblte1));
  appendU32(rec, static_cast<std::uint32_t>(point.iblte2));
  appendU32(out, static_cast<std::uint32_t>(rec.size()));
  out.insert(out.end(), rec.begin(), rec.end());
  appendU32(out, static_cast<std::uint32_t>(rec.size()));

  const int nsides[2] = {point.nside1, point.nside2};
  for (int s = 0; s < 2; ++s) {
    rec.clear();
    const DumpSide& side = point.sides[s];
    for (int i = 0; i < nsides[s]; ++i) {
      const std::size_t idx = static_cast<std::size_t>(i);
      appendReal(rec, side.x[idx], realWidth);
      appendReal(rec, side.cp[idx], realWidth);
      appendReal(rec, side.thet[idx], realWidth);
      appendReal(rec, side.dstr[idx], realWidth);
      appendReal(rec, side.cf[idx], realWidth);
      appendReal(rec, side.ctau[idx], realWidth);
    }
    appendU32(out, static_cast<std::uint32_t>(rec.size()));
    out.insert(out.end(), rec.begin(), rec.end());
    appendU32(out, static_cast<std::uint32_t>(rec.size()));
  }
  return out;
}

}  // namespace xfoil::io
