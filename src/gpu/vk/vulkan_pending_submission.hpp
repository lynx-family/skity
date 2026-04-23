// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_VK_VULKAN_PENDING_SUBMISSION_HPP
#define SRC_GPU_VK_VULKAN_PENDING_SUBMISSION_HPP

#include <vulkan/vulkan.h>

#include <functional>
#include <memory>
#include <vector>

namespace skity {

class GPUBufferVK;

struct VulkanPendingSubmission {
  VulkanPendingSubmission() = default;
  VulkanPendingSubmission(
      VkFence fence, VkCommandPool command_pool,
      std::vector<std::unique_ptr<GPUBufferVK>> stage_buffers,
      std::vector<std::function<void()>> cleanup_actions = {});
  VulkanPendingSubmission(VulkanPendingSubmission&& other) noexcept;
  VulkanPendingSubmission& operator=(VulkanPendingSubmission&& other) noexcept;
  ~VulkanPendingSubmission();

  VulkanPendingSubmission(const VulkanPendingSubmission&) = delete;
  VulkanPendingSubmission& operator=(const VulkanPendingSubmission&) = delete;

  VkFence fence = VK_NULL_HANDLE;
  VkCommandPool command_pool = VK_NULL_HANDLE;
  std::vector<std::unique_ptr<GPUBufferVK>> stage_buffers = {};
  std::vector<std::function<void()>> cleanup_actions = {};
};

}  // namespace skity

#endif  // SRC_GPU_VK_VULKAN_PENDING_SUBMISSION_HPP
