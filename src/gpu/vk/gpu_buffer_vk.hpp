// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_VK_GPU_BUFFER_VK_HPP
#define SRC_GPU_VK_GPU_BUFFER_VK_HPP

#include <vk_mem_alloc.h>
#include <volk.h>

#include "src/gpu/gpu_buffer.hpp"

namespace skity {

class GPUDeviceVk;

class GPUBufferVk : public GPUBuffer {
 public:
  explicit GPUBufferVk(GPUDeviceVk* device, GPUBufferUsageMask usage);
  ~GPUBufferVk() override;

  void UploadData(void* data, size_t size);

  // Vulkan-specific methods
  void* GetMappedPtr();
  void Unmap();
  void FlushMappedRange(uint64_t offset, uint64_t size);

  // Vulkan-specific getters
  VkBuffer GetBuffer() const { return buffer_; }
  VmaAllocation GetAllocation() const { return allocation_; }
  size_t GetSize() const { return size_; }

 private:
  VkBufferUsageFlags GetVulkanUsageFlags() const;
  VmaMemoryUsage GetVmaMemoryUsage() const;

 private:
  GPUDeviceVk* device_ = nullptr;
  VkBuffer buffer_ = VK_NULL_HANDLE;
  VmaAllocation allocation_ = VK_NULL_HANDLE;
  void* mapped_ptr_ = nullptr;
  size_t size_ = 0;
  bool is_mapped_ = false;
};

}  // namespace skity

#endif  // SRC_GPU_VK_GPU_BUFFER_VK_HPP