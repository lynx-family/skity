// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/vk/gpu_surface_vk.hpp"

#include "src/gpu/vk/gpu_device_vk.hpp"
#include "src/logging.hpp"
#include "src/render/hw/layer/hw_root_layer.hpp"
#include "src/render/hw/vk/vk_root_layer.hpp"

namespace skity {

GPUSurfaceVk::GPUSurfaceVk(const GPUSurfaceDescriptor& desc,
                           GPUContextImpl* ctx)
    : GPUSurfaceImpl(desc, ctx) {}

GPUSurfaceVk::~GPUSurfaceVk() { DestroyFramebuffer(); }

bool GPUSurfaceVk::Initialize(const GPUSurfaceDescriptorVk& desc) {
  format_ = static_cast<VkFormat>(desc.vk_format);

  if (desc.surface_type == VkSurfaceType::kImage) {
    // For image-based surfaces, we need a render target texture
    // This will be implemented as needed
    return true;
  }

  LOGE("Unsupported Vulkan surface type");
  return false;
}

void GPUSurfaceVk::SetTargetTexture(std::shared_ptr<GPUTexture> texture) {
  target_texture_ = std::move(texture);
}

std::unique_ptr<GPUSurfaceVk> GPUSurfaceVk::Create(
    GPUContextImpl* ctx, const GPUSurfaceDescriptorVk& desc) {
  if (!ctx) {
    LOGE("Invalid context for Vulkan surface creation");
    return nullptr;
  }

  // Convert the Vulkan descriptor to the base descriptor
  GPUSurfaceDescriptor base_desc{};
  base_desc.backend = ctx->GetBackendType();
  base_desc.width = desc.width;
  base_desc.height = desc.height;
  base_desc.sample_count = desc.sample_count;
  base_desc.content_scale = desc.content_scale;

  auto surface = std::make_unique<GPUSurfaceVk>(base_desc, ctx);
  if (!surface->Initialize(desc)) {
    LOGE("Failed to initialize Vulkan surface");
    return nullptr;
  }

  // Create render target texture for the surface
  if (!surface->CreateRenderTarget()) {
    LOGE("Failed to create render target for Vulkan surface");
    return nullptr;
  }

  return surface;
}

HWRootLayer* GPUSurfaceVk::OnBeginNextFrame(bool clear) {
  if (!target_texture_) {
    LOGE("No target texture available for Vulkan surface");
    return nullptr;
  }

  LOGI("Creating VkExternTextureLayer for frame %dx%d", GetWidth(),
       GetHeight());
  auto root_layer = GetArenaAllocator()->Make<VkExternTextureLayer>(
      target_texture_, Rect::MakeWH(GetWidth(), GetHeight()));

  root_layer->SetClearSurface(clear);
  root_layer->SetSampleCount(GetSampleCount());
  root_layer->SetArenaAllocator(GetArenaAllocator());

  LOGI("VkExternTextureLayer created successfully");
  return root_layer;
}

void GPUSurfaceVk::OnFlush() {
  // OnFlush is called after all drawing commands have been recorded
  // The actual command submission happens in the layer's draw methods
  LOGI("GPUSurfaceVk::OnFlush() called - starting flush sequence");
}

std::shared_ptr<Pixmap> GPUSurfaceVk::ReadPixels(const Rect& rect) {
  // TODO: Implement Vulkan surface pixel reading
  return nullptr;
}

void GPUSurfaceVk::CreateFramebuffer() {
  // TODO: Implement Vulkan framebuffer creation
}

bool GPUSurfaceVk::CreateRenderTarget() {
  auto* context = GetGPUContext();
  if (!context) {
    LOGE("No context available for creating render target");
    return false;
  }

  // Create a texture to serve as the render target
  GPUTextureDescriptor tex_desc;
  tex_desc.width = GetWidth();
  tex_desc.height = GetHeight();
  tex_desc.format = GPUTextureFormat::kRGBA8Unorm;
  tex_desc.usage =
      static_cast<GPUTextureUsageMask>(GPUTextureUsage::kRenderAttachment) |
      static_cast<GPUTextureUsageMask>(GPUTextureUsage::kTextureBinding);
  tex_desc.sample_count = GetSampleCount();

  target_texture_ = context->GetGPUDevice()->CreateTexture(tex_desc);
  if (!target_texture_) {
    LOGE("Failed to create target texture for Vulkan surface");
    return false;
  }

  LOGI("Created Vulkan render target texture: %dx%d", GetWidth(), GetHeight());
  return true;
}

void GPUSurfaceVk::DestroyFramebuffer() { target_texture_.reset(); }

}  // namespace skity
