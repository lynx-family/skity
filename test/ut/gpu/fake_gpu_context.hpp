// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef TEST_UT_GPU_FAKE_GPU_CONTEXT_HPP
#define TEST_UT_GPU_FAKE_GPU_CONTEXT_HPP

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "src/gpu/gpu_context_impl.hpp"
#include "src/gpu/gpu_device.hpp"
#include "src/gpu/gpu_render_pass.hpp"
#include "src/gpu/gpu_surface_impl.hpp"
#include "src/render/hw/layer/hw_root_layer.hpp"

namespace skity::test {

class FakeSampler : public GPUSampler {
 public:
  explicit FakeSampler(const GPUSamplerDescriptor& desc) : GPUSampler(desc) {}

  ~FakeSampler() override = default;
};

class FakeGPUTexture : public GPUTexture {
 public:
  explicit FakeGPUTexture(const GPUTextureDescriptor& desc)
      : GPUTexture(desc) {}

  ~FakeGPUTexture() override = default;

  size_t GetBytes() const override {
    return desc_.width * desc_.height *
           GetTextureFormatBytesPerPixel(desc_.format);
  }

  void UploadData(uint32_t, uint32_t, uint32_t, uint32_t, void*) override {}
};

class FakeRenderPass : public GPURenderPass {
 public:
  FakeRenderPass(const GPURenderPassDescriptor& desc,
                 std::vector<uint32_t>& instance_counts,
                 std::vector<uint32_t>* index_counts = nullptr)
      : GPURenderPass(desc),
        instance_counts_(instance_counts),
        index_counts_(index_counts) {}

  void EncodeCommands(std::optional<GPUViewport> = std::nullopt,
                      std::optional<GPUScissorRect> = std::nullopt) override {
    for (const auto* command : GetCommands()) {
      instance_counts_.push_back(command->instance_count);
      if (index_counts_) {
        index_counts_->push_back(command->index_count);
      }
    }
  }

 private:
  std::vector<uint32_t>& instance_counts_;
  std::vector<uint32_t>* index_counts_;
};

class FakeGPUDevice : public GPUDevice {
 public:
  using GPUDevice::InitCaps;

  FakeGPUDevice();

  std::unique_ptr<GPUBuffer> CreateBuffer(
      const GPUBufferDescriptor& desc) override;

  std::shared_ptr<GPUShaderFunction> CreateShaderFunction(
      const GPUShaderFunctionDescriptor& desc) override;

  std::unique_ptr<GPURenderPipeline> CreateRenderPipeline(
      const GPURenderPipelineDescriptor& desc) override;

  std::unique_ptr<GPURenderPipeline> ClonePipeline(
      GPURenderPipeline*, const GPURenderPipelineDescriptor& desc) override;

  std::shared_ptr<GPUCommandBuffer> CreateCommandBuffer() override;

  std::shared_ptr<GPUSampler> CreateSampler(
      const GPUSamplerDescriptor& desc) override;

  std::shared_ptr<GPUTexture> CreateTexture(
      const GPUTextureDescriptor& desc) override;

  bool CanUseMSAA() override { return true; }

  uint32_t GetBufferAlignment() override { return 256; }

  uint32_t GetMaxTextureSize() override { return 4096; }

  uint32_t shader_function_count() const { return shader_function_count_; }

  uint32_t render_pipeline_count() const { return render_pipeline_count_; }

  uint32_t clone_pipeline_count() const { return clone_pipeline_count_; }

  uint32_t texture_count() const { return texture_count_; }

  uint32_t coverage_aa_line_texture_count() const {
    return coverage_aa_line_texture_count_;
  }

  uint32_t sampler_count() const { return sampler_count_; }

  uint32_t last_render_pass_count() const;

  const std::vector<GPUColorAttachment>& last_colors() const;

  std::vector<uint32_t> last_instance_counts() const;

  std::vector<uint32_t> last_index_counts() const;

  uint32_t disallowed_shader_function_count() const {
    return disallowed_shader_function_count_;
  }

  uint32_t disallowed_render_pipeline_count() const {
    return disallowed_render_pipeline_count_;
  }

  uint32_t disallowed_clone_pipeline_count() const {
    return disallowed_clone_pipeline_count_;
  }

  bool HasFragmentFunctionLabelContaining(const std::string& text) const;

  bool HasFragmentFunctionLabel(const std::string& text) const;

  bool HasVertexFunctionLabelContaining(const std::string& text) const;

  void set_fail_render_pipeline_creation(bool fail) {
    fail_render_pipeline_creation_ = fail;
  }

  void set_fail_texture_creation(bool fail) { fail_texture_creation_ = fail; }

  void set_disallow_shader_pipeline_creation(bool disallow) {
    disallow_shader_pipeline_creation_ = disallow;
    disallowed_shader_function_count_ = 0;
    disallowed_render_pipeline_count_ = 0;
    disallowed_clone_pipeline_count_ = 0;
  }

 private:
  class FakeCommandBuffer;

  uint32_t shader_function_count_ = 0;
  uint32_t render_pipeline_count_ = 0;
  uint32_t clone_pipeline_count_ = 0;
  uint32_t disallowed_shader_function_count_ = 0;
  uint32_t disallowed_render_pipeline_count_ = 0;
  uint32_t disallowed_clone_pipeline_count_ = 0;
  uint32_t texture_count_ = 0;
  uint32_t coverage_aa_line_texture_count_ = 0;
  uint32_t sampler_count_ = 0;
  GPUSamplerMap samplers_;
  std::vector<std::string> fragment_function_labels_;
  std::vector<std::string> vertex_function_labels_;
  std::shared_ptr<FakeCommandBuffer> last_command_buffer_;
  bool disallow_shader_pipeline_creation_ = false;
  bool fail_render_pipeline_creation_ = false;
  bool fail_texture_creation_ = false;
};

class FakeRootLayer : public HWRootLayer {
 public:
  FakeRootLayer(uint32_t width, uint32_t height, const Rect& bounds,
                GPUTextureFormat format, HWDrawState* last_draw_state,
                bool supports_texture_copy_dst_read)
      : HWRootLayer(width, height, bounds, format),
        last_draw_state_(last_draw_state),
        supports_texture_copy_dst_read_(supports_texture_copy_dst_read) {}

  bool SupportsTextureCopyDstRead() const override {
    return supports_texture_copy_dst_read_;
  }

 private:
  std::shared_ptr<GPURenderPass> OnBeginRenderPass(GPUCommandBuffer* cmd,
                                                   bool force_load) override;

  void OnPostDraw(GPURenderPass*, GPUCommandBuffer*) override {}

  bool OnCopyToDstTexture(GPUCommandBuffer*, std::shared_ptr<GPUTexture>,
                          GPURegion) const override {
    return true;
  }

 private:
  HWDrawState* last_draw_state_ = nullptr;
  bool supports_texture_copy_dst_read_ = true;
};

class FakeGPUSurface : public GPUSurfaceImpl {
 public:
  FakeGPUSurface(const GPUSurfaceDescriptor& desc, GPUContextImpl* ctx,
                 GPUTextureFormat format, bool allow_clear_promotion = false)
      : GPUSurfaceImpl(desc, ctx),
        format_(format),
        allow_clear_promotion_(allow_clear_promotion) {}

  GPUTextureFormat GetGPUFormat() const override { return format_; }

  std::shared_ptr<Pixmap> ReadPixels(const Rect&) override { return nullptr; }

  HWDrawState last_draw_state() const { return last_draw_state_; }

  void SetSupportsTextureCopyDstRead(bool supports) {
    supports_texture_copy_dst_read_ = supports;
  }

 protected:
  HWRootLayer* OnBeginNextFrame(bool clear) override;

  void OnFlush() override {}

 private:
  GPUTextureFormat format_;
  bool allow_clear_promotion_;
  HWDrawState last_draw_state_ = HWDrawState::kDrawStateNone;
  bool supports_texture_copy_dst_read_ = true;
};

class FakeGPUContext : public GPUContextImpl {
 public:
  FakeGPUContext() : GPUContextImpl(GPUBackendType::kNone) {}

  FakeGPUDevice* device() const {
    return static_cast<FakeGPUDevice*>(GetGPUDevice());
  }

  std::unique_ptr<GPUSurface> CreateSurface(
      GPUSurfaceDescriptor* desc) override;

 protected:
  std::unique_ptr<GPUDevice> CreateGPUDevice() override;

  std::shared_ptr<GPUTexture> OnWrapTexture(GPUBackendTextureInfo*,
                                            ReleaseCallback,
                                            ReleaseUserData) override {
    return nullptr;
  }

  std::unique_ptr<GPURenderTarget> OnCreateRenderTarget(
      const GPURenderTargetDescriptor& desc,
      std::shared_ptr<Texture> texture) override;

  std::shared_ptr<Data> OnReadPixels(
      const std::shared_ptr<GPUTexture>&) const override {
    return nullptr;
  }
};

}  // namespace skity::test

#endif  // TEST_UT_GPU_FAKE_GPU_CONTEXT_HPP
