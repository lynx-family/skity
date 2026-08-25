// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <memory>
#include <skity/geometry/rect.hpp>

#include "src/gpu/gpu_context_impl.hpp"
#include "src/gpu/gpu_device.hpp"
#include "src/render/hw/layer/hw_sub_layer.hpp"
#include "src/utils/arena_allocator.hpp"

namespace skity {
namespace {

// A GPU device whose texture creation always fails (e.g. GPU memory pressure
// or device loss). Every other resource API returns a null placeholder, since
// the layer discard path never reaches them.
class FailingGPUDevice : public GPUDevice {
 public:
  std::unique_ptr<GPUBuffer> CreateBuffer(const GPUBufferDescriptor&) override {
    return {};
  }

  std::shared_ptr<GPUShaderFunction> CreateShaderFunction(
      const GPUShaderFunctionDescriptor&) override {
    return {};
  }

  std::unique_ptr<GPURenderPipeline> CreateRenderPipeline(
      const GPURenderPipelineDescriptor&) override {
    return {};
  }

  std::unique_ptr<GPURenderPipeline> ClonePipeline(
      GPURenderPipeline*, const GPURenderPipelineDescriptor&) override {
    return {};
  }

  std::shared_ptr<GPUCommandBuffer> CreateCommandBuffer() override {
    return {};
  }

  std::shared_ptr<GPUSampler> CreateSampler(
      const GPUSamplerDescriptor&) override {
    return {};
  }

  std::shared_ptr<GPUTexture> CreateTexture(
      const GPUTextureDescriptor&) override {
    return {};
  }

  bool CanUseMSAA() override { return false; }

  uint32_t GetBufferAlignment() override { return 0; }

  uint32_t GetMaxTextureSize() override { return 4096; }
};

class FailingGPUContext : public GPUContextImpl {
 public:
  FailingGPUContext() : GPUContextImpl(GPUBackendType::kNone) {}

  ~FailingGPUContext() override = default;

 protected:
  std::unique_ptr<GPUSurface> CreateSurface(GPUSurfaceDescriptor*) override {
    return {};
  }

  std::unique_ptr<GPUDevice> CreateGPUDevice() override {
    return std::make_unique<FailingGPUDevice>();
  }

  std::shared_ptr<GPUTexture> OnWrapTexture(GPUBackendTextureInfo*,
                                            ReleaseCallback,
                                            ReleaseUserData) override {
    return {};
  }

  std::unique_ptr<GPURenderTarget> OnCreateRenderTarget(
      const GPURenderTargetDescriptor&, std::shared_ptr<Texture>) override {
    return {};
  }

  std::shared_ptr<Data> OnReadPixels(
      const std::shared_ptr<GPUTexture>&) const override {
    return {};
  }
};

// Minimal HWLayer that hosts child draws without a texture backend, used to
// verify that a child's prepare error is propagated to the parent.
class TestParentLayer : public HWLayer {
 public:
  TestParentLayer(const Rect& bounds, uint32_t width, uint32_t height)
      : HWLayer(Matrix{}, 0, bounds, width, height) {}

 protected:
  std::shared_ptr<GPURenderPass> OnBeginRenderPass(GPUCommandBuffer*,
                                                   bool) override {
    return {};
  }

  void OnPostDraw(GPURenderPass*, GPUCommandBuffer*) override {}
};

// Stub draw that always returns the given prepare state.
class StubDraw : public HWDraw {
 public:
  explicit StubDraw(HWDrawState state) : HWDraw(Matrix{}), state_(state) {}

  void Draw(GPURenderPass*, GPUCommandBuffer*) override {}

 protected:
  HWDrawState OnPrepare(HWDrawContext*) override { return state_; }

  void OnGenerateCommand(HWDrawContext*, HWDrawState) override {}

 private:
  HWDrawState state_;
};

// Minimal GPUTexture that does not touch a real GPU, used to give a sub
// layer a valid color/back texture so its own texture guard does not trigger.
class TestGPUTexture : public GPUTexture {
 public:
  explicit TestGPUTexture(const GPUTextureDescriptor& desc)
      : GPUTexture(desc) {}

  size_t GetBytes() const override { return 0; }

  void UploadData(uint32_t, uint32_t, uint32_t, uint32_t, void*) override {}
};

// HWSubLayer that lets tests inject a texture without a working device.
class TestSubLayer : public HWSubLayer {
 public:
  using HWSubLayer::HWSubLayer;

  void SetLayerTexture(const std::shared_ptr<GPUTexture>& texture) {
    SetTextures(texture, texture);
  }
};

}  // namespace
}  // namespace skity

// Regression test for an online crash in
// HWLayer::CreateDrawLayerShader (hw_layer.cc):
// when the layer back texture cannot be created, HWSubLayer must discard the
// whole layer instead of dereferencing a null texture while building the layer
// back shader / render pass.
TEST(HWSubLayerDiscard, SkipsLayerWhenBackTextureCreationFails) {
  skity::FailingGPUContext context;
  ASSERT_TRUE(context.Init());

  skity::HWSubLayer layer(skity::Matrix{}, 1,
                          skity::Rect::MakeXYWH(0.f, 0.f, 100.f, 100.f), 100,
                          100);
  layer.SetClipDepth(0);

  skity::HWDrawContext draw_context;
  draw_context.total_clip_depth = 1;
  draw_context.gpuContext = &context;

  // Prepare must not crash and must report that the layer is skipped.
  auto state = layer.Prepare(&draw_context);
  EXPECT_NE(state & skity::HWDrawState::kDrawStateError,
            skity::HWDrawState::kDrawStateNone);

  EXPECT_NO_FATAL_FAILURE(layer.GenerateCommand(&draw_context, state));
  EXPECT_NO_FATAL_FAILURE(layer.Draw(nullptr, nullptr));
}

// The render target cache must survive a failed texture allocation even when
// the cache is enabled: byte/key accounting must not dereference the null
// texture (HWRenderTarget::GetBytes/GetKey).
TEST(HWSubLayerDiscard, RenderTargetCacheHandlesAllocationFailure) {
  skity::FailingGPUDevice device;
  auto cache = skity::HWRenderTargetCache::Create(&device);

  skity::GPUTextureDescriptor desc;
  desc.width = 100;
  desc.height = 100;
  desc.format = skity::GPUTextureFormat::kRGBA8Unorm;
  desc.sample_count = 1;

  auto resource = cache->ObtainResource(desc);
  ASSERT_NE(resource, nullptr);
  EXPECT_EQ(resource->GetValue(), nullptr);
  EXPECT_EQ(resource->GetBytes(), static_cast<size_t>(0));
}

// A failing child must make its parent's prepare report kDrawStateError, so the
// error can keep bubbling up to the root layer.
TEST(HWSubLayerDiscard, PropagatesChildPrepareErrorToParent) {
  skity::FailingGPUContext context;
  ASSERT_TRUE(context.Init());

  skity::ArenaAllocator arena;
  skity::TestParentLayer parent(skity::Rect::MakeXYWH(0.f, 0.f, 100.f, 100.f),
                                100, 100);
  parent.SetArenaAllocator(&arena);
  parent.SetClipDepth(0);

  skity::StubDraw failing_draw(skity::HWDrawState::kDrawStateError);
  failing_draw.SetLayerSpaceBounds(skity::Rect::MakeWH(100.f, 100.f));
  parent.AddDraw(&failing_draw);

  skity::HWDrawContext draw_context;
  draw_context.total_clip_depth = 1;
  draw_context.gpuContext = &context;
  draw_context.arena_allocator = &arena;

  auto state = parent.Prepare(&draw_context);
  EXPECT_NE(state & skity::HWDrawState::kDrawStateError,
            skity::HWDrawState::kDrawStateNone);
}

// A parent whose children all prepare successfully must NOT report an error.
TEST(HWSubLayerDiscard, ParentHasNoErrorWhenChildrenPrepareFine) {
  skity::FailingGPUContext context;
  ASSERT_TRUE(context.Init());

  skity::ArenaAllocator arena;
  skity::TestParentLayer parent(skity::Rect::MakeXYWH(0.f, 0.f, 100.f, 100.f),
                                100, 100);
  parent.SetArenaAllocator(&arena);
  parent.SetClipDepth(0);

  skity::StubDraw passing_draw(skity::HWDrawState::kDrawStateNone);
  passing_draw.SetLayerSpaceBounds(skity::Rect::MakeWH(100.f, 100.f));
  parent.AddDraw(&passing_draw);

  skity::HWDrawContext draw_context;
  draw_context.total_clip_depth = 1;
  draw_context.gpuContext = &context;
  draw_context.arena_allocator = &arena;

  auto state = parent.Prepare(&draw_context);
  EXPECT_EQ(state & skity::HWDrawState::kDrawStateError,
            skity::HWDrawState::kDrawStateNone);
}

// A sub layer with a valid texture that still aggregates a failing child's
// prepare error and reports it upward.
TEST(HWSubLayerDiscard, SubLayerPropagatesChildPrepareErrorUpward) {
  skity::FailingGPUContext context;
  ASSERT_TRUE(context.Init());

  skity::ArenaAllocator arena;

  skity::GPUTextureDescriptor desc;
  desc.width = 100;
  desc.height = 100;
  desc.format = skity::GPUTextureFormat::kRGBA8Unorm;
  auto texture = std::make_shared<skity::TestGPUTexture>(desc);

  skity::TestSubLayer layer(skity::Matrix{}, 1,
                            skity::Rect::MakeXYWH(0.f, 0.f, 100.f, 100.f), 100,
                            100);
  layer.SetArenaAllocator(&arena);
  layer.SetClipDepth(0);
  layer.SetLayerTexture(texture);

  skity::StubDraw failing_draw(skity::HWDrawState::kDrawStateError);
  failing_draw.SetLayerSpaceBounds(skity::Rect::MakeWH(100.f, 100.f));
  layer.AddDraw(&failing_draw);

  skity::HWDrawContext draw_context;
  draw_context.total_clip_depth = 1;
  draw_context.gpuContext = &context;
  draw_context.arena_allocator = &arena;

  auto state = layer.Prepare(&draw_context);
  EXPECT_NE(state & skity::HWDrawState::kDrawStateError,
            skity::HWDrawState::kDrawStateNone);
}

// A sub layer with a valid texture and healthy children must not report an
// error (proves the error above comes from the child, not the layer texture).
TEST(HWSubLayerDiscard, SubLayerHasNoErrorWhenChildrenPrepareFine) {
  skity::FailingGPUContext context;
  ASSERT_TRUE(context.Init());

  skity::ArenaAllocator arena;

  skity::GPUTextureDescriptor desc;
  desc.width = 100;
  desc.height = 100;
  desc.format = skity::GPUTextureFormat::kRGBA8Unorm;
  auto texture = std::make_shared<skity::TestGPUTexture>(desc);

  skity::TestSubLayer layer(skity::Matrix{}, 1,
                            skity::Rect::MakeXYWH(0.f, 0.f, 100.f, 100.f), 100,
                            100);
  layer.SetArenaAllocator(&arena);
  layer.SetClipDepth(0);
  layer.SetLayerTexture(texture);

  skity::StubDraw passing_draw(skity::HWDrawState::kDrawStateNone);
  passing_draw.SetLayerSpaceBounds(skity::Rect::MakeWH(100.f, 100.f));
  layer.AddDraw(&passing_draw);

  skity::HWDrawContext draw_context;
  draw_context.total_clip_depth = 1;
  draw_context.gpuContext = &context;
  draw_context.arena_allocator = &arena;

  auto state = layer.Prepare(&draw_context);
  EXPECT_EQ(state & skity::HWDrawState::kDrawStateError,
            skity::HWDrawState::kDrawStateNone);
}
