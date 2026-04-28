// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/vk/gpu_sampler_vk.hpp"

#include "src/gpu/vk/vulkan_context_state.hpp"

namespace skity {

GPUSamplerVK::GPUSamplerVK(std::shared_ptr<const VulkanContextState> state,
                           const GPUSamplerDescriptor& desc, VkSampler sampler)
    : GPUSampler(desc), state_(std::move(state)), sampler_(sampler) {}

GPUSamplerVK::~GPUSamplerVK() {
  if (state_ == nullptr || state_->GetLogicalDevice() == VK_NULL_HANDLE ||
      sampler_ == VK_NULL_HANDLE) {
    return;
  }

  if (state_->DeviceFns().vkDestroySampler != nullptr) {
    state_->DeviceFns().vkDestroySampler(state_->GetLogicalDevice(), sampler_,
                                         nullptr);
  }
  sampler_ = VK_NULL_HANDLE;
}

}  // namespace skity
