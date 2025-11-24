// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/effect/shader.hpp>

#include "src/effect/gradient_shader.hpp"
#include "src/effect/pixmap_shader.hpp"
#include "src/geometry/math.hpp"

namespace skity {

Shader::GradientType Shader::AsGradient(GradientInfo*) const {
  return GradientType::kNone;
}

const std::shared_ptr<Image>* Shader::AsImage() const { return nullptr; }

const SamplingOptions* Shader::GetSamplingOptions() const { return nullptr; }

std::shared_ptr<Shader> Shader::MakeLinear(const Point pts[2],
                                           const Vec4 colors[],
                                           const float pos[], int count,
                                           TileMode tile_mode, int flag) {
  // count must be >= 2
  if (count < 2) {
    return nullptr;
  }

  return std::make_shared<LinearGradientShader>(pts, colors, pos, count,
                                                tile_mode, flag);
}

std::shared_ptr<Shader> Shader::MakeRadial(Point const& center, float radius,
                                           const Vec4 colors[],
                                           const float pos[], int count,
                                           TileMode tile_mode, int flag) {
  if (count < 2) {
    return nullptr;
  }

  if (radius < 0.f) {
    return nullptr;
  }

  return std::make_shared<RadialGradientShader>(center, radius, colors, pos,
                                                count, tile_mode, flag);
}

std::shared_ptr<Shader> Shader::MakeTwoPointConical(
    Point const& start, float start_radius, Point const& end, float end_radius,
    const Vec4 colors[], const float pos[], int count, TileMode tile_mode,
    int flag) {
  if (count < 2) {
    return nullptr;
  }

  bool same_radius = FloatNearlyZero(start_radius - end_radius);
  if (FloatNearlyZero(start.x - end.x) && (FloatNearlyZero(start.y - end.y))) {
    if (same_radius) {
      return nullptr;
    }
    bool start_radius_zero = FloatNearlyZero(start_radius);
    bool end_radius_zero = FloatNearlyZero(end_radius);
    if (start_radius_zero || end_radius_zero) {
      if (end_radius_zero) {
        std::vector<float> pos_reverse(pos, pos + count);
        std::reverse(std::begin(pos_reverse), std::end(pos_reverse));
        for_each(pos_reverse.begin(), pos_reverse.end(),
                 [](float& e) { e = 1.f - e; });
        std::vector<Vec4> colors_reverse(colors, colors + count);
        std::reverse(std::begin(colors_reverse), std::end(colors_reverse));
        return std::make_shared<RadialGradientShader>(
            start, start_radius, colors_reverse.data(), pos_reverse.data(),
            count, tile_mode, flag);
      } else {
        return std::make_shared<RadialGradientShader>(
            start, end_radius, colors, pos, count, tile_mode, flag);
      }
    }
  }

  return std::make_shared<TwoPointConicalGradientShader>(
      start, start_radius, end, end_radius, colors, pos, count, tile_mode,
      flag);
}

std::shared_ptr<Shader> Shader::MakeSweep(float cx, float cy, float start_angle,
                                          float end_angle, const Vec4 colors[],
                                          const float pos[], int count,
                                          TileMode tile_mode, int flag) {
  if (count < 2) {
    return nullptr;
  }
  if (start_angle >= end_angle) {
    return nullptr;
  }

  float t0 = start_angle / 360;
  float t1 = end_angle / 360;
  float bias = -t0;
  float scale = 1 / (t1 - t0);
  return std::make_shared<SweepGradientShader>(cx, cy, bias, scale, colors, pos,
                                               count, tile_mode, flag);
}

std::shared_ptr<Shader> Shader::MakeShader(std::shared_ptr<Image> image,
                                           const SamplingOptions& sampling,
                                           TileMode x_tile_mode,
                                           TileMode y_tile_mode,
                                           const Matrix& local_matrix) {
  if (!image) {
    return nullptr;
  }

  return std::make_shared<PixmapShader>(std::move(image), sampling, x_tile_mode,
                                        y_tile_mode, local_matrix);
}

}  // namespace skity
