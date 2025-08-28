// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#define _USE_MATH_DEFINES

#include <array>
#include <cmath>
#include <cstdlib>
#include <skity/gpu/gpu_context.hpp>
#include <skity/skity.hpp>
#if !defined(SKITY_ANDROID) && !defined(SKITY_HARMONY)
#include <skity/codec/codec.hpp>
#endif
#include <string>

#include "perf.hpp"

void render_frame_demo(
    skity::Canvas* canvas,
    std::vector<std::shared_ptr<skity::Pixmap>> const& images,
    std::shared_ptr<skity::Typeface> typeface,
    std::shared_ptr<skity::Typeface> emoji, float mx, float my, float width,
    float height, float t);

void render_frame_demo(
    skity::Canvas* canvas, skity::GPUContext* gpu_context,
    std::vector<std::shared_ptr<skity::Pixmap>> const& images,
    std::shared_ptr<skity::Typeface> typeface,
    std::shared_ptr<skity::Typeface> emoji, float mx, float my, float width,
    float height, float t);

void draw_eyes(skity::Canvas* canvas, float x, float y, float w, float h,
               float mx, float my, float t);

void draw_graph(skity::Canvas* canvas, float x, float y, float w, float h,
                float t);

void draw_color_wheel(skity::Canvas* canvas, float x, float y, float w, float h,
                      float t);

void draw_lines(skity::Canvas* canvas, float x, float y, float w, float h,
                float t);

void draw_widths(skity::Canvas* canvas, float x, float y, float width);

void draw_caps(skity::Canvas* canvas, float x, float y, float width);

void draw_scissor(skity::Canvas* canvas, float x, float y, float t);

void draw_window(skity::Canvas* canvas, const char* title, float x, float y,
                 float w, float h);

void draw_search_box(skity::Canvas* canvas, const char* title, float x, float y,
                     float w, float h);

void draw_drop_down(skity::Canvas* canvas, const char* text, float x, float y,
                    float w, float h);

void draw_label(skity::Canvas* canvas, const char* text, float x, float y,
                float w, float h);

void draw_edit_box_base(skity::Canvas* canvas, float x, float y, float w,
                        float h);

void draw_edit_box(skity::Canvas* canvas, const char* text, float x, float y,
                   float w, float h);

void draw_check_box(skity::Canvas* canvas, const char* text, float x, float y,
                    float w, float h);

void draw_button(skity::Canvas* canvas, const char* pre_icon, const char* text,
                 float x, float y, float w, float h, skity::Color col);

void draw_edit_box_num(skity::Canvas* canvas, const char* text,
                       const char* units, float x, float y, float w, float h);

void draw_slider(skity::Canvas* canvas, float pos, float x, float y, float w,
                 float h);

void load_images(std::vector<std::shared_ptr<skity::Pixmap>>& images);

void draw_thumbnails(skity::Canvas* canvas, skity::GPUContext* gpu_context,
                     std::vector<std::shared_ptr<skity::Pixmap>> const& images,
                     float x, float y, float w, float h, float t);

void draw_spinner(skity::Canvas* canvas, float cx, float cy, float r, float t);

void draw_paragraph(skity::Canvas* canvas,
                    std::shared_ptr<skity::Typeface> typeface,
                    std::shared_ptr<skity::Typeface> emoji, float x, float y,
                    float width, float height);

void render_frame_demo(
    skity::Canvas* canvas, skity::GPUContext* gpu_context,
    std::vector<std::shared_ptr<skity::Pixmap>> const& images,
    std::shared_ptr<skity::Typeface> typeface,
    std::shared_ptr<skity::Typeface> emoji, float mx, float my, float width,
    float height, float t) {
  float x, y, popy;

  draw_eyes(canvas, width - 250, 50, 150, 100, mx, my, t);
  draw_paragraph(canvas, typeface, emoji, width - 450, 180, 150, 100);
  draw_graph(canvas, 0, height / 2.f, width, height / 2.f, t);
  // this make some performance issue
  draw_color_wheel(canvas, width - 300, height - 300, 250.f, 250.f, t);

  // Line joints
  draw_lines(canvas, 120, height - 50, 600, 50, t);
  draw_widths(canvas, 10, 50, 30);
  // Line caps
  draw_caps(canvas, 10, 300, 30);

  draw_scissor(canvas, 50, height - 80, t);

  // widgets
  draw_window(canvas, "Widgets 'n Stuff", 50, 50, 300, 400);
  x = 60;
  y = 95;
  draw_search_box(canvas, "Search", x, y, 280, 25);
  y += 40;
  draw_drop_down(canvas, "Effects", x, y, 280, 28);
  popy = y + 14.f;
  y += 45.f;

  // Form
  draw_label(canvas, "Login", x, y, 280, 20);
  y += 25.f;
  draw_edit_box(canvas, "Email", x, y, 280, 28);
  y += 35.f;
  draw_edit_box(canvas, "Password", x, y, 280, 28);
  y += 38.f;
  draw_check_box(canvas, "Remember me", x, y, 140, 28);
  draw_button(canvas, "\ufafb" /* login */, "Sign in", x + 138.f, y, 140, 28,
              skity::ColorSetARGB(255, 0, 96, 128));

  y += 45.f;
  // Slider
  draw_label(canvas, "Diameter", x, y, 280, 20);
  y += 25.f;
  draw_edit_box_num(canvas, "123.00", "px", x + 180, y, 100, 28);
  draw_slider(canvas, 0.4f, x, y, 170, 28);

  y += 55;
  draw_button(canvas, "\uf1f8" /* trash */, "Delete", x, y, 160, 28,
              skity::ColorSetARGB(255, 128, 16, 8));
  draw_button(canvas, nullptr, "Cancel", x + 170, y, 110, 28,
              skity::Color_TRANSPARENT);

  // Thumbnails box
  if (!images.empty()) {
    draw_thumbnails(canvas, gpu_context, images, 365, popy - 30, 160, 300, t);
  }
}

void render_frame_demo(
    skity::Canvas* canvas,
    std::vector<std::shared_ptr<skity::Pixmap>> const& images,
    std::shared_ptr<skity::Typeface> typeface,
    std::shared_ptr<skity::Typeface> emoji, float mx, float my, float width,
    float height, float t) {
  render_frame_demo(canvas, nullptr, images, typeface, emoji, mx, my, width,
                    height, t);
}

void draw_eyes(skity::Canvas* canvas, float x, float y, float w, float h,
               float mx, float my, float t) {
  skity::Paint gloss, bg;
  float ex = w * 0.23f;
  float ey = h * 0.5f;
  float lx = x + ex;
  float ly = y + ey;
  float rx = x + w - ex;
  float ry = y + ey;
  float dx, dy, d;
  float br = (ex < ey ? ex : ey) * 0.5f;
  float blink = 1.f - std::pow(std::sinf(t * 0.5f), 200.f) * 0.8f;

  // bg
  std::shared_ptr<skity::Shader> bg_gradient;
  {
    std::array<skity::Point, 2> pts = {
        skity::Point{x, y + h * 0.5f, 0.f, 1.f},
        skity::Point{x + w * 0.1f, y + h, 0.f, 1.f},
    };
    std::array<skity::Vec4, 2> colors = {
        skity::Vec4{0.f, 0.f, 0.f, 32.f / 255.f},
        skity::Vec4{0.f, 0.f, 0.f, 16.f / 255.f},
    };

    bg_gradient =
        skity::Shader::MakeLinear(pts.data(), colors.data(), nullptr, 2);
  }
  bg.SetStyle(skity::Paint::kFill_Style);
  bg.SetShader(bg_gradient);
  bg.SetAntiAlias(true);
  canvas->DrawOval(skity::Rect::MakeLTRB(lx + 0.3f - ex, ly + 16.f - ey,
                                         lx + 3.f + ex, ly + 16.f + ey),
                   bg);
  canvas->DrawOval(skity::Rect::MakeLTRB(rx + 3.f - ex, ry + 16.f - ey,
                                         rx + 3.f + ex, ry + 16.f + ey),
                   bg);

  {
    std::array<skity::Point, 2> pts = {
        skity::Point{x, y + h * 0.25f, 0.f, 1.f},
        skity::Point{x + w * 0.1f, y + h, 0.f, 1.f},
    };
    std::array<skity::Vec4, 2> colors = {
        skity::Vec4{220.f / 225.f, 220.f / 225.f, 220.f / 225.f, 1.f},
        skity::Vec4{128.f / 255.f, 128.f / 255.f, 128.f / 255.f, 1.f},
    };
    bg_gradient =
        skity::Shader::MakeLinear(pts.data(), colors.data(), nullptr, 2);
  }

  bg.SetShader(bg_gradient);
  canvas->DrawOval(skity::Rect::MakeLTRB(lx - ex, ly - ey, lx + ex, ly + ey),
                   bg);
  canvas->DrawOval(skity::Rect::MakeLTRB(rx - ex, ry - ey, rx + ex, ry + ey),
                   bg);

  dx = (mx - rx) / (ex * 10.f);
  dy = (my - ry) / (ey * 10.f);
  d = std::sqrtf(dx * dx + dy * dy);
  if (d > 1.f) {
    dx /= d;
    dy /= d;
  }

  dx *= ex * 0.4f;
  dy *= ey * 0.5f;

  bg.SetShader(nullptr);
  bg.SetFillColor(32.f / 255.f, 32.f / 255.f, 32.f / 255.f, 1.f);
  canvas->DrawOval(
      skity::Rect::MakeLTRB(
          lx + dx - br, ly + dy + ey * 0.25f * (1 - blink) - br * blink,
          lx + dx + br, ly + dy + ey * 0.25f * (1 - blink) + br * blink),
      bg);

  dx = (mx - rx) / (ex * 10.f);
  dy = (my - ry) / (ey * 10.f);
  d = std::sqrtf(dx * dx + dy * dy);
  if (d > 1.f) {
    dx /= d;
    dy /= d;
  }

  dx *= ex * 0.4f;
  dy *= ey * 0.5f;
  canvas->DrawOval(
      skity::Rect::MakeLTRB(
          rx + dx - br, ry + dy + ey * 0.25f * (1 - blink) - br * blink,
          rx + dx + br, ry + dy + ey * 0.25f * (1 - blink) + br * blink),
      bg);

  gloss.SetAntiAlias(true);
  gloss.SetStyle(skity::Paint::kFill_Style);
  {
    std::array<float, 2> stops = {0.0f, 1.f};
    std::array<skity::Vec4, 2> colors = {
        skity::Vec4{225.f / 225.f, 225.f / 225.f, 225.f / 225.f, 128.f / 255.f},
        skity::Vec4{225.f / 225.f, 225.f / 225.f, 225.f / 225.f, 0.f},
    };

    auto radial = skity::Shader::MakeRadial(
        skity::Point{lx - ex * 0.25f, ly - ey * 0.5f, 0.f, 1.f}, ex * 0.75f,
        colors.data(), stops.data(), 2);
    gloss.SetShader(radial);
  }

  canvas->DrawOval(skity::Rect::MakeLTRB(lx - ex, ly - ey, lx + ex, ly + ey),
                   gloss);

  {
    std::array<float, 2> stops = {0.0f, 1.f};
    std::array<skity::Vec4, 2> colors = {
        skity::Vec4{225.f / 225.f, 225.f / 225.f, 225.f / 225.f, 128.f / 255.f},
        skity::Vec4{225.f / 225.f, 225.f / 225.f, 225.f / 225.f, 0.f},
    };

    auto radial = skity::Shader::MakeRadial(
        skity::Point{rx - ex * 0.25f, ry - ey * 0.5f, 0.f, 1.f}, ex * 0.75f,
        colors.data(), stops.data(), 2);
    gloss.SetShader(radial);
  }
  canvas->DrawOval(skity::Rect::MakeLTRB(rx - ex, ry - ey, rx + ex, ry + ey),
                   gloss);
}

void draw_graph(skity::Canvas* canvas, float x, float y, float w, float h,
                float t) {
  std::array<float, 6> samples{};
  std::array<float, 6> sx{};
  std::array<float, 6> sy{};
  float dx = w / 5.f;

  samples[0] =
      (1.f + std::sinf(t * 1.2345f + std::cosf(t * 0.3345f) * 0.44f)) * 0.5f;
  samples[1] =
      (1.f + std::sinf(t * 0.68363f + std::cosf(t * 1.3f) * 1.55f)) * 0.5f;
  samples[2] =
      (1.f + std::sinf(t * 1.1642f + std::cosf(t * 0.33457) * 1.24f)) * 0.5f;
  samples[3] =
      (1.f + std::sinf(t * 0.56345f + std::cosf(t * 1.63f) * 0.14f)) * 0.5f;
  samples[4] =
      (1.f + std::sinf(t * 1.6245f + std::cosf(t * 0.254f) * 0.3f)) * 0.5f;
  samples[5] =
      (1.f + std::sinf(t * 0.345f + std::cosf(t * 0.03f) * 0.6f)) * 0.5f;

  for (int32_t i = 0; i < 6; i++) {
    sx[i] = x + i * dx;
    sy[i] = y + h * samples[i] * 0.8f;
  }

  skity::Paint paint;
  paint.SetAntiAlias(true);
  // Graph background
  {
    std::array<skity::Point, 2> pts{
        skity::Point{x, y, 0.f, 1.f},
        skity::Point{x, y + h, 0.f, 1.f},
    };
    std::array<skity::Vec4, 2> colors{
        skity::Vec4{0.f, 160.f / 255.f, 192.f / 255.f, 0.f},
        skity::Vec4{0.f, 160.f / 255.f, 192.f / 255.f, 64.f / 255.f},
    };

    paint.SetShader(
        skity::Shader::MakeLinear(pts.data(), colors.data(), nullptr, 2));
  }

  skity::Path path;
  path.MoveTo(sx[0], sy[0]);
  for (int32_t i = 1; i < 6; i++) {
    path.CubicTo(sx[i - 1] + dx * 0.5f, sy[i - 1], sx[i] - dx * 0.5f, sy[i],
                 sx[i], sy[i]);
  }
  path.LineTo(x + w, y + h);
  path.LineTo(x, y + h);
  path.Close();
  canvas->DrawPath(path, paint);

  // graph line
  skity::Path graph_line;
  graph_line.MoveTo(sx[0], sy[0] + 2.f);
  for (int32_t i = 1; i < 6; i++) {
    graph_line.CubicTo(sx[i - 1] + dx * 0.5f, sy[i - 1] + 2, sx[i] - dx * 0.5f,
                       sy[i] + 2.f, sx[i], sy[i] + 2.f);
  }
  paint.SetShader(nullptr);
  paint.SetStyle(skity::Paint::kStroke_Style);
  paint.SetStrokeColor(0.f, 0.f, 0.f, 32.f / 255.f);
  paint.SetStrokeWidth(3.f);

  canvas->DrawPath(graph_line, paint);

  skity::Path graph_line2;
  graph_line2.MoveTo(sx[0], sy[0]);
  for (int32_t i = 1; i < 6; i++) {
    graph_line2.CubicTo(sx[i - 1] + dx * 0.5f, sy[i - 1], sx[i] - dx * 0.5f,
                        sy[i], sx[i], sy[i]);
  }
  paint.SetStrokeColor(0.f, 160.f / 255.f, 192.f / 255.f, 1.f);
  canvas->DrawPath(graph_line2, paint);

  // graph sample pos
  paint.SetStyle(skity::Paint::kFill_Style);
  for (int32_t i = 0; i < 6; i++) {
    std::array<skity::Vec4, 2> colors{
        skity::Vec4{0.f, 0.f, 0.f, 32.f / 255.f},
        skity::Vec4{0.f, 0.f, 0.f, 0.f},
    };
    std::array<float, 2> stops = {3.f / 8.f, 1.f};

    auto bg = skity::Shader::MakeRadial({sx[i], sy[i] + 2.f, 0.f, 1.f}, 8.f,
                                        colors.data(), stops.data(), 2);
    paint.SetShader(bg);
    canvas->DrawRect(
        skity::Rect::MakeXYWH(sx[i] - 10.f, sy[i] - 10.f + 2.f, 20.f, 20.f),
        paint);
  }

  paint.SetShader(nullptr);
  paint.SetFillColor(0.f, 160.f / 255.f, 192.f / 255.f, 1.f);
  for (int32_t i = 0; i < 6; i++) {
    canvas->DrawCircle(sx[i], sy[i], 4.f, paint);
  }

  paint.SetFillColor(220.f / 255.f, 220.f / 255.f, 220.f / 255.f, 1.f);
  for (int32_t i = 0; i < 6; i++) {
    canvas->DrawCircle(sx[i], sy[i], 2.f, paint);
  }
}

void draw_color_wheel(skity::Canvas* canvas, float x, float y, float w, float h,
                      float t) {
  float r0, r1, ax, ay, bx, by, cx, cy, aeps, r;
  float hue = std::sinf(t * 0.12f);

  cx = x + w * 0.5f;
  cy = y + h * 0.5f;
  r1 = (w < h ? w : h) * 0.5f - 5.f;
  r0 = r1 - 20.f;
  aeps = 0.5f / r1;  // half a pixel arc length in radians (2pi cancels out).

  for (int32_t i = 0; i < 6; i++) {
    float a0 = (float)i / 6.f * M_PI * 2.f - aeps;
    float a1 = (float)(i + 1.f) / 6.f * M_PI * 2.f + aeps;

    float p1_x = cx + std::cos(a0) * r0;
    float p1_y = cy + std::sin(a0) * r0;

    float p3_x = cx + std::cos(a1) * r0;
    float p3_y = cy + std::sin(a1) * r0;

    skity::Vec2 p1r = (skity::Vec2{p1_x - cx, p1_y - cy}).Normalize();
    skity::Vec2 p3r = (skity::Vec2{p3_x - cx, p3_y - cy}).Normalize();
    skity::Vec2 p2r = ((p1r + p3r) * 0.5f).Normalize();
    p2r = skity::Vec2{cx, cy} +
          (r0 + r0 * M_PI * 0.1f * std::powf((a1 - a0) * 2.f / M_PI, 2)) * p2r;

    float p4_x = cx + std::cos(a0) * r1;
    float p4_y = cy + std::sin(a0) * r1;

    float p6_x = cx + std::cos(a1) * r1;
    float p6_y = cy + std::sin(a1) * r1;

    skity::Vec2 p4r = (skity::Vec2{p4_x - cx, p4_y - cy}).Normalize();
    skity::Vec2 p6r = (skity::Vec2{p6_x - cx, p6_y - cy}).Normalize();
    skity::Vec2 p5r = ((p6r + p4r) * 0.5f).Normalize();
    p5r = skity::Vec2{cx, cy} +
          (r1 + r1 * M_PI * 0.1f * std::powf((a1 - a0) * 2.f / M_PI, 2)) * p5r;

    skity::Vec2 p1c = skity::Vec2{p1_x - cx, p1_y - cy};

    skity::Path path;
    path.MoveTo(p1_x, p1_y);
    path.QuadTo(p2r.x, p2r.y, p3_x, p3_y);
    path.LineTo(p6_x, p6_y);
    path.QuadTo(p5r.x, p5r.y, p4_x, p4_y);
    path.Close();

    ax = cx + std::cosf(a0) * (r0 + r1) * 0.5f;
    ay = cy + std::sinf(a0) * (r0 + r1) * 0.5f;
    bx = cx + std::cosf(a1) * (r0 + r1) * 0.5f;
    by = cy + std::sinf(a1) * (r0 + r1) * 0.5f;

    skity::Paint paint;
    paint.SetAntiAlias(true);
    paint.SetStyle(skity::Paint::kFill_Style);
    paint.SetColor(skity::Color_BLUE);
    std::array<skity::Color4f, 2> colors{
        skity::Color4fFromColor(
            skity::ColorMakeFromHSLA(a0 / (M_PI * 2.f), 1.f, 0.55f, 255)),
        skity::Color4fFromColor(
            skity::ColorMakeFromHSLA(a1 / (M_PI * 2.f), 1.f, 0.55f, 255)),
    };
    std::array<skity::Point, 2> pts{
        skity::Point{ax, ay, 0.f, 1.f},
        skity::Point{bx, by, 0.f, 1.f},
    };
    paint.SetShader(
        skity::Shader::MakeLinear(pts.data(), colors.data(), nullptr, 2));
    canvas->DrawPath(path, paint);
  }

  {
    skity::Paint paint;
    paint.SetAntiAlias(true);
    paint.SetStyle(skity::Paint::kStroke_Style);
    paint.SetStrokeJoin(skity::Paint::kRound_Join);
    paint.SetStrokeWidth(1.f);
    paint.SetColor(skity::ColorSetARGB(64, 0, 0, 0));
    // FIXME: path add two circle got error path move and lineto command, need
    // fix
    // skity::Path path; path.addCircle(cx, cy, r0 - 0.5f);
    // path.addCircle(cx, cy, r1 + 0.5f);
    // canvas->drawPath(path, paint);
    canvas->DrawCircle(cx, cy, r0 - 0.5f, paint);
    canvas->DrawCircle(cx, cy, r1 + 0.5f, paint);
  }

  // selector
  canvas->Save();
  canvas->Translate(cx, cy);
  canvas->Rotate(skity::FloatRadiansToDegrees(hue * M_PI * 2.f));

  // Marker on
  {
    skity::Paint paint;
    paint.SetAntiAlias(true);
    paint.SetStyle(skity::Paint::kStroke_Style);
    paint.SetStrokeWidth(2.f);
    paint.SetColor(skity::ColorSetARGB(192, 255, 255, 255));
    skity::Path path;
    path.AddRect(skity::Rect::MakeXYWH(r0 - 1.f, -3.f, r1 - r0 + 2.f, 6.f));
    canvas->DrawPath(path, paint);

    paint.SetStyle(skity::Paint::kStroke_Style);
    paint.SetColor(skity::ColorSetARGB(64, 0, 0, 0));
    paint.SetStrokeWidth(1.f);
    canvas->DrawRect(
        skity::Rect::MakeXYWH(r0 - 2.f, -4.f, r1 - r0 + 2.f + 2.f, 6.f + 2.f),
        paint);
  }

  // Center triangle
  {
    r = r0 - 6.f;
    ax = std::cosf(120.0f / 180.0f * M_PI) * r;
    ay = std::sinf(120.0f / 180.0f * M_PI) * r;
    bx = std::cosf(-120.0f / 180.0f * M_PI) * r;
    by = std::sinf(-120.0f / 180.0f * M_PI) * r;
    skity::Path triangle;
    triangle.MoveTo(r, 0.f);
    triangle.LineTo(ax, ay);
    triangle.LineTo(bx, by);
    triangle.Close();

    skity::Paint paint;
    paint.SetAntiAlias(true);
    paint.SetStyle(skity::Paint::kFill_Style);

    // r, 0, ax,ay, nvgHSLA(hue,1.0f,0.5f,255), nvgRGBA(255,255,255,255)
    std::array<skity::Color4f, 2> colors{
        skity::Color4fFromColor(skity::ColorMakeFromHSLA(hue, 1.0f, 0.5f, 255)),
        skity::Color4fFromColor(skity::ColorSetARGB(255, 255, 255, 255)),
    };
    std::array<skity::Point, 2> pts{
        skity::Point{r, 0.f, 0.f, 1.f},
        skity::Point{ax, ay, 0.f, 1.f},
    };

    paint.SetShader(
        skity::Shader::MakeLinear(pts.data(), colors.data(), nullptr, 2));
    canvas->DrawPath(triangle, paint);

    // (r+ax)*0.5f,(0+ay)*0.5f, bx,by, nvgRGBA(0,0,0,0), nvgRGBA(0,0,0,255)
    colors[0] = skity::Color4fFromColor(skity::ColorSetARGB(0, 0, 0, 0));
    colors[1] = skity::Color4fFromColor(skity::ColorSetARGB(255, 0, 0, 0));
    pts[0].x = (r + ax) * 0.5f;
    pts[0].y = (0 + ay) * 0.5f;
    pts[1].x = bx;
    pts[1].y = by;

    paint.SetShader(
        skity::Shader::MakeLinear(pts.data(), colors.data(), nullptr, 2));

    canvas->DrawPath(triangle, paint);

    // Select circle on triangle
    ax = std::cosf(120.0f / 180.0f * M_PI) * r * 0.3f;
    ay = std::sinf(120.0f / 180.0f * M_PI) * r * 0.4f;
    paint.SetStyle(skity::Paint::kStroke_Style);
    paint.SetStrokeWidth(2.f);
    skity::Path circle;
    circle.AddCircle(ax, ay, 5.f);
    paint.SetShader(nullptr);
    paint.SetColor(skity::ColorSetARGB(192, 255, 255, 255));
    canvas->DrawPath(circle, paint);

    colors[0] = {0, 0, 0, 64.f / 255.f};
    colors[1] = {0.f, 0.f, 0.f, 0.f};

    std::array<float, 2> stops = {7.f / 9.f, 1.f};
    paint.SetShader(skity::Shader::MakeRadial(
        skity::Point{ax, ay, 0.f, 1.f}, 9.f, colors.data(), stops.data(), 2));
    skity::Path circle2;
    circle2.AddCircle(ax, ay, 8.f);
    canvas->DrawPath(circle2, paint);
  }

  canvas->Restore();
}

void draw_lines(skity::Canvas* canvas, float x, float y, float w, float,
                float t) {
  float pad = 5.0f, s = w / 9.0f - pad * 2;
  float pts[4 * 2], fx, fy;
  skity::Paint::Join joins[3] = {skity::Paint::kMiter_Join,
                                 skity::Paint::kRound_Join,
                                 skity::Paint::kBevel_Join};
  skity::Paint::Cap caps[3] = {skity::Paint::kButt_Cap,
                               skity::Paint::kRound_Cap,
                               skity::Paint::kSquare_Cap};

  pts[0] = -s * 0.25f + std::cosf(t * 0.3f) * s * 0.5f;
  pts[1] = std::sinf(t * 0.3f) * s * 0.5f;
  pts[2] = -s * 0.25f;
  pts[3] = 0.f;
  pts[4] = s * 0.25f;
  pts[5] = 0.f;
  pts[6] = s * 0.25f + std::cosf(-t * 0.3f) * s * 0.5f;
  pts[7] = std::sinf(-t * 0.3f) * s * 0.5f;

  for (int32_t i = 0; i < 3; i++) {
    for (int32_t j = 0; j < 3; j++) {
      fx = x + s * 0.5f + (i * 3 + j) / 9.f * w + pad;
      fy = y - s * 0.5f + pad;
      skity::Paint paint;
      paint.SetStyle(skity::Paint::kStroke_Style);
      paint.SetAntiAlias(true);
      paint.SetStrokeCap(caps[i]);
      paint.SetStrokeJoin(joins[j]);
      paint.SetColor(skity::ColorSetARGB(160, 0, 0, 0));
      paint.SetStrokeWidth(s * 0.3f);

      skity::Path path;
      path.MoveTo(fx + pts[0], fy + pts[1]);
      path.LineTo(fx + pts[2], fy + pts[3]);
      path.LineTo(fx + pts[4], fy + pts[5]);
      path.LineTo(fx + pts[6], fy + pts[7]);

      canvas->DrawPath(path, paint);

      paint.SetStrokeCap(skity::Paint::kButt_Cap);
      paint.SetStrokeJoin(skity::Paint::kBevel_Join);
      paint.SetStrokeWidth(1.f);
      paint.SetColor(skity::ColorSetARGB(255, 0, 192, 255));

      canvas->DrawPath(path, paint);
    }
  }
}

void draw_widths(skity::Canvas* canvas, float x, float y, float width) {
  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(skity::ColorSetARGB(255, 0, 0, 0));
  paint.SetStyle(skity::Paint::kStroke_Style);
  for (int i = 0; i < 20; i++) {
    float w = (i + 0.5f) * 0.1f;
    paint.SetStrokeWidth(w);
    skity::Path path;
    path.MoveTo(x, y);
    path.LineTo(x + width, y + width * 0.3f);
    canvas->DrawPath(path, paint);
    y += 10.f;
  }
}

void draw_caps(skity::Canvas* canvas, float x, float y, float width) {
  skity::Paint::Cap caps[3] = {skity::Paint::kButt_Cap,
                               skity::Paint::kRound_Cap,
                               skity::Paint::kSquare_Cap};

  float line_width = 8.f;

  skity::Paint paint;
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetColor(skity::ColorSetARGB(32, 255, 255, 255));
  canvas->DrawRect(
      skity::Rect::MakeXYWH(x - line_width / 2.f, y, width + line_width, 40.f),
      paint);
  canvas->DrawRect(skity::Rect::MakeXYWH(x, y, width, 40.f), paint);

  paint.SetStrokeWidth(line_width);
  paint.SetStyle(skity::Paint::kStroke_Style);
  paint.SetColor(skity::Color_BLACK);
  for (int32_t i = 0; i < 3; i++) {
    paint.SetStrokeCap(caps[i]);
    skity::Path line;
    line.MoveTo(x, y + i * 10.f + 5.f);
    line.LineTo(x + width, y + i * 10.f + 5.f);
    canvas->DrawPath(line, paint);
  }
}

void draw_scissor(skity::Canvas* canvas, float x, float y, float t) {
  canvas->Save();

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetStyle(skity::Paint::kFill_Style);

  canvas->Translate(x, y);
  canvas->Rotate(5.f);
  // draw first rect and clip rect for it's area.
  skity::Path rect;
  rect.AddRect(skity::Rect::MakeXYWH(-20, -20, 60, 40));
  paint.SetColor(skity::ColorSetARGB(255, 255, 0, 0));
  canvas->DrawPath(rect, paint);

  // draw second rectangle with offset and rotation.
  canvas->Translate(40, 0);
  canvas->Rotate(skity::FloatRadiansToDegrees(t));

  paint.SetColor(skity::ColorSetARGB(64, 255, 128, 0));
  canvas->DrawRect(skity::Rect::MakeXYWH(-20, -10, 60, 30), paint);

  canvas->ClipRect(skity::Rect::MakeXYWH(-20, -10, 60, 30));
  canvas->Rotate(skity::FloatRadiansToDegrees(t));
  paint.SetColor(skity::ColorSetARGB(255, 255, 128, 0));
  canvas->DrawRect(skity::Rect::MakeXYWH(-20, -10, 60, 30), paint);

  canvas->Restore();
}

void draw_window(skity::Canvas* canvas, const char* title, float x, float y,
                 float w, float h) {
  float corner_radius = 3.f;

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetStyle(skity::Paint::kFill_Style);

  skity::RRect rrect;

  // window
  rrect.SetRectXY(skity::Rect::MakeXYWH(x - 5, y - 5, w + 10, h + 10),
                  corner_radius, corner_radius);

  paint.SetColor(skity::ColorSetARGB(64, 0, 0, 0));
  canvas->DrawRRect(rrect, paint);

  paint.SetColor(skity::ColorSetARGB(192, 28, 30, 34));
  rrect.SetRectXY(skity::Rect::MakeXYWH(x, y, w, h), corner_radius,
                  corner_radius);
  canvas->DrawRRect(rrect, paint);

  // header
  std::array<skity::Color4f, 2> colors{};
  colors[0] = skity::Color4fFromColor(skity::ColorSetARGB(8, 255, 255, 255));
  colors[1] = skity::Color4fFromColor(skity::ColorSetARGB(16, 0, 0, 0));
  std::array<skity::Point, 2> pts{skity::Point{x, y, 0, 1},
                                  skity::Point{x, y + 15, 0.f, 1.f}};
  paint.SetShader(
      skity::Shader::MakeLinear(pts.data(), colors.data(), nullptr, 2));
  rrect.SetRectXY(skity::Rect::MakeXYWH(x + 1, y + 1, w - 2, 30),
                  corner_radius - 1.f, corner_radius - 1.f);
  canvas->DrawRRect(rrect, paint);

  paint.SetShader(nullptr);
  paint.SetStyle(skity::Paint::kStroke_Style);
  paint.SetColor(skity::ColorSetARGB(32, 0, 0, 0));
  skity::Path header;
  header.MoveTo(x + 0.5f, y + 0.5f + 30.f);
  header.LineTo(x + 0.5f + w - 1.f, y + 0.5f + 30.f);
  canvas->DrawPath(header, paint);

  paint.SetTextSize(16.f);
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetColor(skity::ColorSetARGB(160, 220, 220, 220));
  canvas->DrawSimpleText2(title, x + w / 2.f - 80.f, y + 16 + 2, paint);
}

void draw_search_box(skity::Canvas* canvas, const char* title, float x, float y,
                     float w, float h) {
  float corner_radius = h / 2.f - 1.f;

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetStyle(skity::Paint::kFill_Style);
  // Edit
  skity::RRect rrect;
  rrect.SetRectXY(skity::Rect::MakeXYWH(x, y, w, h), corner_radius,
                  corner_radius);
  {
    std::array<skity::Color4f, 2> colors;
    std::array<skity::Point, 2> pts;
    colors[0] = skity::Color4fFromColor(skity::ColorSetARGB(16, 0, 0, 0));
    colors[1] = skity::Color4fFromColor(skity::ColorSetARGB(92, 0, 0, 0));
    pts[0] = skity::Point{x, y, 0, 0};
    pts[1] = skity::Point{x, y + h, 0, 0};
    paint.SetShader(
        skity::Shader::MakeLinear(pts.data(), colors.data(), nullptr, 2));
  }
  canvas->DrawRRect(rrect, paint);

  paint.SetShader(nullptr);
  paint.SetStyle(skity::Paint::kStroke_Style);
  paint.SetColor(skity::ColorSetARGB(48, 0, 0, 0));
  rrect.SetRectXY(skity::Rect::MakeXYWH(x + 0.5f, y + 0.5f, w - 1.f, h - 1.f),
                  corner_radius - 0.5f, corner_radius - 0.5f);
  canvas->DrawRRect(rrect, paint);

  std::string search_icon = "\uf002";
  paint.SetTextSize(h * 0.6f);
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetColor(skity::ColorSetARGB(32, 255, 255, 255));
  canvas->DrawSimpleText2(search_icon.c_str(), x + h * 0.3f, y + h * 0.8f,
                          paint);

  paint.SetTextSize(17.f);
  paint.SetColor(skity::ColorSetARGB(32, 255, 255, 255));
  canvas->DrawSimpleText2(title, x + h * 1.05f, y + h * 0.5f + 8.f, paint);

  std::string cancle_icon = "\uf2d3";
  paint.SetTextSize(h * 0.6f);
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetColor(skity::ColorSetARGB(32, 255, 255, 255));
  canvas->DrawSimpleText2(cancle_icon.c_str(), x + w - h * 1.0f, y + h * 0.7f,
                          paint);
}

void draw_drop_down(skity::Canvas* canvas, const char* text, float x, float y,
                    float w, float h) {
  float corner_radius = 4.f;
  skity::RRect rrect;
  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetStyle(skity::Paint::kFill_Style);
  {
    std::array<skity::Color4f, 2> colors{};
    std::array<skity::Point, 2> pts{};
    colors[0] = skity::Color4fFromColor(skity::ColorSetARGB(16, 255, 255, 255));
    colors[1] = skity::Color4fFromColor(skity::ColorSetARGB(16, 0, 0, 0));
    pts[0] = {x, y, 0, 1.f};
    pts[1] = {x, y + h, 0.f, 1.f};
    paint.SetShader(
        skity::Shader::MakeLinear(pts.data(), colors.data(), nullptr, 2));
  }
  rrect.SetRectXY(skity::Rect::MakeXYWH(x + 1.f, y + 1.f, w - 2.f, h - 2.f),
                  corner_radius - 1.f, corner_radius - 1.f);
  canvas->DrawRRect(rrect, paint);

  paint.SetShader(nullptr);
  paint.SetStyle(skity::Paint::kStroke_Style);
  paint.SetColor(skity::ColorSetARGB(48, 0, 0, 0));
  paint.SetStrokeWidth(2.f);
  rrect.SetRectXY(skity::Rect::MakeXYWH(x + 0.5f, y + 0.5f, w - 1.f, h - 1.f),
                  corner_radius - 0.5f, corner_radius - 0.5f);
  canvas->DrawRRect(rrect, paint);

  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetTextSize(17.f);
  paint.SetColor(skity::ColorSetARGB(160, 255, 255, 255));
  canvas->DrawSimpleText2(text, x + h * 0.3f, y + h * 0.7f, paint);

  paint.SetTextSize(h * 1.1f);
  std::string angle_right = "\uf105";
  canvas->DrawSimpleText2(angle_right.c_str(), x + w - h * 0.8f, y + h * 0.9f,
                          paint);
}

void draw_label(skity::Canvas* canvas, const char* text, float x, float y,
                float, float h) {
  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetColor(skity::ColorSetARGB(128, 255, 255, 255));
  paint.SetTextSize(15.f);

  canvas->DrawSimpleText2(text, x, y + h * 0.9f, paint);
}

void draw_edit_box_base(skity::Canvas* canvas, float x, float y, float w,
                        float h) {
  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetColor(skity::ColorSetARGB(32, 255, 255, 255));
  skity::RRect rrect;
  rrect.SetRectXY(skity::Rect::MakeXYWH(x + 1.f, y + 1.f, w - 2.f, h - 2.f),
                  3.f, 3.f);
  canvas->DrawRRect(rrect, paint);

  paint.SetColor(skity::ColorSetARGB(48, 0, 0, 0));
  paint.SetStyle(skity::Paint::kStroke_Style);
  rrect.SetRectXY(skity::Rect::MakeXYWH(x + 0.5f, y + 0.5f, w - 1.f, h - 1.f),
                  3.5f, 3.5f);
  canvas->DrawRRect(rrect, paint);
}

void draw_edit_box(skity::Canvas* canvas, const char* text, float x, float y,
                   float w, float h) {
  draw_edit_box_base(canvas, x, y, w, h);

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetTextSize(17.f);
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetColor(skity::ColorSetARGB(64, 255, 255, 255));
  canvas->DrawSimpleText2(text, x + h * 0.3f, y + h * 0.7f, paint);
}

void draw_check_box(skity::Canvas* canvas, const char* text, float x, float y,
                    float, float h) {
  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetTextSize(15.f);
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetColor(skity::ColorSetARGB(160, 255, 255, 255));
  canvas->DrawSimpleText2(text, x + 28.f, y + h * 0.7f, paint);

  skity::RRect rrect;
  rrect.SetRectXY(
      skity::Rect::MakeXYWH(x + 1.f, y + h * 0.5f - 9.f + 1.f, 18.f, 18.f), 3.f,
      3.f);
  paint.SetColor(skity::ColorSetARGB(128, 0, 0, 0));
  canvas->DrawRRect(rrect, paint);

  paint.SetTextSize(20.f);
  paint.SetColor(skity::ColorSetARGB(128, 255, 255, 255));
  std::string icon_check = "\uf00c";
  canvas->DrawSimpleText2(icon_check.c_str(), x + 1.f, y + h * 0.8f, paint);
}

void draw_button(skity::Canvas* canvas, const char* pre_icon, const char* text,
                 float x, float y, float w, float h, skity::Color col) {
  float corner_radius = 4.f;
  float tw = 0.f, iw = 0.f;

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetStyle(skity::Paint::kFill_Style);
  skity::RRect rrect;
  rrect.SetRectXY(skity::Rect::MakeXYWH(x + 1.f, y + 1.f, w - 2.f, h - 2.f),
                  corner_radius - 1.f, corner_radius - 1.f);
  paint.SetColor(col);
  canvas->DrawRRect(rrect, paint);
  {
    std::array<skity::Color4f, 2> colors{};
    std::array<skity::Point, 2> pts{};
    colors[0] = skity::Color4fFromColor(skity::ColorSetARGB(32, 255, 255, 255));
    colors[1] = skity::Color4fFromColor(skity::ColorSetARGB(32, 0, 0, 0));
    pts[0] = {x, y, 0, 1};
    pts[1] = {x, y + h, 0, 1};
    paint.SetShader(
        skity::Shader::MakeLinear(pts.data(), colors.data(), nullptr, 2));
  }
  canvas->DrawRRect(rrect, paint);

  paint.SetShader(nullptr);
  paint.SetStyle(skity::Paint::kStroke_Style);
  paint.SetColor(skity::ColorSetARGB(48, 0, 0, 0));
  rrect.SetRectXY(skity::Rect::MakeXYWH(x + 0.5f, y + 0.5f, w - 1.f, h - 1.f),
                  corner_radius - 0.5f, corner_radius - 0.5f);
  canvas->DrawRRect(rrect, paint);

  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetTextSize(17.f);
  tw = canvas->SimpleTextBounds(text, paint).x;

  if (pre_icon) {
    paint.SetTextSize(h * 0.8f);
    iw = canvas->SimpleTextBounds(pre_icon, paint).x;
    canvas->DrawSimpleText2(pre_icon, x + w * 0.5f - tw * 0.5f - iw,
                            y + h * 0.75f, paint);
  }

  paint.SetTextSize(17.f);
  paint.SetColor(skity::ColorSetARGB(160, 0, 0, 0));
  canvas->DrawSimpleText2(text, x + w * 0.5f - tw * 0.5f + iw * 0.25f,
                          y + h * 0.7f - 1.f, paint);
  paint.SetColor(skity::ColorSetARGB(160, 255, 255, 255));
  canvas->DrawSimpleText2(text, x + w * 0.5f - tw * 0.5f + iw * 0.25f,
                          y + h * 0.7f, paint);
}

void draw_edit_box_num(skity::Canvas* canvas, const char* text,
                       const char* units, float x, float y, float w, float h) {
  float uw;
  draw_edit_box_base(canvas, x, y, w, h);

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetTextSize(15.f);
  paint.SetStyle(skity::Paint::kFill_Style);

  uw = canvas->SimpleTextBounds(units, paint).x;

  paint.SetColor(skity::ColorSetARGB(64, 255, 255, 255));
  canvas->DrawSimpleText2(units, x + w - h * 0.3f - uw, y + h * 0.6f, paint);

  paint.SetTextSize(17.f);
  paint.SetColor(skity::ColorSetARGB(128, 255, 255, 255));
  float tw = canvas->SimpleTextBounds(text, paint).x;

  canvas->DrawSimpleText2(text, x + w - h * 0.5f - uw - tw, y + h * 0.65f,
                          paint);
}

void draw_slider(skity::Canvas* canvas, float pos, float x, float y, float w,
                 float h) {
  float cy = y + (int)(h * 0.5f);
  float kr = (int)(h * 0.25f);
  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetStyle(skity::Paint::kFill_Style);
  // Slot
  skity::RRect rrect;
  rrect.SetRectXY(skity::Rect::MakeXYWH(x, cy - 2, w, 4), 2, 2);
  paint.SetColor(skity::ColorSetARGB(64, 0, 0, 0));
  canvas->DrawRRect(rrect, paint);

  // Knob Shadow
  {
    std::array<skity::Color4f, 2> colors{};
    colors[0] = skity::Color4fFromColor(skity::ColorSetARGB(64, 0, 0, 0));
    colors[1] = skity::Color4fFromColor(skity::ColorSetARGB(0, 0, 0, 0));
    paint.SetShader(skity::Shader::MakeRadial(
        {x + pos * w, cy + 1.f, 0.f, 1.f}, kr + 3, colors.data(), nullptr, 2));
  }
  skity::Path path;
  path.AddRect(skity::Rect::MakeXYWH(x + pos * w - kr - 5, cy - kr - 5,
                                     kr * 2 + 5 + 5, kr * 2 + 5 + 5 + 3));
  path.AddCircle(x + pos * w, cy, kr, skity::Path::Direction::kCCW);
  canvas->DrawPath(path, paint);

  skity::Path knob;
  knob.AddCircle(x + pos * w, cy, kr - 1);
  paint.SetShader(nullptr);
  paint.SetColor(skity::ColorSetARGB(255, 40, 43, 48));
  canvas->DrawPath(knob, paint);
  {
    std::array<skity::Color4f, 2> colors{};
    std::array<skity::Point, 2> pts{};
    colors[0] = skity::Color4fFromColor(skity::ColorSetARGB(16, 255, 255, 255));
    colors[1] = skity::Color4fFromColor(skity::ColorSetARGB(16, 0, 0, 0));
    pts[0] = {x, cy - kr, 0, 1};
    pts[1] = {x, cy + kr, 0, 1};
    paint.SetShader(
        skity::Shader::MakeLinear(pts.data(), colors.data(), nullptr, 2));
  }
  canvas->DrawPath(knob, paint);

  skity::Path circle;
  circle.AddCircle(x + pos * w, cy, kr - 0.5f);
  paint.SetShader(nullptr);
  paint.SetColor(skity::ColorSetARGB(92, 0, 0, 0));
  paint.SetStyle(skity::Paint::kStroke_Style);
  canvas->DrawPath(circle, paint);
}

#if !defined(SKITY_ANDROID) && !defined(SKITY_HARMONY)
void load_images(std::vector<std::shared_ptr<skity::Pixmap>>& images) {
  for (int32_t i = 0; i < 12; i++) {
    char buffer[128];
    std::sprintf(buffer, "%s/image%d.jpg", EXAMPLE_IMAGE_ROOT, i + 1);
    auto data = skity::Data::MakeFromFileName(buffer);
    if (!data) {
      continue;
    }

    auto codec = skity::Codec::MakeFromData(data);
    if (!codec) {
      continue;
    }

    codec->SetData(data);
    auto pixmap = codec->Decode();
    if (!pixmap) {
      continue;
    }

    images.emplace_back(pixmap);
  }
}
#endif

void draw_thumbnails(skity::Canvas* canvas, skity::GPUContext* gpu_context,
                     std::vector<std::shared_ptr<skity::Pixmap>> const& images,
                     float x, float y, float w, float h, float t) {
  float corner_radius = 3.f;
  float iw, ih;
  float thumb = 60.0f;
  float arry = 30.5f;
  int imgw, imgh;
  float stackh = (images.size() / 2.f) * (thumb + 10) + 10;

  float u = (1.f + std::cosf(t * 0.5f)) * 0.5f;
  float u2 = (1.f - std::cosf(t * 0.2f)) * 0.5f;
  float scrollh, dv;

  // Fake shadow
  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetStyle(skity::Paint::kStroke_Style);
  paint.SetStrokeJoin(skity::Paint::kMiter_Join);
  paint.SetColor(skity::ColorSetARGB(64, 0, 0, 0));
  paint.SetStrokeWidth(5.f);
  skity::Rect rect{x - 2.5f, y - 2.5f, x + w + 2.5f, y + h + 2.5f};
  canvas->DrawRect(rect, paint);

  // window
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetColor(skity::ColorSetARGB(255, 200, 200, 200));
  skity::Path path;
  path.MoveTo(x - 10, y + arry);
  path.LineTo(x + 1.f, y + arry - 11.f);
  path.LineTo(x + 1.f, y + arry + 11.f);
  path.Close();
  canvas->DrawPath(path, paint);
  rect.SetXYWH(x, y, w, h);
  canvas->DrawRRect(
      skity::RRect::MakeRectXY(rect, corner_radius, corner_radius), paint);

  canvas->Save();
  canvas->ClipRect(rect);
  canvas->Translate(0, -(stackh - h) * u);

  dv = 1.0f / (float)(images.size() - 1);

  for (size_t i = 0; i < images.size(); i++) {
    float tx, ty, v, a;
    tx = x + 10;
    ty = y + 10;
    tx += (i % 2) * (thumb + 10);
    ty += (i / 2) * (thumb + 10);
    const auto& img = images[i];
    imgw = img->Width();
    imgh = img->Height();
    if (imgw < imgh) {
      iw = thumb;
      ih = iw * (float)imgh / (float)imgw;
    } else {
      ih = thumb;
      iw = ih * (float)imgw / (float)imgh;
    }

    v = i * dv;
    a = std::clamp((u2 - v) / dv, 0.f, 1.f);
    if (a < 1.0f) {
      // render is not correct
      draw_spinner(canvas, tx + thumb / 2.f, ty + thumb / 2.f, thumb * 0.25f,
                   t);
    }

    paint.SetAlphaF(a);

    skity::Rect image_bounds{};
    image_bounds.SetXYWH(tx, ty, thumb, thumb);
    std::shared_ptr<skity::Image> image;
    if (gpu_context) {
      auto texture =
          gpu_context->CreateTexture(skity::TextureFormat::kRGBA, img->Width(),
                                     img->Height(), img->GetAlphaType());
      texture->UploadImage(img);
      image = skity::Image::MakeHWImage(texture);
    } else {
      image = skity::Image::MakeImage(img);
    }

    {
      auto s = skity::Shader::MakeShader(image);
      s->SetLocalMatrix(skity::Matrix{}.PostTranslate(image_bounds.Left(),
                                                      image_bounds.Top()));

      paint.SetShader(s);
    }

    paint.SetStyle(skity::Paint::kFill_Style);
    canvas->DrawRRect(skity::RRect::MakeRectXY(image_bounds, 5, 5), paint);

    paint.SetAlphaF(1.f);
    paint.SetShader(nullptr);
    paint.SetColor(skity::ColorSetARGB(64, 0, 0, 0));
    paint.SetStrokeWidth(2.f);
    paint.SetStyle(skity::Paint::kStroke_Style);
    image_bounds.SetXYWH(tx - 1.f, ty - 1.f, thumb + 2.f, thumb + 2.f);
    canvas->DrawRRect(skity::RRect::MakeRectXY(image_bounds, 6.f, 6.f), paint);
  }

  canvas->Restore();

  // Hide fades
  paint.SetAlphaF(1.f);
  paint.SetStyle(skity::Paint::kFill_Style);
  std::array<skity::Color4f, 2> colors{};
  std::array<skity::Point, 2> pts{};
  colors[0] = skity::Color4fFromColor(skity::ColorSetARGB(255, 200, 200, 200));
  colors[1] = skity::Color4fFromColor(skity::ColorSetARGB(0, 200, 200, 200));
  pts[0] = {x, y, 0, 1};
  pts[1] = {x, y + 6, 0, 1};

  paint.SetShader(
      skity::Shader::MakeLinear(pts.data(), colors.data(), nullptr, 2));
  canvas->DrawRect(skity::Rect::MakeXYWH(x + 4, y, w - 8, 6), paint);

  pts[0] = {x, y + h, 0, 1};
  pts[1] = {x, y + h - 6, 0, 1};

  paint.SetShader(
      skity::Shader::MakeLinear(pts.data(), colors.data(), nullptr, 2));

  canvas->DrawRect(skity::Rect::MakeXYWH(x + 4, y + h - 6, w - 8, 6), paint);

  // Scroll bar
  paint.SetShader(nullptr);
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetStrokeWidth(1.f);
  paint.SetColor(skity::ColorSetARGB(62, 0, 0, 0));
  skity::Rect scroll_bar;
  scroll_bar.SetXYWH(x + w - 12 - 0.5f, y + 4 - 0.5f, 8 + 1, h - 8 + 1);
  canvas->DrawRRect(skity::RRect::MakeRectXY(scroll_bar, 3, 3), paint);

  scrollh = (h / stackh) * (h - 8);
  paint.SetColor(skity::ColorSetARGB(255, 220, 220, 220));
  scroll_bar.SetXYWH(x + w - 12 + 1, y + 4 + 1 + (h - 8 - scrollh) * u, 8 - 2,
                     scrollh - 2);

  canvas->DrawRRect(skity::RRect::MakeRectXY(scroll_bar, 2, 2), paint);
}

void draw_spinner(skity::Canvas* canvas, float cx, float cy, float r, float t) {
  float a0 = 0.f + t * 6;
  float a1 = M_PI + t * 6;
  float r0 = r;
  float r1 = r * 0.75f;
  float ax, ay, bx, by;
  float cr = (r0 + r1) * 0.5f;

  skity::Path path;
  path.AddCircle(cx, cy, cr);

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetStyle(skity::Paint::kStroke_Style);
  paint.SetStrokeWidth(2.f);
  {
    std::array<skity::Color4f, 2> colors{};
    std::array<skity::Point, 2> pts{};

    ax = cx + std::cosf(a0) * (r0 + r1) * 0.5f;
    ay = cy + std::sinf(a0) * (r0 + r1) * 0.5f;
    bx = cx + std::cosf(a1) * (r0 + r1) * 0.5f;
    by = cy + std::sinf(a1) * (r0 + r1) * 0.5f;

    colors[0] = skity::Color4fFromColor(skity::ColorSetARGB(0, 0, 0, 0));
    colors[1] = skity::Color4fFromColor(skity::ColorSetARGB(128, 0, 0, 0));

    pts[0] = {ax, ay, 0, 1};
    pts[1] = {bx, by, 0, 1};

    paint.SetShader(
        skity::Shader::MakeLinear(pts.data(), colors.data(), nullptr, 2));
  }

  canvas->DrawPath(path, paint);
}

void draw_paragraph(skity::Canvas* canvas,
                    std::shared_ptr<skity::Typeface> typeface,
                    std::shared_ptr<skity::Typeface> emoji, float x, float y,
                    float, float) {
  skity::Paint paint;
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetTextSize(15.f);
  paint.SetTypeface(typeface);

  auto delegate =
      skity::TypefaceDelegate::CreateSimpleFallbackDelegate({emoji});

  skity::TextBlobBuilder builder;

  const char* text1 = "This is longer chunk of text.";
  const char* text2 = "Would have used lorem ipsum.";
  const char* text3 = "but she    was busy jumping";
  const char* text4 = "over the lazy dog with the fox";
  const char* text5 = "and all the men who came to";
  const char* text6 = "the aid of the party.🎉🙃👀";

  auto blob1 = builder.BuildTextBlob(text1, paint, delegate.get());
  auto blob2 = builder.BuildTextBlob(text2, paint, delegate.get());
  auto blob3 = builder.BuildTextBlob(text3, paint, delegate.get());
  auto blob4 = builder.BuildTextBlob(text4, paint, delegate.get());
  auto blob5 = builder.BuildTextBlob(text5, paint, delegate.get());
  auto blob6 = builder.BuildTextBlob(text6, paint, delegate.get());

  skity::Vec2 text1_bounds = blob1->GetBoundSize();
  skity::Vec2 text2_bounds = blob2->GetBoundSize();
  skity::Vec2 text3_bounds = blob3->GetBoundSize();
  skity::Vec2 text4_bounds = blob4->GetBoundSize();
  skity::Vec2 text5_bounds = blob5->GetBoundSize();

  float y_offset = y;
  // line 1
  canvas->DrawTextBlob(blob1.get(), x, y_offset, paint);
  // line 2
  y_offset += text1_bounds.y;
  canvas->DrawTextBlob(blob2.get(), x, y_offset, paint);
  // line 3
  y_offset += text2_bounds.y;
  canvas->DrawTextBlob(blob3.get(), x, y_offset, paint);
  // line 4
  y_offset += text3_bounds.y;
  canvas->DrawTextBlob(blob4.get(), x, y_offset, paint);
  // line 5
  y_offset += text4_bounds.y;
  canvas->DrawTextBlob(blob5.get(), x, y_offset, paint);
  // line 6
  y_offset += text5_bounds.y;
  canvas->DrawTextBlob(blob6.get(), x, y_offset, paint);
}
