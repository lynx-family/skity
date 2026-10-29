// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_PRESENTER_VK_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_PRESENTER_VK_H

#include <skity/c/skity_presenter.h>
#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Vulkan native window kind. Values align with VKNativeWindowType. */
typedef enum {
  SKITY_VK_NATIVE_WINDOW_TYPE_INVALID = 0,
  SKITY_VK_NATIVE_WINDOW_TYPE_WIN32,
  SKITY_VK_NATIVE_WINDOW_TYPE_ANDROID,
  SKITY_VK_NATIVE_WINDOW_TYPE_METAL_LAYER,
  SKITY_VK_NATIVE_WINDOW_TYPE_WAYLAND,
  SKITY_VK_NATIVE_WINDOW_TYPE_XCB,
  SKITY_VK_NATIVE_WINDOW_TYPE_XLIB,
} skity_vk_native_window_type;

/** @brief Platform-neutral native window description for Vulkan surfaces. */
typedef struct skity_vk_native_window_info {
  skity_vk_native_window_type type;
  void* handle;
  void* secondary_handle;
  uint64_t window_id;
} skity_vk_native_window_info;

/**
 * @brief Vulkan presenter extension. Chain via
 *        skity_presenter_create_info::p_next with s_type =
 *        SKITY_STRUCTURE_TYPE_PRESENTER_CREATE_INFO_VK; the base info
 *        carries the physical size.
 */
typedef struct skity_presenter_create_info_vk {
  skity_structure_type
      s_type;         /**< SKITY_STRUCTURE_TYPE_PRESENTER_CREATE_INFO_VK */
  const void* p_next; /**< reserved for future extensions */
  skity_vk_native_window_info native_window; /**< the window to present on */
  VkQueue present_queue;                     /**< queue that will present */
  int32_t present_queue_family_index;
  uint32_t min_image_count;    /**< swapchain image count */
  VkFormat format;             /**< swapchain image format */
  VkColorSpaceKHR color_space; /**< swapchain color space */
  VkPresentModeKHR present_mode;
  VkCompositeAlphaFlagBitsKHR composite_alpha;
  VkSurfaceTransformFlagBitsKHR pre_transform;
  uint32_t clipped; /**< non-zero to clip to the window bounds */
} skity_presenter_create_info_vk;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_PRESENTER_VK_H
