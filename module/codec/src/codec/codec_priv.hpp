// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CODEC_SRC_CODEC_CODEC_PRIV_HPP
#define MODULE_CODEC_SRC_CODEC_CODEC_PRIV_HPP

#include <algorithm>
#include <cstring>
#include <functional>
#include <memory>
#include <skity/codec/codec.hpp>
#include <skity/graphic/alpha_type.hpp>
#include <skity/graphic/color.hpp>
#include <skity/graphic/color_type.hpp>
#include <skity/io/pixmap.hpp>

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

/**
 * Resolves the aspect-fit target size for a source image.
 *
 * Returns false when no downscaling is needed: no target requested, invalid
 * source size, or the source already fits within the requested box (the codec
 * never upscales). Otherwise fills out_width / out_height with the largest
 * size that preserves the source aspect ratio and fits within
 * target_width x target_height.
 *
 * Header-inline: the darwin framework build compiles only the ImageIO codec
 * (codec_apple.mm) without codec_priv.cc, and needs this size negotiation.
 */
inline bool ResolveTargetSize(int32_t src_width, int32_t src_height,
                              const DecodeOptions& options, int32_t* out_width,
                              int32_t* out_height) {
  if (out_width == nullptr || out_height == nullptr) {
    return false;
  }
  *out_width = 0;
  *out_height = 0;

  if (src_width <= 0 || src_height <= 0) {
    return false;
  }

  int32_t target_width = std::max(options.target_width, 0);
  int32_t target_height = std::max(options.target_height, 0);

  if (target_width == 0 && target_height == 0) {
    return false;
  }

  // Derive the unspecified dimension, preserving aspect ratio.
  if (target_width == 0) {
    target_width = std::max(
        1,
        static_cast<int32_t>(
            static_cast<double>(src_width) * target_height / src_height + 0.5));
  }
  if (target_height == 0) {
    target_height = std::max(
        1,
        static_cast<int32_t>(
            static_cast<double>(src_height) * target_width / src_width + 0.5));
  }

  // Never upscale.
  if (target_width >= src_width && target_height >= src_height) {
    return false;
  }

  double scale = std::min(static_cast<double>(target_width) / src_width,
                          static_cast<double>(target_height) / src_height);
  scale = std::min(scale, 1.0);

  *out_width = std::max(
      1, static_cast<int32_t>(static_cast<double>(src_width) * scale + 0.5));
  *out_height = std::max(
      1, static_cast<int32_t>(static_cast<double>(src_height) * scale + 0.5));

  return true;
}

/**
 * Area-average (box) resample of an unpremul RGBA8888 pixmap to the given
 * size, separable horizontal + vertical passes with fixed-point weights.
 *
 * Filtering happens in premultiplied space to avoid color bleed on
 * semi-transparent pixels; the output is converted back to unpremul.
 *
 * Returns nullptr on invalid input. Upscaling is allowed (callers use it to
 * close the small remainder gap left by native decoder scaling); pass sizes
 * from ResolveTargetSize().
 */
std::shared_ptr<Pixmap> ResamplePixmap(const std::shared_ptr<Pixmap>& src,
                                       int32_t dst_width, int32_t dst_height);

/**
 * Resamples src to an explicit size. Returns src unchanged when
 * dst_width/dst_height are non-positive (no resample requested) or when
 * resampling fails. A codec whose native scaled decode misses the requested
 * size uses this to close the remainder.
 */
std::shared_ptr<Pixmap> ResamplePixmapToSize(const std::shared_ptr<Pixmap>& src,
                                             int32_t dst_width,
                                             int32_t dst_height);

/**
 * Decoded-pixmap helper for codecs without native scaled decoding: resamples
 * to the option's target size when one is requested. Returns src unchanged
 * when no scaling is needed or resampling fails.
 */
std::shared_ptr<Pixmap> ResamplePixmapToTarget(
    const std::shared_ptr<Pixmap>& src, const DecodeOptions& options);

}  // namespace codec_priv
}  // namespace skity

#endif  // MODULE_CODEC_SRC_CODEC_CODEC_PRIV_HPP
