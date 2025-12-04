// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/web/gpu_texture_web.hpp"

#include "src/gpu/web/format_web.hpp"
#include "src/gpu/web/gpu_device_web.hpp"

namespace skity {

namespace {

WGPUTextureUsage ConvertToWGPUTextureUsage(GPUTextureUsageMask usage) {
  WGPUTextureUsage wgpu_usage = WGPUTextureUsage_None;
  if (usage & static_cast<uint32_t>(GPUTextureUsage::kCopySrc)) {
    wgpu_usage |= WGPUTextureUsage_CopySrc;
  }

  if (usage & static_cast<uint32_t>(GPUTextureUsage::kCopyDst)) {
    wgpu_usage |= WGPUTextureUsage_CopyDst;
  }

  if (usage & static_cast<uint32_t>(GPUTextureUsage::kTextureBinding)) {
    wgpu_usage |= WGPUTextureUsage_TextureBinding;
    wgpu_usage |= WGPUTextureUsage_CopySrc;
  }

  if (usage & static_cast<uint32_t>(GPUTextureUsage::kStorageBinding)) {
    wgpu_usage |= WGPUTextureUsage_StorageBinding;
  }

  if (usage & static_cast<uint32_t>(GPUTextureUsage::kRenderAttachment)) {
    wgpu_usage |= WGPUTextureUsage_RenderAttachment;
  }

  return wgpu_usage;
}

WGPUTextureDescriptor ConvertToWGPUTextureDescriptor(
    const GPUTextureDescriptor& desc) {
  WGPUTextureDescriptor wgpu_desc = {};

  wgpu_desc.usage = ConvertToWGPUTextureUsage(desc.usage);

  wgpu_desc.dimension = WGPUTextureDimension_2D;
  wgpu_desc.size.width = desc.width;
  wgpu_desc.size.height = desc.height;
  wgpu_desc.size.depthOrArrayLayers = 1;

  wgpu_desc.format = ToWGPUTextureFormat(desc.format);
  wgpu_desc.mipLevelCount = desc.mip_level_count;
  wgpu_desc.sampleCount = desc.sample_count;

  wgpu_desc.viewFormatCount = 1;
  wgpu_desc.viewFormats = &wgpu_desc.format;

  return wgpu_desc;
}

}  // namespace

GPUTextureWEB::GPUTextureWEB(const GPUTextureDescriptor& descriptor,
                             GPUDeviceWEB* device, WGPUTexture texture)
    : GPUTexture(descriptor), device_(device), texture_(texture) {}

GPUTextureWEB::~GPUTextureWEB() {
  wgpuTextureViewRelease(texture_view_);
  wgpuTextureRelease(texture_);
}

size_t GPUTextureWEB::GetBytes() const {
  const auto& desc = GetDescriptor();

  if (desc.storage_mode == GPUTextureStorageMode::kMemoryless) {
    return 0;
  }

  return desc.width * desc.height * GetTextureFormatBytesPerPixel(desc.format);
}

WGPUTextureView GPUTextureWEB::GetTextureView() {
  if (texture_view_ == nullptr) {
    WGPUTextureViewDescriptor desc = {};
    desc.format = wgpuTextureGetFormat(texture_);
    desc.dimension = WGPUTextureViewDimension_2D;
    desc.baseMipLevel = 0;
    desc.mipLevelCount = 1;
    desc.baseArrayLayer = 0;
    desc.arrayLayerCount = 1;
    desc.aspect = WGPUTextureAspect_All;
    desc.usage = wgpuTextureGetUsage(texture_);

    texture_view_ = wgpuTextureCreateView(texture_, &desc);
  }

  return texture_view_;
}

void GPUTextureWEB::UploadData(uint32_t offset_x, uint32_t offset_y,
                               uint32_t width, uint32_t height, void* data) {
  if (device_ == nullptr || data == nullptr || width == 0 || height == 0) {
    return;
  }

  auto cmd = device_->CreateCommandBuffer();

  auto blit_pass = cmd->BeginBlitPass();

  blit_pass->UploadTextureData(shared_from_this(), offset_x, offset_y, width,
                               height, data);

  blit_pass->End();

  cmd->Submit();
}

std::shared_ptr<GPUTexture> GPUTextureWEB::Create(
    GPUDeviceWEB* device, const GPUTextureDescriptor& desc) {
  auto wgpu_desc = ConvertToWGPUTextureDescriptor(desc);
  WGPUTexture texture =
      wgpuDeviceCreateTexture(device->GetDevice(), &wgpu_desc);

  if (!texture) {
    return nullptr;
  }

  return std::make_shared<GPUTextureWEB>(desc, device, texture);
}

}  // namespace skity
