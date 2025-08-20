// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/vk/vk_root_layer.hpp"

#include "src/gpu/gpu_context_impl.hpp"
#include "src/gpu/vk/gpu_texture_vk.hpp"
#include "src/logging.hpp"

namespace skity {

VkRootLayer::VkRootLayer(uint32_t width, uint32_t height, const Rect &bounds)
    : HWRootLayer(width, height, bounds, GPUTextureFormat::kRGBA8Unorm) {}

void VkRootLayer::Draw(GPURenderPass *render_pass) {
  LOGI("VkRootLayer::Draw() called - starting Vulkan rendering");
  // Call the base class implementation which contains the actual drawing logic
  HWRootLayer::Draw(render_pass);
  LOGI("VkRootLayer::Draw() completed");
}

void VkRootLayer::OnPostDraw(GPURenderPass *render_pass,
                             GPUCommandBuffer *cmd) {
  // Post-draw cleanup - for Vulkan this could include command buffer submission
  // For now, we rely on the command buffer's automatic submission
}

VkExternTextureLayer::VkExternTextureLayer(std::shared_ptr<GPUTexture> texture,
                                           const Rect &bounds)
    : VkRootLayer(texture->GetDescriptor().width,
                  texture->GetDescriptor().height, bounds),
      ext_texture_(std::move(texture)) {}

HWDrawState VkExternTextureLayer::OnPrepare(HWDrawContext *context) {
  auto ret = VkRootLayer::OnPrepare(context);

  // Set up render pass descriptor for the external texture
  render_pass_desc_.color_attachment.texture = ext_texture_;
  render_pass_desc_.color_attachment.load_op = GPULoadOp::kDontCare;
  render_pass_desc_.color_attachment.store_op = GPUStoreOp::kStore;
  render_pass_desc_.color_attachment.clear_value = {
      1.0f, 1.0f, 1.0f, 1.0f};  // White background to match GL behavior

  // Create proper depth/stencil textures like GL backend
  CreateDepthStencilTextures(context->gpuContext->GetGPUDevice());

  // Set up stencil and depth attachments like GL backend for path rendering
  if (depth_stencil_texture_) {
    render_pass_desc_.stencil_attachment.texture = depth_stencil_texture_;
    render_pass_desc_.stencil_attachment.load_op = GPULoadOp::kClear;
    render_pass_desc_.stencil_attachment.store_op = GPUStoreOp::kDiscard;
    render_pass_desc_.stencil_attachment.clear_value = 0;
    render_pass_desc_.depth_attachment.texture = depth_stencil_texture_;
    render_pass_desc_.depth_attachment.load_op = GPULoadOp::kClear;
    render_pass_desc_.depth_attachment.store_op = GPUStoreOp::kDiscard;
    render_pass_desc_.depth_attachment.clear_value = 0.0f;
  }

  return ret;
}

std::shared_ptr<GPURenderPass> VkExternTextureLayer::OnBeginRenderPass(
    GPUCommandBuffer *cmd) {
  LOGI("VkExternTextureLayer::OnBeginRenderPass called");
  if (!cmd || !ext_texture_) {
    LOGE(
        "Invalid command buffer or texture for Vulkan render pass - cmd: %p, "
        "texture: %p",
        cmd, ext_texture_.get());
    return nullptr;
  }

  LOGI("Starting Vulkan render pass with texture %dx%d",
       ext_texture_->GetDescriptor().width,
       ext_texture_->GetDescriptor().height);

  auto render_pass = cmd->BeginRenderPass(render_pass_desc_);
  if (!render_pass) {
    LOGE("Failed to begin Vulkan render pass");
    return nullptr;
  }

  LOGI("Vulkan render pass created successfully");
  return render_pass;
}

void VkExternTextureLayer::CreateDepthStencilTextures(GPUDevice *device) {
  if (!ext_texture_ || !device) {
    return;
  }

  // Create depth/stencil texture with same dimensions as color texture
  const auto &color_desc = ext_texture_->GetDescriptor();

  GPUTextureDescriptor depth_stencil_desc = {};
  depth_stencil_desc.width = color_desc.width;
  depth_stencil_desc.height = color_desc.height;
  depth_stencil_desc.format =
      GPUTextureFormat::kDepth24Stencil8;  // Combined depth-stencil format
  depth_stencil_desc.usage =
      static_cast<GPUTextureUsageMask>(GPUTextureUsage::kRenderAttachment);
  depth_stencil_desc.storage_mode =
      GPUTextureStorageMode::kPrivate;  // GPU-only
  depth_stencil_desc.mip_level_count = 1;
  depth_stencil_desc.sample_count = 1;

  // Create the depth/stencil texture using the GPU device
  depth_stencil_texture_ = device->CreateTexture(depth_stencil_desc);

  if (depth_stencil_texture_) {
    LOGI("Depth/stencil texture created successfully");
  } else {
    LOGE("Failed to create depth/stencil texture");
  }
}

}  // namespace skity