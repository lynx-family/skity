// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/codec/codec_priv.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <vector>

#include "src/codec/core_bridge.hpp"

namespace skity {
namespace codec_priv {

namespace {

// Fixed-point scale for resampling weights: 1 << 16 == one source pixel.
constexpr int32_t kFixedOne = 1 << 16;

struct Contribution {
  int32_t index;
  int32_t weight;
};

/**
 * Builds the per-output-pixel list of (source index, weight) contributions
 * along one axis. Weights are fixed-point overlaps of the output pixel's
 * source footprint with each source pixel, so dividing the weighted sums by
 * the accumulated weight yields a true area average — integer ratios become
 * exact subsampling, arbitrary ratios stay alias-free.
 *
 * Extreme upscales (source footprint below one fixed-point unit) would leave
 * an output pixel without contributions and a zero accumulated weight; such
 * pixels fall back to the nearest source pixel at full weight.
 */
void BuildContributions(int32_t src_dim, int32_t dst_dim,
                        std::vector<Contribution>* contributions,
                        std::vector<int32_t>* offsets) {
  offsets->assign(dst_dim + 1, 0);
  contributions->clear();

  for (int32_t dst = 0; dst < dst_dim; dst++) {
    (*offsets)[dst] = static_cast<int32_t>(contributions->size());

    int64_t start = static_cast<int64_t>(dst) * src_dim * kFixedOne / dst_dim;
    int64_t end = static_cast<int64_t>(dst + 1) * src_dim * kFixedOne / dst_dim;

    int32_t first = static_cast<int32_t>(start / kFixedOne);
    int32_t last = static_cast<int32_t>((end + kFixedOne - 1) / kFixedOne);

    for (int32_t i = first; i < last; i++) {
      int64_t lo =
          std::max<int64_t>(start, static_cast<int64_t>(i) * kFixedOne);
      int64_t hi =
          std::min<int64_t>(end, static_cast<int64_t>(i + 1) * kFixedOne);
      if (hi > lo) {
        contributions->push_back({i, static_cast<int32_t>(hi - lo)});
      }
    }

    if ((*offsets)[dst] == static_cast<int32_t>(contributions->size())) {
      // start < src_dim * kFixedOne for dst < dst_dim, so first is in range.
      contributions->push_back({first, kFixedOne});
    }
  }

  (*offsets)[dst_dim] = static_cast<int32_t>(contributions->size());
}

inline uint8_t PremulChannel(uint8_t c, uint8_t a) {
  return static_cast<uint8_t>((static_cast<uint32_t>(c) * a + 127) / 255);
}

inline uint8_t UnpremulChannel(uint8_t c, uint8_t a) {
  if (a == 0) {
    return 0;
  }
  return static_cast<uint8_t>(
      std::min<uint32_t>(255, (static_cast<uint32_t>(c) * 255 + a / 2) / a));
}

inline uint8_t DivRound(int64_t value, int64_t weight) {
  return static_cast<uint8_t>((value + weight / 2) / weight);
}

// Computes a * b into *out in size_t; returns false on overflow. Buffer sizes
// derived from image dimensions must go through this — decoding a pixmap with
// pathological dimensions (width > 2^29) would otherwise overflow the int
// expressions and under-allocate.
inline bool CheckedMul(size_t a, size_t b, size_t* out) {
  if (b != 0 && a > std::numeric_limits<size_t>::max() / b) {
    return false;
  }
  *out = a * b;
  return true;
}

}  // namespace

void CodecTransformLinePremul(uint8_t* dst, uint8_t* src, int width,
                              int bytes_per_pixel) {
  // currently we only support RGBA or BGRA
  if (bytes_per_pixel != 4) {
    return;
  }

  auto dst32 = reinterpret_cast<uint32_t*>(dst);
  auto src32 = reinterpret_cast<uint32_t*>(src);
  for (int x = 0; x < width; x++) {
    auto pm_color = *src32;
    *dst32 = core_bridge::ColorToPMColor(pm_color);

    dst32++;
    src32++;
  }
}

void CodecTransformLineUnpremul(uint8_t* dst, uint8_t* src, int width,
                                int bytes_per_pixel) {
  // currently we only support RGBA or BGRA
  if (bytes_per_pixel != 4) {
    return;
  }

  auto dst32 = reinterpret_cast<uint32_t*>(dst);
  auto src32 = reinterpret_cast<uint32_t*>(src);
  for (int x = 0; x < width; x++) {
    auto pm_color = *src32;
    *dst32 = core_bridge::PMColorToColor(pm_color);

    dst32++;
    src32++;
  }
}

void CodecTransformLineSwizzelRB(uint8_t* dst, uint8_t* src, int width,
                                 int bytes_per_pixel) {
  // currently we only support RGBA or BGRA
  if (bytes_per_pixel != 4) {
    return;
  }

  auto dst32 = reinterpret_cast<uint32_t*>(dst);
  auto src32 = reinterpret_cast<uint32_t*>(src);
  for (int x = 0; x < width; x++) {
    auto pm_color = *src32;
    *dst32 = ColorSwizzleRB(pm_color);

    dst32++;
    src32++;
  }
}

std::function<void(uint8_t* dst, uint8_t* src, int width, int bytes_per_pixel)>
ChooseLineTransformFunc(ColorType color_type, AlphaType alpha_type) {
  if (color_type != ColorType::kRGBA && color_type != ColorType::kBGRA) {
    return CodecTransformLineByPass;
  }

  if (color_type == ColorType::kRGBA) {
    if (alpha_type == AlphaType::kPremul_AlphaType) {
      return CodecTransformLineUnpremul;
    } else {
      return CodecTransformLineByPass;
    }
  }

  if (color_type == ColorType::kBGRA) {
    if (alpha_type == AlphaType::kUnpremul_AlphaType) {
      return CodecTransformLineSwizzelRB;
    } else {
      return [](uint8_t* dst, uint8_t* src, int width, int bytes_per_pixel) {
        CodecTransformLineUnpremul(dst, src, width, bytes_per_pixel);
        CodecTransformLineSwizzelRB(dst, src, width, bytes_per_pixel);
      };
    }
  }

  return CodecTransformLineByPass;
}

std::shared_ptr<Pixmap> ResamplePixmap(const std::shared_ptr<Pixmap>& src,
                                       int32_t dst_width, int32_t dst_height) {
  if (!src || dst_width <= 0 || dst_height <= 0) {
    return nullptr;
  }

  // The module's canonical decode output is unpremul RGBA8888; anything else
  // is a caller bug, refuse rather than produce garbage.
  if (src->GetColorType() != ColorType::kRGBA ||
      src->GetAlphaType() != AlphaType::kUnpremul_AlphaType) {
    return nullptr;
  }

  const int32_t src_width = static_cast<int32_t>(src->Width());
  const int32_t src_height = static_cast<int32_t>(src->Height());

  if (src_width <= 0 || src_height <= 0) {
    return nullptr;
  }

  if (src_width == dst_width && src_height == dst_height) {
    return src;
  }

  std::vector<Contribution> x_contrib;
  std::vector<int32_t> x_offsets;
  BuildContributions(src_width, dst_width, &x_contrib, &x_offsets);

  std::vector<Contribution> y_contrib;
  std::vector<int32_t> y_offsets;
  BuildContributions(src_height, dst_height, &y_contrib, &y_offsets);

  // Pass 1: horizontal box filter into a premul intermediate of
  // src_height x dst_width. All byte sizes and pixel offsets are computed in
  // size_t — at int32 dimensions the plain int expressions overflow first.
  size_t src_row_bytes = 0;
  size_t mid_row_bytes = 0;
  size_t mid_bytes = 0;
  if (!CheckedMul(static_cast<size_t>(src_width), 4, &src_row_bytes) ||
      !CheckedMul(static_cast<size_t>(dst_width), 4, &mid_row_bytes) ||
      !CheckedMul(static_cast<size_t>(src_height), mid_row_bytes, &mid_bytes)) {
    return nullptr;
  }

  std::vector<uint8_t> premul_row(src_row_bytes);
  std::vector<uint8_t> mid(mid_bytes);

  for (int32_t y = 0; y < src_height; y++) {
    const uint8_t* row = core_bridge::PixmapAddr8(src.get(), 0, y);
    for (int32_t x = 0; x < src_width; x++) {
      size_t idx = static_cast<size_t>(x) * 4;
      uint8_t a = row[idx + 3];
      premul_row[idx + 0] = PremulChannel(row[idx + 0], a);
      premul_row[idx + 1] = PremulChannel(row[idx + 1], a);
      premul_row[idx + 2] = PremulChannel(row[idx + 2], a);
      premul_row[idx + 3] = a;
    }

    uint8_t* mid_row = mid.data() + static_cast<size_t>(y) * mid_row_bytes;
    for (int32_t x = 0; x < dst_width; x++) {
      int64_t r = 0, g = 0, b = 0, a = 0, weight = 0;
      for (int32_t c = x_offsets[x]; c < x_offsets[x + 1]; c++) {
        const Contribution& contrib = x_contrib[c];
        const uint8_t* p =
            premul_row.data() + static_cast<size_t>(contrib.index) * 4;
        r += static_cast<int64_t>(p[0]) * contrib.weight;
        g += static_cast<int64_t>(p[1]) * contrib.weight;
        b += static_cast<int64_t>(p[2]) * contrib.weight;
        a += static_cast<int64_t>(p[3]) * contrib.weight;
        weight += contrib.weight;
      }

      uint8_t* out = mid_row + static_cast<size_t>(x) * 4;
      out[0] = DivRound(r, weight);
      out[1] = DivRound(g, weight);
      out[2] = DivRound(b, weight);
      out[3] = DivRound(a, weight);
    }
  }

  // Pass 2: vertical box filter, converting back to unpremul on output.
  auto dst = core_bridge::MakePixmapAlloc(
      static_cast<uint32_t>(dst_width), static_cast<uint32_t>(dst_height),
      AlphaType::kUnpremul_AlphaType, ColorType::kRGBA);

  std::vector<int64_t> acc(mid_row_bytes);
  for (int32_t y = 0; y < dst_height; y++) {
    std::fill(acc.begin(), acc.end(), 0);

    int64_t weight = 0;
    for (int32_t c = y_offsets[y]; c < y_offsets[y + 1]; c++) {
      const uint8_t* mid_row =
          mid.data() + static_cast<size_t>(y_contrib[c].index) * mid_row_bytes;
      int32_t row_weight = y_contrib[c].weight;
      weight += row_weight;
      for (size_t x = 0; x < mid_row_bytes; x++) {
        acc[x] += static_cast<int64_t>(mid_row[x]) * row_weight;
      }
    }

    uint8_t* out_row = core_bridge::PixmapWritableAddr8(dst.get(), 0, y);
    for (int32_t x = 0; x < dst_width; x++) {
      size_t idx = static_cast<size_t>(x) * 4;
      uint8_t a = DivRound(acc[idx + 3], weight);
      out_row[idx + 0] = UnpremulChannel(DivRound(acc[idx + 0], weight), a);
      out_row[idx + 1] = UnpremulChannel(DivRound(acc[idx + 1], weight), a);
      out_row[idx + 2] = UnpremulChannel(DivRound(acc[idx + 2], weight), a);
      out_row[idx + 3] = a;
    }
  }

  return dst;
}

std::shared_ptr<Pixmap> ResamplePixmapToSize(const std::shared_ptr<Pixmap>& src,
                                             int32_t dst_width,
                                             int32_t dst_height) {
  if (!src || dst_width <= 0 || dst_height <= 0) {
    return src;
  }

  auto resampled = ResamplePixmap(src, dst_width, dst_height);
  return resampled ? resampled : src;
}

std::shared_ptr<Pixmap> ResamplePixmapToTarget(
    const std::shared_ptr<Pixmap>& src, const DecodeOptions& options) {
  int32_t target_width = 0;
  int32_t target_height = 0;
  if (!ResolveTargetSize(src ? static_cast<int32_t>(src->Width()) : 0,
                         src ? static_cast<int32_t>(src->Height()) : 0, options,
                         &target_width, &target_height)) {
    return src;
  }

  return ResamplePixmapToSize(src, target_width, target_height);
}

namespace {

// Adjugate / determinant inversion of a row-major 3x3 matrix.
bool Invert3x3(const float m[9], float inv[9]) {
  float det = m[0] * (m[4] * m[8] - m[5] * m[7]) -
              m[1] * (m[3] * m[8] - m[5] * m[6]) +
              m[2] * (m[3] * m[7] - m[4] * m[6]);
  if (det == 0.f || !std::isfinite(det)) {
    return false;
  }
  float inv_det = 1.f / det;
  inv[0] = (m[4] * m[8] - m[5] * m[7]) * inv_det;
  inv[1] = (m[2] * m[7] - m[1] * m[8]) * inv_det;
  inv[2] = (m[1] * m[5] - m[2] * m[4]) * inv_det;
  inv[3] = (m[5] * m[6] - m[3] * m[8]) * inv_det;
  inv[4] = (m[0] * m[8] - m[2] * m[6]) * inv_det;
  inv[5] = (m[2] * m[3] - m[0] * m[5]) * inv_det;
  inv[6] = (m[3] * m[7] - m[4] * m[6]) * inv_det;
  inv[7] = (m[1] * m[6] - m[0] * m[7]) * inv_det;
  inv[8] = (m[0] * m[4] - m[1] * m[3]) * inv_det;
  return true;
}

// CIE xy chromaticity (x, y) to XYZ with Y scaled to 1.
void XYZFromXY(float x, float y, float* xyz) {
  xyz[0] = x / y;
  xyz[1] = 1.f;
  xyz[2] = (1.f - x - y) / y;
}

// Row-major 3x3 product: out = lhs * rhs.
void Multiply3x3(const float lhs[9], const float rhs[9], float out[9]) {
  for (int row = 0; row < 3; row++) {
    for (int col = 0; col < 3; col++) {
      out[row * 3 + col] = lhs[row * 3] * rhs[col] +
                           lhs[row * 3 + 1] * rhs[3 + col] +
                           lhs[row * 3 + 2] * rhs[6 + col];
    }
  }
}

// Bradford cone response matrix (row-major): the standard basis for
// chromatic adaptation between reference whites.
constexpr float kBradford[9] = {
    0.8951f,  0.2664f,  -0.1614f,  //
    -0.7502f, 1.7135f,  0.0367f,   //
    0.0389f,  -0.0685f, 1.0296f,
};

// CIE illuminant D50 as XYZ with Y == 1: the reference white every skity
// gamut matrix is adapted to (see ColorSpace::gamut_xyz).
constexpr float kD50XYZ[3] = {0.96422f, 1.f, 0.82521f};

// D65 chromaticity: the native white point of sRGB, Display P3 and nearly
// every other RGB display space. cHRM chunks are written relative to it.
constexpr float kD65X = 0.3127f;
constexpr float kD65Y = 0.3290f;

// Builds the Bradford chromatic adaptation matrix mapping XYZ values relative
// to src_white onto XYZ values relative to dst_white:
//   adaptation = bradford^-1 * diag(dst_cone / src_cone) * bradford
// where the cone responses are bradford * white. Returns false for a
// degenerate white point.
bool BradfordAdaptation(const float src_white[3], const float dst_white[3],
                        float adaptation[9]) {
  float inv_bradford[9];
  if (!Invert3x3(kBradford, inv_bradford)) {
    return false;
  }
  float scaled[9];
  for (int row = 0; row < 3; row++) {
    const float* cone = kBradford + row * 3;
    float src_cone = cone[0] * src_white[0] + cone[1] * src_white[1] +
                     cone[2] * src_white[2];
    float dst_cone = cone[0] * dst_white[0] + cone[1] * dst_white[1] +
                     cone[2] * dst_white[2];
    if (!(src_cone > 0.f) || !(dst_cone > 0.f) || !std::isfinite(src_cone) ||
        !std::isfinite(dst_cone)) {
      return false;
    }
    float gain = dst_cone / src_cone;
    for (int col = 0; col < 3; col++) {
      scaled[row * 3 + col] = cone[col] * gain;
    }
  }
  Multiply3x3(inv_bradford, scaled, adaptation);
  return true;
}

}  // namespace

bool CodecGamutFromChromaticities(const float chromaticities[8],
                                  float gamut_xyz[9]) {
  const float* c = chromaticities;
  for (int i = 0; i < 8; i++) {
    // Values are ratios; x + y must stay below 1 for a physical color.
    if (!(c[i] > 0.f) || c[i] >= 1.f || !std::isfinite(c[i])) {
      return false;
    }
  }
  for (int i = 0; i < 4; i++) {
    // x + y == 1 is legal (z == 0, e.g. the Display P3 red primary); only
    // a negative z component makes the chromaticity unphysical.
    if (c[i * 2] + c[i * 2 + 1] > 1.f) {
      return false;
    }
  }

  // Row-major primaries matrix (rows X/Y/Z, columns R/G/B, each column with
  // Y == 1) and the white point XYZ.
  float red[3], green[3], blue[3], white[3];
  XYZFromXY(c[2], c[3], red);
  XYZFromXY(c[4], c[5], green);
  XYZFromXY(c[6], c[7], blue);
  XYZFromXY(c[0], c[1], white);

  float primaries[9] = {
      red[0], green[0], blue[0],  //
      red[1], green[1], blue[1],  //
      red[2], green[2], blue[2],
  };

  // Scale each primary column so that RGB (1, 1, 1) maps to the white point:
  // scale = primaries^-1 * white.
  float inv[9];
  if (!Invert3x3(primaries, inv)) {
    return false;
  }
  float scale[3];
  for (int row = 0; row < 3; row++) {
    scale[row] = inv[row * 3] * white[0] + inv[row * 3 + 1] * white[1] +
                 inv[row * 3 + 2] * white[2];
  }
  if (!(scale[0] > 0.f) || !(scale[1] > 0.f) || !(scale[2] > 0.f)) {
    return false;
  }

  // Native gamut: RGB -> XYZ relative to the file's own white point.
  float native[9];
  for (int row = 0; row < 3; row++) {
    for (int col = 0; col < 3; col++) {
      native[row * 3 + col] = primaries[row * 3 + col] * scale[col];
    }
  }

  // skity keeps every gamut in the D50 connection space (see
  // ColorSpace::gamut_xyz), so the native white point is adapted to D50
  // here. Skipping this would combine e.g. a D65-referenced file with the
  // D50-adapted built-in spaces and tint every conversion.
  float adaptation[9];
  if (!BradfordAdaptation(white, kD50XYZ, adaptation)) {
    return false;
  }
  Multiply3x3(adaptation, native, gamut_xyz);
  for (int i = 0; i < 9; i++) {
    if (!std::isfinite(gamut_xyz[i])) {
      return false;
    }
  }
  return true;
}

bool CodecChromaticitiesFromGamut(const float gamut_xyz[9],
                                  float chromaticities[8]) {
  // The white of a gamut matrix is where it maps RGB (1, 1, 1): its row
  // sums, which are D50 by contract.
  const float* m = gamut_xyz;
  float white[3] = {m[0] + m[1] + m[2], m[3] + m[4] + m[5], m[6] + m[7] + m[8]};

  // Undo the D50 adaptation towards D65, so the chunk carries the familiar
  // native primaries. A reader adapting D65 back to D50 lands on the same
  // matrix again, whatever white point the description originally had.
  float d65[3];
  XYZFromXY(kD65X, kD65Y, d65);
  float adaptation[9];
  if (!BradfordAdaptation(white, d65, adaptation)) {
    return false;
  }
  float native[9];
  Multiply3x3(adaptation, gamut_xyz, native);

  // xy of an XYZ triple; rejects anything the reader above would reject, so
  // a written chunk always parses back. The epsilon admits primaries sitting
  // on the x + y == 1 boundary (z == 0, like the Display P3 red), whose
  // un-adapted z is only zero up to float noise; the chunk values themselves
  // quantize to exactly 1.
  auto xy_of = [](float x, float y, float z, float* out) {
    float sum = x + y + z;
    if (!(sum > 0.f) || !std::isfinite(sum)) {
      return false;
    }
    out[0] = x / sum;
    out[1] = y / sum;
    return out[0] > 0.f && out[0] < 1.f && out[1] > 0.f && out[1] < 1.f &&
           out[0] + out[1] <= 1.f + 1e-4f;
  };
  const float* n = native;
  return xy_of(n[0] + n[1] + n[2], n[3] + n[4] + n[5], n[6] + n[7] + n[8],
               chromaticities) &&
         xy_of(n[0], n[3], n[6], chromaticities + 2) &&
         xy_of(n[1], n[4], n[7], chromaticities + 4) &&
         xy_of(n[2], n[5], n[8], chromaticities + 6);
}

}  // namespace codec_priv
}  // namespace skity
