// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "case/pathop/pathop_example.hpp"

#include <skity/skity.hpp>

namespace skity::example::pathop {

void draw_pathop_example(skity::Canvas* canvas) {
  skity::Path one;
  one.MoveTo(10, 10);
  one.ConicTo(0, 90, 50, 50, 3);
  one.ConicTo(90, 0, 90, 90, 2);
  one.Close();

  skity::Path two;
  two.AddRect(skity::Rect::MakeXYWH(40, 40, 100, 100));

  skity::Paint stroke_paint;
  stroke_paint.SetStyle(skity::Paint::kStroke_Style);
  stroke_paint.SetStrokeWidth(2.f);

  stroke_paint.SetColor(skity::Color_RED);

  canvas->DrawPath(one, stroke_paint);

  stroke_paint.SetColor(skity::Color_BLUE);

  canvas->DrawPath(two, stroke_paint);

  // diff result
  skity::Path diff_res;
  skity::PathOp::Execute(one, two, skity::PathOp::Op::kDifference, &diff_res);

  // intersect
  skity::Path intersect_res;
  skity::PathOp::Execute(one, two, skity::PathOp::Op::kIntersect,
                         &intersect_res);

  // union
  skity::Path union_res;
  skity::PathOp::Execute(one, two, skity::PathOp::Op::kUnion, &union_res);

  // xor
  skity::Path xor_res;
  skity::PathOp::Execute(one, two, skity::PathOp::Op::kXor, &xor_res);

  skity::Paint fill_paint;

  fill_paint.SetStyle(skity::Paint::kFill_Style);
  fill_paint.SetColor(skity::Color_GREEN);
  fill_paint.SetAlphaF(0.5f);

  skity::Paint outline_paint;
  outline_paint.SetStyle(skity::Paint::kStroke_Style);
  outline_paint.SetStrokeWidth(1.f);
  outline_paint.SetColor(skity::Color_BLACK);

  stroke_paint.SetStrokeWidth(3.f);

  canvas->Save();
  canvas->Translate(0, 200);

  stroke_paint.SetColor(skity::Color_RED);
  canvas->DrawPath(one, stroke_paint);
  stroke_paint.SetColor(skity::Color_BLUE);
  canvas->DrawPath(two, stroke_paint);

  canvas->DrawPath(diff_res, fill_paint);
  canvas->DrawPath(diff_res, outline_paint);

  canvas->Restore();

  canvas->Save();
  canvas->Translate(150, 200);

  stroke_paint.SetColor(skity::Color_RED);
  canvas->DrawPath(one, stroke_paint);
  stroke_paint.SetColor(skity::Color_BLUE);
  canvas->DrawPath(two, stroke_paint);

  canvas->DrawPath(intersect_res, fill_paint);
  canvas->DrawPath(intersect_res, outline_paint);
  canvas->Restore();

  canvas->Save();
  canvas->Translate(300, 200);

  stroke_paint.SetColor(skity::Color_RED);
  canvas->DrawPath(one, stroke_paint);
  stroke_paint.SetColor(skity::Color_BLUE);
  canvas->DrawPath(two, stroke_paint);

  canvas->DrawPath(union_res, fill_paint);
  canvas->DrawPath(union_res, outline_paint);
  canvas->Restore();

  canvas->Save();
  canvas->Translate(500, 200);

  stroke_paint.SetColor(skity::Color_RED);
  canvas->DrawPath(one, stroke_paint);
  stroke_paint.SetColor(skity::Color_BLUE);
  canvas->DrawPath(two, stroke_paint);

  canvas->DrawPath(xor_res, fill_paint);
  canvas->DrawPath(xor_res, outline_paint);
  canvas->Restore();
}
}  // namespace skity::example::pathop
