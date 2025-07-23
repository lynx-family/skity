// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "case/basic/example.hpp"

#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <skity/skity.hpp>

namespace skity::example::basic {

// same as https://fiddle.skia.org/c/@shapes
static void draw_basic_example(skity::Canvas* canvas) {
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

// same as https://fiddle.skia.org/c/@discrete_path
static void draw_path_effect_example(skity::Canvas* canvas) {
  const float R = 115.2f, C = 128.f;
  skity::Path path;
  path.MoveTo(C + R, C);
  for (int32_t i = 1; i < 8; i++) {
    float a = 2.6927937f * i;
    path.LineTo(C + R * std::cos(a), C + R * std::sin(a));
  }

  skity::Paint paint;
  paint.SetPathEffect(skity::PathEffect::MakeDiscretePathEffect(10.f, 4.f));
  paint.SetStyle(skity::Paint::kStroke_Style);
  paint.SetStrokeWidth(2.f);
  paint.SetAntiAlias(true);
  paint.SetStrokeColor(0x42 / 255.f, 0x85 / 255.f, 0xF4 / 255.f, 1.f);
  canvas->DrawPath(path, paint);
}

static void draw_dash_start_example(skity::Canvas* canvas) {
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

  skity::Paint paint;
  paint.SetStrokeWidth(8.f);
  paint.SetStrokeJoin(skity::Paint::kRound_Join);
  paint.SetStrokeCap(skity::Paint::kRound_Cap);
  paint.SetStrokeColor(0, 0, 1, 1);
  paint.SetFillColor(150.f / 255.f, 150.f / 255.f, 1.f, 1.f);
  paint.SetAntiAlias(true);
  paint.SetStyle(skity::Paint::kStrokeAndFill_Style);
  float pattern[2] = {0.f, 20.f};
  paint.SetPathEffect(skity::PathEffect::MakeDashPathEffect(pattern, 2, 0));

  canvas->DrawPath(path, paint);
}

// same as https://fiddle.skia.org/c/844ab7d5e63876f6c889b33662ece8d5
void draw_linear_gradient_example(skity::Canvas* canvas) {
  skity::Paint p;
  p.SetStyle(skity::Paint::kFill_Style);

  skity::Vec4 colors[] = {
      skity::Vec4{0.f, 1.f, 1.f, 0.f},
      skity::Vec4{0.f, 0.f, 1.f, 1.f},
      skity::Vec4{1.f, 0.f, 0.f, 1.f},
  };
  float positions[] = {0.f, 0.65f, 1.f};

  for (int i = 0; i < 4; i++) {
    float blockX = (i % 2) * 100.f;
    float blockY = (i / 2) * 100.f;

    std::vector<skity::Point> pts = {
        skity::Point{blockX, blockY, 0.f, 1.f},
        skity::Point{blockX + 50, blockY + 100, 0.f, 1.f},
    };

    skity::Matrix matrix = skity::Matrix(1.f);
    int flag = 0;
    if (i % 2 == 1) {
      flag = 1;
    }
    if (i / 2 == 1) {
      matrix = skity::Matrix::RotateDeg(45.f, skity::Vec2(blockX, blockY));
    }
    auto lgs = skity::Shader::MakeLinear(pts.data(), colors, positions, 3,
                                         skity::TileMode::kClamp, flag);
    lgs->SetLocalMatrix(matrix);
    p.SetShader(lgs);
    auto r = skity::Rect::MakeLTRB(blockX, blockY, blockX + 100, blockY + 100);
    canvas->DrawRect(r, p);
  }

  skity::Path circle;
  circle.AddCircle(220, 350, 100);
  skity::Paint paint;
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetAntiAlias(true);
  skity::Point center{220, 350, 0, 1};
  skity::Vec4 radialColors[] = {skity::Vec4{1.f, 1.f, 1.f, 1.f},
                                skity::Vec4{0.f, 0.f, 0.f, 1.f}};
  auto rgs = skity::Shader::MakeRadial(center, 150.f, radialColors, nullptr, 2);
  paint.SetShader(rgs);

  canvas->DrawPath(circle, paint);
}

// same as https://fiddle.skia.org/c/@text_rendering
void draw_simple_text(skity::Canvas* canvas) {
  skity::Paint paint;

  paint.SetTextSize(64.f);
  paint.SetAntiAlias(true);
  paint.SetFillColor(0x42 / 255.f, 0x85 / 255.f, 0xF4 / 255.f, 1.f);
  paint.SetStyle(skity::Paint::kFill_Style);

  skity::TextBlobBuilder builder;
  // std::shared_ptr<skity::Typeface> typeface =
  //     skity::FontManager::RefDefault()->MatchFamilyStyle(
  //         "sans-serif",
  //         skity::FontStyle(skity::FontStyle::Weight::kBlack_Weight,
  //                          skity::FontStyle::Width::kNormal_Width,
  //                          skity::FontStyle::Slant::kItalic_Slant));
  skity::Typeface* typeface = skity::Typeface::GetDefaultTypeface();
  paint.SetTypeface(typeface);
  skity::Typeface* typeface_cjk =
      skity::FontManager::RefDefault()->MatchFamilyStyleCharacter(
          nullptr, skity::FontStyle(), nullptr, 0, 0X7ECF);

  auto delegate =
      skity::TypefaceDelegate::CreateSimpleFallbackDelegate({typeface_cjk});
  auto blob = typeface_cjk
                  ? builder.BuildTextBlob("Skity 你好", paint, delegate.get())
                  : builder.BuildTextBlob("Skity 你好", paint);

  canvas->DrawTextBlob(blob.get(), 20.f, 64.f, paint);

  paint.SetStyle(skity::Paint::kStroke_Style);
  paint.SetStrokeColor(0xDB / 255.f, 0x44 / 255.f, 0x37 / 255.f, 1.f);
  paint.SetStrokeWidth(2.f);

  canvas->DrawTextBlob(blob.get(), 20.f, 144.f, paint);

  paint.SetFillColor(0x0F / 255.f, 0x9D / 255.f, 0x58 / 255.f, 1.f);
  paint.SetStyle(skity::Paint::kFill_Style);

  canvas->Save();

  skity::Vec4 colors[] = {
      skity::Vec4{0.f, 1.f, 1.f, 1.f},
      skity::Vec4{0.f, 0.f, 1.f, 1.f},
      skity::Vec4{1.f, 0.f, 0.f, 1.f},
  };

  std::vector<skity::Point> pts = {
      skity::Point{0.f, 0.f, 0.f, 1.f},
      skity::Point{200.f, 0.f, 0.f, 1.f},
  };

  auto lgs = skity::Shader::MakeLinear(pts.data(), colors, nullptr, 3);
  paint.SetShader(lgs);

  canvas->DrawTextBlob(blob.get(), 20.f, 224.f, paint);
  canvas->Restore();
}

void draw_even_odd_fill(skity::Canvas* canvas) {
  skity::Paint paint;
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetColor(skity::ColorSetA(skity::Color_RED, 64));

  skity::Path path;
  path.MoveTo(100, 10);
  path.LineTo(40, 180);
  path.LineTo(190, 60);
  path.LineTo(10, 60);
  path.LineTo(160, 180);
  path.Close();

  canvas->DrawPath(path, paint);

  canvas->Save();

  canvas->Translate(0, 200);

  path.SetFillType(skity::Path::PathFillType::kEvenOdd);
  canvas->DrawPath(path, paint);

  canvas->Restore();
}

void draw_canvas(skity::Canvas* canvas) {
  draw_basic_example(canvas);

  canvas->Save();
  canvas->Translate(300, 0);
  draw_path_effect_example(canvas);
  canvas->Restore();

  canvas->Save();
  canvas->Translate(0, 300);
  draw_dash_start_example(canvas);
  canvas->Restore();

  canvas->Save();
  canvas->Translate(520, 0);
  draw_simple_text(canvas);
  canvas->Restore();

  canvas->Save();
  canvas->Translate(400, 300);
  draw_linear_gradient_example(canvas);
  canvas->Restore();

  canvas->Save();
  canvas->Translate(800, 0);
  draw_even_odd_fill(canvas);
  canvas->Restore();
}
}  // namespace skity::example::basic
