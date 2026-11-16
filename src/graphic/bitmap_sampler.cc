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

const ColorSpace& DerefOrSRGB(
    const std::shared_ptr<const ColorSpace>& color_space) {
  static const ColorSpace kSRGBInstance{};
  return color_space ? *color_space : kSRGBInstance;
}
}  // namespace

BitmapSampler::BitmapSampler(Bitmap& bitmap,
                             const SamplingOptions& sampling_options,
                             TileMode x_tile_mode, TileMode y_tile_mode,
                             std::shared_ptr<const ColorSpace> output_space)
    : bitmap_(bitmap),
      sampling_options_(sampling_options),
      x_tile_mode_(x_tile_mode),
      y_tile_mode_(y_tile_mode),
      color_space_(bitmap.GetPixmap()->GetColorSpace()),
      alpha_type_(bitmap.GetPixmap()->GetAlphaType()),
      output_space_(std::move(output_space)) {
  // Prepare the conversion invariants once: the source and output spaces
  // are fixed for the sampler's lifetime, so per-pixel work must not redo
  // the matrix inversion or the curve flips.
  if (!ColorSpaceEquals(color_space_, output_space_)) {
    needs_conversion_ = true;
    prepared_ = PrepareConversion(DerefOrSRGB(color_space_),
                                  DerefOrSRGB(output_space_));
  }
}

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
  Vec4 sampled{};
  // The SW backend does not implement cubic resampling; fall back to bilinear.
  if (sampling_options_.UseCubic()) {
    sampled = SampleUnitLinear(uv);
  } else {
    switch (sampling_options_.filter) {
      case FilterMode::kNearest:
        sampled = SampleUnitNearest(uv);
        break;
      case FilterMode::kLinear:
        sampled = SampleUnitLinear(uv);
        break;
    }
  }
  // Decode the source-space color to the output space after filtering, the
  // same semantics as the GPU texture decode (filter in the stored space).
  if (needs_conversion_) {
    if (alpha_type_ == kPremul_AlphaType) {
      // Transfer functions are defined on unpremultiplied values; feeding
      // premultiplied ones scales the curve by alpha and can push channels
      // above their alpha. Convert in the unpremultiplied domain and scale
      // back; a zero-alpha sample keeps its exact zeros.
      if (sampled.w > 0.f) {
        float rgb[3] = {sampled.x / sampled.w, sampled.y / sampled.w,
                        sampled.z / sampled.w};
        ApplyConversion(prepared_, rgb);
        // Out-of-gamut conversions exceed [0,1] (ColorSpace keeps the
        // extended range by contract), but the software blend path needs
        // canonical premultiplied channels (RGB <= A). Clamp in the
        // unpremultiplied domain before scaling back — the same clamp the
        // GPU decode applies before its re-premultiply.
        rgb[0] = std::clamp(rgb[0], 0.f, 1.f);
        rgb[1] = std::clamp(rgb[1], 0.f, 1.f);
        rgb[2] = std::clamp(rgb[2], 0.f, 1.f);
        sampled = Vec4{rgb[0] * sampled.w, rgb[1] * sampled.w,
                       rgb[2] * sampled.w, sampled.w};
      }
    } else {
      // Unpremultiplied output stays extended-range: the quantization
      // clamp at the end keeps every channel in [0,255], which is enough
      // for the downstream premultiply to preserve RGB <= A.
      float rgb[3] = {sampled.x, sampled.y, sampled.z};
      ApplyConversion(prepared_, rgb);
      sampled = Vec4{rgb[0], rgb[1], rgb[2], sampled.w};
    }
  }
  return Color4fToColor(sampled);
}

}  // namespace skity
