// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "harness/web/cases.hpp"

#include <skity/skity.hpp>

namespace skity {
namespace web_harness {

namespace {

// Draw logic extracted from SimpleShapeGolden.DrawFilledRect
// (test/golden/cases/simple_shape/simple_shape.cc).
void DrawFilledRect(Canvas* canvas) {
  Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(Color_GREEN);

  canvas->Save();
  canvas->Translate(50.f, 50.f);
  canvas->DrawRect(Rect::MakeWH(50, 50), paint);

  canvas->Translate(100.3f, 50.f);
  canvas->DrawRect(Rect::MakeWH(50, 50), paint);
  canvas->Restore();
}

// Draw logic extracted from SimpleShapeGolden.DrawStrokeRect.
void DrawStrokeRect(Canvas* canvas) {
  Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(Color_GREEN);
  paint.SetStrokeWidth(1);
  paint.SetStyle(Paint::kStroke_Style);
  canvas->Save();
  canvas->Translate(3.f, 50.f);
  canvas->DrawRect(Rect::MakeWH(50, 50), paint);

  canvas->Translate(100, 0);
  paint.SetStrokeWidth(20);
  canvas->DrawRect(Rect::MakeWH(50, 50), paint);

  canvas->Translate(100, 0);
  paint.SetStrokeWidth(49);
  canvas->DrawRect(Rect::MakeWH(50, 50), paint);

  canvas->Translate(120, 0);
  paint.SetStrokeWidth(50);
  canvas->DrawRect(Rect::MakeWH(50, 50), paint);
  canvas->Restore();
}

// Draw logic extracted from SimpleShapeGolden.DrawFilledRRect.
void DrawFilledRRect(Canvas* canvas) {
  Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(Color_GREEN);
  canvas->Save();

  canvas->Translate(3.f, 50.f);
  canvas->DrawRRect(RRect::MakeRectXY(Rect::MakeLTRB(0, 0, 100, 150), 20, 20),
                    paint);

  canvas->Translate(110, 0);
  canvas->DrawRRect(RRect::MakeRectXY(Rect::MakeLTRB(0, 0, 100, 150), 20, 30),
                    paint);

  canvas->Translate(110, 0);
  canvas->DrawRRect(RRect::MakeRectXY(Rect::MakeLTRB(0, 0, 100, 150), 50, 75),
                    paint);

  canvas->Restore();
}

// Path helper extracted from ShapeGolden.CatHeadPath
// (test/golden/cases/shape/shape.cc).
Path MakeCatHeadPath() {
  Path path;

  path.MoveTo(-16.539f, -28.104f);
  path.CubicTo(-17.643f, -28.136f, -19.440f, -25.925f, -20.838f, -23.154f);
  path.CubicTo(-23.157f, -18.556f, -24.958f, -12.417f, -24.958f, -12.417f);
  path.LineTo(-25.989f, -8.666f);
  path.CubicTo(-25.989f, -8.666f, -29.171f, -3.228f, -29.669f, 2.129f);
  path.CubicTo(-30.198f, 7.815f, -29.179f, 14.140f, -23.104f, 19.063f);
  path.CubicTo(-19.060f, 22.340f, -15.698f, 24.053f, -12.331f, 24.900f);
  path.CubicTo(-10.336f, 25.402f, -5.067f, 26.521f, 0.176f, 26.520f);
  path.CubicTo(5.541f, 26.519f, 10.549f, 25.387f, 12.295f, 24.923f);
  path.CubicTo(15.501f, 24.071f, 18.384f, 23.172f, 22.896f, 19.708f);
  path.CubicTo(28.624f, 15.310f, 30.589f, 8.891f, 29.999f, 3.034f);
  path.CubicTo(29.410f, -2.811f, 26.000f, -8.681f, 26.000f, -8.681f);
  path.CubicTo(26.000f, -8.681f, 25.538f, -10.640f, 25.313f, -11.417f);
  path.CubicTo(24.568f, -13.992f, 22.830f, -19.497f, 21.003f, -23.073f);
  path.CubicTo(19.513f, -25.990f, 17.713f, -28.039f, 16.292f, -28.167f);
  path.CubicTo(14.086f, -28.365f, 7.708f, -21.062f, 7.708f, -21.062f);
  path.CubicTo(7.708f, -21.062f, 3.205f, -22.021f, -0.146f, -22.021f);
  path.CubicTo(-4.086f, -22.021f, -7.563f, -21.104f, -7.563f, -21.104f);
  path.CubicTo(-7.563f, -21.104f, -13.170f, -28.007f, -16.539f, -28.104f);
  path.Close();
  return path;
}

// Draw logic extracted from ShapeGolden.CatHeadPath.
void DrawCatHeadPath(Canvas* canvas) {
  Paint background;
  background.SetColor(ColorSetARGB(0xFF, 0x18, 0x18, 0x18));
  canvas->DrawPaint(background);

  Paint paint;
  paint.SetAntiAlias(true);
  paint.SetStyle(Paint::kStroke_Style);
  paint.SetStrokeCap(Paint::kRound_Cap);
  paint.SetStrokeJoin(Paint::kMiter_Join);
  paint.SetStrokeWidth(3.5f);
  paint.SetColor(ColorSetARGB(0x4D, 0xFF, 0xFF, 0xFF));

  auto cat_head = MakeCatHeadPath();

  canvas->Save();
  canvas->Translate(200.f, 200.f);
  canvas->Scale(4.f, 4.f);
  canvas->DrawPath(cat_head, paint);
  canvas->Restore();
}

// Path helper extracted from ClipGolden cases
// (test/golden/cases/clip/clip.cc).
Path MakeStarPath() {
  Path path;
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

  return path;
}

// Draw logic extracted from ClipGolden.ClipPath.
void DrawClipPath(Canvas* canvas) {
  auto path = MakeStarPath();

  Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(Color_GREEN);
  paint.SetStyle(Paint::Style::kStroke_Style);
  paint.SetStrokeWidth(1.f);

  canvas->DrawPath(path, paint);

  Path clip_path;

  clip_path.MoveTo(10.f, 10.f);
  clip_path.QuadTo(300.f, 10.f, 150.f, 150.f);
  clip_path.QuadTo(10.f, 300.f, 300.f, 300.f);
  clip_path.Close();
  paint.SetColor(Color_RED);

  canvas->DrawPath(clip_path, paint);

  canvas->ClipPath(clip_path);

  paint.SetColor(Color_BLUE);
  paint.SetStyle(Paint::Style::kFill_Style);
  canvas->DrawPath(path, paint);
}

// Draw logic extracted from GradientGolden.LinearGradientTileMode
// (test/golden/cases/gradient/gradient.cc). The golden case records a 300x300
// picture and compares a 500x500 surface, so the case size is 500x500.
void DrawLinearGradientTileMode(Canvas* canvas) {
  canvas->Save();
  canvas->Translate(50, 50);

  Paint paint;
  paint.SetAntiAlias(true);
  Vec3 offsets[] = {{0, 0, 0}, {0, 150, 0}, {150, 0, 0}, {150, 150, 0}};
  TileMode tile_modes[] = {TileMode::kClamp, TileMode::kRepeat,
                           TileMode::kMirror, TileMode::kDecal};
  for (int i = 0; i < 4; i++) {
    canvas->Save();
    canvas->Translate(offsets[i].x, offsets[i].y);
    Vec4 gradient_colors[] = {Vec4{0.9019f, 0.3921f, 0.3960f, 1.0f},
                              Vec4{0.5686f, 0.5960f, 0.8980f, 1.0f}};
    float gradient_positions[] = {0.f, 1.f};
    std::vector<Point> gradient_points = {
        Point{0.f, 0.f, 0.f, 1.f},
        Point{50.f, 50.f, 0.f, 1.f},
    };
    auto lgs = Shader::MakeLinear(gradient_points.data(), gradient_colors,
                                  gradient_positions, 2, tile_modes[i]);
    paint.SetShader(lgs);
    canvas->DrawRect({0, 0, 100, 100}, paint);
    canvas->Restore();
  }

  canvas->Restore();
}

// Draw logic extracted from SaveLayerGolden.TwoCircle
// (test/golden/cases/save_layer/save_layer.cc).
void DrawSaveLayerTwoCircle(Canvas* canvas) {
  canvas->Save();
  Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(Color_GREEN);
  canvas->Scale(10, 10);
  canvas->DrawCircle(20, 20, 10, paint);

  canvas->SaveLayer(Rect::MakeLTRB(0, 0, 40, 40), Paint{});
  paint.SetColor(Color_RED);
  canvas->DrawCircle(20, 20, 10, paint);
  canvas->Restore();
  canvas->Restore();
}

// Draw logic extracted from SaveLayerGolden.ThreeCircle: two nested layers.
void DrawSaveLayerThreeCircle(Canvas* canvas) {
  canvas->Save();
  Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(Color_GREEN);
  canvas->Scale(10.1, 10.1);
  canvas->DrawCircle(20.3, 20.3, 10, paint);

  canvas->SaveLayer(Rect::MakeLTRB(10.3, 10.3, 30.3, 30.3), Paint{});
  paint.SetColor(Color_RED);
  canvas->DrawCircle(20.3, 20.3, 10, paint);
  canvas->SaveLayer(Rect::MakeLTRB(10.3, 10.3, 30.3, 30.3), Paint{});
  paint.SetColor(Color_BLUE);
  canvas->DrawCircle(20.3, 20.3, 10, paint);

  canvas->Restore();
  canvas->Restore();
  canvas->Restore();
}

// Draw logic extracted from SaveLayerGolden.TwoCircleWithTranslate: the layer
// is restored through a MatrixTransform image filter.
void DrawSaveLayerTwoCircleWithTranslate(Canvas* canvas) {
  canvas->Save();
  Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(Color_GREEN);
  canvas->Scale(10, 10);
  canvas->DrawCircle(20, 20, 10, paint);
  Paint restore_paint;
  restore_paint.SetImageFilter(
      ImageFilters::MatrixTransform(Matrix::Translate(5, 0)));
  canvas->SaveLayer(Rect::MakeLTRB(0, 0, 400, 400), restore_paint);
  paint.SetColor(Color_RED);
  canvas->DrawCircle(20, 20, 10, paint);
  canvas->Restore();
  canvas->Restore();
}

// New case (no golden counterpart): guards the texture upload path, which
// no other case exercises. Two procedurally built solid images (no codec,
// no image files) drawn through DrawImageRect; a broken upload renders
// them as nothing (the stage buffer lands zeroed).
void DrawImageRect(Canvas* canvas) {
  auto make_solid_image = [](uint32_t w, uint32_t h, Color color) {
    auto pixmap = std::make_shared<Pixmap>(w, h);
    Bitmap bitmap(pixmap, false);
    for (uint32_t y = 0; y < h; y++) {
      for (uint32_t x = 0; x < w; x++) {
        bitmap.SetPixel(x, y, color);
      }
    }
    return Image::MakeImage(pixmap);
  };

  canvas->Clear(Color_WHITE);
  auto red = make_solid_image(32, 64, ColorSetARGB(255, 200, 40, 20));
  canvas->DrawImageRect(red, Rect::MakeWH(32.f, 64.f),
                        Rect::MakeXYWH(0.f, 0.f, 32.f, 64.f),
                        SamplingOptions{});
  auto blue = make_solid_image(32, 64, ColorSetARGB(255, 20, 80, 200));
  canvas->DrawImageRect(blue, Rect::MakeWH(32.f, 64.f),
                        Rect::MakeXYWH(32.f, 0.f, 32.f, 64.f),
                        SamplingOptions{});
}

void DrawColorSpaceDecode(Canvas* canvas) {
  auto make_solid_image = [](uint32_t w, uint32_t h, Color4f color,
                             std::shared_ptr<ColorSpace> color_space) {
    auto pixmap = std::make_shared<Pixmap>(w, h);
    Bitmap bitmap(pixmap, false);
    auto stored = Color4fToColor(color);
    for (uint32_t y = 0; y < h; y++) {
      for (uint32_t x = 0; x < w; x++) {
        bitmap.SetPixel(x, y, stored);
      }
    }
    pixmap->SetColorInfo(kUnpremul_AlphaType, ColorType::kRGBA, color_space);
    return Image::MakeImage(pixmap);
  };

  canvas->Clear(Color_WHITE);

  const Color4f p3_source{0.9f, 0.5f, 0.1f, 1.f};
  auto p3_image =
      make_solid_image(32, 64,
                       ColorSpace::Convert(ColorSpace::SRGB(), p3_source,
                                           ColorSpace::DisplayP3()),
                       ColorSpace::DisplayP3());
  canvas->DrawImageRect(p3_image, Rect::MakeWH(32.f, 64.f),
                        Rect::MakeXYWH(0.f, 0.f, 32.f, 64.f),
                        SamplingOptions{});

  const Color4f linear_source{0.25f, 0.5f, 0.75f, 1.f};
  auto linear_image =
      make_solid_image(32, 64,
                       ColorSpace::Convert(ColorSpace::SRGB(), linear_source,
                                           ColorSpace::SRGBLinear()),
                       ColorSpace::SRGBLinear());
  canvas->DrawImageRect(linear_image, Rect::MakeWH(32.f, 64.f),
                        Rect::MakeXYWH(32.f, 0.f, 32.f, 64.f),
                        SamplingOptions{});
}

const GeometryCase kGeometryCases[] = {
    {"DrawFilledRect", 400, 400, &DrawFilledRect},
    {"DrawStrokeRect", 400, 400, &DrawStrokeRect},
    {"DrawFilledRRect", 400, 400, &DrawFilledRRect},
    {"CatHeadPath", 400, 400, &DrawCatHeadPath},
    {"ClipPath", 400, 400, &DrawClipPath},
    {"LinearGradientTileMode", 500, 500, &DrawLinearGradientTileMode},
    {"SaveLayerTwoCircle", 400, 400, &DrawSaveLayerTwoCircle},
    {"SaveLayerThreeCircle", 400, 400, &DrawSaveLayerThreeCircle},
    {"SaveLayerTwoCircleWithTranslate", 400, 400,
     &DrawSaveLayerTwoCircleWithTranslate},
    {"DrawImageRect", 64, 64, &DrawImageRect},
    {"ColorSpaceDecode", 64, 64, &DrawColorSpaceDecode},
};

}  // namespace

const GeometryCase* GetGeometryCases() { return kGeometryCases; }

uint32_t GetCaseCount() {
  return sizeof(kGeometryCases) / sizeof(kGeometryCases[0]);
}

}  // namespace web_harness
}  // namespace skity
