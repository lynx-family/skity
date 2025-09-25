// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_VK_VK_INTERFACE_HPP
#define SRC_GPU_VK_VK_INTERFACE_HPP

#include <volk.h>

#include <memory>
#include <vector>

namespace skity {

/**
 * Vulkan interface wrapper that manages instance and function loading
 */
class VkInterface {
 public:
  VkInterface();
  ~VkInterface();

  /**
   * Initialize Vulkan loader and create instance
   */
  bool Init();

  /**
   * Get the Vulkan instance
   */
  VkInstance GetInstance() const { return instance_; }

  /**
   * Get available physical devices
   */
  const std::vector<VkPhysicalDevice>& GetPhysicalDevices() const {
    return physical_devices_;
  }

  /**
   * Select the best physical device for rendering
   */
  VkPhysicalDevice SelectBestPhysicalDevice() const;

  /**
   * Check if validation layers are available
   */
  bool IsValidationLayersAvailable() const;

  /**
   * Get required instance extensions
   */
  std::vector<const char*> GetRequiredInstanceExtensions() const;

  /**
   * Get required device extensions
   */
  std::vector<const char*> GetRequiredDeviceExtensions() const;

  /**
   * Get required device extensions for a specific physical device
   */
  std::vector<const char*> GetRequiredDeviceExtensions(
      VkPhysicalDevice device) const;

 private:
  bool CreateInstance();
  void EnumeratePhysicalDevices();
  bool CheckValidationLayerSupport() const;

 private:
  VkInstance instance_ = VK_NULL_HANDLE;
  std::vector<VkPhysicalDevice> physical_devices_;
  bool validation_layers_enabled_ = false;

  static const std::vector<const char*> kValidationLayers;
};

/**
 * Get global Vulkan interface instance
 */
VkInterface* GetVkInterface();

}  // namespace skity

#endif  // SRC_GPU_VK_VK_INTERFACE_HPP