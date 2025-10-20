// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_VK_FORMATS_VK_H
#define SRC_GPU_VK_FORMATS_VK_H

#include <volk.h>

#include "skity/graphic/color_type.hpp"
#include "src/gpu/gpu_texture.hpp"

namespace skity {

/**
 * Convert Skity GPU texture format to Vulkan format
 */
VkFormat GPUTextureFormatToVkFormat(GPUTextureFormat format);

/**
 * Convert Skity color type to Vulkan format
 */
VkFormat ColorTypeToVkFormat(ColorType color_type);

/**
 * Get the number of bytes per pixel for a Vulkan format
 */
uint32_t VkFormatBytesPerPixel(VkFormat format);

/**
 * Check if a Vulkan format is supported as a render target
 */
bool IsVkFormatRenderTargetSupported(VkFormat format,
                                     VkPhysicalDevice physical_device);

/**
 * Get optimal tiling mode for a format
 */
VkImageTiling GetOptimalTiling(VkFormat format,
                               VkPhysicalDevice physical_device,
                               VkFormatFeatureFlags features);

/**
 * Convert GPU texture usage to Vulkan image usage flags
 */
VkImageUsageFlags GPUTextureUsageToVkImageUsage(GPUTextureUsageMask usage);

/**
 * Convert GPU texture usage to Vulkan image usage flags with format information
 */
VkImageUsageFlags GPUTextureUsageToVkImageUsage(GPUTextureUsageMask usage,
                                                GPUTextureFormat format);

}  // namespace skity

#endif  // SRC_GPU_VK_FORMATS_VK_H