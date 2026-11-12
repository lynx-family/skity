// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#import <Metal/Metal.h>
#include <gtest/gtest.h>
#include <skity/gpu/gpu_context_mtl.h>
#include <algorithm>
#include <functional>
#include <limits>
#include <skity/skity.hpp>
#include <vector>

namespace skity {
namespace {
class ColorGlyphThresholdTest : public ::testing::Test {
 protected:
  static constexpr uint32_t kWidth = 192, kHeight = 128;
  void SetUp() override {
    device_ = MTLCreateSystemDefaultDevice();
    if (device_ == nil) GTEST_SKIP() << "Metal is required";
    RecordProperty("metal_device", device_.name.UTF8String);
    queue_ = [device_ newCommandQueue];
    ASSERT_NE(queue_, nil);
    context_ = MTLContextCreate(device_, queue_);
    ASSERT_NE(context_, nullptr);
    face_ = FontManager::RefDefault()->MatchFamilyStyle("Apple Color Emoji", FontStyle{});
    ASSERT_NE(face_, nullptr);
    ASSERT_TRUE(face_->ContainsColorTable());
    glyph_ = face_->UnicharToGlyph(0x1f600);
    ASSERT_NE(glyph_, 0);
  }
  std::vector<uint8_t> Render(float density, const Paint& paint, float font_size = 24.f) {
    auto desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                   width:kWidth
                                                                  height:kHeight
                                                               mipmapped:NO];
    desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    desc.storageMode = MTLStorageModeShared;
    id<MTLTexture> texture = [device_ newTextureWithDescriptor:desc];
    if (texture == nil) {
      ADD_FAILURE() << "Texture allocation failed";
      return {};
    }
    GPUSurfaceDescriptorMTL target;
    target.backend = GPUBackendType::kMetal;
    target.surface_type = MTLSurfaceType::kTexture;
    target.texture = texture;
    target.width = kWidth;
    target.height = kHeight;
    target.sample_count = 1;
    target.content_scale = density;
    auto surface = context_->CreateSurface(&target);
    if (!surface) {
      ADD_FAILURE() << "Surface creation failed";
      return {};
    }
    auto* canvas = surface->LockCanvas();
    if (!canvas) {
      ADD_FAILURE() << "Canvas unavailable";
      return {};
    }
    canvas->Clear(Color_WHITE);
    Font font(face_, font_size);
    font.SetSubpixel(false);
    font.SetBaselineSnap(false);
    std::vector<TextRun> runs;
    runs.emplace_back(font, std::vector<GlyphID>{glyph_}, std::vector<float>{0.f},
                      std::vector<float>{0.f});
    TextBlob blob(std::move(runs));
    float x = 32.f, y = 56.f;
    if (font_size > 24.f) {
      const auto bounds = blob.GetBoundsRect();
      x = kWidth / (2.f * density) - (bounds.Left() + bounds.Right()) / 2.f;
      y = kHeight / (2.f * density) - (bounds.Top() + bounds.Bottom()) / 2.f;
    }
    canvas->DrawTextBlob(&blob, x, y, paint);
    canvas->Flush();
    surface->Flush();
    auto completion = [queue_ commandBuffer];
    if (completion == nil) {
      ADD_FAILURE() << "Completion buffer unavailable";
      return {};
    }
    [completion commit];
    [completion waitUntilCompleted];
    if (completion.status != MTLCommandBufferStatusCompleted || completion.error != nil) {
      ADD_FAILURE() << "Metal completion failed";
      return {};
    }
    std::vector<uint8_t> pixels(kWidth * kHeight * 4);
    [texture getBytes:pixels.data()
          bytesPerRow:kWidth * 4
           fromRegion:MTLRegionMake2D(0, 0, kWidth, kHeight)
          mipmapLevel:0];
    // Retire the surface/arena before ARC releases its retained texture.
    surface.reset();
    return pixels;
  }
  void Compare(bool filtered) {
    for (float density : {1.f, 2.f})
      for (bool stroke : {false, true})
        for (bool gradient : {false, true}) {
          SCOPED_TRACE(::testing::Message()
                       << "density=" << density << " stroke=" << stroke << " gradient=" << gradient
                       << " filtered=" << filtered);
          Paint paint;
          paint.SetAntiAlias(true);
          paint.SetColor(ColorSetARGB(128, 32, 64, 160));
          paint.SetStyle(stroke ? Paint::kStroke_Style : Paint::kFill_Style);
          paint.SetStrokeWidth(1.25f);
          if (gradient) {
            Point points[2] = {{0, 0, 0, 1}, {128, 0, 0, 1}};
            Vec4 colors[2] = {{1, 0, 0, 1}, {0, 0, 1, 1}};
            paint.SetShader(Shader::MakeLinear(points, colors, nullptr, 2, TileMode::kClamp));
          }
          if (filtered) paint.SetMaskFilter(MaskFilter::MakeBlur(BlurStyle::kNormal, 1.25f));
          paint.SetFontThreshold(256.f);
          auto reference = Render(density, paint);
          ASSERT_EQ(reference.size(), kWidth * kHeight * 4);
          ASSERT_TRUE(std::any_of(reference.begin(), reference.end(), [](auto n) {
            return n != 255;
          })) << "Bitmap reference blank";
          paint.SetFontThreshold(std::numeric_limits<float>::min());
          auto actual = Render(density, paint);
          ASSERT_EQ(actual.size(), reference.size());
          EXPECT_TRUE(std::any_of(actual.begin(), actual.end(), [](auto n) { return n != 255; }))
              << "Bitmap-only color glyph was dropped above the path threshold";
          EXPECT_EQ(actual, reference) << "Intrinsic color/alpha changed across the path threshold";
        }
  }
  id<MTLDevice> device_;
  id<MTLCommandQueue> queue_;
  std::unique_ptr<GPUContext> context_;
  std::shared_ptr<Typeface> face_;
  GlyphID glyph_ = 0;
};
TEST_F(ColorGlyphThresholdTest, PreservesBitmapOnlyColorAndAlpha) { Compare(false); }
TEST_F(ColorGlyphThresholdTest, PreservesFilteredBitmapOnlyColorAndAlpha) { Compare(true); }
TEST_F(ColorGlyphThresholdTest, OversizedRunUsesBoundedBitmapFitting) {
  Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(ColorSetARGB(128, 32, 64, 160));
  paint.SetFontThreshold(std::numeric_limits<float>::min());
  const auto actual = Render(1.f, paint, 384.f);
  ASSERT_EQ(actual.size(), kWidth * kHeight * 4);
  EXPECT_TRUE(std::any_of(actual.begin(), actual.end(), [](auto n) { return n != 255; }))
      << "Oversized intrinsic-color run was lost instead of using bounded fitting";
}
}  // namespace
}  // namespace skity
