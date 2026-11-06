// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#import <Metal/Metal.h>
#include <gtest/gtest.h>
#include <skity/gpu/gpu_context_mtl.h>
#include <skity/skity.hpp>

#include <algorithm>
#include <array>
#include <functional>
#include <vector>

#include "src/gpu/gpu_context_impl.hpp"
#include "src/gpu/gpu_device.hpp"
#include "src/render/hw/draw/hw_dynamic_text_draw.hpp"
#include "src/render/hw/hw_canvas.hpp"

namespace skity {
// Test-only recording seam: exercise the actual text draw/shader without any
// font rasterizer, atlas packer or private access in a client drawing API.
class HWTextSamplingTestPeer {
 public:
  static void Record(HWCanvas& canvas, std::shared_ptr<GPUTexture> texture,
                     std::shared_ptr<GPUSampler> sampler, uint32_t x, uint32_t y,
                     TextAtlasEffect effect) {
    ArrayList<GlyphRect, 16> quads;
    quads.SetArenaAllocator(canvas.arena_allocator_);
    // Fractional quads include both overlapping and isolated coverage.
    const std::array<Vec4, 4> bounds = {{{23.61904716f, 43.80952454f, 38.47618866f, 61.7142868f},
                                         {34.66666794f, 45.33333206f, 49.52381134f, 63.23809433f},
                                         {51.80952454f, 46.85713959f, 60.57143021f, 64.76190186f},
                                         {73.90476227f, 49.52381134f, 85.7142868f, 68.19047546f}}};
    for (const auto& rect : bounds)
      quads.emplace_back(rect, Vec2{float(x), float(y)}, Vec2{float(x + 39), float(y + 47)});
    TextGeometryData::BatchedTexture textures{};
    textures[0] = std::move(texture);
    Paint paint;
    paint.SetAntiAlias(true);
    paint.SetColor(ColorSetARGB(128, 32, 64, 160));
    auto* draw = canvas.arena_allocator_->Make<HWDynamicTextDraw>(
        Matrix{}, Matrix{}, std::move(quads), paint, std::move(textures), std::move(sampler),
        TextAtlasEffect::kA8Coverage, false);
    draw->SetSampleCount(canvas.GetCanvasSampleCount());
    canvas.SetupLayerSpaceBoundsForDraw(draw, Rect::MakeXYWH(20, 40, 70, 32), Matrix{});
    canvas.SetupBlendPlanForDraw(draw, paint, true);
    canvas.CurrentLayer()->AddDraw(draw);
  }
};
namespace {
class TextAtlasSamplingTest : public ::testing::Test {
 protected:
  static constexpr uint32_t kWidth = 1008, kHeight = 420, kAtlas = 1024;
  void SetUp() override {
    device_ = MTLCreateSystemDefaultDevice();
    if (device_ == nil) GTEST_SKIP() << "Metal required";
    queue_ = [device_ newCommandQueue];
    ASSERT_NE(queue_, nil);
    context_ = MTLContextCreate(device_, queue_);
    ASSERT_NE(context_, nullptr);
  }
  std::vector<uint8_t> Render(uint32_t x, uint32_t y, bool linear, bool logical,
                              TextAtlasEffect effect) {
    auto* gpu = static_cast<GPUContextImpl*>(context_.get())->GetGPUDevice();
    GPUTextureDescriptor atlas{};
    atlas.width = atlas.height = kAtlas;
    const bool coverage = effect == TextAtlasEffect::kA8Coverage;
    const size_t channels = coverage ? 1 : 4;
    atlas.format = coverage ? GPUTextureFormat::kR8Unorm : GPUTextureFormat::kRGBA8Unorm;
    atlas.storage_mode = GPUTextureStorageMode::kHostVisible;
    atlas.usage = static_cast<GPUTextureUsageMask>(GPUTextureUsage::kTextureBinding) |
                  static_cast<GPUTextureUsageMask>(GPUTextureUsage::kCopyDst);
    auto texture = gpu->CreateTexture(atlas);
    if (!texture) {
      ADD_FAILURE() << "Atlas unavailable";
      return {};
    }
    std::vector<uint8_t> pixels(kAtlas * kAtlas * channels, 0);
    for (uint32_t row = 2; row < 45; ++row)
      for (uint32_t col = 2; col < 37; ++col) {
        auto* pixel = pixels.data() + ((y + row) * kAtlas + x + col) * channels;
        const auto alpha = uint8_t((17 * col + 29 * row) % 256);
        if (coverage) {
          pixel[0] = alpha;
          continue;
        }
        pixel[0] = alpha / 2;
        pixel[1] = alpha / 3;
        pixel[2] = alpha / 4;
        pixel[3] = alpha;
        if (effect == TextAtlasEffect::kColorSwizzleRB) std::swap(pixel[0], pixel[2]);
      }
    texture->UploadData(0, 0, kAtlas, kAtlas, pixels.data());
    GPUSamplerDescriptor sampling;
    sampling.min_filter = sampling.mag_filter =
        linear ? GPUFilterMode::kLinear : GPUFilterMode::kNearest;
    auto sampler = gpu->CreateSampler(sampling);
    if (!sampler) {
      ADD_FAILURE() << "Sampler unavailable";
      return {};
    }
    auto desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                   width:kWidth
                                                                  height:kHeight
                                                               mipmapped:NO];
    desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    desc.storageMode = MTLStorageModeShared;
    id<MTLTexture> output = [device_ newTextureWithDescriptor:desc];
    if (output == nil) {
      ADD_FAILURE() << "Target unavailable";
      return {};
    }
    GPUSurfaceDescriptorMTL target;
    target.backend = GPUBackendType::kMetal;
    target.surface_type = MTLSurfaceType::kTexture;
    target.texture = output;
    target.width = logical ? 384 : kWidth;
    target.height = logical ? 160 : kHeight;
    target.content_scale = 2.625f;
    target.sample_count = 1;
    auto surface = context_->CreateSurface(&target);
    if (!surface) {
      ADD_FAILURE() << "Surface unavailable";
      return {};
    }
    auto* canvas = surface->LockCanvas();
    if (!canvas) {
      ADD_FAILURE() << "Canvas unavailable";
      return {};
    }
    canvas->Clear(Color_WHITE);
    HWTextSamplingTestPeer::Record(static_cast<HWCanvas&>(*canvas), std::move(texture),
                                   std::move(sampler), x, y, effect);
    canvas->Flush();
    surface->Flush();
    auto completion = [queue_ commandBuffer];
    if (completion == nil) {
      ADD_FAILURE() << "Completion unavailable";
      return {};
    }
    [completion commit];
    [completion waitUntilCompleted];
    if (completion.status != MTLCommandBufferStatusCompleted || completion.error != nil) {
      ADD_FAILURE() << "GPU completion failed";
      return {};
    }
    std::vector<uint8_t> rgba(kWidth * kHeight * 4);
    [output getBytes:rgba.data()
         bytesPerRow:kWidth * 4
          fromRegion:MTLRegionMake2D(0, 0, kWidth, kHeight)
         mipmapLevel:0];
    surface.reset();
    return rgba;
  }
  void Compare(bool linear, TextAtlasEffect effect = TextAtlasEffect::kA8Coverage) {
    unsigned nonidentical = 0;
    for (bool logical : {false, true}) {
      const auto reference = Render(0, 0, linear, logical, effect);
      ASSERT_EQ(reference.size(), kWidth * kHeight * 4);
      ASSERT_TRUE(std::any_of(reference.begin(), reference.end(), [](auto n) { return n != 255; }));
      for (const auto origin :
           {std::pair<uint32_t, uint32_t>{514, 902}, {1, 1}, {997 - 39, 973 - 47}}) {
        SCOPED_TRACE(::testing::Message() << "linear=" << linear << " logical=" << logical
                                          << " origin=" << origin.first << "," << origin.second);
        const auto actual = Render(origin.first, origin.second, linear, logical, effect);
        ASSERT_EQ(actual.size(), reference.size());
        size_t differences = 0;
        for (size_t i = 0; i < actual.size(); ++i) differences += actual[i] != reference[i];
        if (differences) ++nonidentical;
        EXPECT_EQ(differences, 0u) << "Identical texels depend on their atlas origin";
      }
    }
    RecordProperty("nonidentical_origins", nonidentical);
    RecordProperty("origin_comparisons", 6);
  }
  id<MTLDevice> device_;
  id<MTLCommandQueue> queue_;
  std::unique_ptr<GPUContext> context_;
};
TEST_F(TextAtlasSamplingTest, LinearCoverageIsIndependentOfAtlasOrigin) { Compare(true); }
TEST_F(TextAtlasSamplingTest, NearestCoverageIsIndependentOfAtlasOrigin) { Compare(false); }
TEST_F(TextAtlasSamplingTest, LinearColorIsIndependentOfAtlasOrigin) {
  Compare(true, TextAtlasEffect::kColor);
}
TEST_F(TextAtlasSamplingTest, NearestColorIsIndependentOfAtlasOrigin) {
  Compare(false, TextAtlasEffect::kColor);
}
TEST_F(TextAtlasSamplingTest, LinearSwizzledColorIsIndependentOfAtlasOrigin) {
  Compare(true, TextAtlasEffect::kColorSwizzleRB);
}
TEST_F(TextAtlasSamplingTest, NearestSwizzledColorIsIndependentOfAtlasOrigin) {
  Compare(false, TextAtlasEffect::kColorSwizzleRB);
}
}  // namespace
}  // namespace skity
