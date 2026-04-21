// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/vk/vulkan_pending_submission.hpp"

#include "src/gpu/vk/gpu_buffer_vk.hpp"

namespace skity {

VulkanPendingSubmission::VulkanPendingSubmission(
    VkFence fence_in, VkCommandPool command_pool_in,
    std::vector<std::unique_ptr<GPUBufferVK>> stage_buffers_in)
    : fence(fence_in),
      command_pool(command_pool_in),
      stage_buffers(std::move(stage_buffers_in)) {}

VulkanPendingSubmission::VulkanPendingSubmission(
    VulkanPendingSubmission&& other) noexcept = default;

VulkanPendingSubmission& VulkanPendingSubmission::operator=(
    VulkanPendingSubmission&& other) noexcept = default;

VulkanPendingSubmission::~VulkanPendingSubmission() = default;

}  // namespace skity
