// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_GPU_GPU_CONTEXT_VK_HPP
#define INCLUDE_SKITY_GPU_GPU_CONTEXT_VK_HPP

#include <vulkan/vulkan.h>

#include <skity/gpu/gpu_context.hpp>
#include <skity/gpu/gpu_surface.hpp>
#include <skity/gpu/texture.hpp>
#include <skity/macros.hpp>

namespace skity {

/**
 * Info struct to pass Vulkan info to create GPUContext.
 */
struct GPUContextInfoVK {
  /**
   * User provided Vulkan instance. The user is responsible for picking required
   * extensions like `VK_KHR_surface`.
   *
   * @note The user is responsible for destroying the instance.
   */
  VkInstance instance = VK_NULL_HANDLE;

  /**
   * procedure address loader to load Vulkan instance procedures.
   *
   * @note can not be nullptr.
   */
  PFN_vkGetInstanceProcAddr get_instance_proc_addr = nullptr;

  /**
   * Enabled Vulkan instance extensions for the provided or to-be-created
   * instance.
   *
   * For user provided instance, Vulkan does not expose a way to query enabled
   * instance extensions from a live handle, so caller should provide them when
   * this information matters to later rendering logic.
   */
  const char* const* enabled_instance_extensions = nullptr;

  /**
   * Count of `enabled_instance_extensions`.
   */
  uint32_t enabled_instance_extension_count = 0;

  /**
   * Whether `enabled_instance_extensions` fully describes the enabled
   * extensions for the instance.
   */
  bool enabled_instance_extensions_known = false;

  /**
   * User provided Vulkan physical device.
   *
   * If nullptr, the engine will use the first physical device if available.
   */
  VkPhysicalDevice physical_device = VK_NULL_HANDLE;

  /**
   * User provided Vulkan logical device.
   * If user provides logical device, needs to load some useful extensions
   * `VK_KHR_timeline_semaphore` or `VK_KHR_external_memory`.
   *
   * If nullptr, the engine will create a logical device and load required
   * extensions.
   *
   * @note The user is responsible for destroying the device.
   */
  VkDevice logical_device = VK_NULL_HANDLE;

  /**
   * procedure address loader to load Vulkan logical device procedures.
   *
   * @note can not be nullptr.
   */
  PFN_vkGetDeviceProcAddr get_device_proc_addr = nullptr;

  /**
   * Enabled Vulkan device extensions for the provided or to-be-created device.
   *
   * For user provided logical device, Vulkan does not expose a way to query
   * enabled device extensions from a live handle, so caller should provide
   * them when this information matters to later rendering logic.
   *
   * @note if some advance extensions are in this list, the engine assumed that
   * the corresponding feature was already enabled when the device was created.
   * For example, `VK_KHR_synchronization2` needs to put
   * VkPhysicalDeviceSynchronization2Features in device create info.
   */
  const char* const* enabled_device_extensions = nullptr;

  /**
   * Count of `enabled_device_extensions`.
   */
  uint32_t enabled_device_extension_count = 0;

  /**
   * Whether `enabled_device_extensions` fully describes the enabled
   * extensions for the device.
   */
  bool enabled_device_extensions_known = false;

  /**
   * User provided Vulkan graphics queue.
   *
   * If nullptr, the engine will use the first graphics queue if available.
   */
  VkQueue graphics_queue = VK_NULL_HANDLE;

  /**
   * User provided Vulkan graphics queue family index.
   *
   * If -1, the engine will use the first graphics queue family index.
   */
  int32_t graphics_queue_family_index = -1;

  /**
   * User provided Vulkan compute queue.
   *
   * If nullptr, the engine will use the first compute queue if available.
   */
  VkQueue compute_queue = VK_NULL_HANDLE;

  /**
   * User provided Vulkan compute queue family index.
   *
   * If -1, the engine will use the first compute queue family index.
   */
  int32_t compute_queue_family_index = -1;

  /**
   * User provided Vulkan transfer queue.
   *
   * If nullptr, the engine will use the first transfer queue if available.
   */
  VkQueue transfer_queue = VK_NULL_HANDLE;

  /**
   * User provided Vulkan transfer queue family index.
   *
   * If -1, the engine will use the first transfer queue family index.
   */
  int32_t transfer_queue_family_index = -1;

  /**
   * Enable Vulkan debug runtime features for engine-created instances.
   *
   * When enabled in a Debug build, Skity will try to enable
   * `VK_EXT_debug_utils` and `VK_LAYER_KHRONOS_validation` independently.
   *
   * If the validation layer is unavailable but `VK_EXT_debug_utils` is
   * available, the engine will still enable `VK_EXT_debug_utils` so debug
   * labels and related utilities can still be used.
   *
   * In non-Debug builds this hint is compiled out and has no effect.
   *
   * This flag is ignored for user provided Vulkan instances because those
   * layers and extensions must already be chosen during instance creation.
   */
  bool enable_debug_runtime = false;
};

/**
 * Create GPUContext with Vulkan info.
 *
 * The engine needs vulkan at least version 1.1.
 * If user provides logical device, needs to load some useful extensions.
 * For example:
 * - `VK_KHR_timeline_semaphore` is required for timeline semaphore.
 * - `VK_KHR_external_memory` for external memory.
 * - `VK_KHR_dynamic_rendering` for simplified render-pass creation.
 */
std::unique_ptr<GPUContext> SKITY_API
CreateGPUContextVK(const GPUContextInfoVK* info);

/**
 * Create GPUContext with Vulkan instance procedure address loader.
 *
 * When using this function, the engine will create a Vulkan instance and handle
 * all initialization.
 * But the instance and device are not available for outside use. Which means
 * the engine can not be mixed with other Vulkan code.
 */
std::unique_ptr<GPUContext> SKITY_API
CreateGPUContextVK(PFN_vkGetInstanceProcAddr get_instance_proc_addr);

}  // namespace skity

#endif  // INCLUDE_SKITY_GPU_GPU_CONTEXT_VK_HPP
