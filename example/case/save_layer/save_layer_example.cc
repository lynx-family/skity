// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "case/save_layer/save_layer_example.hpp"

#include <skity/skity.hpp>

#include "case/basic/example.hpp"

namespace skity::example::save_layer {

void draw_save_layer(skity::Canvas* canvas, float time) {
  // skity::Rect outer = skity::Rect::MakeWH(1000, 800);

  float u = (1.f + std::cosf(time * 0.5f)) * 0.5f;

  (void)u;
  skity::Rect bounds = skity::Rect::MakeXYWH(250, 200 + 100 * u, 500, 400);

  skity::Paint paint;
  paint.SetStyle(skity::Paint::Style::kStroke_Style);
  paint.SetStrokeWidth(1.f);
  paint.SetColor(skity::Color_RED);

  canvas->DrawRect(bounds, paint);

  canvas->Save();

  canvas->ClipRect(bounds, skity::Canvas::ClipOp::kDifference);

  skity::example::basic::draw_canvas(canvas);

  canvas->Restore();

  paint.SetMaskFilter(
      skity::MaskFilter::MakeBlur(skity::BlurStyle::kNormal, 20.f));

  canvas->SaveLayer(bounds, paint);

  skity::example::basic::draw_canvas(canvas);

  canvas->Restore();
}

}  // namespace skity::example::save_layer
