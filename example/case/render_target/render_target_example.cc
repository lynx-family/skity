// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "case/render_target/render_target_example.hpp"

namespace skity::example::render_target {

void draw_to_render_target(skity::GPURenderTarget* target) {
  auto canvas = target->GetCanvas();

  skity::Paint paint;
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetAntiAlias(true);
  paint.SetStrokeWidth(4.f);
  paint.SetFillColor(0x42 / 255.f, 0x85 / 255.f, 0xF4 / 255.f, 1.f);

  skity::Rect rect = skity::Rect::MakeXYWH(10, 10, 100, 160);
  canvas->DrawRect(rect, paint);

  skity::RRect oval;
  oval.SetOval(rect);
  oval.Offset(40, 80);
  paint.SetFillColor(0xDB / 255.f, 0x44 / 255.f, 0x37 / 255.f, 1.f);
  canvas->DrawRRect(oval, paint);

  paint.SetFillColor(0x0F / 255.f, 0x9D / 255.f, 0x58 / 255.f, 1.f);
  canvas->DrawCircle(180, 50, 25, paint);

  rect.Offset(80, 50);
  paint.SetStrokeColor(0xF4 / 255.f, 0xB4 / 255.f, 0x0, 1.f);
  paint.SetStyle(skity::Paint::kStroke_Style);
  canvas->DrawRoundRect(rect, 10, 10, paint);
}

void draw_render_target(skity::Canvas* canvas, skity::GPUContext* context) {
  uint32_t width = 500;
  uint32_t height = 500;

  skity::GPURenderTargetDescriptor desc;
  desc.width = width;
  desc.height = height;
  desc.sample_count = 4;

  auto target = context->CreateRenderTarget(desc);

  draw_to_render_target(target.get());

  auto image = context->MakeSnapshot(std::move(target));

  canvas->DrawImage(image, skity::Rect::MakeXYWH(10, 10, 500, 500));

  skity::Paint paint;
  paint.SetStyle(skity::Paint::kStroke_Style);
  paint.SetStrokeWidth(3.f);
  paint.SetColor(skity::Color_RED);

  canvas->DrawRect(skity::Rect::MakeXYWH(10, 10, 500, 500), paint);
}

}  // namespace skity::example::render_target
