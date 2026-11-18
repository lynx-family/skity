// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/gpu_device.hpp"

#include <cstring>

namespace skity {

std::shared_ptr<GPUBuffer> GPUDevice::CreateReadOnlyStorageBuffer(
    const void* data, size_t size) {
  if (data == nullptr || size == 0 ||
      size > GetCaps().max_vertex_storage_buffer_range ||
      !GetCaps().supports_host_visible_buffer) {
    return nullptr;
  }

  auto buffer = CreateBuffer(
      {GPUBufferUsage::kStorageBuffer, GPUBufferStorageMode::kHostVisible});
  if (!buffer) {
    return nullptr;
  }

  void* mapped = buffer->Map(size);
  if (mapped == nullptr) {
    return nullptr;
  }

  std::memcpy(mapped, data, size);
  buffer->Unmap();
  return buffer;
}

std::shared_ptr<GPUShaderModule> GPUDevice::CreateShaderModule(
    const GPUShaderModuleDescriptor& desc) {
  return GPUShaderModule::Create(desc);
}

}  // namespace skity
