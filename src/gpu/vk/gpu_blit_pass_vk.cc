// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/vk/gpu_blit_pass_vk.hpp"

#include "src/gpu/vk/gpu_buffer_vk.hpp"
#include "src/gpu/vk/gpu_command_buffer_vk.hpp"
#include "src/gpu/vk/gpu_device_vk.hpp"
#include "src/gpu/vk/gpu_texture_vk.hpp"
#include "src/logging.hpp"

namespace skity {

GPUBlitPassVk::GPUBlitPassVk(GPUCommandBufferVk* command_buffer)
    : command_buffer_(command_buffer) {}

GPUBlitPassVk::~GPUBlitPassVk() = default;

void GPUBlitPassVk::UploadTextureData(std::shared_ptr<GPUTexture> texture,
                                      uint32_t offset_x, uint32_t offset_y,
                                      uint32_t width, uint32_t height,
                                      void* data) {
  if (!texture || !data) {
    LOGE("Invalid parameters for texture data upload");
    return;
  }

  auto* texture_vk = static_cast<GPUTextureVk*>(texture.get());
  auto* device = command_buffer_->GetDevice();

  if (!texture_vk || !device) {
    LOGE("Invalid texture or device for upload");
    return;
  }

  // Use the texture's built-in upload functionality
  texture_vk->UploadData(device, offset_x, offset_y, width, height, data);
  LOGI("Uploaded texture data: %ux%u at (%u, %u)", width, height, offset_x,
       offset_y);
}

void GPUBlitPassVk::UploadBufferData(GPUBuffer* buffer, void* data,
                                     size_t size) {
  if (!buffer || !data || size == 0) {
    LOGE("Invalid parameters for buffer data upload");
    return;
  }

  auto* buffer_vk = static_cast<GPUBufferVk*>(buffer);
  if (!buffer_vk) {
    LOGE("Invalid buffer type for upload");
    return;
  }

  // Use the buffer's built-in upload functionality
  buffer_vk->UploadData(data, size);
  LOGI("Uploaded buffer data: %zu bytes", size);
}

void GPUBlitPassVk::End() {
  // The blit pass operations are immediately executed, so no cleanup is needed
  // In a more sophisticated implementation, we might batch operations here
  LOGI("Blit pass completed");
}

}  // namespace skity