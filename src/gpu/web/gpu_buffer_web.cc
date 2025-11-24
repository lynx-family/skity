// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/web/gpu_buffer_web.hpp"

namespace skity {

namespace {

WGPUBufferUsage ConvertGPUBufferUsageMask(GPUBufferUsageMask usage) {
  WGPUBufferUsage ret = WGPUBufferUsage_None;

  if (usage & GPUBufferUsage::kVertexBuffer) {
    ret |= WGPUBufferUsage_Vertex;
  }

  if (usage & GPUBufferUsage::kUniformBuffer) {
    ret |= WGPUBufferUsage_Uniform;
  }

  if (usage & GPUBufferUsage::kIndexBuffer) {
    ret |= WGPUBufferUsage_Index;
  }

  // append copy dst, since we need to copy data into this buffer
  ret |= WGPUBufferUsage_CopyDst;

  return ret;
}

}  // namespace

GPUBufferWEB::GPUBufferWEB(GPUBufferUsageMask usage) : GPUBuffer(usage) {}

GPUBufferWEB::~GPUBufferWEB() {
  if (buffer_) {
    wgpuBufferDestroy(buffer_);
    wgpuBufferRelease(buffer_);
  }
}

void GPUBufferWEB::ResizeIfNeeded(WGPUDevice device, size_t size) {
  if (buffer_ != nullptr && wgpuBufferGetSize(buffer_) >= size) {
    return;
  }

  if (buffer_ != nullptr) {
    wgpuBufferDestroy(buffer_);
    wgpuBufferRelease(buffer_);
    buffer_ = nullptr;
  }

  WGPUBufferDescriptor desc{};

  desc.mappedAtCreation = false;
  desc.size = size;
  desc.usage = ConvertGPUBufferUsageMask(GetUsage());

  buffer_ = wgpuDeviceCreateBuffer(device, &desc);
}

}  // namespace skity
