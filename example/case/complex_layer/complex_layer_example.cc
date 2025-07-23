// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/skity.hpp>

namespace skity::example::complex_layer {

void draw_complex_layer_internal(skity::Canvas* canvas) {
  canvas->Save();
  canvas->ClipRect(skity::Rect::MakeXYWH(30, 30, 300, 400));

  canvas->Scale(0.2, 0.2);

  auto bounds = skity::Rect::MakeXYWH(50 * 5, 50 * 5, 600 * 5, 600 * 5);
  skity::Paint bounds_paint;
  bounds_paint.SetStyle(skity::Paint::kStroke_Style);
  bounds_paint.SetStrokeWidth(3.f);
  bounds_paint.SetColor(skity::Color_BLACK);

  auto bd2 = skity::Rect::MakeXYWH(50 * 5, 50 * 5, 200 * 5, 300 * 5);

  canvas->DrawRect(bounds, bounds_paint);

  bounds_paint.SetStrokeWidth(5.f);
  bounds_paint.SetColor(skity::Color_BLUE);

  canvas->SaveLayer(bounds, bounds_paint);

  auto rect = skity::Rect::MakeXYWH(100 * 5, 100 * 5, 300 * 5, 300 * 5);
  skity::Paint paint;
  paint.SetColor(skity::Color_RED);

  canvas->DrawRect(rect, paint);

  canvas->DrawRect(bd2, bounds_paint);

  auto rect2 = skity::Rect::MakeXYWH(200 * 5, 150 * 5, 300 * 5, 300 * 5);

  paint.SetColor(skity::Color_GREEN);
  paint.SetBlendMode(skity::BlendMode::kDstIn);

  canvas->ClipRect(bd2);

  canvas->SaveLayer(bounds, paint);

  paint.SetBlendMode(skity::BlendMode::kDefault);
  paint.SetAlphaF(0.5f);
  canvas->DrawRect(rect2, paint);

  canvas->Restore();

  canvas->Restore();

  canvas->Restore();
}

void draw_complex_layer(skity::Canvas* canvas) {
  draw_complex_layer_internal(canvas);
  canvas->Save();
  canvas->Translate(700, 0);
  canvas->Rotate(45.f);
  draw_complex_layer_internal(canvas);
  canvas->Restore();
}

}  // namespace skity::example::complex_layer
