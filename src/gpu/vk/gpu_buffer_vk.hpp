// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_VK_GPU_BUFFER_VK_HPP
#define SRC_GPU_VK_GPU_BUFFER_VK_HPP

#include <vk_mem_alloc.h>

#include <memory>

#include "src/gpu/gpu_buffer.hpp"

namespace skity {

class VulkanContextState;

enum class GPUBufferVKMemoryType {
  kDeviceLocal,
  kHostVisible,
};

class GPUBufferVK : public GPUBuffer {
 public:
  GPUBufferVK(GPUBufferUsageMask usage,
              std::shared_ptr<const VulkanContextState> state,
              GPUBufferVKMemoryType memory_type =
                  GPUBufferVKMemoryType::kDeviceLocal);

  ~GPUBufferVK() override;

  bool ResizeIfNeeded(size_t size);

  bool UploadData(const void* data, size_t size);

  bool IsValid() const {
    return buffer_ != VK_NULL_HANDLE && allocation_ != VK_NULL_HANDLE;
  }

  VkBuffer GetBuffer() const { return buffer_; }

  VmaAllocation GetAllocation() const { return allocation_; }

  void* GetMappedData() const { return mapped_data_; }

  VkDeviceSize GetSize() const { return size_; }

  GPUBufferVKMemoryType GetMemoryType() const { return memory_type_; }

 private:
  bool CreateBuffer(VkDeviceSize size);
  void DestroyBuffer();

  std::shared_ptr<const VulkanContextState> state_ = {};
  GPUBufferVKMemoryType memory_type_ = GPUBufferVKMemoryType::kDeviceLocal;
  VkBuffer buffer_ = VK_NULL_HANDLE;
  VmaAllocation allocation_ = VK_NULL_HANDLE;
  void* mapped_data_ = nullptr;
  VkDeviceSize size_ = 0;
};

}  // namespace skity

#endif  // SRC_GPU_VK_GPU_BUFFER_VK_HPP
