// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/web/gpu_context_impl_web.hpp"

#include <skity/gpu/gpu_context_web.hpp>

#include "src/gpu/web/gpu_device_web.hpp"
#include "src/gpu/web/gpu_surface_web.hpp"
#include "src/gpu/web/gpu_texture_web.hpp"

namespace skity {

std::unique_ptr<GPUContext> WebContextCreate(WGPUDevice device,
                                             WGPUQueue queue) {
  if (!device || !queue) {
    return nullptr;
  }

  auto ctx = std::make_unique<GPUContextImplWEB>(device, queue);

  ctx->Init();

  return ctx;
}

GPUContextImplWEB::GPUContextImplWEB(WGPUDevice device, WGPUQueue queue)
    : GPUContextImpl(GPUBackendType::kWebGPU), device_(device), queue_(queue) {
  wgpuDeviceAddRef(device_);
  wgpuQueueAddRef(queue_);
}

GPUContextImplWEB::~GPUContextImplWEB() {
  wgpuQueueRelease(queue_);
  wgpuDeviceRelease(device_);
}

std::unique_ptr<GPUSurface> GPUContextImplWEB::CreateSurface(
    GPUSurfaceDescriptor *desc) {
  if (desc == nullptr || desc->backend != GPUBackendType::kWebGPU) {
    return {};
  }

  auto desc_web = static_cast<GPUSurfaceDescriptorWEB *>(desc);

  if (!desc_web->texture) {
    return {};
  }

  return std::make_unique<GPUSurfaceImplWEB>(*desc, this, desc_web->texture);
}

std::unique_ptr<GPUSurface> GPUContextImplWEB::CreateFxaaSurface(
    GPUSurfaceDescriptor *desc) {
  return {};
}

std::unique_ptr<GPUDevice> GPUContextImplWEB::CreateGPUDevice() {
  return std::make_unique<GPUDeviceWEB>(device_, queue_);
}

std::shared_ptr<GPUTexture> GPUContextImplWEB::OnWrapTexture(
    GPUBackendTextureInfo *info, ReleaseCallback callback,
    ReleaseUserData user_data) {
  if (info == nullptr || info->backend != GPUBackendType::kWebGPU) {
    return {};
  }

  auto web_info = static_cast<GPUBackendTextureInfoWEB *>(info);

  if (!web_info->texture) {
    return {};
  }

  GPUTextureDescriptor descriptor{};
  descriptor.width = web_info->width;
  descriptor.height = web_info->height;
  descriptor.format = static_cast<GPUTextureFormat>(web_info->format);
  descriptor.usage =
      static_cast<GPUTextureUsageMask>(GPUTextureUsage::kTextureBinding);
  descriptor.storage_mode = GPUTextureStorageMode::kPrivate;

  // add ref for the texture
  wgpuTextureAddRef(web_info->texture);

  auto texture = std::make_shared<GPUTextureWEB>(descriptor, web_info->texture);

  texture->SetRelease(callback, user_data);

  return texture;
}

std::unique_ptr<GPURenderTarget> GPUContextImplWEB::OnCreateRenderTarget(
    const GPURenderTargetDescriptor &desc, std::shared_ptr<Texture> texture) {
  return {};
}

std::shared_ptr<Data> GPUContextImplWEB::OnReadPixels(
    const std::shared_ptr<GPUTexture> &texture) const {
  return {};
}

}  // namespace skity
