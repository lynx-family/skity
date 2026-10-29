// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <skity/graphic/bitmap.hpp>
#include <skity/graphic/color.hpp>
#include <skity/graphic/paint.hpp>
#include <skity/render/canvas.hpp>

namespace {

void DrawRotatedSaveLayerScene(skity::Bitmap* bitmap,
                               const skity::Rect& layer_bounds) {
  auto canvas = skity::Canvas::MakeSoftwareCanvas(bitmap);
  ASSERT_TRUE(canvas);

  skity::Paint paint;
  paint.SetColor(skity::Color_RED);

  canvas->Translate(16.f, 16.f);
  canvas->Rotate(30.f);
  canvas->Translate(-16.f, -16.f);

  canvas->SaveLayer(layer_bounds, skity::Paint{});
  canvas->DrawRect(skity::Rect::MakeXYWH(8.f, 8.f, 16.f, 16.f), paint);
  canvas->Restore();
}

}  // namespace

TEST(SWCanvas, SaveLayerClipsHugeBoundsToDevice) {
  skity::Bitmap bitmap(16, 16, skity::AlphaType::kPremul_AlphaType,
                       skity::ColorType::kRGBA);
  auto canvas = skity::Canvas::MakeSoftwareCanvas(&bitmap);
  ASSERT_TRUE(canvas);

  skity::Paint paint;
  paint.SetColor(skity::Color_RED);

  canvas->SaveLayer(skity::kMaxCullRect, skity::Paint{});
  canvas->DrawRect(skity::Rect::MakeXYWH(4.f, 4.f, 4.f, 4.f), paint);
  canvas->Restore();

  EXPECT_EQ(bitmap.GetPixel(5, 5), skity::Color_RED);
  EXPECT_EQ(bitmap.GetPixel(0, 0), skity::Color_TRANSPARENT);
}

TEST(SWCanvas, SaveLayerHugeBoundsPreservesRotatedDraw) {
  skity::Bitmap bounded(32, 32, skity::AlphaType::kPremul_AlphaType,
                        skity::ColorType::kRGBA);
  skity::Bitmap huge(32, 32, skity::AlphaType::kPremul_AlphaType,
                     skity::ColorType::kRGBA);

  DrawRotatedSaveLayerScene(&bounded, skity::Rect::MakeWH(32.f, 32.f));
  DrawRotatedSaveLayerScene(&huge, skity::kMaxCullRect);

  for (uint32_t y = 0; y < bounded.Height(); ++y) {
    for (uint32_t x = 0; x < bounded.Width(); ++x) {
      EXPECT_EQ(huge.GetPixel(x, y), bounded.GetPixel(x, y))
          << "x=" << x << " y=" << y;
    }
  }
}

TEST(SWCanvas, SetAndResetMatrixInsideSaveLayerUseGlobalMatrix) {
  skity::Bitmap bitmap(64, 64, skity::AlphaType::kPremul_AlphaType,
                       skity::ColorType::kRGBA);
  auto canvas = skity::Canvas::MakeSoftwareCanvas(&bitmap);
  ASSERT_TRUE(canvas);

  canvas->Translate(20.f, 0.f);
  canvas->SaveLayer(skity::kMaxCullRect, skity::Paint{});

  skity::Paint red;
  red.SetColor(skity::Color_RED);
  canvas->SetMatrix(skity::Matrix::Translate(5.f, 0.f));
  canvas->DrawRect(skity::Rect::MakeXYWH(0.f, 5.f, 4.f, 4.f), red);

  skity::Paint blue;
  blue.SetColor(skity::Color_BLUE);
  canvas->ResetMatrix();
  canvas->DrawRect(skity::Rect::MakeXYWH(0.f, 20.f, 4.f, 4.f), blue);
  canvas->Restore();

  EXPECT_EQ(bitmap.GetPixel(6, 6), skity::Color_RED);
  EXPECT_EQ(bitmap.GetPixel(26, 6), skity::Color_TRANSPARENT);
  EXPECT_EQ(bitmap.GetPixel(1, 21), skity::Color_BLUE);
  EXPECT_EQ(bitmap.GetPixel(21, 21), skity::Color_TRANSPARENT);
}

TEST(SWCanvas, ResetMatrixInsideSmallScaleLayerUsesGlobalMatrix) {
  skity::Bitmap bitmap(128, 128, skity::AlphaType::kPremul_AlphaType,
                       skity::ColorType::kRGBA);
  auto canvas = skity::Canvas::MakeSoftwareCanvas(&bitmap);
  ASSERT_TRUE(canvas);

  canvas->Scale(0.01f, 0.01f);
  canvas->Skew(0.25f, 0.f);
  canvas->SaveLayer(skity::Rect::MakeWH(10000.f, 10000.f), skity::Paint{});
  canvas->ResetMatrix();

  skity::Paint red;
  red.SetColor(skity::Color_RED);
  canvas->DrawRect(skity::Rect::MakeXYWH(20.f, 20.f, 20.f, 20.f), red);
  canvas->Restore();

  EXPECT_EQ(bitmap.GetPixel(30, 30), skity::Color_RED);
}

TEST(SWCanvas, StrokeThenFillDrawsFillAfterStroke) {
  skity::Bitmap bitmap(48, 48, skity::AlphaType::kPremul_AlphaType,
                       skity::ColorType::kRGBA);
  auto canvas = skity::Canvas::MakeSoftwareCanvas(&bitmap);
  ASSERT_TRUE(canvas);

  skity::Paint paint;
  paint.SetStyle(skity::Paint::kStrokeThenFill_Style);
  paint.SetStrokeWidth(10.f);
  paint.SetStrokeColor(skity::Color_RED);
  paint.SetFillColor(skity::Color_WHITE);
  paint.SetAntiAlias(false);

  skity::Path path;
  path.AddRect(skity::Rect::MakeXYWH(10.f, 10.f, 24.f, 24.f));
  canvas->DrawPath(path, paint);

  EXPECT_EQ(bitmap.GetPixel(12, 20), skity::Color_WHITE);
  EXPECT_EQ(bitmap.GetPixel(7, 20), skity::Color_RED);
}

TEST(SWCanvas, PremultipliedSurfaceHonorsChannelOrder) {
  for (auto type : {skity::ColorType::kRGBA, skity::ColorType::kBGRA}) {
    SCOPED_TRACE(static_cast<int>(type));
    skity::Bitmap bitmap(8, 8, skity::AlphaType::kPremul_AlphaType, type);
    auto canvas = skity::Canvas::MakeSoftwareCanvas(&bitmap);
    skity::Paint paint;
    paint.SetColor(skity::ColorSetARGB(255, 36, 105, 222));
    canvas->DrawRect(skity::Rect::MakeWH(8, 8), paint);
    canvas->Flush();
    EXPECT_EQ(bitmap.GetPixel(4, 4), skity::ColorSetARGB(255, 36, 105, 222));
  }
}
