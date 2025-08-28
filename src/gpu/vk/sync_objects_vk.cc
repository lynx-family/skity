// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/vk/sync_objects_vk.hpp"

#include "src/gpu/vk/gpu_device_vk.hpp"
#include "src/logging.hpp"

namespace skity {

// VkSemaphore implementation
VkSemaphore::VkSemaphore(GPUDeviceVk* device) : device_(device) {
  if (!device_) {
    LOGE("Invalid device for semaphore creation");
    return;
  }

  VkSemaphoreCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkResult result = vkCreateSemaphore(device_->GetDevice(), &create_info,
                                      nullptr, &semaphore_);
  if (result != VK_SUCCESS) {
    LOGE("Failed to create Vulkan semaphore: %d", result);
    semaphore_ = VK_NULL_HANDLE;
  }
}

VkSemaphore::~VkSemaphore() { Destroy(); }

VkSemaphore::VkSemaphore(VkSemaphore&& other) noexcept
    : device_(other.device_), semaphore_(other.semaphore_) {
  other.device_ = nullptr;
  other.semaphore_ = VK_NULL_HANDLE;
}

VkSemaphore& VkSemaphore::operator=(VkSemaphore&& other) noexcept {
  if (this != &other) {
    Destroy();
    device_ = other.device_;
    semaphore_ = other.semaphore_;
    other.device_ = nullptr;
    other.semaphore_ = VK_NULL_HANDLE;
  }
  return *this;
}

void VkSemaphore::Destroy() {
  if (semaphore_ != VK_NULL_HANDLE && device_) {
    vkDestroySemaphore(device_->GetDevice(), semaphore_, nullptr);
    semaphore_ = VK_NULL_HANDLE;
  }
}

// VkFence implementation
VkFence::VkFence(GPUDeviceVk* device, bool signaled) : device_(device) {
  if (!device_) {
    LOGE("Invalid device for fence creation");
    return;
  }

  VkFenceCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  if (signaled) {
    create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
  }

  VkResult result =
      vkCreateFence(device_->GetDevice(), &create_info, nullptr, &fence_);
  if (result != VK_SUCCESS) {
    LOGE("Failed to create Vulkan fence: %d", result);
    fence_ = VK_NULL_HANDLE;
  }
}

VkFence::~VkFence() { Destroy(); }

VkFence::VkFence(VkFence&& other) noexcept
    : device_(other.device_), fence_(other.fence_) {
  other.device_ = nullptr;
  other.fence_ = VK_NULL_HANDLE;
}

VkFence& VkFence::operator=(VkFence&& other) noexcept {
  if (this != &other) {
    Destroy();
    device_ = other.device_;
    fence_ = other.fence_;
    other.device_ = nullptr;
    other.fence_ = VK_NULL_HANDLE;
  }
  return *this;
}

void VkFence::Destroy() {
  if (fence_ != VK_NULL_HANDLE && device_) {
    vkDestroyFence(device_->GetDevice(), fence_, nullptr);
    fence_ = VK_NULL_HANDLE;
  }
}

bool VkFence::Wait(uint64_t timeout_ns) const {
  if (!IsValid()) {
    return false;
  }

  VkResult result =
      vkWaitForFences(device_->GetDevice(), 1, &fence_, VK_TRUE, timeout_ns);
  if (result == VK_SUCCESS) {
    return true;
  } else if (result == VK_TIMEOUT) {
    return false;
  } else {
    LOGE("Failed to wait for fence: %d", result);
    return false;
  }
}

bool VkFence::IsSignaled() const {
  if (!IsValid()) {
    return false;
  }

  VkResult result = vkGetFenceStatus(device_->GetDevice(), fence_);
  return result == VK_SUCCESS;
}

void VkFence::Reset() {
  if (!IsValid()) {
    return;
  }

  VkResult result = vkResetFences(device_->GetDevice(), 1, &fence_);
  if (result != VK_SUCCESS) {
    LOGE("Failed to reset fence: %d", result);
  }
}

// VkSyncManager implementation
VkSyncManager::VkSyncManager(GPUDeviceVk* device) : device_(device) {}

void VkSyncManager::AddMemoryBarrier(const VkMemoryBarrier& barrier) {
  VkMemoryBarrier2KHR vk_barrier{};
  vk_barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2_KHR;
  vk_barrier.srcStageMask = barrier.src_stage_mask;
  vk_barrier.srcAccessMask = barrier.src_access_mask;
  vk_barrier.dstStageMask = barrier.dst_stage_mask;
  vk_barrier.dstAccessMask = barrier.dst_access_mask;

  memory_barriers_.push_back(vk_barrier);
  src_stage_mask_ |= barrier.src_stage_mask;
  dst_stage_mask_ |= barrier.dst_stage_mask;
}

void VkSyncManager::AddImageBarrier(const VkImageBarrier& barrier) {
  VkImageMemoryBarrier2KHR vk_barrier{};
  vk_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR;
  vk_barrier.srcStageMask = barrier.src_stage_mask;
  vk_barrier.srcAccessMask = barrier.src_access_mask;
  vk_barrier.dstStageMask = barrier.dst_stage_mask;
  vk_barrier.dstAccessMask = barrier.dst_access_mask;
  vk_barrier.oldLayout = barrier.old_layout;
  vk_barrier.newLayout = barrier.new_layout;
  vk_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  vk_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  vk_barrier.image = barrier.image;
  vk_barrier.subresourceRange = barrier.subresource_range;

  image_barriers_.push_back(vk_barrier);
  src_stage_mask_ |= barrier.src_stage_mask;
  dst_stage_mask_ |= barrier.dst_stage_mask;
}

void VkSyncManager::AddBufferBarrier(const VkBufferBarrier& barrier) {
  VkBufferMemoryBarrier2KHR vk_barrier{};
  vk_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR;
  vk_barrier.srcStageMask = barrier.src_stage_mask;
  vk_barrier.srcAccessMask = barrier.src_access_mask;
  vk_barrier.dstStageMask = barrier.dst_stage_mask;
  vk_barrier.dstAccessMask = barrier.dst_access_mask;
  vk_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  vk_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  vk_barrier.buffer = barrier.buffer;
  vk_barrier.offset = barrier.offset;
  vk_barrier.size = barrier.size;

  buffer_barriers_.push_back(vk_barrier);
  src_stage_mask_ |= barrier.src_stage_mask;
  dst_stage_mask_ |= barrier.dst_stage_mask;
}

void VkSyncManager::ExecuteBarriers(VkCommandBuffer cmd_buffer) {
  if (memory_barriers_.empty() && image_barriers_.empty() &&
      buffer_barriers_.empty()) {
    return;
  }

  // Use VK_KHR_synchronization2 if available, fallback to legacy barriers
  VkDependencyInfoKHR dependency_info{};
  dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
  dependency_info.memoryBarrierCount =
      static_cast<uint32_t>(memory_barriers_.size());
  dependency_info.pMemoryBarriers = memory_barriers_.data();
  dependency_info.imageMemoryBarrierCount =
      static_cast<uint32_t>(image_barriers_.size());
  dependency_info.pImageMemoryBarriers = image_barriers_.data();
  dependency_info.bufferMemoryBarrierCount =
      static_cast<uint32_t>(buffer_barriers_.size());
  dependency_info.pBufferMemoryBarriers = buffer_barriers_.data();

  // Try to use synchronization2 extension, fallback to legacy if not available
  if (device_->HasSynchronization2Support() &&
      vkCmdPipelineBarrier2KHR != nullptr) {
    vkCmdPipelineBarrier2KHR(cmd_buffer, &dependency_info);
  } else {
    // Fall back to legacy barriers (either not supported or function not
    // loaded) Convert to legacy barriers for compatibility
    std::vector<::VkMemoryBarrier> legacy_memory_barriers;
    std::vector<::VkImageMemoryBarrier> legacy_image_barriers;
    std::vector<::VkBufferMemoryBarrier> legacy_buffer_barriers;

    for (const auto& barrier : memory_barriers_) {
      ::VkMemoryBarrier legacy_barrier{};
      legacy_barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
      legacy_barrier.srcAccessMask = barrier.srcAccessMask;
      legacy_barrier.dstAccessMask = barrier.dstAccessMask;
      legacy_memory_barriers.push_back(legacy_barrier);
    }

    for (const auto& barrier : image_barriers_) {
      ::VkImageMemoryBarrier legacy_barrier{};
      legacy_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      legacy_barrier.srcAccessMask = barrier.srcAccessMask;
      legacy_barrier.dstAccessMask = barrier.dstAccessMask;
      legacy_barrier.oldLayout = barrier.oldLayout;
      legacy_barrier.newLayout = barrier.newLayout;
      legacy_barrier.srcQueueFamilyIndex = barrier.srcQueueFamilyIndex;
      legacy_barrier.dstQueueFamilyIndex = barrier.dstQueueFamilyIndex;
      legacy_barrier.image = barrier.image;
      legacy_barrier.subresourceRange = barrier.subresourceRange;
      legacy_image_barriers.push_back(legacy_barrier);
    }

    for (const auto& barrier : buffer_barriers_) {
      ::VkBufferMemoryBarrier legacy_barrier{};
      legacy_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
      legacy_barrier.srcAccessMask = barrier.srcAccessMask;
      legacy_barrier.dstAccessMask = barrier.dstAccessMask;
      legacy_barrier.srcQueueFamilyIndex = barrier.srcQueueFamilyIndex;
      legacy_barrier.dstQueueFamilyIndex = barrier.dstQueueFamilyIndex;
      legacy_barrier.buffer = barrier.buffer;
      legacy_barrier.offset = barrier.offset;
      legacy_barrier.size = barrier.size;
      legacy_buffer_barriers.push_back(legacy_barrier);
    }

    vkCmdPipelineBarrier(cmd_buffer, src_stage_mask_, dst_stage_mask_, 0,
                         static_cast<uint32_t>(legacy_memory_barriers.size()),
                         legacy_memory_barriers.data(),
                         static_cast<uint32_t>(legacy_buffer_barriers.size()),
                         legacy_buffer_barriers.data(),
                         static_cast<uint32_t>(legacy_image_barriers.size()),
                         legacy_image_barriers.data());
  }
}

void VkSyncManager::Reset() {
  memory_barriers_.clear();
  image_barriers_.clear();
  buffer_barriers_.clear();
  src_stage_mask_ = 0;
  dst_stage_mask_ = 0;
}

VkImageBarrier VkSyncManager::CreateImageTransitionBarrier(
    VkImage image, VkImageLayout old_layout, VkImageLayout new_layout,
    VkImageAspectFlags aspect_mask) {
  VkImageBarrier barrier{};
  barrier.old_layout = old_layout;
  barrier.new_layout = new_layout;
  barrier.image = image;
  barrier.subresource_range.aspectMask = aspect_mask;
  barrier.subresource_range.baseMipLevel = 0;
  barrier.subresource_range.levelCount = 1;
  barrier.subresource_range.baseArrayLayer = 0;
  barrier.subresource_range.layerCount = 1;

  // Set appropriate access masks and pipeline stages based on layouts
  if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
      new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.src_access_mask = 0;
    barrier.dst_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.src_stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    barrier.dst_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.src_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dst_access_mask = VK_ACCESS_SHADER_READ_BIT;
    barrier.src_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    barrier.dst_stage_mask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
             new_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
    barrier.src_access_mask = 0;
    barrier.dst_access_mask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                              VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.src_stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    barrier.dst_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  } else {
    // General case - may need optimization for specific transitions
    barrier.src_access_mask =
        VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    barrier.dst_access_mask =
        VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    barrier.src_stage_mask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    barrier.dst_stage_mask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
  }

  return barrier;
}

VkBufferBarrier VkSyncManager::CreateBufferBarrier(VkBuffer buffer,
                                                   VkAccessFlags src_access,
                                                   VkAccessFlags dst_access,
                                                   VkDeviceSize offset,
                                                   VkDeviceSize size) {
  VkBufferBarrier barrier{};
  barrier.buffer = buffer;
  barrier.src_access_mask = src_access;
  barrier.dst_access_mask = dst_access;
  barrier.offset = offset;
  barrier.size = size;

  // Set appropriate pipeline stages based on access patterns
  if (src_access &
      (VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDEX_READ_BIT)) {
    barrier.src_stage_mask |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
  }
  if (src_access & VK_ACCESS_UNIFORM_READ_BIT) {
    barrier.src_stage_mask |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                              VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  }
  if (src_access & VK_ACCESS_TRANSFER_WRITE_BIT) {
    barrier.src_stage_mask |= VK_PIPELINE_STAGE_TRANSFER_BIT;
  }

  if (dst_access &
      (VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDEX_READ_BIT)) {
    barrier.dst_stage_mask |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
  }
  if (dst_access & VK_ACCESS_UNIFORM_READ_BIT) {
    barrier.dst_stage_mask |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                              VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  }
  if (dst_access & VK_ACCESS_TRANSFER_READ_BIT) {
    barrier.dst_stage_mask |= VK_PIPELINE_STAGE_TRANSFER_BIT;
  }

  // Default stages if none set
  if (barrier.src_stage_mask == 0) {
    barrier.src_stage_mask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
  }
  if (barrier.dst_stage_mask == 0) {
    barrier.dst_stage_mask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
  }

  return barrier;
}

}  // namespace skity