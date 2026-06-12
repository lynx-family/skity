// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_VK_GPU_EXTERNAL_TEXTURE_AHB_HPP
#define SRC_GPU_VK_GPU_EXTERNAL_TEXTURE_AHB_HPP

#if defined(SKITY_ANDROID)

#include <vulkan/vulkan.h>

#include <memory>

#include "src/gpu/vk/gpu_texture_vk.hpp"

namespace skity {

/**
 * GPUExternalTextureAHB wraps an AHardwareBuffer imported as a Vulkan texture.
 *
 * Unlike regular GPUTextureVK which uses VMA for memory management, this class
 * uses vkDestroyImage + vkFreeMemory for cleanup because the image/memory were
 * created via external memory import (not through VMA).
 */
class GPUExternalTextureAHB : public GPUTextureVK {
 public:
  static std::shared_ptr<GPUTexture> Make(
      std::shared_ptr<const VulkanContextState> state,
      const GPUTextureDescriptor& descriptor, VkImage image,
      VkDeviceMemory memory, VkImageView image_view,
      VkImageLayout initial_layout, VkImageLayout preferred_layout,
      VkFormat format);

  ~GPUExternalTextureAHB() override;

 private:
  GPUExternalTextureAHB(std::shared_ptr<const VulkanContextState> state,
                        const GPUTextureDescriptor& descriptor, VkImage image,
                        VkDeviceMemory memory, VkImageView image_view,
                        VkImageLayout initial_layout,
                        VkImageLayout preferred_layout, VkFormat format);

  std::shared_ptr<const VulkanContextState> state_;
  VkDeviceMemory memory_ = VK_NULL_HANDLE;
};

}  // namespace skity

#endif  // defined(SKITY_ANDROID)

#endif  // SRC_GPU_VK_GPU_EXTERNAL_TEXTURE_AHB_HPP
