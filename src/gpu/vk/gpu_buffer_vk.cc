// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/vk/gpu_buffer_vk.hpp"

#include "src/gpu/vk/vulkan_context_state.hpp"
#include "src/logging.hpp"

namespace skity {

namespace {

VkBufferUsageFlags ConvertGPUBufferUsageMask(GPUBufferUsageMask usage) {
  VkBufferUsageFlags flags = 0;

  if ((usage & GPUBufferUsage::kVertexBuffer) != 0) {
    flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  }

  if ((usage & GPUBufferUsage::kUniformBuffer) != 0) {
    flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  }

  if ((usage & GPUBufferUsage::kIndexBuffer) != 0) {
    flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  }

  return flags;
}

}  // namespace

GPUBufferVK::GPUBufferVK(GPUBufferUsageMask usage,
                         std::shared_ptr<const VulkanContextState> state,
                         GPUBufferVKMemoryType memory_type)
    : GPUBuffer(usage),
      state_(std::move(state)),
      memory_type_(memory_type) {}

GPUBufferVK::~GPUBufferVK() { DestroyBuffer(); }

bool GPUBufferVK::ResizeIfNeeded(size_t size) {
  if (size == 0) {
    return true;
  }

  if (size_ >= size && IsValid()) {
    return true;
  }

  return CreateBuffer(size);
}

bool GPUBufferVK::UploadData(const void* data, size_t size) {
  if (data == nullptr || size == 0) {
    LOGW("Uploading data to Vulkan buffer with empty source");
    return false;
  }

  if (!ResizeIfNeeded(size)) {
    return false;
  }

  if (state_ == nullptr || state_->GetAllocator() == nullptr ||
      allocation_ == VK_NULL_HANDLE) {
    LOGE("Failed to upload Vulkan buffer data: allocator is unavailable");
    return false;
  }

  if (memory_type_ != GPUBufferVKMemoryType::kHostVisible) {
    LOGE("Failed to upload Vulkan buffer data: destination buffer is not "
         "host visible");
    return false;
  }

  const VkResult result = vmaCopyMemoryToAllocation(
      state_->GetAllocator(), data, allocation_, 0, size);
  if (result != VK_SUCCESS) {
    LOGE("Failed to upload {} bytes to Vulkan buffer: {}", size,
         static_cast<int32_t>(result));
    return false;
  }

  return true;
}

bool GPUBufferVK::CreateBuffer(VkDeviceSize size) {
  DestroyBuffer();

  if (state_ == nullptr || state_->GetLogicalDevice() == VK_NULL_HANDLE ||
      state_->GetAllocator() == nullptr) {
    LOGE("Failed to create Vulkan buffer: device or allocator is unavailable");
    return false;
  }

  VkBufferCreateInfo buffer_info = {};
  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.size = size;
  buffer_info.usage = ConvertGPUBufferUsageMask(GetUsage());
  if (memory_type_ == GPUBufferVKMemoryType::kHostVisible) {
    buffer_info.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  } else {
    buffer_info.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  }
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocation_info = {};
  if (memory_type_ == GPUBufferVKMemoryType::kHostVisible) {
    allocation_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
    allocation_info.flags =
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_MAPPED_BIT;
  } else {
    allocation_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    allocation_info.flags = 0;
  }

  VmaAllocationInfo vma_info = {};
  const VkResult result =
      vmaCreateBuffer(state_->GetAllocator(), &buffer_info, &allocation_info,
                      &buffer_, &allocation_, &vma_info);
  if (result != VK_SUCCESS || buffer_ == VK_NULL_HANDLE ||
      allocation_ == VK_NULL_HANDLE) {
    LOGE("Failed to create Vulkan buffer with size {}: {}",
         static_cast<uint64_t>(size), static_cast<int32_t>(result));
    DestroyBuffer();
    return false;
  }

  mapped_data_ = vma_info.pMappedData;
  size_ = size;
  return true;
}

void GPUBufferVK::DestroyBuffer() {
  if (state_ != nullptr && state_->GetAllocator() != nullptr &&
      buffer_ != VK_NULL_HANDLE && allocation_ != VK_NULL_HANDLE) {
    vmaDestroyBuffer(state_->GetAllocator(), buffer_, allocation_);
  }

  buffer_ = VK_NULL_HANDLE;
  allocation_ = VK_NULL_HANDLE;
  mapped_data_ = nullptr;
  size_ = 0;
}

}  // namespace skity
