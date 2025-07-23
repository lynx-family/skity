// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_HW_STAGE_BUFFER_HPP
#define SRC_RENDER_HW_HW_STAGE_BUFFER_HPP

#include <cstdint>
#include <memory>
#include <vector>

#include "src/gpu/gpu_buffer.hpp"

namespace skity {

class GPUDevice;

struct HWBufferAllocation {
  void* addr = nullptr;
  uint32_t offset;
  uint32_t size;
};

class HWStageBuffer final {
 public:
  explicit HWStageBuffer(GPUDevice* device);
  ~HWStageBuffer();

  GPUBufferView Push(void* data, uint32_t size, bool align_offset = false);

  HWBufferAllocation Allocate(uint32_t size, bool align_offset = false);

  GPUBufferView PushIndex(void* data, uint32_t size);

  void Flush();

  GPUBuffer* GetGPUBuffer() const { return gpu_buffer_.get(); }

  GPUBuffer* GetGPUIndexBuffer() const { return gpu_index_buffer_.get(); }

 private:
  static void ResizeIfNeed(std::vector<uint8_t>& buffer, uint32_t curr_pos,
                           uint32_t size);

  void AlignGpuOffset(uint32_t size);

 private:
  std::vector<uint8_t> stage_buffer_;
  uint32_t stage_pos_ = 0;
  std::vector<uint8_t> stage_index_buffer_;
  uint32_t stage_index_pos_ = 0;
  std::unique_ptr<GPUBuffer> gpu_buffer_;
  std::unique_ptr<GPUBuffer> gpu_index_buffer_;
  uint32_t ubo_alignment_;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_HW_STAGE_BUFFER_HPP
