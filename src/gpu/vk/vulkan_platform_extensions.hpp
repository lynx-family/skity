// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_VK_VULKAN_PLATFORM_EXTENSIONS_HPP
#define SRC_GPU_VK_VULKAN_PLATFORM_EXTENSIONS_HPP

#include <vulkan/vulkan.h>

#include <string>
#include <vector>

namespace skity {

void EnablePlatformSurfaceInstanceExtensions(
    const std::vector<VkExtensionProperties>& available_instance_extensions,
    std::vector<std::string>* enabled_instance_extensions);

}  // namespace skity

#endif  // SRC_GPU_VK_VULKAN_PLATFORM_EXTENSIONS_HPP
