// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/macros.hpp>

#if defined(SKITY_ANDROID)

#include "src/gpu/vk/gpu_external_texture_ahb.hpp"
#include "src/gpu/vk/vulkan_context_state.hpp"
#include "src/logging.hpp"

namespace skity {

GPUExternalTextureAHB::GPUExternalTextureAHB(
    std::shared_ptr<const VulkanContextState> state,
    const GPUTextureDescriptor& descriptor, VkImage image,
    VkDeviceMemory memory, VkImageView image_view, VkImageLayout initial_layout,
    VkImageLayout preferred_layout, VkFormat format)
    : GPUTextureVK(state, descriptor, image,
                   /*allocation=*/nullptr, image_view, preferred_layout, format,
                   /*owns_image=*/false, /*owns_image_view=*/false),
      state_(std::move(state)),
      memory_(memory) {
  SetCurrentLayout(initial_layout);
}

GPUExternalTextureAHB::~GPUExternalTextureAHB() {
  if (state_ == nullptr || state_->GetLogicalDevice() == VK_NULL_HANDLE) {
    return;
  }

  VkDevice device = state_->GetLogicalDevice();
  const auto& fns = state_->DeviceFns();

  if (GetImageView() != VK_NULL_HANDLE && fns.vkDestroyImageView != nullptr) {
    fns.vkDestroyImageView(device, GetImageView(), nullptr);
  }

  if (GetImage() != VK_NULL_HANDLE && fns.vkDestroyImage != nullptr) {
    fns.vkDestroyImage(device, GetImage(), nullptr);
  }

  if (memory_ != VK_NULL_HANDLE && fns.vkFreeMemory != nullptr) {
    fns.vkFreeMemory(device, memory_, nullptr);
  }
}

std::shared_ptr<GPUTexture> GPUExternalTextureAHB::Make(
    std::shared_ptr<const VulkanContextState> state,
    const GPUTextureDescriptor& descriptor, VkImage image,
    VkDeviceMemory memory, VkImageView image_view, VkImageLayout initial_layout,
    VkImageLayout preferred_layout, VkFormat format) {
  if (state == nullptr || image == VK_NULL_HANDLE ||
      image_view == VK_NULL_HANDLE || format == VK_FORMAT_UNDEFINED) {
    LOGE("GPUExternalTextureAHB::Make: invalid parameters");
    return {};
  }

  return std::shared_ptr<GPUTexture>(new GPUExternalTextureAHB(
      std::move(state), descriptor, image, memory, image_view, initial_layout,
      preferred_layout, format));
}

}  // namespace skity

#endif  // defined(SKITY_ANDROID)
