// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/vk/formats_vk.h"

namespace skity {

VkFormat GPUTextureFormatToVkFormat(GPUTextureFormat format) {
  switch (format) {
    case GPUTextureFormat::kR8Unorm:
      return VK_FORMAT_R8_UNORM;
    case GPUTextureFormat::kRGB8Unorm:
      return VK_FORMAT_R8G8B8_UNORM;
    case GPUTextureFormat::kRGB565Unorm:
      return VK_FORMAT_R5G6B5_UNORM_PACK16;
    case GPUTextureFormat::kRGBA8Unorm:
      return VK_FORMAT_R8G8B8A8_UNORM;
    case GPUTextureFormat::kBGRA8Unorm:
      return VK_FORMAT_B8G8R8A8_UNORM;
    case GPUTextureFormat::kStencil8:
      return VK_FORMAT_S8_UINT;
    case GPUTextureFormat::kDepth24Stencil8:
      return VK_FORMAT_D24_UNORM_S8_UINT;
    case GPUTextureFormat::kInvalid:
    default:
      return VK_FORMAT_UNDEFINED;
  }
}

VkFormat ColorTypeToVkFormat(ColorType color_type) {
  switch (color_type) {
    case ColorType::kRGBA:
      return VK_FORMAT_R8G8B8A8_UNORM;
    case ColorType::kBGRA:
      return VK_FORMAT_B8G8R8A8_UNORM;
    case ColorType::kRGB565:
      return VK_FORMAT_R5G6B5_UNORM_PACK16;
    case ColorType::kA8:
      return VK_FORMAT_R8_UNORM;
    case ColorType::kUnknown:
    default:
      return VK_FORMAT_UNDEFINED;
  }
}

uint32_t VkFormatBytesPerPixel(VkFormat format) {
  switch (format) {
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_S8_UINT:
      return 1;
    case VK_FORMAT_R5G6B5_UNORM_PACK16:
      return 2;
    case VK_FORMAT_R8G8B8_UNORM:
      return 3;
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_D24_UNORM_S8_UINT:
      return 4;
    default:
      return 4;  // Default to 4 bytes
  }
}

bool IsVkFormatRenderTargetSupported(VkFormat format,
                                     VkPhysicalDevice physical_device) {
  VkFormatProperties properties;
  vkGetPhysicalDeviceFormatProperties(physical_device, format, &properties);

  return (properties.optimalTilingFeatures &
          VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) ||
         (properties.optimalTilingFeatures &
          VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

VkImageTiling GetOptimalTiling(VkFormat format,
                               VkPhysicalDevice physical_device,
                               VkFormatFeatureFlags features) {
  VkFormatProperties properties;
  vkGetPhysicalDeviceFormatProperties(physical_device, format, &properties);

  if ((properties.optimalTilingFeatures & features) == features) {
    return VK_IMAGE_TILING_OPTIMAL;
  } else if ((properties.linearTilingFeatures & features) == features) {
    return VK_IMAGE_TILING_LINEAR;
  } else {
    return VK_IMAGE_TILING_OPTIMAL;  // Default to optimal, may require format
                                     // fallback
  }
}

VkImageUsageFlags GPUTextureUsageToVkImageUsage(GPUTextureUsageMask usage) {
  VkImageUsageFlags vk_usage = 0;

  if (usage & static_cast<uint32_t>(GPUTextureUsage::kCopySrc)) {
    vk_usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  }
  if (usage & static_cast<uint32_t>(GPUTextureUsage::kCopyDst)) {
    vk_usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  }
  if (usage & static_cast<uint32_t>(GPUTextureUsage::kTextureBinding)) {
    vk_usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
  }
  if (usage & static_cast<uint32_t>(GPUTextureUsage::kStorageBinding)) {
    vk_usage |= VK_IMAGE_USAGE_STORAGE_BIT;
  }
  if (usage & static_cast<uint32_t>(GPUTextureUsage::kRenderAttachment)) {
    vk_usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  }

  // Always add transfer destination for data uploads
  vk_usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

  return vk_usage;
}

VkImageUsageFlags GPUTextureUsageToVkImageUsage(GPUTextureUsageMask usage,
                                                GPUTextureFormat format) {
  VkImageUsageFlags vk_usage = 0;

  if (usage & static_cast<uint32_t>(GPUTextureUsage::kCopySrc)) {
    vk_usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  }
  if (usage & static_cast<uint32_t>(GPUTextureUsage::kCopyDst)) {
    vk_usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  }
  if (usage & static_cast<uint32_t>(GPUTextureUsage::kTextureBinding)) {
    vk_usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
  }
  if (usage & static_cast<uint32_t>(GPUTextureUsage::kStorageBinding)) {
    vk_usage |= VK_IMAGE_USAGE_STORAGE_BIT;
  }
  if (usage & static_cast<uint32_t>(GPUTextureUsage::kRenderAttachment)) {
    // Check if this is a depth/stencil format
    if (format == GPUTextureFormat::kDepth24Stencil8 ||
        format == GPUTextureFormat::kStencil8) {
      vk_usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    } else {
      vk_usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }
  }

  // Always add transfer destination for data uploads
  vk_usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

  return vk_usage;
}

}  // namespace skity