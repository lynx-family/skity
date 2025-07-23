// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "case/blend/blend_example.hpp"

#include <skity/skity.hpp>

#include "common/app_utils.hpp"

namespace skity::example::blend {

static bool g_debug_vulkan_pipeline_family = false;

static skity::BlendMode g_blend_mode = skity::BlendMode::kDefault;

// same as https://fiddle.skia.org/c/@shapes
static void draw_basic_example(skity::Canvas* canvas) {
  skity::Paint paint;
  paint.SetBlendMode(g_blend_mode);
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
  paint.SetBlendMode(g_blend_mode);
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
  paint.SetBlendMode(g_blend_mode);
  paint.SetStrokeWidth(3.f);
  paint.SetStrokeJoin(skity::Paint::kRound_Join);
  paint.SetStrokeCap(skity::Paint::kRound_Cap);
  paint.SetStrokeColor(0, 0, 1, 1);
  paint.SetFillColor(150.f / 255.f, 150.f / 255.f, 1.f, 1.f);
  paint.SetAntiAlias(true);
  paint.SetStyle(skity::Paint::kStrokeAndFill_Style);
  float pattern[2] = {10.f, 10.f};
  paint.SetPathEffect(skity::PathEffect::MakeDashPathEffect(pattern, 2, 0));

  canvas->DrawPath(path, paint);
}

// same as https://fiddle.skia.org/c/@text_rendering
static void draw_simple_text(skity::Canvas* canvas) {
  skity::Paint paint;
  paint.SetBlendMode(g_blend_mode);

  paint.SetTextSize(64.f);
  paint.SetAntiAlias(true);
  paint.SetFillColor(0x42 / 255.f, 0x85 / 255.f, 0xF4 / 255.f, 1.f);
  paint.SetStyle(skity::Paint::kFill_Style);

  skity::TextBlobBuilder builder;
  paint.SetTypeface(skity::Typeface::GetDefaultTypeface());
  auto blob = builder.BuildTextBlob("Skity", paint);

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

// same as https://fiddle.skia.org/c/844ab7d5e63876f6c889b33662ece8d5
static void draw_linear_gradient_example(skity::Canvas* canvas) {
  skity::Paint p;
  p.SetBlendMode(g_blend_mode);
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
      matrix = skity::Matrix::RotateDeg(45, skity::Vec2(blockX, blockY));
    }
    auto lgs = skity::Shader::MakeLinear(pts.data(), colors, positions, 3,
                                         skity::TileMode::kClamp, flag);
    lgs->SetLocalMatrix(matrix);
    p.SetShader(lgs);
    auto r = skity::Rect::MakeLTRB(blockX, blockY, blockX + 100, blockY + 100);
    canvas->DrawRect(r, p);
  }
  skity::Path circle;
  circle.AddCircle(220, 300, 100);
  skity::Paint paint;
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetAntiAlias(true);
  skity::Point center{220, 300, 0, 1};
  skity::Vec4 radialColors[] = {skity::Vec4{1.f, 1.f, 1.f, 1.f},
                                skity::Vec4{0.f, 0.f, 0.f, 1.f}};
  auto rgs = skity::Shader::MakeRadial(center, 150.f, radialColors, nullptr, 2);
  paint.SetShader(rgs);

  canvas->DrawPath(circle, paint);
}

// same as https://fiddle.skia.org/c/@Canvas_clipRect
static void draw_clip_example(skity::Canvas* canvas) {
  canvas->Rotate(10);
  skity::Paint paint;
  paint.SetBlendMode(g_blend_mode);
  paint.SetAntiAlias(true);

  canvas->Save();
  canvas->ClipRect(skity::Rect::MakeWH(90, 80),
                   skity::Canvas::ClipOp::kIntersect);
  canvas->DrawCircle(100, 60, 60, paint);
  canvas->Restore();
}

void draw_clip_difference(skity::Canvas* canvas) {
  canvas->Save();
  skity::Rect clip_inner_rect = skity::Rect::MakeXYWH(200, 200, 50, 50);
  skity::Rect clip_outer_rect = skity::Rect::MakeXYWH(100, 100, 200, 200);
  canvas->ClipRect(clip_outer_rect);
  canvas->ClipRect(clip_inner_rect, skity::Canvas::ClipOp::kDifference);

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
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetColor(skity::Color_BLUE);
  paint.SetBlendMode(g_blend_mode);

  canvas->DrawPath(path, paint);
  canvas->Restore();
}

static void draw_blend_image(skity::Canvas* canvas,
                             const std::shared_ptr<skity::Pixmap>& dst_img,
                             const std::shared_ptr<skity::Pixmap>& src_img,
                             skity::Rect rect, skity::BlendMode mode) {
  skity::Paint paint;
  paint.SetBlendMode(skity::BlendMode::kSrc);
  canvas->DrawImage(skity::Image::MakeImage(dst_img), rect, &paint);
  paint.SetBlendMode(mode);
  canvas->DrawImage(skity::Image::MakeImage(src_img), rect, &paint);
  paint.SetBlendMode(skity::BlendMode::kSrc);
  paint.SetStyle(skity::Paint::kStroke_Style);
  paint.SetColor(skity::Color_GREEN);
  canvas->DrawRect(rect, paint);
}

static void set_linear_shader_paint(skity::Paint& p) {
  skity::Vec4 colors[] = {
      skity::Vec4{0.f, 1.f, 1.f, 1.f},
      skity::Vec4{0.f, 0.f, 1.f, 1.f},
      skity::Vec4{1.f, 0.f, 0.f, 1.f},
  };
  std::vector<skity::Point> pts = {
      skity::Point{0.f, 0.f, 0.f, 1.f},
      skity::Point{256.f, 0.f, 0.f, 1.f},
  };
  p.SetShader(skity::Shader::MakeLinear(pts.data(), colors, nullptr, 3));
}

static void draw_pipeline_family(
    skity::Canvas* canvas,
    skity::BlendMode blend_mode = skity::BlendMode::kDefault,
    bool shader = false, bool clip = false, bool path = false,
    bool image = false, bool round_rect = false) {
  canvas->Save();
  constexpr float wh = 256;
  skity::Paint dst_paint;
  dst_paint.SetBlendMode(skity::BlendMode::kSrc);
  dst_paint.SetColor(skity::ColorSetARGB(0xFF, 0xFF, 0x00, 0x00));
  canvas->DrawRect(skity::Rect::MakeXYWH(0, 0, wh, wh), dst_paint);

  skity::Paint src_paint;
  if (shader) {
    set_linear_shader_paint(src_paint);
  } else {
    src_paint.SetColor(skity::ColorSetARGB(0x80, 0x00, 0x00, 0xFF));
  }
  src_paint.SetBlendMode(blend_mode);
  if (clip) {
    canvas->ClipRect(
        skity::Rect::MakeXYWH(wh * 0.25, wh * 0.25, wh * 0.5, wh * 0.5),
        skity::Canvas::ClipOp::kIntersect);
  }
  if (path) {
    skity::Path dpath;
    dpath.MoveTo(wh * 0.5, wh * 0.125);
    dpath.LineTo(wh * 0.125, wh * 0.875);
    dpath.LineTo(wh * 0.875, wh * 0.875);
    dpath.Close();
    canvas->DrawPath(dpath, src_paint);
  } else if (round_rect) {
    src_paint.SetStrokeColor(0, 1, 0, 1);
    src_paint.SetStyle(skity::Paint::kStroke_Style);
    canvas->DrawRoundRect(
        skity::Rect::MakeXYWH(wh * 0.25, wh * 0.25, wh * 0.5, wh * 0.5),
        wh * 0.05, wh * 0.05, src_paint);
  } else if (image) {
    auto dst_img = make_rect_image(wh * 3 / 4, skity::Color_TRANSPARENT, wh / 2,
                                   skity::Color_GREEN, 0);
    auto src_img = make_rect_image(wh * 3 / 4, skity::Color_TRANSPARENT, wh / 2,
                                   skity::Color_BLUE, wh / 4);
    canvas->DrawImage(Image::MakeImage(dst_img),
                      skity::Rect::MakeXYWH(wh / 8, wh / 8, dst_img->Width(),
                                            dst_img->Height()),
                      &dst_paint);
    canvas->DrawImage(Image::MakeImage(src_img),
                      skity::Rect::MakeXYWH(wh / 8, wh / 8, src_img->Width(),
                                            src_img->Height()),
                      &src_paint);
  } else {
    canvas->DrawRect(
        skity::Rect::MakeXYWH(wh * 0.25, wh * 0.25, wh * 0.5, wh * 0.5),
        src_paint);
  }
  canvas->Restore();
}

static void draw_blend_rects(skity::Canvas* canvas, skity::Color dst_color,
                             skity::Color src_color, float x, float y, float wh,
                             skity::BlendMode mode) {
  float in_size = wh * 3 / 4;
  float in_off = wh / 4;
  // in_size = wh * 2 / 3; in_off = wh / 3; // TOFIX when wh = 80
  skity::Paint paint;
  paint.SetColor(dst_color);
  paint.SetBlendMode(skity::BlendMode::kSrc);
  canvas->DrawRect(skity::Rect::MakeXYWH(x, y, in_size, in_size), paint);
  paint.SetColor(src_color);
  paint.SetBlendMode(mode);
  canvas->DrawRect(
      skity::Rect::MakeXYWH(x + in_off, y + in_off, in_size, in_size), paint);
  paint.SetStyle(skity::Paint::kStroke_Style);
  paint.SetColor(skity::Color_GREEN);
  paint.SetBlendMode(skity::BlendMode::kSrc);
  canvas->DrawRect(skity::Rect::MakeXYWH(x, y, wh, wh), paint);
}

static void draw_blend_grid(
    skity::Canvas* canvas, uint32_t x, uint32_t y, bool img, uint8_t alpha,
    uint32_t tile_size,
    const std::vector<std::vector<skity::BlendMode>>& modes) {
  const auto dst_color = skity::ColorSetARGB(alpha, 0xC0, 0x00, 0x00);
  const auto src_color = skity::ColorSetARGB(alpha, 0x00, 0x00, 0xC0);
  std::shared_ptr<skity::Pixmap> dst_img = make_rect_image(
      tile_size, skity::Color_TRANSPARENT, 2.f * tile_size / 3, dst_color, 0);
  std::shared_ptr<skity::Pixmap> src_img =
      make_rect_image(tile_size, skity::Color_TRANSPARENT, 2.f * tile_size / 3,
                      src_color, tile_size / 3.f);

  for (uint32_t j = 0; j < modes.size(); ++j) {
    for (uint32_t i = 0; i < modes[j].size(); ++i) {
      if (img) {
        draw_blend_image(
            canvas, dst_img, src_img,
            skity::Rect::MakeXYWH(x + i * tile_size, y + j * tile_size,
                                  tile_size, tile_size),
            modes[j][i]);
      } else {
        draw_blend_rects(canvas, dst_color, src_color, x + i * tile_size,
                         y + j * tile_size, tile_size, modes[j][i]);
      }
    }
  }
}

static void draw_blend_example(skity::Canvas* canvas) {
  std::vector<std::vector<skity::BlendMode>> modes = {
      {skity::BlendMode::kSrc, skity::BlendMode::kDst,
       skity::BlendMode::kSrcOver, skity::BlendMode::kDstOver},
      {skity::BlendMode::kSrcIn, skity::BlendMode::kDstIn,
       skity::BlendMode::kSrcOut, skity::BlendMode::kDstOut},
      {skity::BlendMode::kSrcATop, skity::BlendMode::kDstATop,
       skity::BlendMode::kXor, skity::BlendMode::kClear},
      {skity::BlendMode::kPlus, skity::BlendMode::kModulate,
       skity::BlendMode::kScreen}};
  constexpr uint32_t tile_size = 80;
  uint32_t stride_h = (tile_size + 1) * (modes.empty() ? 1 : modes[0].size());
  uint32_t stride_v = (tile_size + 1) * modes.size();
  canvas->DrawColor(skity::Color_TRANSPARENT, skity::BlendMode::kSrc);
  draw_blend_grid(canvas, 0, 0, false, 0xFF, tile_size, modes);
  draw_blend_grid(canvas, stride_h, 0, false, 0x80, tile_size, modes);
  draw_blend_grid(canvas, 0, stride_v, true, 0xFF, tile_size, modes);
  draw_blend_grid(canvas, stride_h, stride_v, true, 0x80, tile_size, modes);
}

static void draw_examples(skity::Canvas* canvas, skity::BlendMode blend_mode) {
  g_blend_mode = blend_mode;
  canvas->DrawColor(skity::ColorSetARGB(0x80, 0x80, 0x00, 0x00),
                    skity::BlendMode::kSrc);
  draw_basic_example(canvas);
  canvas->Save();
  canvas->Translate(300, 0);
  draw_path_effect_example(canvas);
  canvas->Restore();
  canvas->Save();
  canvas->Translate(0, 180);
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
  canvas->Translate(0, 460);
  draw_clip_difference(canvas);
  canvas->Restore();
  canvas->Save();
  canvas->Translate(0, 600);
  draw_clip_example(canvas);
  canvas->Restore();
}

static const char* blend_mode_name(skity::BlendMode bm) {
  switch (bm) {
    case skity::BlendMode::kClear:
      return "Clear";
    case skity::BlendMode::kSrc:
      return "Src";
    case skity::BlendMode::kDst:
      return "Dst";
    case skity::BlendMode::kSrcOver:
      return "SrcOver";
    case skity::BlendMode::kDstOver:
      return "DstOver";
    case skity::BlendMode::kSrcIn:
      return "SrcIn";
    case skity::BlendMode::kDstIn:
      return "DstIn";
    case skity::BlendMode::kSrcOut:
      return "SrcOut";
    case skity::BlendMode::kDstOut:
      return "DstOut";
    case skity::BlendMode::kSrcATop:
      return "SrcATop";
    case skity::BlendMode::kDstATop:
      return "DstATop";
    case skity::BlendMode::kXor:
      return "Xor";
    case skity::BlendMode::kPlus:
      return "Plus";
    case skity::BlendMode::kModulate:
      return "Modulate";
    case skity::BlendMode::kScreen:
      return "Screen";

    case skity::BlendMode::kOverlay:
      return "Overlay";
    case skity::BlendMode::kDarken:
      return "Darken";
    case skity::BlendMode::kLighten:
      return "Lighten";
    case skity::BlendMode::kColorDodge:
      return "ColorDodge";
    case skity::BlendMode::kColorBurn:
      return "ColorBurn";
    case skity::BlendMode::kHardLight:
      return "HardLight";
    case skity::BlendMode::kSoftLight:
      return "SoftLight";
    case skity::BlendMode::kDifference:
      return "Difference";
    case skity::BlendMode::kExclusion:
      return "Exclusion";
    case skity::BlendMode::kMultiply:
      return "Multiply";

    case skity::BlendMode::kHue:
      return "Hue";
    case skity::BlendMode::kSaturation:
      return "Saturation";
    case skity::BlendMode::kColor:
      return "Color";
    case skity::BlendMode::kLuminosity:
      return "Luminosity";
    default:
      return "Unknown";
  }
}

static std::vector<skity::BlendMode> g_modes = {
    skity::BlendMode::kClear,    skity::BlendMode::kClear,
    skity::BlendMode::kSrc,      skity::BlendMode::kDst,
    skity::BlendMode::kSrcOver,  skity::BlendMode::kDstOver,
    skity::BlendMode::kSrcIn,    skity::BlendMode::kDstIn,
    skity::BlendMode::kSrcOut,   skity::BlendMode::kDstOut,
    skity::BlendMode::kSrcATop,  skity::BlendMode::kDstATop,
    skity::BlendMode::kXor,      skity::BlendMode::kPlus,
    skity::BlendMode::kModulate, skity::BlendMode::kScreen};

const char* draw_blend_case(skity::Canvas* canvas, uint32_t index) {
  if (g_debug_vulkan_pipeline_family) {
    draw_pipeline_family(canvas, skity::BlendMode::kDefault, false, false,
                         false, false, false);
    return "";
  }
  // index = 0;
  if (index == 0 || index >= g_modes.size()) {
    draw_blend_example(canvas);
    return "";
  }
  draw_examples(canvas, g_modes[index]);

  return blend_mode_name(g_modes[index]);
}

uint32_t get_blend_case_count() {
  return 1;
  // return g_modes.size();
}
}  // namespace skity::example::blend
