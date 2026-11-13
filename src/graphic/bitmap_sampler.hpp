// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GRAPHIC_BITMAP_SAMPLER_HPP
#define SRC_GRAPHIC_BITMAP_SAMPLER_HPP

#include <skity/graphic/bitmap.hpp>
#include <skity/graphic/color_space.hpp>
#include <skity/graphic/sampling_options.hpp>
#include <skity/graphic/tile_mode.hpp>

#include "src/graphic/color_space_priv.hpp"

namespace skity {
class BitmapSampler {
 public:
  // `output_space` is the space the caller wants GetColor() to return
  // colors in: null (the default) means the working space (sRGB), and a
  // caller resampling into a pixmap tagged with the source's own space
  // passes that space to skip the conversion entirely.
  BitmapSampler(Bitmap& bitmap, const SamplingOptions& sampling_options,
                TileMode x_tile_mode, TileMode y_tile_mode,
                std::shared_ptr<const ColorSpace> output_space = nullptr);

  Color GetColor(Vec2 uv) const;

 private:
  Vec4 SampleUnitNearest(Vec2 uv) const;

  Vec4 SampleUnitLinear(Vec2 uv) const;

  Vec4 SampleXY(Vec2 xy) const;

  Bitmap& bitmap_;
  const SamplingOptions sampling_options_;
  TileMode x_tile_mode_ = TileMode::kClamp;
  TileMode y_tile_mode_ = TileMode::kClamp;
  std::shared_ptr<const ColorSpace> color_space_;
  AlphaType alpha_type_ = kUnpremul_AlphaType;
  std::shared_ptr<const ColorSpace> output_space_;
  // Conversion invariants prepared once per sampler (matrix inversion,
  // curve flips); per-pixel work only applies them.
  bool needs_conversion_ = false;
  PreparedConversion prepared_{};
};
}  // namespace skity

#endif  // SRC_GRAPHIC_BITMAP_SAMPLER_HPP
