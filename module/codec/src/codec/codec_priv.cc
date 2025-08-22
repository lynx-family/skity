// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/codec/codec_priv.hpp"

namespace skity {
namespace codec_priv {

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

}  // namespace codec_priv
}  // namespace skity
