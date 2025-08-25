// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "test/bench/case/draw_circle.hpp"

#include <random>

namespace skity {

void DrawCircleBenchmark::OnDraw(Canvas *canvas, int index) {
  canvas->Clear(0xFFFFFFFF);
  std::mt19937 rng(42);
  std::uniform_real_distribution<float> x_dist(0.f, 1024.f - radius_);
  std::uniform_real_distribution<float> y_dist(0.0f, 1024.f - radius_);
  std::uniform_int_distribution<uint32_t> color_dist(0x00000000, 0xFFFFFFFF);
  Paint paint;
  if (is_stroke_) {
    paint.SetStyle(Paint::kStroke_Style);
    paint.SetStrokeWidth(stroke_width_);
  }
  for (auto i = 0; i < count_; i++) {
    auto color = color_dist(rng);
    if (is_opaque_) {
      color = color | 0xFF000000;
    }
    paint.SetColor(color);
    paint.SetAntiAlias(true);
    canvas->DrawCircle(x_dist(rng), y_dist(rng), radius_, paint);
  }
}
}  // namespace skity
