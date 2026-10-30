// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <skity/graphic/bitmap.hpp>
#include <skity/graphic/color.hpp>
#include <skity/graphic/paint.hpp>
#include <skity/render/canvas.hpp>

#include "src/graphic/path_priv.hpp"
#include "src/render/sw/sw_edge.hpp"

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

TEST(SWCanvas, PathEdgesCloseEachContourAndSkipEmptyEdges) {
  skity::SWEdgeBuilder empty;
  skity::Path path;
  const auto bounds = skity::Rect::MakeWH(64.f, 64.f);
  EXPECT_EQ(empty.BuildEdges(path, bounds), 0);
  path.MoveTo(1, 1).Close().MoveTo(2, 2);
  EXPECT_EQ(empty.BuildEdges(path, bounds), 0);

  for (bool explicit_close : {false, true}) {
    SCOPED_TRACE(explicit_close);
    path.Reset();
    path.MoveTo(4, 4).LineTo(12, 16).LineTo(20, 8);
    if (explicit_close) {
      path.Close();
    }
    // The quad splits at its y extremum; its horizontal closing edge is
    // skipped.
    path.MoveTo(24, 4).QuadTo(32, 28, 40, 4);
    if (explicit_close) {
      path.Close();
    }
    // Already closed geometry must not produce an extra zero-length edge.
    path.MoveTo(44, 4).LineTo(52, 16).LineTo(44, 4);
    if (explicit_close) {
      path.Close();
    }
    path.MoveTo(60, 60);
    skity::SWEdgeBuilder builder;
    ASSERT_EQ(builder.BuildEdges(path, bounds), 7);
    const auto& edges = builder.GetEdges();
    EXPECT_EQ(edges[0]->winding, 1);
    EXPECT_EQ(edges[1]->winding, -1);
    EXPECT_EQ(edges[2]->winding, -1);
    EXPECT_EQ(edges[3]->winding, 1);
    EXPECT_EQ(edges[4]->winding, -1);
    EXPECT_EQ(edges[5]->winding, 1);
    EXPECT_EQ(edges[6]->winding, -1);
  }
}

TEST(PathEdgeIter, PlanarReadsPreserveClosingEdgesAndContourBoundaries) {
  skity::Path path;
  path.MoveTo(1, 2).ConicTo(3, 4, 5, 6, 0.5f);
  path.MoveTo(10, 20).LineTo(10, 20).Close();
  skity::PathEdgeIter iter(path);

  auto edge = iter.next();
  ASSERT_TRUE(static_cast<bool>(edge));
  EXPECT_EQ(edge.edge, skity::PathEdgeIter::Edge::kConic);
  EXPECT_TRUE(edge.is_new_contour);
  EXPECT_EQ(edge.GetPoint2D(0), (skity::Vec2{1, 2}));
  EXPECT_EQ(edge.GetPoint2D(1), (skity::Vec2{3, 4}));
  EXPECT_EQ(edge.GetPoint2D(2), (skity::Vec2{5, 6}));
  EXPECT_EQ(iter.conicWeight(), 0.5f);

  edge = iter.next();
  ASSERT_TRUE(static_cast<bool>(edge));
  EXPECT_EQ(edge.edge, skity::PathEdgeIter::Edge::kLine);
  EXPECT_FALSE(edge.is_new_contour);
  EXPECT_EQ(edge.GetPoint2D(0), (skity::Vec2{5, 6}));
  EXPECT_EQ(edge.GetPoint2D(1), (skity::Vec2{1, 2}));

  edge = iter.next();
  ASSERT_TRUE(static_cast<bool>(edge));
  EXPECT_TRUE(edge.is_new_contour);
  EXPECT_EQ(edge.GetPoint2D(0), (skity::Vec2{10, 20}));
  EXPECT_EQ(edge.GetPoint2D(1), (skity::Vec2{10, 20}));
  edge = iter.next();
  ASSERT_TRUE(static_cast<bool>(edge));
  EXPECT_FALSE(edge.is_new_contour);
  EXPECT_EQ(edge.GetPoint2D(0), (skity::Vec2{10, 20}));
  EXPECT_EQ(edge.GetPoint2D(1), (skity::Vec2{10, 20}));
  EXPECT_FALSE(iter.next());
}
