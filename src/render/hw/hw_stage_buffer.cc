// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/hw_stage_buffer.hpp"

#include <cmath>
#include <cstddef>
#include <cstring>

#include "src/gpu/gpu_buffer.hpp"
#include "src/gpu/gpu_device.hpp"
#include "src/logging.hpp"

namespace skity {

enum { STAGE_DEFAULT_BUFFER_SIZE = 1024 };

HWStageBuffer::HWStageBuffer(GPUDevice* device)
    : gpu_device_(device),
      stage_buffer_(STAGE_DEFAULT_BUFFER_SIZE),
      ubo_alignment_(device->GetBufferAlignment()) {
  GPUBufferStorageMode storage_mode =
      device->GetCaps().supports_host_visible_buffer
          ? GPUBufferStorageMode::kHostVisible
          : GPUBufferStorageMode::kPrivate;

  GPUBufferDescriptor buffer_desc = {};
  buffer_desc.usage =
      GPUBufferUsage::kVertexBuffer | GPUBufferUsage::kUniformBuffer;
  buffer_desc.storage_mode = storage_mode;
  gpu_buffer_ = device->CreateBuffer(buffer_desc);

  GPUBufferDescriptor index_buffer_desc = {};
  index_buffer_desc.usage = GPUBufferUsage::kIndexBuffer;
  index_buffer_desc.storage_mode = storage_mode;
  gpu_index_buffer_ = device->CreateBuffer(index_buffer_desc);

  use_map_upload_ =
      gpu_buffer_ != nullptr &&
      gpu_buffer_->GetStorageMode() == GPUBufferStorageMode::kHostVisible;
}

HWStageBuffer::HWStageBuffer(GPUDevice* device,
                             std::unique_ptr<GPUBuffer> gpu_buffer,
                             std::unique_ptr<GPUBuffer> gpu_index_buffer,
                             size_t ubo_alignment)
    : gpu_device_(device),
      stage_buffer_(STAGE_DEFAULT_BUFFER_SIZE),
      gpu_buffer_(std::move(gpu_buffer)),
      gpu_index_buffer_(std::move(gpu_index_buffer)),
      ubo_alignment_(ubo_alignment) {
  use_map_upload_ =
      gpu_buffer_ != nullptr &&
      gpu_buffer_->GetStorageMode() == GPUBufferStorageMode::kHostVisible;
}

HWStageBuffer::~HWStageBuffer() = default;

void HWStageBuffer::BeginWritingInstance(uint32_t estimate_size,
                                         uint32_t align) {
  DEBUG_CHECK(!writing_offset_.has_value());
  uint32_t aligned_stage_pos = ((stage_pos_ + align - 1) & ~(align - 1));
  ResizeIfNeed(stage_buffer_, aligned_stage_pos, estimate_size);
  stage_pos_ = aligned_stage_pos;
  writing_offset_ = stage_pos_;
}

GPUBufferView HWStageBuffer::EndWritingInstance() {
  DEBUG_CHECK(writing_offset_.has_value());
  uint32_t offset = writing_offset_.value();
  uint32_t size = stage_pos_ - writing_offset_.value();

  writing_offset_.reset();
  return GPUBufferView{
      GetGPUBuffer(),
      offset,
      size,
  };
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

void HWStageBuffer::Flush(GPUCommandBuffer* command_buffer) {
  if (use_map_upload_) {
    UploadByMap();
  } else {
    UploadByBlit(command_buffer);
  }

  stage_pos_ = 0;
  stage_index_pos_ = 0;
}

void HWStageBuffer::UploadByMap() {
  UploadBufferByMap(gpu_buffer_.get(), stage_buffer_.data(), stage_pos_);

  if (stage_index_pos_ > 0) {
    UploadBufferByMap(gpu_index_buffer_.get(), stage_index_buffer_.data(),
                      stage_index_pos_);
  }
}

void HWStageBuffer::UploadBufferByMap(GPUBuffer* buffer, void* data,
                                      size_t size) {
  if (size == 0) {
    return;
  }

  if (buffer == nullptr || data == nullptr) {
    LOGE("UploadBufferByMap called with empty buffer or data");
    return;
  }

  void* mapped = buffer->Map(size);
  if (mapped == nullptr) {
    LOGE("Failed to map GPUBuffer for {} bytes", size);
    return;
  }

  std::memcpy(mapped, data, size);
  buffer->Unmap();
}

void HWStageBuffer::UploadByBlit(GPUCommandBuffer* command_buffer) {
  if (stage_pos_ == 0 && stage_index_pos_ == 0) {
    return;
  }

  auto blit_pass = command_buffer->BeginBlitPass();
  if (stage_pos_ > 0) {
    blit_pass->UploadBufferData(gpu_buffer_.get(), stage_buffer_.data(),
                                stage_pos_);
  }

  // The index buffer data here can be empty if all rendering in the scene
  // uses instanced rendering.
  if (stage_index_pos_ > 0) {
    blit_pass->UploadBufferData(gpu_index_buffer_.get(),
                                stage_index_buffer_.data(), stage_index_pos_);
  }

  blit_pass->End();
}

void HWStageBuffer::ResizeIfNeed(std::vector<uint8_t>& buffer,
                                 uint32_t curr_pos, uint32_t size) {
  if (buffer.size() > curr_pos + size) {
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
