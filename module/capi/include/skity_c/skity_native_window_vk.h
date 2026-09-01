// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_NATIVE_WINDOW_VK_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_NATIVE_WINDOW_VK_H

#include <skity_c/skity_base.h>
#include <skity_c/skity_context.h>
#include <skity_c/skity_context_vk.h>
#include <skity_c/skity_surface.h>
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

/** @brief Vulkan native window and swapchain creation descriptor. */
typedef struct skity_native_window_create_info_vk {
  skity_vk_native_window_info native_window;
  uint32_t width;
  uint32_t height;
  VkQueue present_queue;
  int32_t present_queue_family_index;
  uint32_t min_image_count;
  VkFormat format;
  VkColorSpaceKHR color_space;
  VkPresentModeKHR present_mode;
  VkCompositeAlphaFlagBitsKHR composite_alpha;
  VkSurfaceTransformFlagBitsKHR pre_transform;
  uint32_t clipped;
} skity_native_window_create_info_vk;

/** @brief Opaque handle to a Vulkan native window presenter. */
typedef struct skity_native_window_vk_s* skity_native_window_vk;

/** @brief Create a Vulkan native window and its initial swapchain. */
SKITY_C_API skity_native_window_vk skity_native_window_create_vk(
    skity_context context, const skity_native_window_create_info_vk* info);

/** @brief Destroy the native window presenter and its swapchain. */
SKITY_C_API void skity_native_window_destroy_vk(skity_native_window_vk window);

/** @brief Return the current physical presentation width in pixels. */
SKITY_C_API uint32_t
skity_native_window_get_width_vk(skity_native_window_vk window);

/** @brief Return the current physical presentation height in pixels. */
SKITY_C_API uint32_t
skity_native_window_get_height_vk(skity_native_window_vk window);

/**
 * @brief Recreate the presenter and swapchain for a new physical size.
 * @return SKITY_SUCCESS on success, or a SKITY_ERROR_* code on failure
 */
SKITY_C_API skity_result skity_native_window_resize_vk(
    skity_native_window_vk window, uint32_t width, uint32_t height);

/**
 * @brief Acquire the next one-shot render surface. It must be passed to
 *        skity_native_window_present_vk before acquiring another surface.
 */
SKITY_C_API skity_result skity_native_window_acquire_next_surface_vk(
    skity_native_window_vk window, uint32_t sample_count, float content_scale,
    skity_surface* out_surface);

/**
 * @brief Present a surface acquired from this window. On success or
 *        SKITY_ERROR_NEED_RECREATE the surface handle is consumed and must not
 *        be used or destroyed again.
 */
SKITY_C_API skity_result skity_native_window_present_vk(
    skity_native_window_vk window, skity_surface* surface);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_NATIVE_WINDOW_VK_H
