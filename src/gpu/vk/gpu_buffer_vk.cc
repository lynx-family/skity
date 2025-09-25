// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/vk/gpu_buffer_vk.hpp"

#include "src/gpu/vk/gpu_device_vk.hpp"
#include "src/logging.hpp"
#include "src/tracing.hpp"

namespace skity {

GPUBufferVk::GPUBufferVk(GPUDeviceVk* device, GPUBufferUsageMask usage)
    : GPUBuffer(usage), device_(device) {}

GPUBufferVk::~GPUBufferVk() {
  if (is_mapped_) {
    Unmap();
  }

  if (buffer_ != VK_NULL_HANDLE && device_) {
    vmaDestroyBuffer(device_->GetAllocator(), buffer_, allocation_);
  }
}

void GPUBufferVk::UploadData(void* data, size_t size) {
  SKITY_TRACE_EVENT("GPUBufferVk::UploadData");

  if (size == 0 || data == nullptr) {
    return;
  }

  if (buffer_ != VK_NULL_HANDLE && size_ < size) {
    vmaDestroyBuffer(device_->GetAllocator(), buffer_, allocation_);
    buffer_ = VK_NULL_HANDLE;
  }

  if (buffer_ == VK_NULL_HANDLE) {
    size_ = size;
    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size_;
    buffer_info.usage = GetVulkanUsageFlags();
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = GetVmaMemoryUsage();

    VkResult result =
        vmaCreateBuffer(device_->GetAllocator(), &buffer_info, &alloc_info,
                        &buffer_, &allocation_, nullptr);
    if (result != VK_SUCCESS) {
      LOGE("Failed to create Vulkan buffer: {}", result);
      return;
    }
  }

  void* mapped_data = GetMappedPtr();
  if (mapped_data) {
    memcpy(mapped_data, data, size);
    FlushMappedRange(0, size);
    Unmap();
  } else {
    LOGE("Failed to map buffer for data upload");
  }
}

void* GPUBufferVk::GetMappedPtr() {
  if (is_mapped_) {
    return mapped_ptr_;
  }

  VkResult result =
      vmaMapMemory(device_->GetAllocator(), allocation_, &mapped_ptr_);
  if (result != VK_SUCCESS) {
    LOGE("Failed to map buffer memory: {}", result);
    return nullptr;
  }

  is_mapped_ = true;
  return mapped_ptr_;
}

void GPUBufferVk::Unmap() {
  if (is_mapped_) {
    vmaUnmapMemory(device_->GetAllocator(), allocation_);
    mapped_ptr_ = nullptr;
    is_mapped_ = false;
  }
}

void GPUBufferVk::FlushMappedRange(uint64_t offset, uint64_t size) {
  if (!is_mapped_) {
    return;
  }

  VkResult result =
      vmaFlushAllocation(device_->GetAllocator(), allocation_, offset, size);
  if (result != VK_SUCCESS) {
    LOGE("Failed to flush buffer range: {}", result);
  }
}

VkBufferUsageFlags GPUBufferVk::GetVulkanUsageFlags() const {
  VkBufferUsageFlags usage = 0;

  if (GetUsage() & GPUBufferUsage::kVertexBuffer) {
    usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  }
  if (GetUsage() & GPUBufferUsage::kIndexBuffer) {
    usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  }
  if (GetUsage() & GPUBufferUsage::kUniformBuffer) {
    usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  }

  // Always add transfer usage for data uploads
  usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

  return usage;
}

VmaMemoryUsage GPUBufferVk::GetVmaMemoryUsage() const {
  auto usage = GetUsage();

  // Optimize memory usage based on buffer type
  if (usage & GPUBufferUsage::kIndexBuffer ||
      usage & GPUBufferUsage::kVertexBuffer) {
    // Vertex/index buffers: prefer GPU-optimized memory, but allow CPU write
    // access
    return VMA_MEMORY_USAGE_CPU_TO_GPU;  // Upload from CPU, then GPU access
  }

  if (usage & GPUBufferUsage::kUniformBuffer) {
    // Uniform buffers: frequently updated, need fast CPU write access
    return VMA_MEMORY_USAGE_CPU_TO_GPU;
  }

  // Default: CPU-to-GPU for general buffers
  return VMA_MEMORY_USAGE_CPU_TO_GPU;
}

}  // namespace skity