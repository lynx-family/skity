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

namespace skity {
namespace {

class TransformedTextPixelTest : public ::testing::Test {
 protected:
  static constexpr uint32_t kWidth = 768;
  static constexpr uint32_t kHeight = 768;

  void SetUp() override {
    device_ = MTLCreateSystemDefaultDevice();
    if (device_ == nil) {
      GTEST_SKIP() << "A Metal device is required for pixel comparisons";
    }
    queue_ = [device_ newCommandQueue];
    context_ = MTLContextCreate(device_, queue_);
    ASSERT_NE(context_, nullptr);
#if SKITY_TEST_CT_FONT
    face_ = Typeface::MakeFromFile("/System/Library/Fonts/Apple Color Emoji.ttc");
#else
    face_ = Typeface::MakeFromFile(SKITY_FONT_DIR "fonts/resources/NotoColorEmoji.ttf");
#endif
    ASSERT_NE(face_, nullptr);
    glyph_ = face_->UnicharToGlyph(0x1F600);
    ASSERT_NE(glyph_, 0);
  }

  struct Ink {
    int left = kWidth;
    int top = kHeight;
    int right = -1;
    int bottom = -1;
    size_t count = 0;
    size_t colored = 0;
    std::vector<uint8_t> pixels;
  };

  Ink Render(float density, const std::function<void(Canvas*)>& draw) {
    auto descriptor =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                           width:kWidth
                                                          height:kHeight
                                                       mipmapped:NO];
    descriptor.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    descriptor.storageMode = MTLStorageModeShared;
    id<MTLTexture> texture = [device_ newTextureWithDescriptor:descriptor];
    GPUSurfaceDescriptorMTL target;
    target.backend = GPUBackendType::kMetal;
    target.width = kWidth;
    target.height = kHeight;
    target.sample_count = 1;
    target.content_scale = density;
    target.texture = texture;
    target.surface_type = MTLSurfaceType::kTexture;
    auto surface = context_->CreateSurface(&target);
    if (!surface) {
      ADD_FAILURE() << "Failed to create the render surface";
      return {};
    }
    auto canvas = surface->LockCanvas();
    canvas->Clear(Color_WHITE);
    draw(canvas);
    canvas->Flush();
    surface->Flush();
    auto completion = [queue_ commandBuffer];
    [completion commit];
    [completion waitUntilCompleted];
    std::vector<uint8_t> pixels(kWidth * kHeight * 4);
    [texture getBytes:pixels.data()
          bytesPerRow:kWidth * 4
           fromRegion:MTLRegionMake2D(0, 0, kWidth, kHeight)
          mipmapLevel:0];
    return AnalyzePixels(std::move(pixels));
  }

  Ink AnalyzePixels(std::vector<uint8_t> pixels) {
    Ink ink;
    for (int y = 0; y < int(kHeight); ++y) {
      for (int x = 0; x < int(kWidth); ++x) {
        auto* p = &pixels[(y * kWidth + x) * 4];
        if (std::min({p[0], p[1], p[2]}) >= 240) {
          continue;
        }
        ink.left = std::min(ink.left, x);
        ink.top = std::min(ink.top, y);
        ink.right = std::max(ink.right, x);
        ink.bottom = std::max(ink.bottom, y);
        ++ink.count;
        ink.colored += std::max({p[0], p[1], p[2]}) - std::min({p[0], p[1], p[2]}) > 20;
      }
    }
    ink.pixels = std::move(pixels);
    return ink;
  }

  void CompareImages(const Ink& actual, const Ink& reference, double tolerance = 8.,
                     int edge_tolerance = 3) {
    ASSERT_GT(reference.count, 100u);
    ASSERT_GT(actual.count, 100u);
    ASSERT_EQ(actual.pixels.size(), reference.pixels.size());
    EXPECT_NEAR(actual.left, reference.left, edge_tolerance);
    EXPECT_NEAR(actual.top, reference.top, edge_tolerance);
    EXPECT_NEAR(actual.right, reference.right, edge_tolerance);
    EXPECT_NEAR(actual.bottom, reference.bottom, edge_tolerance);
    uint64_t error = 0;
    size_t samples = 0;
    for (size_t i = 0; i < actual.pixels.size(); i += 4) {
      if (std::min({actual.pixels[i], actual.pixels[i + 1], actual.pixels[i + 2],
                    reference.pixels[i], reference.pixels[i + 1], reference.pixels[i + 2]}) < 240) {
        for (size_t c = 0; c < 3; ++c) {
          error += std::abs(int(actual.pixels[i + c]) - int(reference.pixels[i + c]));
        }
        samples += 3;
      }
    }
    ASSERT_GT(samples, 0u);
    EXPECT_LT(double(error) / samples, tolerance);
  }

  Ink TransformPixels(const Ink& source, const Matrix& transform) {
    Matrix inverse;
    EXPECT_TRUE(transform.Invert(&inverse));
    std::vector<uint8_t> pixels(kWidth * kHeight * 4, 255);
    for (uint32_t y = 0; y < kHeight; ++y) {
      for (uint32_t x = 0; x < kWidth; ++x) {
        Vec2 dst{float(x) + .5f, float(y) + .5f}, src;
        inverse.MapPoints(&src, &dst, 1);
        src = src - Vec2{.5f, .5f};
        const int sx = int(std::floor(src.x)), sy = int(std::floor(src.y));
        if (sx < 0 || sy < 0 || sx + 1 >= int(kWidth) || sy + 1 >= int(kHeight)) {
          continue;
        }
        const float fx = src.x - sx, fy = src.y - sy;
        for (int c = 0; c < 4; ++c) {
          auto at = [&](int px, int py) { return source.pixels[(py * kWidth + px) * 4 + c]; };
          const float top = at(sx, sy) * (1.f - fx) + at(sx + 1, sy) * fx;
          const float bottom = at(sx, sy + 1) * (1.f - fx) + at(sx + 1, sy + 1) * fx;
          pixels[(y * kWidth + x) * 4 + c] = uint8_t(std::lround(top * (1.f - fy) + bottom * fy));
        }
      }
    }
    return AnalyzePixels(std::move(pixels));
  }

  void DrawGlyph(Canvas* canvas, bool blob, const Font& font, GlyphID glyph, float x, float y,
                 const Paint& paint) {
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

  void CheckBearing(bool blob) {
    Font font(face_, 64.f);
    font.SetSubpixel(true);
    for (float density : {1.f, 2.f}) {
      for (bool layer : {false, true}) {
        for (bool clip : {false, true}) {
          SCOPED_TRACE(::testing::Message()
                       << "density=" << density << " layer=" << layer << " clip=" << clip);
          auto render = [&](bool perspective) {
            return Render(density, [&](Canvas* canvas) {
              if (layer) {
                canvas->SaveLayer(Rect::MakeLTRB(10.f, 55.f, 150.f, 155.f), Paint{});
              }
              Matrix matrix;
              if (perspective) {
                matrix.SetPersp0(0.00001f);
              }
              canvas->Concat(matrix);
              if (clip) {
                canvas->ClipRect(Rect::MakeLTRB(45.f, 80.f, 80.f, 125.f));
              }
              const float x = 32.375f;
              const float y = 136.625f;
              if (blob) {
                std::vector<TextRun> runs;
                runs.emplace_back(font, std::vector<GlyphID>{glyph_}, std::vector<float>{0.f},
                                  std::vector<float>{0.f});
                TextBlob text(std::move(runs));
                canvas->DrawTextBlob(&text, x, y, Paint{});
              } else {
                canvas->DrawGlyphs(1, &glyph_, &x, &y, font, Paint{});
              }
              if (layer) {
                canvas->Restore();
              }
            });
          };
          const Ink direct = render(false);
          const Ink transformed = render(true);
          ASSERT_GT(direct.colored, 100u);
          ASSERT_GT(transformed.colored, 100u);
          // The very weak perspective moves an edge by less than a pixel.
          // Allow rasterization/filtering rounding, but not a bearing shift.
          EXPECT_NEAR(direct.left, transformed.left, 2);
          EXPECT_NEAR(direct.top, transformed.top, 2);
          EXPECT_NEAR(direct.right, transformed.right, 2);
          EXPECT_NEAR(direct.bottom, transformed.bottom, 2);
          EXPECT_NEAR(double(direct.count), double(transformed.count), direct.count * 0.08);
        }
      }
    }
  }

  id<MTLDevice> device_;
  id<MTLCommandQueue> queue_;
  std::unique_ptr<GPUContext> context_;
  std::shared_ptr<Typeface> face_;
  GlyphID glyph_ = 0;
};

TEST_F(TransformedTextPixelTest, GlyphBearingSurvivesClipAndSaveLayer) { CheckBearing(false); }

TEST_F(TransformedTextPixelTest, BlobBearingSurvivesClipAndSaveLayer) { CheckBearing(true); }

TEST_F(TransformedTextPixelTest, ColorFallbackMatchesLargeAtlasControl) {
  for (bool blob : {false, true}) {
    for (float density : {1.f, 2.f}) {
      for (float size : {64.f, 240.f, 254.f, 320.f}) {
        SCOPED_TRACE(::testing::Message()
                     << "blob=" << blob << " density=" << density << " size=" << size);
        Font font(face_, size);
        Paint paint;
        paint.SetFontThreshold(size == 64.f ? 60.f : 256.f);
        auto render = [&](bool large) {
          context_ = MTLContextCreate(device_, queue_);
          context_->SetLargerAtlasMask(large ? 8 : 0);
          return Render(density, [&](Canvas* canvas) {
            DrawGlyph(canvas, blob, font, glyph_, 32.f, size, paint);
          });
        };
        const auto reference = render(true);
        const auto actual = render(false);
        CompareImages(actual, reference, 8., int(2 * density));
        EXPECT_NEAR(double(actual.colored), double(reference.colored), reference.colored * .08);
      }
    }
  }
}

TEST_F(TransformedTextPixelTest, AffineColorFallbackPreservesFullTransform) {
  for (int scenario = 0; scenario < 4; ++scenario) {
    for (bool blob : {false, true}) {
      SCOPED_TRACE(::testing::Message() << "scenario=" << scenario << " blob=" << blob);
      Font font(face_, scenario < 2 ? 64.f : 320.f);
      Matrix transform;
      float x = 100.f, y = 320.f;
      if (scenario < 2) {
        transform = Matrix::Scale(5.f, scenario == 0 ? 5.f : 1.f);
        x /= 5.f;
        if (scenario == 0) {
          y /= 5.f;
        }
      } else if (scenario == 2) {
        transform = Matrix::RotateDeg(12.f);
      } else {
        transform.SetSkewX(.4f);
      }
      auto render = [&](bool large) {
        context_ = MTLContextCreate(device_, queue_);
        context_->SetLargerAtlasMask(large ? 8 : 0);
        return Render(1.f, [&](Canvas* canvas) {
          // Compare a shear against independently transformed upright pixels.
          // Native color-font rasterization under a sheared text matrix can
          // choose different glyph bounds from transforming its upright image.
          if (!large || scenario != 3) {
            canvas->Concat(transform);
          }
          DrawGlyph(canvas, blob, font, glyph_, x, y, Paint{});
        });
      };
      const auto direct = render(true);
      const auto reference = scenario == 3 ? TransformPixels(direct, transform) : direct;
      CompareImages(render(false), reference);
    }
  }
}

TEST_F(TransformedTextPixelTest, BitmapFallbackAppliesPaintOnce) {
  auto face = std::make_shared<test::GlyphTestTypeface>(
      std::vector<test::TestGlyphSpec>{{20.f, 20.f, false, GlyphFormat::A8, 600.f}});
  Font font(face, 20.f);
  for (bool blob : {false, true}) {
    for (auto style :
         {Paint::kStroke_Style, Paint::kStrokeAndFill_Style, Paint::kStrokeThenFill_Style}) {
      Paint paint;
      paint.SetStyle(style);
      paint.SetFillColor(Color_RED);
      paint.SetStrokeColor(Color_BLUE);
      paint.SetAlphaF(.5f);
      paint.SetStrokeWidth(2.f);
      const auto actual =
          Render(1.f, [&](Canvas* canvas) { DrawGlyph(canvas, blob, font, 0, 32.f, 40.f, paint); });
      Paint fill = paint;
      fill.SetStyle(Paint::kFill_Style);
      if (style == Paint::kStroke_Style) {
        fill.SetFillColor(paint.GetStrokeColor());
      }
      const auto reference = Render(1.f, [&](Canvas* canvas) {
        canvas->DrawRect(Rect::MakeLTRB(29.f, 23.f, 49.f, 43.f), fill);
      });
      CompareImages(actual, reference, 1.);
    }
  }
}

TEST_F(TransformedTextPixelTest, ColorWithOutlinePreservesBothLayersAndAlpha) {
  for (auto format : {GlyphFormat::RGBA32, GlyphFormat::BGRA32}) {
    auto face = std::make_shared<test::GlyphTestTypeface>(
        std::vector<test::TestGlyphSpec>{{12.f, 12.f, true, format}});
    Font font(face, 320.f);
    for (bool blob : {false, true}) {
      for (float alpha : {1.f, .5f}) {
        Paint paint;
        paint.SetAlphaF(alpha);
        const auto actual = Render(
            1.f, [&](Canvas* canvas) { DrawGlyph(canvas, blob, font, 0, 80.f, 320.f, paint); });
        const auto reference = Render(1.f, [&](Canvas* canvas) {
          Paint layer;
          layer.SetColor(format == GlyphFormat::RGBA32 ? Color_RED : Color_BLUE);
          layer.SetAlphaF(alpha);
          canvas->DrawRect(Rect::MakeLTRB(32.f, 48.f, 128.f, 240.f), layer);
          layer.SetColor(format == GlyphFormat::RGBA32 ? Color_BLUE : Color_RED);
          layer.SetAlphaF(alpha);
          canvas->DrawRect(Rect::MakeLTRB(128.f, 48.f, 224.f, 240.f), layer);
        });
        CompareImages(actual, reference, 2.);
        EXPECT_TRUE(face->state->path_requests == 0);
      }
    }
  }
}

}  // namespace
}  // namespace skity
