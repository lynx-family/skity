// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/skity.hpp>

namespace skity::example::clip {

float degree = 0.f;

void draw_clip_demo(skity::Canvas* canvas) {
  degree += 0.2f;
  skity::Paint clip_paint;
  clip_paint.SetStyle(skity::Paint::kStroke_Style);
  clip_paint.SetStrokeWidth(2.f);

  skity::Paint stroke_paint;
  stroke_paint.SetStyle(skity::Paint::kStroke_Style);
  stroke_paint.SetStrokeWidth(5.f);
  stroke_paint.SetColor(skity::Color_RED);

  skity::Rect rect1 = skity::Rect::MakeXYWH(100, 100, 200, 200);

  skity::Rect rect2 = skity::Rect::MakeXYWH(150, 150, 100, 100);

  clip_paint.SetColor(skity::Color_BLACK);

  canvas->DrawRect(rect1, clip_paint);

  canvas->Save();

  canvas->ClipRect(rect1);

  int save_count = canvas->Save();

  canvas->DrawLine(100, 170, 400, 200, stroke_paint);

  canvas->Rotate(degree, 170.f, 170.f);

  clip_paint.SetColor(skity::Color_BLUE);

  canvas->DrawRect(rect2, clip_paint);

  canvas->ClipRect(rect2);

  canvas->Rotate(-degree, 170.f, 170.f);

  canvas->DrawLine(100, 180, 400, 230, stroke_paint);

  canvas->RestoreToCount(save_count);

  canvas->DrawLine(80, 250, 400, 330, stroke_paint);

  canvas->Restore();
}

void draw_clip_difference(skity::Canvas* canvas) {
  // single clip

  skity::Paint paint;
  paint.SetColor(skity::Color_RED);
  paint.SetStyle(skity::Paint::Style::kStroke_Style);
  paint.SetStrokeWidth(1.f);

  skity::Rect clip_inner_rect = skity::Rect::MakeXYWH(200, 200, 50, 50);
  skity::Rect clip_outer_rect = skity::Rect::MakeXYWH(100, 100, 200, 200);

  canvas->DrawRect(clip_inner_rect, paint);
  canvas->DrawRect(clip_outer_rect, paint);

  canvas->Save();

  canvas->ClipRect(clip_outer_rect);
  canvas->ClipRect(clip_inner_rect, skity::Canvas::ClipOp::kDifference);

  canvas->Rotate(degree, 200, 200);

  skity::Path path;
  path.MoveTo(199, 34);
  path.LineTo(253, 143);
  path.LineTo(374, 160);
  path.LineTo(287, 244);
  path.LineTo(307, 365);
  path.LineTo(199, 309);
  path.LineTo(97, 365);
  path.LineTo(112, 245);
  path.LineTo(26, 161);
  path.LineTo(146, 143);
  path.Close();

  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetColor(skity::Color_BLUE);

  canvas->DrawPath(path, paint);

  canvas->Restore();
}
}  // namespace skity::example::clip
