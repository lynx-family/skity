#ifndef SRC_GPU_VK_VULKAN_PLATFORM_EXTENSIONS_HPP
#define SRC_GPU_VK_VULKAN_PLATFORM_EXTENSIONS_HPP

#include <string>
#include <vector>

#include <vulkan/vulkan.h>

namespace skity {

void EnablePlatformSurfaceInstanceExtensions(
    const std::vector<VkExtensionProperties>& available_instance_extensions,
    std::vector<std::string>* enabled_instance_extensions);

}  // namespace skity

#endif  // SRC_GPU_VK_VULKAN_PLATFORM_EXTENSIONS_HPP
