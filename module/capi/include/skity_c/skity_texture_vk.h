// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_TEXTURE_VK_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_TEXTURE_VK_H

#include <skity_c/skity_texture.h>
#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Vulkan backend extension for wrapping an existing VkImage. Chain it
 *        through skity_backend_texture_info::p_next with s_type =
 *        SKITY_STRUCTURE_TYPE_BACKEND_TEXTURE_INFO_VK.
 */
typedef struct skity_backend_texture_info_vk {
  skity_structure_type s_type;
  const void* p_next;
  VkImage image;
  VkImageView image_view;
  VkFormat vk_format;
  VkImageUsageFlags image_usage;
  VkImageLayout initial_layout;
  VkImageLayout final_layout;
  uint32_t owns_image;
  uint32_t owns_image_view;
} skity_backend_texture_info_vk;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_TEXTURE_VK_H
