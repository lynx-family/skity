// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/vk/gpu_semaphore_vk.hpp"

#include "src/gpu/vk/vulkan_context_state.hpp"

namespace skity {

GPUSemaphoreVK::GPUSemaphoreVK(std::shared_ptr<const VulkanContextState> state,
                               VkSemaphore semaphore)
    : GPUSemaphore(GPUBackendType::kVulkan),
      state_(std::move(state)),
      semaphore_(semaphore) {}

GPUSemaphoreVK::~GPUSemaphoreVK() {
  if (semaphore_ != VK_NULL_HANDLE && state_) {
    state_->DeviceFns().vkDestroySemaphore(state_->GetLogicalDevice(),
                                           semaphore_, nullptr);
  }
}

}  // namespace skity
