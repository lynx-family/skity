// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/codec/codec_priv.hpp"

#include <algorithm>
#include <cstdlib>
#include <vector>

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
    *dst32 = ColorToPMColor(pm_color);

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
    *dst32 = PMColorToColor(pm_color);

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
  // src_height x dst_width.
  std::vector<uint8_t> premul_row(src_width * 4);
  std::vector<uint8_t> mid(static_cast<size_t>(src_height) * dst_width * 4);

  for (int32_t y = 0; y < src_height; y++) {
    const uint8_t* row = src->Addr8(0, y);
    for (int32_t x = 0; x < src_width; x++) {
      uint8_t a = row[x * 4 + 3];
      premul_row[x * 4 + 0] = PremulChannel(row[x * 4 + 0], a);
      premul_row[x * 4 + 1] = PremulChannel(row[x * 4 + 1], a);
      premul_row[x * 4 + 2] = PremulChannel(row[x * 4 + 2], a);
      premul_row[x * 4 + 3] = a;
    }

    uint8_t* mid_row = mid.data() + static_cast<size_t>(y) * dst_width * 4;
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
  auto dst = std::make_shared<Pixmap>(
      static_cast<uint32_t>(dst_width), static_cast<uint32_t>(dst_height),
      AlphaType::kUnpremul_AlphaType, ColorType::kRGBA);

  std::vector<int64_t> acc(static_cast<size_t>(dst_width) * 4);
  for (int32_t y = 0; y < dst_height; y++) {
    std::fill(acc.begin(), acc.end(), 0);

    int64_t weight = 0;
    for (int32_t c = y_offsets[y]; c < y_offsets[y + 1]; c++) {
      const uint8_t* mid_row =
          mid.data() + static_cast<size_t>(y_contrib[c].index) * dst_width * 4;
      int32_t row_weight = y_contrib[c].weight;
      weight += row_weight;
      for (int32_t x = 0; x < dst_width * 4; x++) {
        acc[x] += static_cast<int64_t>(mid_row[x]) * row_weight;
      }
    }

    uint8_t* out_row = dst->WritableAddr8(0, y);
    for (int32_t x = 0; x < dst_width; x++) {
      uint8_t a = DivRound(acc[x * 4 + 3], weight);
      out_row[x * 4 + 0] = UnpremulChannel(DivRound(acc[x * 4 + 0], weight), a);
      out_row[x * 4 + 1] = UnpremulChannel(DivRound(acc[x * 4 + 1], weight), a);
      out_row[x * 4 + 2] = UnpremulChannel(DivRound(acc[x * 4 + 2], weight), a);
      out_row[x * 4 + 3] = a;
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

}  // namespace codec_priv
}  // namespace skity
