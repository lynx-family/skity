// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_CONTEXT_VK_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_CONTEXT_VK_H

#include <skity_c/skity_base.h>
#include <skity_c/skity_context.h>
#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a GPUContext targeting the Vulkan backend.
 *
 * The engine creates and owns its own VkInstance / VkDevice, so the resulting
 * context cannot be mixed with other Vulkan code. The instance and device are
 * not available for outside use.
 *
 * Native Vulkan types (PFN_vkGetInstanceProcAddr) are reused directly because
 * they are already C and ABI-stable.
 *
 * Only available when skity is built with SKITY_VK_BACKEND=ON.
 *
 * @param get_instance_proc_addr  instance procedure-address loader
 * @param out_context             receives the new context handle on success
 * @return SKITY_SUCCESS on success, or a SKITY_ERROR_* code on failure
 */
SKITY_C_API skity_result
skity_context_create_vk(PFN_vkGetInstanceProcAddr get_instance_proc_addr,
                        skity_context* out_context);

/**
 * @brief Descriptor for creating a Vulkan context from caller-provided Vulkan
 * state.
 *
 * Each native Vulkan handle is optional: passing NULL lets skity create and own
 * that object. Caller-provided handles must stay valid for the lifetime of the
 * resulting context and remain owned by the caller.
 */
typedef struct skity_context_create_info_vk {
  /**
   * Vulkan instance. If NULL, skity creates and owns one.
   *
   * @note The caller is responsible for enabling required instance extensions
   * (for example VK_KHR_surface) and for destroying the instance.
   */
  VkInstance instance;

  /**
   * Loader for instance-level Vulkan procedures.
   *
   * @note Must not be NULL.
   */
  PFN_vkGetInstanceProcAddr get_instance_proc_addr;

  /**
   * Instance extensions enabled on `instance`.
   *
   * For a caller-provided instance this should list the extensions actually
   * enabled. For an engine-created instance these entries are treated as
   * additional requested extensions and are merged with skity's defaults.
   */
  const char* const* enabled_instance_extensions;

  /** Number of entries in `enabled_instance_extensions`. */
  uint32_t enabled_instance_extension_count;

  /**
   * Nonzero when `enabled_instance_extensions` fully describes the enabled
   * instance extensions.
   *
   * Only meaningful for caller-provided instances.
   */
  uint32_t enabled_instance_extensions_known;

  /**
   * Vulkan physical device. If NULL, skity picks the first available device.
   */
  VkPhysicalDevice physical_device;

  /**
   * Vulkan logical device. If NULL, skity creates and owns one.
   *
   * When supplied, the device must have been created with the extensions skity
   * needs (for example VK_KHR_timeline_semaphore or VK_KHR_external_memory).
   *
   * @note The caller remains responsible for destroying the device.
   */
  VkDevice logical_device;

  /**
   * Loader for device-level Vulkan procedures.
   *
   * @note Must not be NULL when `logical_device` is supplied.
   */
  PFN_vkGetDeviceProcAddr get_device_proc_addr;

  /**
   * Device extensions enabled on `logical_device`.
   *
   * For a caller-provided device this should list the extensions actually
   * enabled. For an engine-created device these entries are treated as
   * additional requested extensions.
   */
  const char* const* enabled_device_extensions;

  /** Number of entries in `enabled_device_extensions`. */
  uint32_t enabled_device_extension_count;

  /**
   * Nonzero when `enabled_device_extensions` fully describes the enabled
   * device extensions.
   *
   * Only meaningful for caller-provided devices.
   */
  uint32_t enabled_device_extensions_known;

  /**
   * Nonzero when `logical_device` was created with the core `dualSrcBlend`
   * feature enabled.
   *
   * Vulkan cannot query enabled core features from an existing device, so this
   * must be declared explicitly. Ignored when skity creates the device.
   */
  uint32_t dual_source_blending_enabled;

  /** Graphics queue. If NULL, skity picks the first graphics queue. */
  VkQueue graphics_queue;

  /**
   * Graphics queue family index. If -1, skity picks the first graphics queue
   * family index.
   */
  int32_t graphics_queue_family_index;

  /** Compute queue. If NULL, skity picks the first compute queue. */
  VkQueue compute_queue;

  /**
   * Compute queue family index. If -1, skity picks the first compute queue
   * family index.
   */
  int32_t compute_queue_family_index;

  /** Transfer queue. If NULL, skity picks the first transfer queue. */
  VkQueue transfer_queue;

  /**
   * Transfer queue family index. If -1, skity picks the first transfer queue
   * family index.
   */
  int32_t transfer_queue_family_index;

  /**
   * Nonzero to enable Vulkan debug runtime (for example VK_EXT_debug_utils and
   * the Khronos validation layer) for engine-created instances.
   *
   * Ignored for caller-provided instances and in non-Debug builds.
   */
  uint32_t enable_debug_runtime;
} skity_context_create_info_vk;

/**
 * @brief Create a GPUContext using caller-provided Vulkan state where
 * available.
 *
 * Passing NULL for instance/device lets skity create those objects. When a
 * logical device is supplied, its proc-address loader must also be supplied;
 * the caller remains responsible for destroying instance, device, and queues.
 *
 * @param info  creation descriptor
 * @param out_context receives the new context handle on success
 */
SKITY_C_API skity_result skity_context_create_vk_ex(
    const skity_context_create_info_vk* info, skity_context* out_context);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_CONTEXT_VK_H
