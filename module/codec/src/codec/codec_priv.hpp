// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CODEC_SRC_CODEC_CODEC_PRIV_HPP
#define MODULE_CODEC_SRC_CODEC_CODEC_PRIV_HPP

#include <cstring>
#include <functional>
#include <skity/graphic/alpha_type.hpp>
#include <skity/graphic/color.hpp>
#include <skity/graphic/color_type.hpp>

namespace skity {
namespace codec_priv {

static void CodecTransformLineByPass(uint8_t* dst, uint8_t* src, int width,
                                     int bytes_per_pixel) {
  for (int x = 0; x < width; x++) {
    memcpy(dst, src, bytes_per_pixel);
    dst += bytes_per_pixel;
    src += bytes_per_pixel;
  }
}

void CodecTransformLinePremul(uint8_t* dst, uint8_t* src, int width,
                              int bytes_per_pixel);

void CodecTransformLineUnpremul(uint8_t* dst, uint8_t* src, int width,
                                int bytes_per_pixel);

void CodecTransformLineSwizzelRB(uint8_t* dst, uint8_t* src, int width,
                                 int bytes_per_pixel);

using TransformLineFunc = std::function<void(uint8_t* dst, uint8_t* src,
                                             int width, int bytes_per_pixel)>;

TransformLineFunc ChooseLineTransformFunc(ColorType color_type,
                                          AlphaType alpha_type);

}  // namespace codec_priv
}  // namespace skity

#endif  // MODULE_CODEC_SRC_CODEC_CODEC_PRIV_HPP
