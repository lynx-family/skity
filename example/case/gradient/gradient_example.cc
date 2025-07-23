// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "case/gradient/gradient_example.hpp"

#include <skity/skity.hpp>

namespace skity::example::gradient {

static constexpr float g_case_size = 128;

static void draw_gradient_with_tile_mode(skity::Canvas* canvas) {
  skity::Paint paint;
  std::vector<skity::Vec3> offsets = {
      {0, 0, 0}, {0, 150, 0}, {150, 0, 0}, {150, 150, 0}};
  std::vector<skity::TileMode> tile_modes = {
      skity::TileMode::kClamp, skity::TileMode::kRepeat,
      skity::TileMode::kMirror, skity::TileMode::kDecal};
  for (int i = 0; i < 4; i++) {
    canvas->Save();
    canvas->Translate(offsets[i].x, offsets[i].y);
    skity::Vec4 gradient_colors[] = {
        skity::Vec4{0.9019f, 0.3921f, 0.3960f, 1.0f},
        skity::Vec4{0.5686f, 0.5960f, 0.8980f, 1.0f}};
    float gradient_positions[] = {0.f, 1.f};
    std::vector<skity::Point> gradient_points = {
        skity::Point{0.f, 0.f, 0.f, 1.f},
        skity::Point{50.f, 50.f, 0.f, 1.f},
    };
    auto lgs =
        skity::Shader::MakeLinear(gradient_points.data(), gradient_colors,
                                  gradient_positions, 2, tile_modes[i]);
    paint.SetShader(lgs);
    canvas->DrawRect({0, 0, 100, 100}, paint);
    canvas->Restore();
  }

  canvas->Save();
  canvas->Translate(300, 0);
  paint.Reset();
  for (int i = 0; i < 4; i++) {
    canvas->Save();
    canvas->Translate(offsets[i].x, offsets[i].y);
    skity::Vec4 gradient_colors[] = {
        skity::Vec4{0.9019f, 0.3921f, 0.3960f, 1.0f},
        skity::Vec4{0.5686f, 0.5960f, 0.8980f, 1.0f}};
    float gradient_positions[] = {0.f, 1.f};
    auto rgs =
        skity::Shader::MakeRadial({25.f, 25.f, 0.f, 1.f}, 25, gradient_colors,
                                  gradient_positions, 2, tile_modes[i]);
    paint.SetShader(rgs);
    canvas->DrawRect({0, 0, 100, 100}, paint);
    canvas->Restore();
  }
  canvas->Restore();

  canvas->Save();
  canvas->Translate(600, 0);
  paint.Reset();
  for (int i = 0; i < 4; i++) {
    canvas->Save();
    canvas->Translate(offsets[i].x, offsets[i].y);
    skity::Vec4 gradient_colors[] = {
        skity::Vec4{0.9019f, 0.3921f, 0.3960f, 1.0f},
        skity::Vec4{0.5686f, 0.5960f, 0.8980f, 1.0f}};
    float gradient_positions[] = {0.f, 1.f};
    auto rgs = skity::Shader::MakeSweep(25.f, 25.f, 45, 135, gradient_colors,
                                        gradient_positions, 2, tile_modes[i]);
    paint.SetShader(rgs);
    canvas->DrawRect({0, 0, 100, 100}, paint);
    canvas->Restore();
  }
  canvas->Restore();
}

static void draw_radial_gradient(skity::Canvas* canvas, float x0, float y0,
                                 float r0, float sz) {
  skity::Vec4 colors[] = {skity::Color4fFromColor(skity::Color_RED),
                          skity::Color4fFromColor(skity::Color_GREEN),
                          skity::Color4fFromColor(skity::Color_BLUE)};
  float positions[] = {0.f, 0.5, 1.f};
  std::vector<skity::Point> points = {
      skity::Point{0.f, 0.f, 0.f, 1.f},
      skity::Point{50.f, 50.f, 0.f, 1.f},
  };

  auto gs = skity::Shader::MakeRadial({x0, y0, 0, 1}, r0, colors, positions, 3,
                                      skity::TileMode::kClamp);

  skity::Paint paint;
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetShader(gs);

  auto r = skity::Rect::MakeLTRB(0, 0, sz, sz);
  canvas->DrawRect(r, paint);
}

static void draw_radial_gradient(skity::Canvas* canvas, float x0, float y0,
                                 float r0, float x1, float y1, float r1,
                                 float sz) {
  skity::Vec4 colors[] = {skity::Color4fFromColor(skity::Color_RED),
                          skity::Color4fFromColor(skity::Color_YELLOW),
                          skity::Color4fFromColor(skity::Color_GREEN),
                          skity::Color4fFromColor(skity::Color_BLUE)};
  float positions[] = {0.f, 0.33, 0.66, 1.f};

  auto gs = skity::Shader::MakeTwoPointConical(
      {x0, y0, 0, 1}, r0, {x1, y1, 0, 1}, r1, colors, positions,
      sizeof(colors) / sizeof(colors[0]), skity::TileMode::kClamp);

  skity::Paint paint;
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetShader(gs);

  auto r = skity::Rect::MakeLTRB(0, 0, sz, sz);
  canvas->DrawRect(r, paint);
}

static void draw_gradient_basic(skity::Canvas* canvas) {
  canvas->Save();
  canvas->Translate(0, 0);
  draw_radial_gradient(canvas, g_case_size / 2, g_case_size / 2,
                       g_case_size / 2, g_case_size);
  canvas->Restore();
}

static void draw_conical_gradient(skity::Canvas* canvas) {
  canvas->Save();
  canvas->Translate(0, 0);
  draw_radial_gradient(canvas, g_case_size / 2, g_case_size / 2, 0,
                       g_case_size / 2, g_case_size / 2, g_case_size / 2,
                       g_case_size);
  canvas->Restore();

  canvas->Save();
  canvas->Translate(g_case_size, 0);
  draw_radial_gradient(canvas, g_case_size / 2, g_case_size / 2,
                       g_case_size / 4, g_case_size / 2, g_case_size / 2,
                       g_case_size / 2, g_case_size);
  canvas->Restore();

  canvas->Save();
  canvas->Translate(g_case_size * 2, 0);
  draw_radial_gradient(canvas, g_case_size / 4, g_case_size / 4, 0,
                       g_case_size / 2, g_case_size / 2, g_case_size / 2,
                       g_case_size);
  canvas->Restore();

  canvas->Save();
  canvas->Translate(g_case_size * 3, 0);
  draw_radial_gradient(canvas, g_case_size / 4, g_case_size / 4,
                       g_case_size / 2, g_case_size / 2, g_case_size / 2, 0,
                       g_case_size);
  canvas->Restore();

  canvas->Save();
  canvas->Translate(0, g_case_size);
  draw_radial_gradient(canvas, g_case_size / 4, g_case_size / 4,
                       g_case_size / 4, g_case_size / 2, g_case_size / 2,
                       g_case_size / 2, g_case_size);
  canvas->Restore();

  canvas->Save();
  canvas->Translate(g_case_size, g_case_size);
  draw_radial_gradient(canvas, g_case_size / 4, g_case_size / 4,
                       g_case_size / 16, g_case_size / 2, g_case_size / 2,
                       g_case_size / 8, g_case_size);
  canvas->Restore();

  canvas->Save();
  canvas->Translate(g_case_size * 2, g_case_size);
  draw_radial_gradient(canvas, g_case_size / 4, g_case_size / 4,
                       g_case_size / 8, g_case_size / 2, g_case_size / 2,
                       g_case_size / 16, g_case_size);
  canvas->Restore();

  canvas->Save();
  canvas->Translate(g_case_size * 3, g_case_size);
  draw_radial_gradient(canvas, g_case_size / 8, g_case_size / 8,
                       g_case_size / 8, g_case_size / 2, g_case_size / 2,
                       g_case_size / 8, g_case_size);
  canvas->Restore();
}

void draw_gradient(skity::Canvas* canvas) {
  canvas->Save();
  canvas->Translate(0, 0);
  draw_gradient_with_tile_mode(canvas);
  canvas->Restore();

  canvas->Save();
  canvas->Translate(0, g_case_size * 2);
  draw_conical_gradient(canvas);
  canvas->Restore();

  canvas->Save();
  canvas->Translate(0, g_case_size * 4);
  draw_gradient_basic(canvas);
  canvas->Restore();
}
}  // namespace skity::example::gradient
