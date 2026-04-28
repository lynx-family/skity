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
   * When enabled in a Debug build, the engine will try to enable
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
 * @enum VKSurfaceType indicates which Vulkan target a GPUSurface is backed by.
 */
enum class VKSurfaceType {
  /**
   * empty type, default value
   */
  kInvalid,
  /**
   * Indicate the Surface targets a user provided Vulkan image.
   *
   * @note The image must be renderable with the supplied format and image view.
   */
  kTexture,
  /**
   * Indicate the Surface targets a swapchain image acquired for the current
   * frame.
   *
   * @note Presentation and swapchain lifecycle should be managed by a higher
   * level Vulkan presenter rather than the Surface itself.
   */
  kSwapchainImage,
};

struct GPUSurfaceSyncInfoVK {
  /**
   * Optional semaphore that must be signaled before the engine starts
   * rendering to this Surface.
   */
  VkSemaphore wait_semaphore = VK_NULL_HANDLE;

  /**
   * The earliest pipeline stage that waits on `wait_semaphore`.
   */
  VkPipelineStageFlags wait_dst_stage_mask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  /**
   * Optional semaphore signaled after the engine finishes rendering to this
   * Surface.
   */
  VkSemaphore signal_semaphore = VK_NULL_HANDLE;

  /**
   * Optional fence signaled after the engine finishes rendering to this
   * Surface.
   */
  VkFence signal_fence = VK_NULL_HANDLE;
};

struct GPUSurfaceDescriptorVK : public GPUSurfaceDescriptor {
  VKSurfaceType surface_type = VKSurfaceType::kInvalid;

  /**
   * User provided Vulkan image used as the render target for this Surface.
   *
   * @note If `surface_type` is `VKSurfaceType::kSwapchainImage`, this should be
   * the image acquired for the current frame.
   */
  VkImage image = VK_NULL_HANDLE;

  /**
   * User provided image view matching `image`.
   *
   * @note The image view should be compatible with `format`.
   */
  VkImageView image_view = VK_NULL_HANDLE;

  /**
   * Vulkan format of the render target image.
   */
  VkFormat format = VK_FORMAT_UNDEFINED;

  /**
   * Optional pre-transform for the current frame target.
   *
   * This is especially useful when `image` comes from a swapchain image whose
   * physical orientation differs from the logical surface coordinate system.
   */
  VkSurfaceTransformFlagBitsKHR pre_transform =
      VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;

  /**
   * Current image layout when the engine begins rendering to this Surface.
   */
  VkImageLayout initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;

  /**
   * Image layout should transition to after rendering finishes.
   */
  VkImageLayout final_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  /**
   * Whether the engine owns `image` and should destroy it when the Surface is
   * released.
   */
  bool owns_image = false;

  /**
   * Whether the engine owns `image_view` and should destroy it when the Surface
   * is released.
   */
  bool owns_image_view = false;

  /**
   * Optional Vulkan synchronization information for this one-shot Surface.
   *
   * The pointed synchronization object is not owned by the engine and must
   * outlive the create/flush sequence that consumes it.
   */
  const GPUSurfaceSyncInfoVK* sync_info = nullptr;
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
