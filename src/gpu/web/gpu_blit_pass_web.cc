// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/web/gpu_blit_pass_web.hpp"

#include <cstring>

#include "src/gpu/web/gpu_buffer_web.hpp"
#include "src/gpu/web/gpu_command_buffer_web.hpp"
#include "src/gpu/web/gpu_texture_web.hpp"

namespace skity {

GPUBlitPassWEB::GPUBlitPassWEB(WGPUDevice device, WGPUCommandEncoder encoder,
                               GPUCommandBufferWEB* command_buffer)
    : device_(device), encoder_(encoder), command_buffer_(command_buffer) {
  // Add ref to encoder_ to make sure it is not destroyed
  wgpuCommandEncoderAddRef(encoder_);
}

GPUBlitPassWEB::~GPUBlitPassWEB() { wgpuCommandEncoderRelease(encoder_); }

void GPUBlitPassWEB::UploadTextureData(std::shared_ptr<GPUTexture> texture,
                                       uint32_t offset_x, uint32_t offset_y,
                                       uint32_t width, uint32_t height,
                                       void* data) {
  auto texture_web = dynamic_cast<GPUTextureWEB*>(texture.get());
  if (!texture_web) {
    // check texture is valid
    return;
  }

  uint32_t buffer_width = width;
  if (buffer_width % 256 != 0) {
    // webgpu require buffer width must be multiple of 256
    buffer_width = (width + 255) & ~255;
  }

  auto bytes_per_pixel =
      GetTextureFormatBytesPerPixel(texture->GetDescriptor().format);
  auto size = buffer_width * height * bytes_per_pixel;
  // create a stage buffer
  WGPUBufferDescriptor desc = {};
  desc.size = size;
  desc.usage = WGPUBufferUsage_CopySrc | WGPUBufferUsage_MapWrite;
  desc.mappedAtCreation = true;

  WGPUBuffer stage_buffer = wgpuDeviceCreateBuffer(device_, &desc);

  if (!stage_buffer) {
    // check stage buffer is valid
    return;
  }

  auto ptr = wgpuBufferGetMappedRange(stage_buffer, 0, size);

  if (!ptr) {
    // check mapped range is valid
    wgpuBufferRelease(stage_buffer);
    wgpuBufferDestroy(stage_buffer);
    return;
  }

  if (buffer_width != width) {
    auto buffer_ptr = static_cast<uint8_t*>(ptr);
    auto data_ptr = static_cast<const uint8_t*>(data);

    for (uint32_t y = 0; y < height; y++) {
      std::memcpy(buffer_ptr, data_ptr, width * bytes_per_pixel);
      buffer_ptr += buffer_width * bytes_per_pixel;
      data_ptr += width * bytes_per_pixel;
    }

  } else {
    std::memcpy(ptr, data, size);
  }

  wgpuBufferUnmap(stage_buffer);

  WGPUTexelCopyBufferInfo src_info{};
  src_info.buffer = stage_buffer;
  src_info.layout.offset = 0;
  src_info.layout.bytesPerRow = buffer_width * bytes_per_pixel;
  src_info.layout.rowsPerImage = height;

  WGPUTexelCopyTextureInfo dst_info{};
  dst_info.texture = texture_web->GetTexture();
  dst_info.mipLevel = 0;
  dst_info.origin.x = offset_x;
  dst_info.origin.y = offset_y;
  dst_info.origin.z = 0;
  dst_info.aspect = WGPUTextureAspect_All;

  WGPUExtent3D copy_size = {width, height, 1};

  wgpuCommandEncoderCopyBufferToTexture(encoder_, &src_info, &dst_info,
                                        &copy_size);

  command_buffer_->RecordStageBuffer(stage_buffer);
}

void GPUBlitPassWEB::UploadBufferData(GPUBuffer* buffer, void* data,
                                      size_t size) {
  auto buffer_web = dynamic_cast<GPUBufferWEB*>(buffer);

  if (!buffer_web) {
    // check buffer is valid
    return;
  }

  buffer_web->ResizeIfNeeded(device_, size);

  // using encoder to make sure data update sync with other commands

  // create a stage buffer
  WGPUBufferDescriptor desc = {};
  desc.size = size;
  desc.usage = WGPUBufferUsage_CopySrc | WGPUBufferUsage_MapWrite;
  desc.mappedAtCreation = true;

  WGPUBuffer stage_buffer = wgpuDeviceCreateBuffer(device_, &desc);

  if (!stage_buffer) {
    // check stage buffer is valid
    return;
  }

  auto ptr = wgpuBufferGetMappedRange(stage_buffer, 0, size);

  if (!ptr) {
    // check mapped range is valid
    wgpuBufferRelease(stage_buffer);
    wgpuBufferDestroy(stage_buffer);
    return;
  }

  std::memcpy(ptr, data, size);

  wgpuBufferUnmap(stage_buffer);

  wgpuCommandEncoderCopyBufferToBuffer(encoder_, stage_buffer, 0,
                                       buffer_web->GetBuffer(), 0, size);

  command_buffer_->RecordStageBuffer(stage_buffer);
}

void GPUBlitPassWEB::End() {}

}  // namespace skity
