// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GRAPHIC_BITMAP_SAMPLER_HPP
#define SRC_GRAPHIC_BITMAP_SAMPLER_HPP

#include <skity/graphic/bitmap.hpp>
#include <skity/graphic/sampling_options.hpp>
#include <skity/graphic/tile_mode.hpp>

namespace skity {
class BitmapSampler {
 public:
  BitmapSampler(Bitmap& bitmap, const SamplingOptions& sampling_options,
                TileMode x_tile_mode, TileMode y_tile_mode)
      : bitmap_(bitmap),
        sampling_options_(sampling_options),
        x_tile_mode_(x_tile_mode),
        y_tile_mode_(y_tile_mode) {}

  Color GetColor(Vec2 uv) const;

 private:
  Vec4 SampleUnitNearest(Vec2 uv) const;

  Vec4 SampleUnitLinear(Vec2 uv) const;

  Vec4 SampleXY(Vec2 xy) const;

  Bitmap& bitmap_;
  const SamplingOptions sampling_options_;
  TileMode x_tile_mode_ = TileMode::kClamp;
  TileMode y_tile_mode_ = TileMode::kClamp;
};
}  // namespace skity

#endif  // SRC_GRAPHIC_BITMAP_SAMPLER_HPP
