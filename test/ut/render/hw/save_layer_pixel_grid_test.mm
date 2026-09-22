// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#import <AppKit/AppKit.h>
#import <Metal/Metal.h>

#include <gtest/gtest.h>

#include <skity/gpu/gpu_context_mtl.h>
#include <algorithm>
#include <cmath>
#include <functional>
#include <skity/skity.hpp>
#include <string>
#include <vector>

namespace skity {
namespace {

class SaveLayerPixelGridTest : public ::testing::Test {
 protected:
  static constexpr uint32_t kWidth = 512;
  static constexpr uint32_t kHeight = 256;

  void SetUp() override {
    device_ = MTLCreateSystemDefaultDevice();
    if (device_ == nil) {
      GTEST_SKIP() << "A Metal device is required for pixel comparisons";
    }
    queue_ = [device_ newCommandQueue];
    context_ = MTLContextCreate(device_, queue_);
    ASSERT_NE(context_, nullptr);

    auto face = FontManager::RefDefault()->MatchFamilyStyle(
        [NSFont systemFontOfSize:14].familyName.UTF8String, FontStyle{});
    ASSERT_NE(face, nullptr);
    Font font(face, 15.f);
    font.SetSubpixel(true);
    font.SetBaselineSnap(true);
    font.SetHinting(Font::FontHinting::kSlight);
    std::string text = "list-item-16";
    std::vector<Unichar> characters(text.begin(), text.end());
    std::vector<GlyphID> glyphs(characters.size());
    face->UnicharsToGlyphs(characters.data(), characters.size(), glyphs.data());
    std::vector<TextRun> runs;
    runs.emplace_back(font, glyphs);
    blob_ = std::make_shared<TextBlob>(std::move(runs));
  }

  std::vector<uint8_t> Render(float density, uint32_t samples,
                              const std::function<void(Canvas*)>& draw) {
    auto descriptor =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                           width:kWidth
                                                          height:kHeight
                                                       mipmapped:NO];
    descriptor.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    descriptor.storageMode = MTLStorageModeShared;
    id<MTLTexture> texture = [device_ newTextureWithDescriptor:descriptor];
    if (texture == nil) {
      ADD_FAILURE() << "Failed to allocate the render texture";
      return {};
    }
    GPUSurfaceDescriptorMTL surface_descriptor;
    surface_descriptor.backend = GPUBackendType::kMetal;
    surface_descriptor.width = kWidth;
    surface_descriptor.height = kHeight;
    surface_descriptor.sample_count = samples;
    surface_descriptor.content_scale = density;
    surface_descriptor.texture = texture;
    surface_descriptor.surface_type = MTLSurfaceType::kTexture;
    auto surface = context_->CreateSurface(&surface_descriptor);
    if (!surface) {
      ADD_FAILURE() << "Failed to create the render surface";
      return {};
    }
    auto canvas = surface->LockCanvas();
    canvas->Clear(ColorSetRGB(255, 182, 193));
    draw(canvas);
    canvas->Flush();
    surface->Flush();
    id<MTLCommandBuffer> completion = [queue_ commandBuffer];
    [completion commit];
    [completion waitUntilCompleted];
    std::vector<uint8_t> pixels(kWidth * kHeight * 4);
    [texture getBytes:pixels.data()
          bytesPerRow:kWidth * 4
           fromRegion:MTLRegionMake2D(0, 0, kWidth, kHeight)
          mipmapLevel:0];
    return pixels;
  }

  void CompareText(float density, float scale, float translation, uint32_t samples, bool nested,
                   bool clipped, bool flip_x = false, bool flip_y = false) {
    SCOPED_TRACE(::testing::Message()
                 << "density=" << density << " scale=" << scale << " translation=" << translation
                 << " samples=" << samples << " nested=" << nested << " clipped=" << clipped
                 << " flip_x=" << flip_x << " flip_y=" << flip_y);
    auto render = [&](bool use_layer) {
      return Render(density, samples, [&](Canvas* canvas) {
        if (use_layer) {
          canvas->Translate(flip_x ? kWidth / density : 0.f, flip_y ? kHeight / density : 0.f);
          canvas->Scale(flip_x ? -1.f : 1.f, flip_y ? -1.f : 1.f);
        }
        canvas->Translate(20.f + translation, 20.f + translation);
        canvas->Scale(scale, scale);
        if (clipped) {
          canvas->ClipRect(Rect::MakeLTRB(25.25f, 0.f, 73.75f, 30.f));
        }
        if (use_layer) {
          if (nested) {
            canvas->SaveLayer(Rect::MakeLTRB(-3.4f, -7.2f, 100.3f, 42.6f), Paint{});
          }
          auto bounds = blob_->GetBoundsRect().MakeOutset(2.f, 2.f);
          bounds.Offset(0.f, 18.f);
          Paint layer_paint;
          layer_paint.SetAlphaF(0.6f);
          canvas->SaveLayer(bounds, layer_paint);
        }
        Paint text_paint;
        text_paint.SetColor(Color_BLACK);
        text_paint.SetAlphaF(use_layer ? 1.f : 0.6f);
        canvas->DrawTextBlob(blob_, 0.f, 18.f, text_paint);
        if (use_layer) {
          canvas->Restore();
          if (nested) {
            canvas->Restore();
          }
        }
      });
    };
    auto direct = render(false);
    auto layered = render(true);
    ASSERT_FALSE(direct.empty());
    ASSERT_EQ(direct.size(), layered.size());
    size_t different_pixels = 0;
    size_t foreground_pixels = 0;
    int maximum_delta = 0;
    for (size_t offset = 0; offset < direct.size(); offset += 4) {
      // Layers reflect their rasterized content during composition. Compare
      // with mirrored upright pixels rather than changing glyph rasterization
      // by passing a negative transform to the reference text draw.
      size_t x = (offset / 4) % kWidth;
      size_t y = (offset / 4) / kWidth;
      if (flip_x) {
        x = kWidth - 1 - x;
      }
      if (flip_y) {
        y = kHeight - 1 - y;
      }
      size_t layer_offset = (y * kWidth + x) * 4;
      int delta = 0;
      for (size_t channel = 0; channel < 4; ++channel) {
        delta = std::max(
            delta, std::abs(int(direct[offset + channel]) - int(layered[layer_offset + channel])));
      }
      // The extra alpha composite may round by one channel value.
      different_pixels += delta > 1;
      maximum_delta = std::max(maximum_delta, delta);
      foreground_pixels += direct[offset] < 250;
    }
    ASSERT_GT(foreground_pixels, 30u);
    EXPECT_EQ(different_pixels, 0u) << "maximum channel delta=" << maximum_delta;
  }

  id<MTLDevice> device_;
  id<MTLCommandQueue> queue_;
  std::unique_ptr<GPUContext> context_;
  std::shared_ptr<TextBlob> blob_;
};

TEST_F(SaveLayerPixelGridTest, FractionalBoundsPreserveTextPixels) {
  @autoreleasepool {
    for (float density : {1.f, 2.f}) {
      for (float scale : {1.f, 1.25f, 2.f}) {
        for (float translation : {0.f, 0.375f}) {
          CompareText(density, scale, translation, 1, false, false);
        }
      }
    }
  }
}

TEST_F(SaveLayerPixelGridTest, NestedLayersPreserveTextPixels) {
  @autoreleasepool {
    for (float density : {1.f, 2.f}) {
      for (float scale : {1.f, 1.25f, 2.f}) {
        CompareText(density, scale, 0.375f, 1, true, false);
      }
    }
  }
}

TEST_F(SaveLayerPixelGridTest, PreservesParentClipWithMultisampling) {
  @autoreleasepool {
    for (uint32_t samples : {1u, 4u}) {
      CompareText(1.f, 1.f, 0.375f, samples, true, true);
      CompareText(2.f, 1.25f, 0.375f, samples, true, true);
    }
  }
}

TEST_F(SaveLayerPixelGridTest, ReflectedLayersPreserveTextPixels) {
  @autoreleasepool {
    for (auto reflection : {Vec2{-1.f, 1.f}, Vec2{1.f, -1.f}, Vec2{-1.f, -1.f}}) {
      for (float density : {1.f, 2.f}) {
        for (float scale : {1.f, 1.25f, 2.f}) {
          for (bool nested : {false, true}) {
            CompareText(density, scale, 0.375f, 1, nested, false, reflection.x < 0,
                        reflection.y < 0);
          }
        }
      }
    }
  }
}

TEST_F(SaveLayerPixelGridTest, ReflectedLayersPreserveParentClipWithMultisampling) {
  @autoreleasepool {
    for (auto reflection : {Vec2{-1.f, 1.f}, Vec2{1.f, -1.f}, Vec2{-1.f, -1.f}}) {
      for (uint32_t samples : {1u, 4u}) {
        CompareText(1.f, 1.f, 0.375f, samples, true, true, reflection.x < 0, reflection.y < 0);
        CompareText(2.f, 1.25f, 0.375f, samples, true, true, reflection.x < 0, reflection.y < 0);
      }
    }
  }
}

}  // namespace
}  // namespace skity
