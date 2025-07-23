// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "case/shape/shape_example.hpp"

namespace skity::example::shape {

void draw_shapes(skity::Canvas* canvas) {
  Paint paint;
  paint.SetColor(Color_WHITE);
  canvas->DrawPaint(paint);

  paint.SetColor(Color_RED);
  paint.SetStrokeWidth(20);
  paint.SetStyle(Paint::kStroke_Style);

  canvas->Save();
  canvas->Translate(50, 50);
  paint.SetStrokeMiter(1.415);
  skity::Path path;
  path.MoveTo(0, 0);
  path.LineTo(100, 0);
  path.LineTo(100, 100);
  path.LineTo(0, 100);
  path.Close();
  canvas->DrawPath(path, paint);

  canvas->Translate(0, 200);
  paint.SetStrokeMiter(1.414);
  canvas->DrawPath(path, paint);
  canvas->Restore();

  canvas->Save();
  paint.SetColor(Color_BLUE);
  canvas->Translate(250, 50);
  paint.SetStrokeWidth(50);
  paint.SetStrokeMiter(4);
  canvas->DrawRect(Rect::MakeWH(50, 0), paint);

  canvas->Translate(0, 100);
  paint.SetStrokeJoin(Paint::Join::kRound_Join);
  canvas->DrawRect(Rect::MakeWH(50, 0), paint);

  canvas->Translate(0, 100);
  canvas->DrawRect(Rect::MakeWH(0, 0), paint);
  canvas->Restore();

  skity::Path curve;
  curve.MoveTo(10, 10);
  curve.QuadTo(256, 64, 128, 128);
  curve.QuadTo(10, 192, 250, 250);

  skity::Path polyline;
  polyline.MoveTo(10, 10);
  polyline.LineTo(200, 140);
  polyline.LineTo(50, 140);
  polyline.LineTo(10, 10);

  canvas->Save();
  canvas->Translate(400, 50);

  paint.SetStrokeWidth(20.f);
  paint.SetStrokeCap(Paint::kSquare_Cap);

  canvas->DrawPath(curve, paint);

  canvas->Translate(180, 0);

  paint.SetStrokeJoin(Paint::kRound_Join);
  paint.SetStrokeCap(Paint::kSquare_Cap);

  canvas->DrawPath(polyline, paint);

  canvas->Translate(-250, 250);

  paint.SetStrokeWidth(10.f);
  float pattern[2] = {0.f, 20.f};
  paint.SetPathEffect(skity::PathEffect::MakeDashPathEffect(pattern, 2, 0));

  canvas->DrawPath(polyline, paint);

  canvas->Translate(250, 0);
  paint.SetStrokeCap(Paint::kRound_Cap);
  canvas->DrawPath(polyline, paint);
  canvas->Restore();
  canvas->Save();
  canvas->Translate(0, 400);
  paint.Reset();
  paint.SetColor(0x8000ff00);
  path.Reset();
  path.MoveTo(0, 0);
  path.LineTo(100, 0);
  path.LineTo(100, 50);
  path.LineTo(0, 50);
  path.Close();
  path.MoveTo(10, 10);
  path.LineTo(90, 10);
  path.LineTo(90, 40);
  path.LineTo(10, 40);
  path.Close();
  canvas->DrawPath(path, paint);
  canvas->Restore();
}

}  // namespace skity::example::shape
