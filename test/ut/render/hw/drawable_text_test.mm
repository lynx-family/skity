// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#import <Metal/Metal.h>

#include <gtest/gtest.h>

#include <skity/gpu/gpu_context_mtl.h>
#include <algorithm>
#include <functional>
#include <skity/skity.hpp>
#include <vector>

#include "test/ut/text/glyph_test_typeface.hpp"

namespace skity::test {
namespace {

class DrawableTextPixelTest : public ::testing::Test {
 protected:
  static constexpr uint32_t kSize = 512;

  void SetUp() override {
    device_ = MTLCreateSystemDefaultDevice();
    if (device_ == nil) {
      GTEST_SKIP() << "A Metal device is required for pixel comparisons";
    }
    queue_ = [device_ newCommandQueue];
    context_ = MTLContextCreate(device_, queue_);
    ASSERT_NE(context_, nullptr);
    TestGlyphSpec spec;
    spec.format = GlyphFormat::RGBA32;
    spec.drawable = true;
    face_ = std::make_shared<GlyphTestTypeface>(std::vector<TestGlyphSpec>{spec});
  }

  std::vector<uint8_t> Render(float density, const std::function<void(Canvas*)>& draw,
                              Color background = Color_WHITE) {
    auto descriptor =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                           width:kSize
                                                          height:kSize
                                                       mipmapped:NO];
    descriptor.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    descriptor.storageMode = MTLStorageModeShared;
    id<MTLTexture> texture = [device_ newTextureWithDescriptor:descriptor];
    GPUSurfaceDescriptorMTL target;
    target.backend = GPUBackendType::kMetal;
    target.width = kSize;
    target.height = kSize;
    target.sample_count = 1;
    target.content_scale = density;
    target.texture = texture;
    target.surface_type = MTLSurfaceType::kTexture;
    auto surface = context_->CreateSurface(&target);
    if (!surface) {
      ADD_FAILURE() << "Failed to create the render surface";
      return {};
    }
    auto* canvas = surface->LockCanvas();
    canvas->Clear(background);
    draw(canvas);
    canvas->Flush();
    surface->Flush();
    auto completion = [queue_ commandBuffer];
    [completion commit];
    [completion waitUntilCompleted];
    std::vector<uint8_t> pixels(kSize * kSize * 4);
    [texture getBytes:pixels.data()
          bytesPerRow:kSize * 4
           fromRegion:MTLRegionMake2D(0, 0, kSize, kSize)
          mipmapLevel:0];
    return pixels;
  }

  void Compare(const std::vector<uint8_t>& actual, const std::vector<uint8_t>& expected,
               Color background = Color_WHITE, double tolerance = 2.0) {
    ASSERT_EQ(actual.size(), kSize * kSize * 4u);
    ASSERT_EQ(actual.size(), expected.size());
    uint64_t error = 0;
    size_t samples = 0;
    for (size_t i = 0; i < actual.size(); i += 4) {
      auto is_background = [&](const auto& pixels) {
        return pixels[i] == ColorGetR(background) && pixels[i + 1] == ColorGetG(background) &&
               pixels[i + 2] == ColorGetB(background);
      };
      if (!is_background(actual) || !is_background(expected)) {
        for (size_t c = 0; c < 4; ++c) {
          error += std::abs(int(actual[i + c]) - int(expected[i + c]));
        }
        samples += 4;
      }
    }
    ASSERT_GT(samples, 1000u);
    EXPECT_LT(double(error) / samples, tolerance);
  }

  void DrawGlyph(Canvas* canvas, const Font& font, const Paint& paint, bool blob) {
    GlyphID glyph = 0;
    float x = 40.f, y = 185.f;
    if (blob) {
      std::vector<TextRun> runs;
      runs.emplace_back(font, std::vector<GlyphID>{glyph}, std::vector<float>{0.f},
                        std::vector<float>{0.f});
      TextBlob text(std::move(runs));
      canvas->DrawTextBlob(&text, x, y, paint);
    } else {
      canvas->DrawGlyphs(1, &glyph, &x, &y, font, paint);
    }
  }

  // Independent image oracle: two disjoint rectangles with alpha applied
  // directly. The recorded test glyph instead overlaps an opaque foreground
  // layer over a red background, so this catches per-layer/double alpha.
  void DrawReference(Canvas* canvas, const Font& font, const Paint& paint) {
    canvas->Save();
    canvas->Translate(40.f, 185.f);
    Matrix font_matrix;
    font_matrix.SetScaleX(font.GetScaleX());
    font_matrix.SetSkewX(font.GetSkewX());
    canvas->Concat(font_matrix);
    const float size = font.GetSize();
    Paint left;
    left.SetColor(Color_RED);
    left.SetAlphaF(paint.GetFillColor().a);
    left.SetBlendMode(paint.GetBlendMode());
    Paint right = left;
    right.SetColor(Color_BLUE);
    right.SetAlphaF(paint.GetFillColor().a);
    canvas->DrawRect(Rect::MakeLTRB(-.15f * size, -.85f * size, .35f * size, .15f * size), left);
    canvas->DrawRect(Rect::MakeLTRB(.35f * size, -.85f * size, .85f * size, .15f * size), right);
    canvas->Restore();
  }

  id<MTLDevice> device_ = nil;
  id<MTLCommandQueue> queue_ = nil;
  std::unique_ptr<GPUContext> context_;
  std::shared_ptr<GlyphTestTypeface> face_;
};

TEST_F(DrawableTextPixelTest, PlacementPerspectiveClipAndLayers) {
  Font font(face_, 140.f);
  font.SetScaleX(.9f);
  font.SetSkewX(-.15f);
  Paint paint;
  paint.SetColor(Color_BLUE);
  paint.SetFontThreshold(10.f);
  for (float density : {1.f, 2.f}) {
    for (bool blob : {false, true}) {
      for (bool layer : {false, true}) {
        for (bool perspective : {false, true}) {
          SCOPED_TRACE(::testing::Message() << density << ",blob=" << blob << ",layer=" << layer
                                            << ",perspective=" << perspective);
          auto draw = [&](Canvas* canvas, bool reference) {
            Matrix matrix = Matrix::Translate(12.f, 10.f);
            if (perspective) {
              matrix.SetPersp0(.0015f);
              matrix.SetPersp1(.0008f);
            }
            canvas->Concat(matrix);
            if (layer) {
              canvas->SaveLayer(Rect::MakeLTRB(8.f, 35.f, 230.f, 225.f), Paint{});
            }
            canvas->Save();
            canvas->ClipRect(Rect::MakeLTRB(24.f, 25.f, 180.f, 210.f));
            const int count = canvas->GetSaveCount();
            const Matrix before = canvas->GetTotalMatrix();
            if (reference) {
              DrawReference(canvas, font, paint);
            } else {
              DrawGlyph(canvas, font, paint, blob);
            }
            EXPECT_EQ(canvas->GetSaveCount(), count);
            EXPECT_EQ(canvas->GetTotalMatrix(), before);
            canvas->Restore();
            Paint marker;
            marker.SetColor(Color_GREEN);
            canvas->DrawRect(Rect::MakeXYWH(190.f, 180.f, 20.f, 20.f), marker);
            if (layer) {
              canvas->Restore();
            }
          };
          Compare(Render(density, [&](Canvas* c) { draw(c, false); }),
                  Render(density, [&](Canvas* c) { draw(c, true); }));
        }
      }
    }
  }
  EXPECT_EQ(face_->state->image_requests[0], 0u);
  EXPECT_EQ(face_->state->drawable_requests[0], 1u);
}

TEST_F(DrawableTextPixelTest, AlphaAndBlendApplyOnceToCompletedGlyph) {
  Font font(face_, 140.f);
  for (auto style : {Paint::kFill_Style, Paint::kStroke_Style, Paint::kStrokeAndFill_Style,
                     Paint::kStrokeThenFill_Style}) {
    for (auto blend : {BlendMode::kSrcOver, BlendMode::kMultiply}) {
      SCOPED_TRACE(::testing::Message() << "style=" << style << ",blend=" << int(blend));
      Paint paint;
      paint.SetColor(Color_BLUE);
      paint.SetAlphaF(.5f);
      paint.SetStyle(style);
      paint.SetBlendMode(blend);
      paint.SetFontThreshold(10.f);
      const Color background = ColorSetRGB(200, 170, 120);
      const auto draw_glyph = [&](Canvas* c) { DrawGlyph(c, font, paint, false); };
      const auto draw_reference = [&](Canvas* c) { DrawReference(c, font, paint); };
      Compare(Render(1.f, draw_glyph, background), Render(1.f, draw_reference, background),
              background);
    }
  }
}

TEST_F(DrawableTextPixelTest, FilteredBlobUsesConsistentCanvasLayerState) {
  Font font(face_, 140.f);
  Paint paint;
  paint.SetColor(Color_BLUE);
  paint.SetFontThreshold(10.f);
  // A filtered glyph in an explicit enclosing layer must retain color and
  // position. Compare with applying the filter to the reference content.
  auto filter = ColorFilters::Blend(Color_GREEN, BlendMode::kModulate);
  paint.SetColorFilter(filter);
  auto actual = Render(2.f, [&](Canvas* canvas) {
    canvas->Translate(12.f, 8.f);
    canvas->SaveLayer(Rect::MakeLTRB(0.f, 20.f, 230.f, 230.f), Paint{});
    DrawGlyph(canvas, font, paint, true);
    canvas->Restore();
  });
  auto expected = Render(2.f, [&](Canvas* canvas) {
    canvas->Translate(12.f, 8.f);
    canvas->SaveLayer(Rect::MakeLTRB(0.f, 20.f, 230.f, 230.f), Paint{});
    Paint layer;
    layer.SetColorFilter(filter);
    canvas->SaveLayer(Rect::MakeLTRB(18.f, 65.f, 160.f, 207.f), layer);
    DrawReference(canvas, font, paint);
    canvas->Restore();
    canvas->Restore();
  });
  Compare(actual, expected);
}

TEST_F(DrawableTextPixelTest, FiltersKeepTextUnitsAcrossStrikeScales) {
  for (float size : {64.f, 128.f, 192.f}) {
    for (bool blob : {false, true}) {
      SCOPED_TRACE(::testing::Message() << "size=" << size << ",blob=" << blob);
      Font font(face_, size);
      Paint paint;
      paint.SetColor(Color_BLUE);
      paint.SetAlphaF(.5f);
      paint.SetFontThreshold(10.f);
      paint.SetMaskFilter(MaskFilter::MakeBlur(BlurStyle::kNormal, 4.f));
      auto actual = Render(1.f, [&](Canvas* canvas) {
        canvas->Scale(1.15f, 1.f);
        DrawGlyph(canvas, font, paint, blob);
      });
      auto expected = Render(1.f, [&](Canvas* canvas) {
        canvas->Scale(1.15f, 1.f);
        Rect bounds = Rect::MakeLTRB(40.f - .15f * size, 185.f - .85f * size, 40.f + .85f * size,
                                     185.f + .15f * size);
        // The text-blob entry point reserves one source unit around its ink.
        canvas->SaveLayer(blob ? bounds.MakeOutset(1.f, 1.f) : bounds, paint);
        Paint content;
        content.SetColor(Color_BLUE);
        DrawReference(canvas, font, content);
        canvas->Restore();
      });
      auto ink_bounds = [](const auto& pixels) {
        Rect bounds;
        for (uint32_t y = 0; y < kSize; ++y) {
          for (uint32_t x = 0; x < kSize; ++x) {
            auto* p = pixels.data() + 4 * (y * kSize + x);
            if (std::min({p[0], p[1], p[2]}) < 250) {
              bounds.Join(Rect::MakeXYWH(x, y, 1.f, 1.f));
            }
          }
        }
        return bounds;
      };
      const auto a = ink_bounds(actual), b = ink_bounds(expected);
      EXPECT_NEAR(a.Left(), b.Left(), 1.f);
      EXPECT_NEAR(a.Top(), b.Top(), 1.f);
      EXPECT_NEAR(a.Right(), b.Right(), 1.f);
      EXPECT_NEAR(a.Bottom(), b.Bottom(), 1.f);
      Compare(actual, expected);
    }
  }
}

}  // namespace
}  // namespace skity::test
