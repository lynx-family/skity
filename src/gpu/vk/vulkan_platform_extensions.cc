// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/vk/vulkan_platform_extensions.hpp"

#include <skity/macros.hpp>
#include <string_view>

namespace skity {
namespace {

bool ContainsExtension(const std::vector<VkExtensionProperties>& extensions,
                       const char* extension_name) {
  if (extension_name == nullptr) {
    return false;
  }

  for (const auto& extension : extensions) {
    if (std::string_view(extension.extensionName) == extension_name) {
      return true;
    }
  }

  return false;
}

void TryEnableInstanceExtension(
    std::vector<std::string>* enabled_instance_extensions,
    const std::vector<VkExtensionProperties>& available_instance_extensions,
    const char* extension_name) {
  if (enabled_instance_extensions == nullptr || extension_name == nullptr) {
    return;
  }

  for (const auto& extension : *enabled_instance_extensions) {
    if (extension == extension_name) {
      return;
    }
  }

  if (!ContainsExtension(available_instance_extensions, extension_name)) {
    return;
  }

  enabled_instance_extensions->emplace_back(extension_name);
}

}  // namespace

void EnablePlatformSurfaceInstanceExtensions(
    const std::vector<VkExtensionProperties>& available_instance_extensions,
    std::vector<std::string>* enabled_instance_extensions) {
#if defined(SKITY_ANDROID)
  TryEnableInstanceExtension(enabled_instance_extensions,
                             available_instance_extensions,
                             "VK_KHR_android_surface");
#elif defined(SKITY_WIN)
  TryEnableInstanceExtension(enabled_instance_extensions,
                             available_instance_extensions,
                             "VK_KHR_win32_surface");
#elif defined(SKITY_MACOS) || defined(SKITY_IOS)
  TryEnableInstanceExtension(enabled_instance_extensions,
                             available_instance_extensions,
                             "VK_EXT_metal_surface");
  TryEnableInstanceExtension(enabled_instance_extensions,
                             available_instance_extensions,
                             "VK_MVK_macos_surface");
#elif defined(SKITY_LINUX)
  TryEnableInstanceExtension(enabled_instance_extensions,
                             available_instance_extensions,
                             "VK_KHR_wayland_surface");
  TryEnableInstanceExtension(enabled_instance_extensions,
                             available_instance_extensions,
                             "VK_KHR_xcb_surface");
  TryEnableInstanceExtension(enabled_instance_extensions,
                             available_instance_extensions,
                             "VK_KHR_xlib_surface");
#endif
}

}  // namespace skity
