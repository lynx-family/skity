// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_HW_STAGE_BUFFER_HPP
#define SRC_RENDER_HW_HW_STAGE_BUFFER_HPP

#include <cstdint>
#include <memory>
#include <optional>
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

  HWStageBuffer(GPUDevice* device, std::unique_ptr<GPUBuffer> gpu_buffer,
                std::unique_ptr<GPUBuffer> gpu_index_buffer,
                size_t ubo_alignment);

  ~HWStageBuffer();

  GPUBufferView Push(void* data, uint32_t size, bool align_offset = false);

  HWBufferAllocation Allocate(uint32_t size, bool align_offset = false);

  GPUBufferView PushIndex(void* data, uint32_t size);

  void BeginWritingInstance(uint32_t estimate_size, uint32_t align);

  template <typename T, typename... Args>
  uint32_t AppendInstance(Args&&... args) {
    static_assert(std::is_trivially_destructible<T>::value);
    ResizeIfNeed(stage_buffer_, stage_pos_, sizeof(T));
    uint32_t offset = stage_pos_;
    if constexpr (sizeof...(Args) == 0 && std::is_standard_layout<T>::value &&
                  std::is_trivial<T>::value) {
      stage_pos_ += sizeof(T);
      return offset;
    } else {
      uint8_t* p = stage_buffer_.data() + stage_pos_;
      stage_pos_ += sizeof(T);
      new (p) T{std::forward<Args>(args)...};
    }
    return offset;
  }

  template <typename T>
  constexpr T* ToInstance(uint32_t offset) const {
    uint8_t* p = const_cast<uint8_t*>(stage_buffer_.data()) + offset;
    return reinterpret_cast<T*>(p);
  }

  GPUBufferView EndWritingInstance();

  void Flush();

  GPUBuffer* GetGPUBuffer() const { return gpu_buffer_.get(); }

  GPUBuffer* GetGPUIndexBuffer() const { return gpu_index_buffer_.get(); }

 private:
  static void ResizeIfNeed(std::vector<uint8_t>& buffer, uint32_t curr_pos,
                           uint32_t size);

  void AlignGpuOffset(uint32_t size);

 private:
  GPUDevice* gpu_device_;
  std::vector<uint8_t> stage_buffer_;
  uint32_t stage_pos_ = 0;
  std::vector<uint8_t> stage_index_buffer_;
  uint32_t stage_index_pos_ = 0;
  std::unique_ptr<GPUBuffer> gpu_buffer_;
  std::unique_ptr<GPUBuffer> gpu_index_buffer_;
  uint32_t ubo_alignment_;
  std::optional<uint32_t> writing_offset_ = std::nullopt;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_HW_STAGE_BUFFER_HPP
