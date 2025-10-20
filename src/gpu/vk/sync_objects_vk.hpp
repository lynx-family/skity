// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_VK_SYNC_OBJECTS_VK_HPP
#define SRC_GPU_VK_SYNC_OBJECTS_VK_HPP

#include <volk.h>

#include <memory>
#include <vector>

namespace skity {

class GPUDeviceVk;

// Vulkan semaphore wrapper for GPU-GPU synchronization
class VkSemaphore {
 public:
  explicit VkSemaphore(GPUDeviceVk* device);
  ~VkSemaphore();

  VkSemaphore(const VkSemaphore&) = delete;
  VkSemaphore& operator=(const VkSemaphore&) = delete;

  VkSemaphore(VkSemaphore&& other) noexcept;
  VkSemaphore& operator=(VkSemaphore&& other) noexcept;

  VkSemaphore_T* GetHandle() const { return semaphore_; }
  bool IsValid() const { return semaphore_ != VK_NULL_HANDLE; }

 private:
  void Destroy();

  GPUDeviceVk* device_ = nullptr;
  VkSemaphore_T* semaphore_ = VK_NULL_HANDLE;
};

// Vulkan fence wrapper for CPU-GPU synchronization
class VkFence {
 public:
  explicit VkFence(GPUDeviceVk* device, bool signaled = false);
  ~VkFence();

  VkFence(const VkFence&) = delete;
  VkFence& operator=(const VkFence&) = delete;

  VkFence(VkFence&& other) noexcept;
  VkFence& operator=(VkFence&& other) noexcept;

  VkFence_T* GetHandle() const { return fence_; }
  bool IsValid() const { return fence_ != VK_NULL_HANDLE; }

  // Fence operations
  bool Wait(uint64_t timeout_ns = UINT64_MAX) const;
  bool IsSignaled() const;
  void Reset();

 private:
  void Destroy();

  GPUDeviceVk* device_ = nullptr;
  VkFence_T* fence_ = VK_NULL_HANDLE;
};

// Memory barrier wrapper for resource transitions
struct VkMemoryBarrier {
  VkPipelineStageFlags src_stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  VkPipelineStageFlags dst_stage_mask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
  VkAccessFlags src_access_mask = 0;
  VkAccessFlags dst_access_mask = 0;
};

// Image layout transition helper
struct VkImageBarrier {
  VkPipelineStageFlags src_stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  VkPipelineStageFlags dst_stage_mask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
  VkAccessFlags src_access_mask = 0;
  VkAccessFlags dst_access_mask = 0;
  VkImageLayout old_layout = VK_IMAGE_LAYOUT_UNDEFINED;
  VkImageLayout new_layout = VK_IMAGE_LAYOUT_UNDEFINED;
  VkImage image = VK_NULL_HANDLE;
  VkImageSubresourceRange subresource_range{};
};

// Buffer barrier for buffer access synchronization
struct VkBufferBarrier {
  VkPipelineStageFlags src_stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  VkPipelineStageFlags dst_stage_mask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
  VkAccessFlags src_access_mask = 0;
  VkAccessFlags dst_access_mask = 0;
  VkBuffer buffer = VK_NULL_HANDLE;
  VkDeviceSize offset = 0;
  VkDeviceSize size = VK_WHOLE_SIZE;
};

// Synchronization manager for efficient barrier management
class VkSyncManager {
 public:
  explicit VkSyncManager(GPUDeviceVk* device);
  ~VkSyncManager() = default;

  // Barrier recording
  void AddMemoryBarrier(const VkMemoryBarrier& barrier);
  void AddImageBarrier(const VkImageBarrier& barrier);
  void AddBufferBarrier(const VkBufferBarrier& barrier);

  // Execute all recorded barriers
  void ExecuteBarriers(VkCommandBuffer cmd_buffer);

  // Clear recorded barriers
  void Reset();

  // Utility functions for common transitions
  static VkImageBarrier CreateImageTransitionBarrier(
      VkImage image, VkImageLayout old_layout, VkImageLayout new_layout,
      VkImageAspectFlags aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT);

  static VkBufferBarrier CreateBufferBarrier(VkBuffer buffer,
                                             VkAccessFlags src_access,
                                             VkAccessFlags dst_access,
                                             VkDeviceSize offset = 0,
                                             VkDeviceSize size = VK_WHOLE_SIZE);

 private:
  GPUDeviceVk* device_ = nullptr;

  std::vector<VkMemoryBarrier2KHR> memory_barriers_;
  std::vector<VkImageMemoryBarrier2KHR> image_barriers_;
  std::vector<VkBufferMemoryBarrier2KHR> buffer_barriers_;

  VkPipelineStageFlags src_stage_mask_ = 0;
  VkPipelineStageFlags dst_stage_mask_ = 0;
};

}  // namespace skity

#endif  // SRC_GPU_VK_SYNC_OBJECTS_VK_HPP