// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/graphic/bitmap_sampler.hpp"

#include "src/geometry/math.hpp"

namespace skity {

namespace {
float RemapFloatTile(float t, TileMode tile_mode) {
  if (tile_mode == TileMode::kClamp) {
    t = std::clamp(t, 0.0f, 1.0f);
  } else if (tile_mode == TileMode::kRepeat) {
    t = FloatFract(t);
  } else if (tile_mode == TileMode::kMirror) {
    float t1 = t - 1;
    float t2 = t1 - 2 * std::floor(t1 * 0.5) - 1;
    t = std::abs(t2);
  }
  return t;
}
}  // namespace

Vec4 BitmapSampler::SampleXY(Vec2 xy) const {
  uint32_t w = bitmap_.Width();
  uint32_t h = bitmap_.Height();

  xy.x = glm::clamp<uint32_t>(xy.x, 0, w - 1);
  xy.y = glm::clamp<uint32_t>(xy.y, 0, h - 1);

  return Color4fFromColor(bitmap_.GetPixel(xy.x, xy.y));
}

Vec4 BitmapSampler::SampleUnitNearest(Vec2 uv) const {
  uint32_t w = bitmap_.Width();
  uint32_t h = bitmap_.Height();
  return SampleXY({uv.x * w, uv.y * h});
}

Vec4 BitmapSampler::SampleUnitLinear(Vec2 uv) const {
  uint32_t w = bitmap_.Width();
  uint32_t h = bitmap_.Height();

  float x = uv.x * w;
  float y = uv.y * h;

  float i0 = glm::floor(x - 0.5f);
  float j0 = glm::floor(y - 0.5f);

  if (x_tile_mode_ == TileMode::kRepeat) {
    i0 = glm::mod(i0, static_cast<float>(w));
  }

  if (y_tile_mode_ == TileMode::kRepeat) {
    j0 = glm::mod(j0, static_cast<float>(h));
  }

  float i1 = i0 + 1.0f;
  float j1 = j0 + 1.0f;

  if (x_tile_mode_ == TileMode::kRepeat) {
    i1 = glm::mod(i1, static_cast<float>(w));
  }

  if (y_tile_mode_ == TileMode::kRepeat) {
    j1 = glm::mod(j1, static_cast<float>(h));
  }

  float a = glm::fract(x - 0.5f);
  float b = glm::fract(y - 0.5f);

  Vec4 ti0j0 = SampleXY({i0, j0});
  Vec4 ti1j0 = SampleXY({i1, j0});
  Vec4 ti0j1 = SampleXY({i0, j1});
  Vec4 ti1j1 = SampleXY({i1, j1});

  return ((1 - a) * (1 - b) * ti0j0) +  //
         (a * (1 - b) * ti1j0) +        //
         ((1 - a) * b * ti0j1) +        //
         (a * b * ti1j1);
}

Color BitmapSampler::GetColor(Vec2 uv) const {
  if ((x_tile_mode_ == TileMode::kDecal && (uv.x < 0.0 || uv.x >= 1.0)) ||
      (y_tile_mode_ == TileMode::kDecal && (uv.y < 0.0 || uv.y >= 1.0))) {
    return Color_TRANSPARENT;
  }

  uv = Vec2{RemapFloatTile(uv.x, x_tile_mode_),
            RemapFloatTile(uv.y, y_tile_mode_)};
  Color color;
  switch (sampling_options_.filter) {
    case FilterMode::kNearest:
      color = Color4fToColor(SampleUnitNearest(uv));
      break;
    case FilterMode::kLinear:
      color = Color4fToColor(SampleUnitLinear(uv));
      break;
  }
  return color;
}

}  // namespace skity
