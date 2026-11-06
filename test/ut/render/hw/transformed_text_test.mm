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

namespace skity {
namespace {

class TransformedTextPixelTest : public ::testing::Test {
 protected:
  static constexpr uint32_t kWidth = 320;
  static constexpr uint32_t kHeight = 320;

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
    return ink;
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

}  // namespace
}  // namespace skity
