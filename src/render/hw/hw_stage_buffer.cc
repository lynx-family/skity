// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/hw_stage_buffer.hpp"

#include <cmath>
#include <cstring>

#include "src/gpu/gpu_buffer.hpp"
#include "src/gpu/gpu_device.hpp"
#include "src/logging.hpp"

namespace skity {

enum { STAGE_DEFAULT_BUFFER_SIZE = 1024 };

HWStageBuffer::HWStageBuffer(GPUDevice* device)
    : stage_buffer_(STAGE_DEFAULT_BUFFER_SIZE),
      gpu_buffer_(device->CreateBuffer(GPUBufferUsage::kVertexBuffer |
                                       GPUBufferUsage::kUniformBuffer)),
      gpu_index_buffer_(device->CreateBuffer(GPUBufferUsage::kIndexBuffer)),
      ubo_alignment_(device->GetBufferAlignment()) {}

HWStageBuffer::HWStageBuffer(GPUDevice* device,
                             std::unique_ptr<GPUBuffer> gpu_buffer,
                             std::unique_ptr<GPUBuffer> gpu_index_buffer,
                             size_t ubo_alignment)
    : stage_buffer_(STAGE_DEFAULT_BUFFER_SIZE),
      gpu_buffer_(std::move(gpu_buffer)),
      gpu_index_buffer_(std::move(gpu_index_buffer)),
      ubo_alignment_(ubo_alignment) {}

HWStageBuffer::~HWStageBuffer() {
  LOGI("HWStageBuffer: [ {:p} ] destroyed", reinterpret_cast<void*>(this));
}

HWBufferAllocation HWStageBuffer::Allocate(uint32_t size, bool align_offset) {
  ResizeIfNeed(stage_buffer_, stage_pos_, size);

  if (align_offset) {
    AlignGpuOffset(size);
  }

  uint32_t offset = stage_pos_;

  stage_pos_ += size;

  return HWBufferAllocation{
      stage_buffer_.data() + offset,
      offset,
      size,
  };
}

GPUBufferView HWStageBuffer::Push(void* data, uint32_t size,
                                  bool align_offset) {
  ResizeIfNeed(stage_buffer_, stage_pos_, size);

  if (align_offset) {
    AlignGpuOffset(size);
  }

  uint32_t offset = stage_pos_;

  std::memcpy(stage_buffer_.data() + offset, data, size);

  stage_pos_ += size;

  return GPUBufferView{
      GetGPUBuffer(),
      offset,
      size,
  };
}

GPUBufferView HWStageBuffer::PushIndex(void* data, uint32_t size) {
  ResizeIfNeed(stage_index_buffer_, stage_index_pos_, size);

  uint32_t offset = stage_index_pos_;

  std::memcpy(stage_index_buffer_.data() + offset, data, size);

  stage_index_pos_ += size;

  return GPUBufferView{
      GetGPUIndexBuffer(),
      offset,
      size,
  };
}

void HWStageBuffer::Flush() {
  gpu_buffer_->UploadData(stage_buffer_.data(), stage_pos_);

  gpu_index_buffer_->UploadData(stage_index_buffer_.data(), stage_index_pos_);

  stage_pos_ = 0;
  stage_index_pos_ = 0;
}

void HWStageBuffer::ResizeIfNeed(std::vector<uint8_t>& buffer,
                                 uint32_t curr_pos, uint32_t size) {
  if (buffer.size() - curr_pos > size) {
    return;
  }

  size_t new_size =
      std::max(static_cast<size_t>(size), buffer.size()) + buffer.size();

  buffer.resize(new_size);
}

void HWStageBuffer::AlignGpuOffset(uint32_t size) {
  if (stage_pos_ % ubo_alignment_ == 0) {
    return;
  }

  auto offset = ubo_alignment_ - stage_pos_ % ubo_alignment_;

  ResizeIfNeed(stage_buffer_, stage_pos_, offset + size);

  stage_pos_ += offset;
}

}  // namespace skity
