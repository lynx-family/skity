// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_GPU_GPU_CONTEXT_VK_HPP
#define INCLUDE_SKITY_GPU_GPU_CONTEXT_VK_HPP

#include <skity/gpu/gpu_context.hpp>
#include <skity/gpu/gpu_surface.hpp>
#include <skity/gpu/texture.hpp>
#include <skity/macros.hpp>

namespace skity {

/**
 * @enum VkSurfaceType indicates which type the Vulkan backend Surface targets
 */
enum class VkSurfaceType {
  /**
   * empty type, default value
   */
  kInvalid,
  /**
   * Indicate the Surface targets a Vulkan image
   */
  kImage,
  /**
   * Indicate the Surface targets a Vulkan swapchain for on-screen rendering
   */
  kSwapchain,
};

struct GPUSurfaceDescriptorVk : public GPUSurfaceDescriptor {
  VkSurfaceType surface_type = VkSurfaceType::kInvalid;

  /**
   * Platform-specific surface handle for swapchain creation
   * This should be set when surface_type is kSwapchain
   */
  void* native_surface = nullptr;

  /**
   * Vulkan image handle when surface_type is kImage
   */
  uint64_t vk_image = 0;

  /**
   * Vulkan image format
   */
  uint32_t vk_format = 0;
};

struct GPUBackendTextureInfoVk : public GPUBackendTextureInfo {
  /**
   * Vulkan image handle
   */
  uint64_t vk_image = 0;

  /**
   * Vulkan image format
   */
  uint32_t vk_format = 0;

  /**
   * Indicate whether the engine is responsible for destroying the image
   */
  bool owned_by_engine = false;
};

/**
 * Vulkan device selection preferences
 */
struct VkDevicePreferences {
  /**
   * Preferred device type (integrated, discrete, etc.)
   * VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU = 2
   * VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU = 1
   */
  uint32_t preferred_device_type = 2;  // Discrete GPU by default

  /**
   * Require specific Vulkan API version (encoded as VK_MAKE_VERSION)
   * 0 means use whatever is available
   */
  uint32_t required_api_version = 0;

  /**
   * Enable validation layers for debugging
   */
  bool enable_validation = false;

  /**
   * Custom instance extensions to enable
   */
  std::vector<const char*> instance_extensions = {};

  /**
   * Custom device extensions to enable
   */
  std::vector<const char*> device_extensions = {};
};

/**
 * Create a GPUContext instance targeting Vulkan backend with default settings.
 * Uses automatic device selection and standard configuration.
 *
 * @return GPUContext instance or null if creation failed
 */
std::unique_ptr<GPUContext> SKITY_API VkContextCreate();

/**
 * Create a GPUContext instance targeting Vulkan backend with custom
 * preferences. Allows fine-grained control over Vulkan instance and device
 * selection.
 *
 * @param preferences Device selection and configuration preferences
 * @return GPUContext instance or null if creation failed
 */
std::unique_ptr<GPUContext> SKITY_API
VkContextCreate(const VkDevicePreferences& preferences);

/**
 * Create a GPUContext instance targeting Vulkan backend using existing Vulkan
 * objects. Useful for integration with external Vulkan applications.
 *
 * @param instance   Existing VkInstance handle (as uint64_t to avoid Vulkan
 * headers)
 * @param device     Existing VkDevice handle
 * @param queue      Existing VkQueue handle for graphics operations
 * @param queue_family_index Graphics queue family index
 * @return GPUContext instance or null if creation failed
 */
std::unique_ptr<GPUContext> SKITY_API
VkContextCreateWithExisting(uint64_t instance, uint64_t device, uint64_t queue,
                            uint32_t queue_family_index);

/**
 * Check if Vulkan is available on the current system
 *
 * @return true if Vulkan is available, false otherwise
 */
bool SKITY_API IsVulkanAvailable();

/**
 * Get information about available Vulkan devices
 *
 * @param device_count Output parameter for number of devices found
 * @return Array of device names, or null if Vulkan is not available
 */
const char** SKITY_API VkGetAvailableDevices(uint32_t* device_count);

/**
 * Get the Vulkan instance handle from a Vulkan context
 *
 * @param context GPU context (must be a Vulkan context)
 * @return VkInstance handle as uint64_t, or 0 if context is not Vulkan
 */
uint64_t SKITY_API VkGetInstance(GPUContext* context);

}  // namespace skity

#endif  // INCLUDE_SKITY_GPU_GPU_CONTEXT_VK_HPP