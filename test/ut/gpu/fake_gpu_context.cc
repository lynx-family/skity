// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "test/ut/gpu/fake_gpu_context.hpp"

#include <utility>

#include "src/gpu/gpu_blit_pass.hpp"

namespace skity::test {
namespace {

class FakeShaderFunction : public GPUShaderFunction {
 public:
  explicit FakeShaderFunction(GPULabel label)
      : GPUShaderFunction(std::move(label)) {}

  bool IsValid() const override { return true; }
};

class FakeRenderPipeline : public GPURenderPipeline {
 public:
  explicit FakeRenderPipeline(const GPURenderPipelineDescriptor& desc)
      : GPURenderPipeline(desc) {}
};

class FakeBlitPass : public GPUBlitPass {
 public:
  void UploadTextureData(std::shared_ptr<GPUTexture>, uint32_t, uint32_t,
                         uint32_t, uint32_t, void*) override {}

  void UploadBufferData(GPUBuffer*, void*, size_t) override {}

  void GenerateMipmaps(const std::shared_ptr<GPUTexture>&) override {}

  void End() override {}
};

}  // namespace

class FakeGPUDevice::FakeCommandBuffer : public GPUCommandBuffer {
 public:
  std::shared_ptr<GPURenderPass> BeginRenderPass(
      const GPURenderPassDescriptor& desc) override {
    colors_.push_back(desc.color_attachment);
    return std::make_shared<FakeRenderPass>(desc, instance_counts_,
                                            &index_counts_);
  }

  std::shared_ptr<GPUBlitPass> BeginBlitPass() override {
    return std::make_shared<FakeBlitPass>();
  }

  bool Submit(const GPUSubmitInfo* = nullptr) override { return true; }

  const std::vector<GPUColorAttachment>& colors() const { return colors_; }

  const std::vector<uint32_t>& instance_counts() const {
    return instance_counts_;
  }

  const std::vector<uint32_t>& index_counts() const { return index_counts_; }

 private:
  std::vector<GPUColorAttachment> colors_;
  std::vector<uint32_t> instance_counts_;
  std::vector<uint32_t> index_counts_;
};

FakeGPUDevice::FakeGPUDevice() {
  auto caps = std::make_unique<GPUCaps>();
  InitCaps(std::move(caps));
}

std::unique_ptr<GPUBuffer> FakeGPUDevice::CreateBuffer(
    const GPUBufferDescriptor& desc) {
  return std::make_unique<GPUBuffer>(desc);
}

std::shared_ptr<GPUShaderFunction> FakeGPUDevice::CreateShaderFunction(
    const GPUShaderFunctionDescriptor& desc) {
  shader_function_count_++;
  if (disallow_shader_pipeline_creation_) {
    disallowed_shader_function_count_++;
  }

  auto function = std::make_shared<FakeShaderFunction>(desc.label);
  if (desc.source_type == GPUShaderSourceType::kWGX &&
      desc.shader_source != nullptr) {
    auto source = static_cast<GPUShaderSourceWGX*>(desc.shader_source);
    function->SetWGXContext(source->context);
    function->SetBindGroups(
        source->module->GetProgram()->GetWGSLBindGroups(source->entry_point));
  }
  return function;
}

std::unique_ptr<GPURenderPipeline> FakeGPUDevice::CreateRenderPipeline(
    const GPURenderPipelineDescriptor& desc) {
  render_pipeline_count_++;
  if (disallow_shader_pipeline_creation_) {
    disallowed_render_pipeline_count_++;
  }
  if (fail_render_pipeline_creation_) {
    return nullptr;
  }
  if (desc.fragment_function != nullptr) {
    fragment_function_labels_.push_back(desc.fragment_function->GetLabel());
  }
  if (desc.vertex_function != nullptr) {
    vertex_function_labels_.push_back(desc.vertex_function->GetLabel());
  }
  return std::make_unique<FakeRenderPipeline>(desc);
}

std::unique_ptr<GPURenderPipeline> FakeGPUDevice::ClonePipeline(
    GPURenderPipeline*, const GPURenderPipelineDescriptor& desc) {
  clone_pipeline_count_++;
  if (disallow_shader_pipeline_creation_) {
    disallowed_clone_pipeline_count_++;
  }
  return std::make_unique<FakeRenderPipeline>(desc);
}

std::shared_ptr<GPUCommandBuffer> FakeGPUDevice::CreateCommandBuffer() {
  last_command_buffer_ = std::make_shared<FakeCommandBuffer>();
  return last_command_buffer_;
}

std::shared_ptr<GPUSampler> FakeGPUDevice::CreateSampler(
    const GPUSamplerDescriptor& desc) {
  // Real devices share sampler identity, which text draw merging relies on.
  auto [it, inserted] = samplers_.try_emplace(desc);
  if (inserted) {
    sampler_count_++;
    it->second = std::make_shared<FakeSampler>(desc);
  }
  return it->second;
}

std::shared_ptr<GPUTexture> FakeGPUDevice::CreateTexture(
    const GPUTextureDescriptor& desc) {
  texture_count_++;
  auto texture_binding =
      static_cast<GPUTextureUsageMask>(GPUTextureUsage::kTextureBinding);
  if (desc.format == GPUTextureFormat::kRGBA16Uint &&
      desc.storage_mode == GPUTextureStorageMode::kHostVisible &&
      (desc.usage & texture_binding) != 0) {
    coverage_aa_line_texture_count_++;
  }
  if (fail_texture_creation_) {
    return nullptr;
  }
  return std::make_shared<FakeGPUTexture>(desc);
}

uint32_t FakeGPUDevice::last_render_pass_count() const {
  return last_command_buffer_ == nullptr
             ? 0u
             : last_command_buffer_->colors().size();
}

const std::vector<GPUColorAttachment>& FakeGPUDevice::last_colors() const {
  return last_command_buffer_->colors();
}

std::vector<uint32_t> FakeGPUDevice::last_instance_counts() const {
  return last_command_buffer_ == nullptr
             ? std::vector<uint32_t>{}
             : last_command_buffer_->instance_counts();
}

std::vector<uint32_t> FakeGPUDevice::last_index_counts() const {
  return last_command_buffer_ == nullptr ? std::vector<uint32_t>{}
                                         : last_command_buffer_->index_counts();
}

bool FakeGPUDevice::HasFragmentFunctionLabelContaining(
    const std::string& text) const {
  for (const auto& label : fragment_function_labels_) {
    if (label.find(text) != std::string::npos) {
      return true;
    }
  }
  return false;
}

bool FakeGPUDevice::HasFragmentFunctionLabel(const std::string& text) const {
  for (const auto& label : fragment_function_labels_) {
    if (label == text) {
      return true;
    }
  }
  return false;
}

bool FakeGPUDevice::HasVertexFunctionLabelContaining(
    const std::string& text) const {
  for (const auto& label : vertex_function_labels_) {
    if (label.find(text) != std::string::npos) {
      return true;
    }
  }
  return false;
}

std::shared_ptr<GPURenderPass> FakeRootLayer::OnBeginRenderPass(
    GPUCommandBuffer* cmd, bool force_load) {
  if (last_draw_state_ != nullptr) {
    *last_draw_state_ = GetLayerDrawState();
  }

  GPUTextureDescriptor texture_desc{};
  texture_desc.width = GetWidth();
  texture_desc.height = GetHeight();
  texture_desc.format = GetColorFormat();

  auto texture = std::make_shared<FakeGPUTexture>(texture_desc);

  GPURenderPassDescriptor desc{};
  desc.color_attachment.texture = texture;
  desc.stencil_attachment.texture = texture;
  desc.depth_attachment.texture = texture;
  desc.color_attachment.load_op = (force_load || !NeedClearSurface())
                                      ? GPULoadOp::kLoad
                                      : GPULoadOp::kClear;
  desc.stencil_attachment.load_op = GPULoadOp::kClear;
  desc.depth_attachment.load_op = GPULoadOp::kClear;
  desc.color_attachment.clear_value = GetClearColor();
  desc.label = "FakeRootLayer";
  return cmd->BeginRenderPass(desc);
}

HWRootLayer* FakeGPUSurface::OnBeginNextFrame(bool clear) {
  auto* root_layer = GetArenaAllocator()->Make<FakeRootLayer>(
      static_cast<uint32_t>(GetWidth() * ContentScale()),
      static_cast<uint32_t>(GetHeight() * ContentScale()),
      Rect::MakeWH(GetWidth(), GetHeight()), GetGPUFormat(), &last_draw_state_,
      supports_texture_copy_dst_read_);
  if (allow_clear_promotion_) {
    root_layer->EnableInitialClearPromotion();
  }
  root_layer->SetClearSurface(clear);
  root_layer->SetSampleCount(GetSampleCount());
  root_layer->SetArenaAllocator(GetArenaAllocator());
  return root_layer;
}

std::unique_ptr<GPUSurface> FakeGPUContext::CreateSurface(
    GPUSurfaceDescriptor* desc) {
  return std::make_unique<FakeGPUSurface>(*desc, this,
                                          GPUTextureFormat::kRGBA8Unorm);
}

std::unique_ptr<GPUDevice> FakeGPUContext::CreateGPUDevice() {
  return std::make_unique<FakeGPUDevice>();
}

std::unique_ptr<GPURenderTarget> FakeGPUContext::OnCreateRenderTarget(
    const GPURenderTargetDescriptor& desc, std::shared_ptr<Texture> texture) {
  GPUSurfaceDescriptor surface_desc{};
  surface_desc.width = desc.width;
  surface_desc.height = desc.height;
  surface_desc.sample_count = desc.sample_count;
  surface_desc.render_options = desc.render_options;
  auto surface = std::make_unique<FakeGPUSurface>(
      surface_desc, this, GPUTextureFormat::kRGBA8Unorm);
  return std::make_unique<GPURenderTarget>(std::move(surface),
                                           std::move(texture));
}

}  // namespace skity::test
