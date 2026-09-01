// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_SURFACE_VK_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_SURFACE_VK_H

#include <skity_c/skity_surface.h>
#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Vulkan render target kind. */
typedef enum {
  SKITY_VK_SURFACE_TYPE_INVALID = 0,
  SKITY_VK_SURFACE_TYPE_TEXTURE,
  SKITY_VK_SURFACE_TYPE_SWAPCHAIN_IMAGE,
} skity_vk_surface_type;

/**
 * @brief External synchronization for a one-shot Vulkan surface.
 *
 * The semaphore and fence handles remain owned by the caller and must outlive
 * the surface create/flush sequence.
 */
typedef struct skity_surface_sync_info_vk {
  VkSemaphore wait_semaphore;
  VkPipelineStageFlags wait_dst_stage_mask;
  VkSemaphore signal_semaphore;
  VkFence signal_fence;
} skity_surface_sync_info_vk;

/**
 * @brief Vulkan backend extension for surface creation. Chain it through
 *        skity_surface_create_info::p_next with s_type =
 *        SKITY_STRUCTURE_TYPE_SURFACE_CREATE_INFO_VK.
 */
typedef struct skity_surface_create_info_vk {
  skity_structure_type s_type;
  const void* p_next;
  skity_vk_surface_type surface_type;
  VkImage image;
  VkImageView image_view;
  VkFormat format;
  VkImageUsageFlags image_usage;
  VkSurfaceTransformFlagBitsKHR pre_transform;
  VkImageLayout initial_layout;
  VkImageLayout final_layout;
  uint32_t owns_image;
  uint32_t owns_image_view;
  const skity_surface_sync_info_vk* sync_info;
} skity_surface_create_info_vk;

/** @brief Opaque handle to a skity::GPUSemaphore. */
typedef struct skity_semaphore_s* skity_semaphore;

/**
 * @brief Add an external wait semaphore to the next frame. Must be called
 *        between skity_surface_lock_canvas and skity_surface_flush.
 */
SKITY_C_API void skity_surface_add_external_wait_semaphore_vk(
    skity_surface surface, skity_semaphore semaphore);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_SURFACE_VK_H
