// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_VK_VULKAN_DEBUG_RUNTIME_STATE_HPP
#define SRC_GPU_VK_VULKAN_DEBUG_RUNTIME_STATE_HPP

#include <vulkan/vulkan.h>

#include <string>
#include <vector>

namespace skity {

#if defined(SKITY_VK_DEBUG_RUNTIME)
struct VulkanDebugRuntimeState {
  std::vector<VkLayerProperties> available_instance_layers = {};
  std::vector<std::string> enabled_instance_layers = {};
  bool enabled_instance_layers_known = false;
  VkDebugUtilsMessengerEXT debug_utils_messenger = VK_NULL_HANDLE;
};
#else
struct VulkanDebugRuntimeState {};
#endif

}  // namespace skity

#endif  // SRC_GPU_VK_VULKAN_DEBUG_RUNTIME_STATE_HPP
