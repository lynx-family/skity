// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <array>
#include <skity/effect/color_filter.hpp>
#include <skity/effect/shader.hpp>
#include <skity/graphic/bitmap.hpp>
#include <skity/graphic/color.hpp>
#include <skity/graphic/image.hpp>
#include <skity/graphic/paint.hpp>
#include <skity/io/data.hpp>
#include <skity/render/canvas.hpp>
#include <skity/text/font.hpp>

#include "src/graphic/path_priv.hpp"
#include "src/render/sw/sw_edge.hpp"
#include "src/render/sw/sw_render_target.hpp"

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

TEST(SWCanvas, DrawGlyphsWithZeroCountLeavesPixelsUnchanged) {
  skity::Bitmap bitmap(1, 1, skity::kPremul_AlphaType);
  auto canvas = skity::Canvas::MakeSoftwareCanvas(&bitmap);
  ASSERT_TRUE(canvas);
  canvas->Clear(skity::Color_BLUE);
  canvas->DrawGlyphs(0, nullptr, nullptr, nullptr, skity::Font{},
                     skity::Paint{});
  EXPECT_EQ(bitmap.GetPixel(0, 0), skity::Color_BLUE);
}

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

TEST(SWCanvas, ScreenKeepsOpaqueDestinationAcrossSpan) {
  skity::Bitmap bitmap(17, 1, skity::kPremul_AlphaType);
  auto canvas = skity::Canvas::MakeSoftwareCanvas(&bitmap);
  canvas->Clear(skity::ColorSetARGB(255, 200, 64, 0));
  skity::Paint paint;
  paint.SetColor(skity::ColorSetARGB(255, 200, 0, 128));
  paint.SetBlendMode(skity::BlendMode::kScreen);
  canvas->DrawPaint(paint);
  for (uint32_t x = 0; x < bitmap.Width(); ++x) {
    EXPECT_EQ(bitmap.GetPixel(x, 0), skity::ColorSetARGB(255, 243, 64, 128));
  }
}

TEST(SWCanvas, DifferenceBlendsSampledSpans) {
  using namespace skity;
  Bitmap source(1, 1, kPremul_AlphaType);
  source.SetPixel(0, 0, ColorSetARGB(128, 128, 0, 0));
  Bitmap bitmap(17, 1, kPremul_AlphaType);
  auto canvas = Canvas::MakeSoftwareCanvas(&bitmap);
  canvas->Clear(ColorSetARGB(128, 0, 0, 255));
  bitmap.SetPixel(16, 0, Color_TRANSPARENT);

  Paint paint;
  paint.SetBlendMode(BlendMode::kDifference);
  canvas->DrawImage(Image::MakeImage(source.GetPixmap()), Rect::MakeWH(17, 1),
                    SamplingOptions{}, &paint);
  for (uint32_t x = 0; x < 16; ++x) {
    const Color color = bitmap.GetPixel(x, 0);
    EXPECT_NEAR(ColorGetA(color), 192, 1);
    EXPECT_NEAR(ColorGetR(color), 128, 1);
    EXPECT_EQ(ColorGetG(color), 0u);
    EXPECT_NEAR(ColorGetB(color), 128, 1);
  }
  EXPECT_EQ(bitmap.GetPixel(16, 0), source.GetPixel(0, 0));
}

TEST(SWCanvas, CoverageInterpolatesClearAndPlusWithDestination) {
  using namespace skity;
  Bitmap bitmap(3, 2, kPremul_AlphaType);
  auto canvas = Canvas::MakeSoftwareCanvas(&bitmap);
  const Rect rect = Rect::MakeLTRB(0.5f, 0.f, 2.f, 2.f);
  canvas->Clear(Color_BLUE);
  Paint paint;
  paint.SetAntiAlias(true);
  paint.SetBlendMode(BlendMode::kClear);
  canvas->DrawRect(rect, paint);
  EXPECT_NEAR(ColorGetA(bitmap.GetPixel(0, 0)), 128, 1);
  EXPECT_NEAR(ColorGetB(bitmap.GetPixel(0, 0)), 128, 1);
  EXPECT_EQ(bitmap.GetPixel(1, 0), Color_TRANSPARENT);
  EXPECT_EQ(bitmap.GetPixel(2, 0), Color_BLUE);

  canvas->Clear(ColorSetRGB(192, 0, 0));
  paint.SetColor(ColorSetRGB(128, 0, 0));
  paint.SetBlendMode(BlendMode::kPlus);
  canvas->DrawRect(rect, paint);
  EXPECT_NEAR(ColorGetR(bitmap.GetPixel(0, 0)), 224, 1);
  EXPECT_EQ(bitmap.GetPixel(1, 0), Color_RED);
  EXPECT_EQ(bitmap.GetPixel(2, 0), ColorSetRGB(192, 0, 0));
}

TEST(SWCanvas, ImageCoverageFollowsPaintOpacityAndColorFilter) {
  using namespace skity;
  Bitmap source(1, 1, kPremul_AlphaType);
  source.SetPixel(0, 0, Color_WHITE);
  Bitmap bitmap(17, 2, kPremul_AlphaType);
  auto canvas = Canvas::MakeSoftwareCanvas(&bitmap);
  const Rect rect = Rect::MakeWH(17, 1.5f);
  Paint paint;
  paint.SetShader(Shader::MakeShader(Image::MakeImage(source.GetPixmap())));
  paint.SetAntiAlias(true);
  paint.SetAlphaF(0.5f);
  canvas->Clear(Color_TRANSPARENT);
  canvas->DrawRect(rect, paint);
  for (uint32_t x = 0; x < bitmap.Width(); ++x) {
    EXPECT_NEAR(ColorGetA(bitmap.GetPixel(x, 0)), 128, 1);
    EXPECT_NEAR(ColorGetA(bitmap.GetPixel(x, 1)), 64, 1);
  }

  paint.SetColorFilter(ColorFilters::Blend(Color_WHITE, BlendMode::kSrc));
  canvas->Clear(Color_TRANSPARENT);
  canvas->DrawRect(rect, paint);
  EXPECT_EQ(bitmap.GetPixel(0, 0), Color_WHITE);
  EXPECT_NEAR(ColorGetA(bitmap.GetPixel(0, 1)), 128, 1);
}

TEST(SWCanvas, NearestImageSpansMatchSinglePixelDraws) {
  using namespace skity;
  constexpr int width = 19, height = 5;
  constexpr int stride = 4 * 4 + 1;
  constexpr TileMode tiles[] = {TileMode::kClamp, TileMode::kRepeat,
                                TileMode::kMirror, TileMode::kDecal};
  for (auto format : {ColorType::kRGBA, ColorType::kBGRA}) {
    for (auto alpha : {kPremul_AlphaType, kUnpremul_AlphaType}) {
      std::array<uint8_t, stride + 4 * 4> storage{};
      auto data = Data::MakeWithCopy(storage.data(), storage.size());
      auto pixmap = std::make_shared<Pixmap>(data, stride, 4, 2, alpha, format);
      Bitmap source(pixmap, false);
      for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 4; ++x) {
          auto color = ColorSetARGB(39 + 67 * x, 255 - 47 * x, 199 * y, 73);
          source.SetPixel(
              x, y, alpha == kPremul_AlphaType ? ColorToPMColor(color) : color);
        }
      }
      auto image = Image::MakeImage(pixmap);
      // The first pixel maps to (-1, -1) in normalized texture coordinates.
      auto translate = Matrix::Translate(4.5f, 2.5f);
      for (auto matrix : {translate, translate * Matrix::Skew(0.5f, -0.5f)}) {
        for (auto tx : tiles) {
          for (auto ty : tiles) {
            Paint paint;
            paint.SetBlendMode(BlendMode::kSrc);
            paint.SetAlpha(137);
            paint.SetShader(
                Shader::MakeShader(image, SamplingOptions{}, tx, ty, matrix));
            Bitmap actual(width, height, kPremul_AlphaType);
            Bitmap expected(width, height, kPremul_AlphaType);
            auto rows = Canvas::MakeSoftwareCanvas(&actual);
            auto pixels = Canvas::MakeSoftwareCanvas(&expected);
            rows->DrawRect(Rect::MakeWH(width, height), paint);
            for (int y = 0; y < height; ++y) {
              for (int x = 0; x < width; ++x) {
                pixels->DrawRect(Rect::MakeXYWH(x, y, 1, 1), paint);
                ASSERT_EQ(actual.GetPixel(x, y), expected.GetPixel(x, y))
                    << "tile=" << static_cast<int>(tx) << ","
                    << static_cast<int>(ty) << " pixel=" << x << "," << y;
              }
            }
          }
        }
      }
    }
  }
}

TEST(SWCanvas, ClippedClearRespectsDestinationPixelLayout) {
  using namespace skity;
  for (ColorType format :
       {ColorType::kA8, ColorType::kBGRA, ColorType::kRGB565}) {
    SCOPED_TRACE(static_cast<int>(format));
    auto alpha =
        format == ColorType::kRGB565 ? kOpaque_AlphaType : kPremul_AlphaType;
    Bitmap reference(2, 1, alpha, format);
    reference.SetPixel(0, 0, Color_RED);
    reference.SetPixel(1, 0, Color_TRANSPARENT);
    Bitmap bitmap(19, 3, alpha, format);
    auto canvas = Canvas::MakeSoftwareCanvas(&bitmap);
    canvas->Clear(Color_RED);
    canvas->ClipRect(Rect::MakeXYWH(3, 1, 13, 1));
    canvas->Clear(Color_TRANSPARENT);
    for (uint32_t y = 0; y < bitmap.Height(); ++y) {
      for (uint32_t x = 0; x < bitmap.Width(); ++x) {
        bool cleared = y == 1 && x >= 3 && x < 16;
        EXPECT_EQ(bitmap.GetPixel(x, y),
                  reference.GetPixel(cleared ? 1 : 0, 0));
      }
    }
  }
}

TEST(SWCanvas, BGRABlendsSolidAndSampledSpansLikeRGBA) {
  using namespace skity;
  constexpr uint32_t kWidth = 19;  // Two eight-pixel batches and a tail.
  constexpr PMColor kPremulRed = ColorSetARGB(85, 85, 0, 0);
  constexpr PMColor kPremulBlue = ColorSetARGB(85, 0, 0, 85);
  Bitmap rgba(kWidth, 4, kPremul_AlphaType, ColorType::kRGBA);
  Bitmap bgra(kWidth, 4, kPremul_AlphaType, ColorType::kBGRA);
  Bitmap source(kWidth, 1, kPremul_AlphaType);
  for (uint32_t x = 0; x < kWidth; ++x) {
    source.SetPixel(x, 0, x % 2 == 0 ? kPremulRed : kPremulBlue);
  }
  Paint solid;
  solid.SetAntiAlias(true);
  solid.SetColor(ColorSetARGB(85, 255, 0, 0));
  Paint sampled;
  sampled.SetAntiAlias(true);
  sampled.SetShader(Shader::MakeShader(Image::MakeImage(source.GetPixmap())));
  const Color background = ColorSetARGB(128, 64, 128, 192);
  const PMColor dst = ColorToPMColor(background);
  for (int i = 0; i <= static_cast<int>(BlendMode::kLastCoeffMode); ++i) {
    const auto mode = static_cast<BlendMode>(i);
    SCOPED_TRACE(i);
    solid.SetBlendMode(mode);
    sampled.SetBlendMode(mode);
    for (Bitmap* bitmap : {&rgba, &bgra}) {
      auto canvas = Canvas::MakeSoftwareCanvas(bitmap);
      canvas->Clear(background);
      canvas->DrawRect(Rect::MakeWH(kWidth, 1.5f), solid);
      canvas->DrawRect(Rect::MakeXYWH(0, 2, kWidth, 1.5f), sampled);
    }
    for (uint32_t x = 0; x < kWidth; ++x) {
      SCOPED_TRACE(x);
      for (uint32_t y : {0u, 2u}) {
        const Color full = rgba.GetPixel(x, y);
        const Color edge = rgba.GetPixel(x, y + 1);
        // Each pair of source colors repeats through the scalar tail.
        EXPECT_EQ(full, rgba.GetPixel(x % 2, y));
        EXPECT_EQ(edge, rgba.GetPixel(x % 2, y + 1));
        EXPECT_EQ(bgra.GetPixel(x, y), full);
        EXPECT_EQ(bgra.GetPixel(x, y + 1), edge);
        auto half = [](uint32_t from, uint32_t to) {
          return (127 * from + 128 * to + 127) / 255;
        };
        EXPECT_EQ(edge, ColorSetARGB(half(ColorGetA(dst), ColorGetA(full)),
                                     half(ColorGetR(dst), ColorGetR(full)),
                                     half(ColorGetG(dst), ColorGetG(full)),
                                     half(ColorGetB(dst), ColorGetB(full))));
      }
      if (mode == BlendMode::kSrc) {
        EXPECT_EQ(bgra.GetPixel(x, 0), kPremulRed);
        EXPECT_EQ(bgra.GetPixel(x, 2), source.GetPixel(x, 0));
      }
    }
  }
}

TEST(SWCanvas, BlendRowsMatchIndividualPremultipliedPixels) {
  using namespace skity;
  constexpr uint32_t width = 20;
  constexpr uint8_t alphas[] = {0, 1, 85, 127, 128, 191, 254, 255};
  std::array<PMColor, width> source, background;
  for (uint32_t x = 0; x < width; ++x) {
    source[x] = ColorToPMColor(
        ColorSetARGB(alphas[x % 8], (37 * x) % 256, 255 - 9 * x, 11 * x));
    background[x] = ColorToPMColor(
        ColorSetARGB(alphas[(x + 3) % 8], 7 * x, (53 * x) % 256, 201));
  }
  for (auto format : {ColorType::kRGBA, ColorType::kBGRA}) {
    Bitmap actual(width, 2, kPremul_AlphaType, format);
    Bitmap expected(width, 2, kPremul_AlphaType, format);
    SWRenderTarget rows(&actual), pixels(&expected);
    for (int i = 0; i <= static_cast<int>(BlendMode::kLastCoeffMode); ++i) {
      auto mode = static_cast<BlendMode>(i);
      for (uint8_t coverage : {0, 1, 127, 128, 254, 255}) {
        for (uint32_t length : {0, 1, 3, 4, 5, 7, 8, 9, 16, 17}) {
          for (uint32_t y = 0; y < 2; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
              actual.SetPixel(x, y, background[x]);
              expected.SetPixel(x, y, background[x]);
            }
          }
          rows.BlendPixelH(1, 0, source[4], length, mode, coverage);
          rows.BlendPixelH(1, 1, source.data(), length, mode, coverage);
          for (uint32_t x = 0; x < length; ++x) {
            pixels.BlendPixel(x + 1, 0, source[4], mode, coverage);
            pixels.BlendPixel(x + 1, 1, source[x], mode, coverage);
          }
          for (uint32_t y = 0; y < 2; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
              ASSERT_EQ(expected.GetPixel(x, y), actual.GetPixel(x, y))
                  << BlendMode_Name(mode) << " coverage=" << +coverage
                  << " length=" << length << " pixel=" << x << "," << y;
            }
          }
        }
      }
    }
  }
}

TEST(SWCanvas, AtopPreservesAlphaAcrossAntialiasedSpans) {
  using namespace skity;
  Bitmap bitmap(19, 2, kPremul_AlphaType);
  auto canvas = Canvas::MakeSoftwareCanvas(&bitmap);
  const Color src = ColorSetARGB(85, 255, 64, 0);
  const Color dst = ColorSetARGB(128, 64, 128, 192);
  Paint paint;
  paint.SetAntiAlias(true);
  const Rect rect = Rect::MakeWH(19, 1.75f);
  for (auto mode : {BlendMode::kSrcATop, BlendMode::kDstATop}) {
    SCOPED_TRACE(BlendMode_Name(mode));
    const bool src_atop = mode == BlendMode::kSrcATop;
    canvas->Clear(src_atop ? dst : src);
    paint.SetColor(src_atop ? src : dst);
    paint.SetBlendMode(mode);
    canvas->DrawRect(rect, paint);
    // Swapping the inputs for DstATop gives the same fully covered color.
    for (uint32_t x = 0; x < bitmap.Width(); ++x) {
      EXPECT_EQ(bitmap.GetPixel(x, 0), ColorSetARGB(128, 64, 53, 64));
      EXPECT_EQ(ColorGetA(bitmap.GetPixel(x, 1)), src_atop ? 128u : 117u);
    }
    for (int i = 1; i < 64; ++i) {
      canvas->DrawRect(rect, paint);
    }
    for (uint32_t x = 0; x < bitmap.Width(); ++x) {
      EXPECT_EQ(ColorGetA(bitmap.GetPixel(x, 1)), 128u);
    }
  }
}

TEST(SWCanvas, ReadOnlyBGRATargetKeepsItsPixels) {
  using namespace skity;
  Bitmap bitmap(9, 1, kPremul_AlphaType, ColorType::kBGRA);
  Bitmap read_only(bitmap.GetPixmap(), true);
  auto canvas = Canvas::MakeSoftwareCanvas(&read_only);
  canvas->Clear(Color_RED);
  for (uint32_t x = 0; x < bitmap.Width(); ++x) {
    EXPECT_EQ(bitmap.GetPixel(x, 0), Color_TRANSPARENT);
  }
}

TEST(SWCanvas, ClearRespectsPaddedRGBAAndBGRARows) {
  using namespace skity;
  constexpr size_t kRowBytes = 9 * 4 + 1;
  std::array<uint8_t, kRowBytes * 2> bytes;
  bytes.fill(0x5a);
  for (auto format : {ColorType::kRGBA, ColorType::kBGRA}) {
    SCOPED_TRACE(static_cast<int>(format));
    auto data = Data::MakeWithCopy(bytes.data(), bytes.size());
    Bitmap bitmap(std::make_shared<Pixmap>(data, kRowBytes, 9, 2,
                                           kPremul_AlphaType, format),
                  false);
    auto canvas = Canvas::MakeSoftwareCanvas(&bitmap);
    canvas->Clear(Color_RED);
    for (uint32_t y = 0; y < 2; ++y) {
      EXPECT_EQ(bitmap.GetPixel(0, y), Color_RED);
      EXPECT_EQ(bitmap.GetPixel(8, y), Color_RED);
      EXPECT_EQ(data->Bytes()[(y + 1) * kRowBytes - 1], 0x5a);
    }
  }
}

TEST(SWCanvas, UnpremultipliedTargetPreservesStraightChannels) {
  using namespace skity;
  Bitmap bitmap(3, 2, kUnpremul_AlphaType);
  auto canvas = Canvas::MakeSoftwareCanvas(&bitmap);
  canvas->Clear(ColorSetARGB(128, 255, 0, 0));
  EXPECT_EQ(bitmap.GetPixel(1, 1), ColorSetARGB(128, 255, 0, 0));

  const Color original = ColorSetARGB(37, 17, 119, 209);
  bitmap.SetPixel(0, 0, original);
  Paint paint;
  paint.SetAntiAlias(true);
  paint.SetBlendMode(BlendMode::kDst);
  canvas->DrawRect(Rect::MakeLTRB(0.5f, 0.f, 2.f, 1.f), paint);
  EXPECT_EQ(bitmap.GetPixel(0, 0), original);
}

TEST(SWCanvas, DstOverReadsAlphaInDestinationLayout) {
  using namespace skity;
  for (auto format : {ColorType::kRGBA, ColorType::kBGRA, ColorType::kA8,
                      ColorType::kRGB565}) {
    SCOPED_TRACE(static_cast<int>(format));
    const bool rgb565 = format == ColorType::kRGB565;
    Bitmap bitmap(2, 1, rgb565 ? kOpaque_AlphaType : kUnpremul_AlphaType,
                  format);
    bitmap.SetPixel(0, 0, Color_RED);
    const Color opaque = bitmap.GetPixel(0, 0);
    Paint paint;
    paint.SetColor(Color_BLUE);
    paint.SetBlendMode(BlendMode::kDstOver);
    Canvas::MakeSoftwareCanvas(&bitmap)->DrawPaint(paint);
    EXPECT_EQ(bitmap.GetPixel(0, 0), opaque);
    EXPECT_EQ(bitmap.GetPixel(1, 0),
              format == ColorType::kRGBA || format == ColorType::kBGRA
                  ? Color_BLUE
                  : Color_BLACK);
  }
}

TEST(SWCanvas, ErasingPreservesStraightRGB) {
  using namespace skity;
  for (auto format : {ColorType::kRGBA, ColorType::kBGRA}) {
    SCOPED_TRACE(static_cast<int>(format));
    Bitmap bitmap(1, 2, kUnpremul_AlphaType, format);
    auto canvas = Canvas::MakeSoftwareCanvas(&bitmap);
    const Color color = ColorSetRGB(128, 63, 207);
    bitmap.SetPixel(0, 0, ColorSetA(color, 2));
    bitmap.SetPixel(0, 1, ColorSetA(color, 2));
    Paint paint;
    paint.SetAntiAlias(true);
    paint.SetBlendMode(BlendMode::kClear);
    canvas->DrawRect(Rect::MakeWH(1, 1.5f), paint);
    EXPECT_EQ(bitmap.GetPixel(0, 0), Color_TRANSPARENT);
    EXPECT_EQ(bitmap.GetPixel(0, 1), ColorSetA(color, 1));

    for (auto mode : {BlendMode::kDstIn, BlendMode::kDstOut}) {
      SCOPED_TRACE(BlendMode_Name(mode));
      canvas->Clear(color);
      paint.SetBlendMode(mode);
      paint.SetColor(
          ColorSetA(Color_BLACK, mode == BlendMode::kDstIn ? 254 : 1));
      for (int i = 0; i < 64; ++i) {
        canvas->DrawRect(Rect::MakeWH(1, 1.75f), paint);
      }
      EXPECT_EQ(bitmap.GetPixel(0, 0), ColorSetA(color, 191));
      EXPECT_EQ(bitmap.GetPixel(0, 1), ColorSetA(color, 191));
    }
  }
}
